# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
# Copyright (c) 2025 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""Evaluation script for Clifford synthesis using MQT QMAP with SQLite database storage."""

from __future__ import annotations

import json
import logging
import multiprocessing as mp
import os
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from typing import Any

from qiskit.circuit.random import random_clifford_circuit
from qiskit.qpy import dump, load
from qiskit.quantum_info import Clifford
from qiskit.synthesis import synth_clifford_ag, synth_clifford_greedy
from sqlalchemy import Column, Float, Integer, create_engine, exists, select
from sqlalchemy.orm import Session, declarative_base

from mqt.qmap.plugins.qiskit.clifford_synthesis import optimize_clifford, synthesize_clifford

EVAL_ROOT = Path(__file__).resolve().parent
EVAL_ROOT.mkdir(parents=True, exist_ok=True)
CIRCUITS_DIR = EVAL_ROOT / "circuits"
CIRCUITS_DIR.mkdir(parents=True, exist_ok=True)

# Set up logging
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s - %(levelname)s - %(message)s",
    handlers=[logging.FileHandler(EVAL_ROOT / "synthesis.log"), logging.StreamHandler()],
)
logger = logging.getLogger(__name__)

# Database setup
Base = declarative_base()
engine = create_engine(
    f"sqlite:///{(EVAL_ROOT / 'synthesis_results.db').as_posix()}",
    echo=False,
    connect_args={"timeout": 30},
)


class SynthesisResult(Base):
    """Database model for synthesis results."""

    __tablename__ = "synthesis_results"

    id = Column(Integer, primary_key=True)
    num_qubits = Column(Integer)
    num_gates = Column(Integer)
    instance = Column(Integer)

    # Method results
    ag_runtime = Column(Float, nullable=True)
    ag_gate_count = Column(Integer, nullable=True)

    greedy_runtime = Column(Float, nullable=True)
    greedy_gate_count = Column(Integer, nullable=True)

    qmap_cliff_iter_runtime = Column(Float, nullable=True)
    qmap_cliff_iter_gate_count = Column(Integer, nullable=True)

    qmap_cliff_maxsat_runtime = Column(Float, nullable=True)
    qmap_cliff_maxsat_gate_count = Column(Integer, nullable=True)

    qmap_circ_iter_runtime = Column(Float, nullable=True)
    qmap_circ_iter_gate_count = Column(Integer, nullable=True)

    qmap_circ_maxsat_runtime = Column(Float, nullable=True)
    qmap_circ_maxsat_gate_count = Column(Integer, nullable=True)

    # Largest timeout (in seconds) tried per method
    ag_timeout_tried = Column(Integer, nullable=True)
    greedy_timeout_tried = Column(Integer, nullable=True)
    qmap_cliff_iter_timeout_tried = Column(Integer, nullable=True)
    qmap_cliff_maxsat_timeout_tried = Column(Integer, nullable=True)
    qmap_circ_iter_timeout_tried = Column(Integer, nullable=True)
    qmap_circ_maxsat_timeout_tried = Column(Integer, nullable=True)


# Create tables
Base.metadata.create_all(engine)


def _ensure_timeout_columns() -> None:
    """Ensure timeout tracking columns exist with default 0."""
    with engine.begin() as conn:
        rows = conn.exec_driver_sql("PRAGMA table_info(synthesis_results)").fetchall()
        existing = {row[1] for row in rows}
        cols = [
            "ag_timeout_tried",
            "greedy_timeout_tried",
            "qmap_cliff_iter_timeout_tried",
            "qmap_cliff_maxsat_timeout_tried",
            "qmap_circ_iter_timeout_tried",
            "qmap_circ_maxsat_timeout_tried",
        ]
        for name in cols:
            if name not in existing:
                conn.exec_driver_sql(f"ALTER TABLE synthesis_results ADD COLUMN {name} INTEGER DEFAULT 0")


def _backfill_timeout_columns() -> None:
    """Backfill timeout columns: 7200 for completed results, 0 for missing."""
    with engine.begin() as conn:
        # Set NULLs to 0
        for name in [
            "ag_timeout_tried",
            "greedy_timeout_tried",
            "qmap_cliff_iter_timeout_tried",
            "qmap_cliff_maxsat_timeout_tried",
            "qmap_circ_iter_timeout_tried",
            "qmap_circ_maxsat_timeout_tried",
        ]:
            conn.exec_driver_sql(f"UPDATE synthesis_results SET {name}=0 WHERE {name} IS NULL")  # noqa: S608
        # For completed methods, set to 7200 if less
        pairs = [
            ("ag_runtime", "ag_timeout_tried"),
            ("greedy_runtime", "greedy_timeout_tried"),
            ("qmap_cliff_iter_runtime", "qmap_cliff_iter_timeout_tried"),
            ("qmap_cliff_maxsat_runtime", "qmap_cliff_maxsat_timeout_tried"),
            ("qmap_circ_iter_runtime", "qmap_circ_iter_timeout_tried"),
            ("qmap_circ_maxsat_runtime", "qmap_circ_maxsat_timeout_tried"),
        ]
        for runtime_col, timeout_col in pairs:
            conn.exec_driver_sql(
                f"UPDATE synthesis_results SET {timeout_col}=7200 "  # noqa: S608
                f"WHERE {runtime_col} IS NOT NULL AND ({timeout_col} IS NULL OR {timeout_col} < 7200)"
            )


# Ensure DB schema is up-to-date
_ensure_timeout_columns()


def generate_test_circuits() -> None:
    """Generate test circuits and populate database with initial entries.

    Also prints a progress table indicating how many generated circuits already have results per method,
    grouped by number of qubits and number of gates.
    """
    logger.info("Starting test circuit generation...")
    eval_dir = CIRCUITS_DIR
    with Session(engine) as session:
        num_qubits_list = [1, 2, 3, 4, 5, 6]
        for num_qubits in num_qubits_list:
            logger.info(f"Generating circuits for {num_qubits} qubits...")
            num_gates_list = sorted(_allowed_num_gates(num_qubits))
            for num_gates in num_gates_list:
                logger.debug(f"Processing {num_gates} gates...")
                num_instances = 100
                for i in range(num_instances):
                    # Check if entry already exists
                    exists_query = select(
                        exists().where(
                            SynthesisResult.num_qubits == num_qubits,
                            SynthesisResult.num_gates == num_gates,
                            SynthesisResult.instance == i,
                        )
                    )

                    if session.scalar(exists_query):
                        continue
                    circuit = random_clifford_circuit(num_qubits=num_qubits, num_gates=num_gates, seed=42 + i)
                    # Save circuit
                    circuit_path = eval_dir.joinpath(f"circuit_{num_qubits}_{num_gates:03d}_{i:03d}.qpy")
                    with circuit_path.open("wb") as f:
                        dump(circuit, f)

                    # Create database entry
                    result = SynthesisResult(
                        num_qubits=num_qubits,
                        num_gates=num_gates,
                        instance=i,
                        ag_timeout_tried=0,
                        greedy_timeout_tried=0,
                        qmap_cliff_iter_timeout_tried=0,
                        qmap_cliff_maxsat_timeout_tried=0,
                        qmap_circ_iter_timeout_tried=0,
                        qmap_circ_maxsat_timeout_tried=0,
                    )
                    session.add(result)

                session.commit()
    logger.info("Test circuit generation completed")


# Mapping of method names to corresponding ORM field names
_METHOD_FIELDS: dict[str, tuple[str, str, str]] = {
    "ag": ("ag_runtime", "ag_gate_count", "ag_timeout_tried"),
    "greedy": ("greedy_runtime", "greedy_gate_count", "greedy_timeout_tried"),
    "qmap_from_clifford_iter": (
        "qmap_cliff_iter_runtime",
        "qmap_cliff_iter_gate_count",
        "qmap_cliff_iter_timeout_tried",
    ),
    "qmap_from_clifford_maxsat": (
        "qmap_cliff_maxsat_runtime",
        "qmap_cliff_maxsat_gate_count",
        "qmap_cliff_maxsat_timeout_tried",
    ),
    "qmap_from_circuit_iter": (
        "qmap_circ_iter_runtime",
        "qmap_circ_iter_gate_count",
        "qmap_circ_iter_timeout_tried",
    ),
    "qmap_from_circuit_maxsat": (
        "qmap_circ_maxsat_runtime",
        "qmap_circ_maxsat_gate_count",
        "qmap_circ_maxsat_timeout_tried",
    ),
}


def _compute_synthesis_for_method(circuit_file: Path, method: str) -> tuple[float, int]:
    """Compute synthesis for a single circuit and method.

    Returns (runtime_seconds, gate_count).
    """
    with circuit_file.open("rb") as cf:
        circuit = load(cf)[0]

    cliff_op = Clifford(circuit)
    start_time = time.time()
    if method == "ag":
        synth_result = synth_clifford_ag(cliff_op)
        runtime = time.time() - start_time
        return runtime, synth_result.size()
    if method == "greedy":
        synth_result = synth_clifford_greedy(cliff_op)
        runtime = time.time() - start_time
        return runtime, synth_result.size()
    if method == "qmap_from_clifford_iter":
        ag_circuit = synth_clifford_ag(cliff_op)
        greedy_circuit = synth_clifford_greedy(cliff_op)
        initial_timestep_limit = min(ag_circuit.size(), greedy_circuit.size())
        result_circuit, _ = synthesize_clifford(
            target_tableau=cliff_op,
            include_destabilizers=False,
            use_maxsat=False,
            initial_timestep_limit=initial_timestep_limit,
        )
        runtime = time.time() - start_time
        return runtime, result_circuit.size()
    if method == "qmap_from_clifford_maxsat":
        ag_circuit = synth_clifford_ag(cliff_op)
        greedy_circuit = synth_clifford_greedy(cliff_op)
        initial_timestep_limit = min(ag_circuit.size(), greedy_circuit.size())
        result_circuit, _ = synthesize_clifford(
            target_tableau=cliff_op,
            include_destabilizers=False,
            use_maxsat=True,
            initial_timestep_limit=initial_timestep_limit,
        )
        runtime = time.time() - start_time
        return runtime, result_circuit.size()
    if method == "qmap_from_circuit_iter":
        ag_circuit = synth_clifford_ag(cliff_op)
        greedy_circuit = synth_clifford_greedy(cliff_op)
        input_circuit = ag_circuit if ag_circuit.size() < greedy_circuit.size() else greedy_circuit
        result_circuit, _ = optimize_clifford(circuit=input_circuit, include_destabilizers=False, use_maxsat=False)
        runtime = time.time() - start_time
        return runtime, result_circuit.size()
    if method == "qmap_from_circuit_maxsat":
        ag_circuit = synth_clifford_ag(cliff_op)
        greedy_circuit = synth_clifford_greedy(cliff_op)
        input_circuit = ag_circuit if ag_circuit.size() < greedy_circuit.size() else greedy_circuit
        result_circuit, _ = optimize_clifford(circuit=input_circuit, include_destabilizers=False, use_maxsat=True)
        runtime = time.time() - start_time
        return runtime, result_circuit.size()

    msg = f"Unknown method: {method}"
    raise ValueError(msg)


def _child_worker(circuit_path: str, method: str, q: mp.Queue) -> None:
    try:
        runtime, gate_count = _compute_synthesis_for_method(Path(circuit_path), method)
        q.put(("ok", runtime, gate_count))
    except Exception as e:  # pragma: no cover - robust safety
        q.put(("err", repr(e)))


def _run_with_timeout_in_subprocess(
    circuit_file: Path, method: str, timeout_sec: float
) -> tuple[bool, float | None, int | None, str | None]:
    """Run synthesis in a child process with a hard timeout.

    Returns (success, runtime, gate_count, err_msg).
    """
    ctx = mp.get_context("spawn")
    q: mp.Queue = ctx.Queue()
    p = ctx.Process(target=_child_worker, args=(str(circuit_file), method, q))
    p.start()
    p.join(timeout_sec)
    if p.is_alive():
        try:
            p.terminate()
        finally:
            p.join()
        return False, None, None, "timeout"
    try:
        status, a, b = q.get_nowait()
    except Exception as e:
        return False, None, None, f"noresult:{e!r}"
    if status == "ok":
        return True, float(a), int(b), None
    return False, None, None, str(a)


def _evaluate_single_circuit(circuit_file: Path, method: str, session: Session, **kwargs: dict[str, Any]) -> None:
    """Evaluate a single circuit and update database with timeout handling."""
    parts = circuit_file.stem.split("_")
    num_qubits = int(parts[1])
    num_gates = int(parts[2])
    instance = int(parts[3])

    logger.debug(f"Evaluating circuit: qubits={num_qubits}, gates={num_gates}, instance={instance}, method={method}")

    # Get DB entry
    result = (
        session.query(SynthesisResult).filter_by(num_qubits=num_qubits, num_gates=num_gates, instance=instance).first()
    )
    if result is None:
        logger.warning(f"No database entry found for circuit {circuit_file.name}")
        return

    # Size limits
    num_qubits_limit = kwargs.get("num_qubits_limit", float("inf"))
    num_gates_limit = kwargs.get("num_gates_limit", float("inf"))
    if num_qubits > num_qubits_limit or num_gates > num_gates_limit:
        logger.debug(f"Skipping circuit due to size limits: qubits={num_qubits}, gates={num_gates}")
        return

    # Resolve fields
    if method not in _METHOD_FIELDS:
        logger.error(f"Unknown method: {method}")
        return
    runtime_field, gate_field, timeout_field = _METHOD_FIELDS[method]

    # Already completed? If yes, ensure timeout_tried is set to 7200 and skip
    already_done = getattr(result, runtime_field) is not None
    if already_done:
        current_t = getattr(result, timeout_field) or 0
        if current_t < 7200:
            setattr(result, timeout_field, 7200)
            session.commit()
        logger.debug(f"Results for method {method} already exist")
        return

    # Skip if this timeout (in seconds) has already been attempted
    timeout_sec = float(kwargs.get("timeout_sec", os.environ.get("MQT_EVAL_TIMEOUT_SEC", "1.0")))
    try_secs = int(timeout_sec)
    prior_try = getattr(result, timeout_field) or 0
    if prior_try >= try_secs:
        logger.debug(f"Skipping circuit because timeout {try_secs}s was already attempted (prior={prior_try}s)")
        return

    success, runtime, gate_count, err = _run_with_timeout_in_subprocess(circuit_file, method, timeout_sec)

    # Update timeout tried regardless
    setattr(result, timeout_field, max(prior_try, try_secs))

    if success and runtime is not None and gate_count is not None:
        setattr(result, runtime_field, float(runtime))
        setattr(result, gate_field, int(gate_count))
        # Mark completed entries with 7200 as per spec
        if getattr(result, timeout_field) < 7200:
            setattr(result, timeout_field, 7200)
        try:
            session.commit()
            logger.info(f"Successfully evaluated {method} on circuit {circuit_file.name} in {runtime:.3f}s")
        except Exception:
            session.rollback()
            logger.exception(f"Error committing results for {method} on circuit {circuit_file.name}")
    else:
        # Do not write results on timeout/failure; only persist timeout_tried
        try:
            session.commit()
        except Exception:
            session.rollback()
        if err == "timeout":
            logger.info(f"Timeout after {timeout_sec:.3f}s for {method} on circuit {circuit_file.name}")
        else:
            logger.warning(f"Failed to evaluate {method} on circuit {circuit_file.name}: {err}")


def _parallel_worker(circuit_path_str: str, method: str, kwargs: dict[str, Any]) -> None:
    """Worker process entry: opens its own DB session and evaluates a single circuit."""
    try:
        with Session(engine) as session:
            _evaluate_single_circuit(Path(circuit_path_str), method, session, **kwargs)
    except Exception:
        logger.exception(f"Worker error for {method} on {circuit_path_str}")


def _parallel_worker_packed(args: tuple[str, str, dict[str, Any]]) -> None:
    circuit_path_str, method, kwargs = args
    _parallel_worker(circuit_path_str, method, kwargs)


def evaluate_circuits(method: str, **kwargs: dict[str, Any]) -> None:
    """Evaluate circuits and update database for a specific method.

    Enforces a hard per-run timeout (default 1s, configurable via kwargs or env MQT_EVAL_TIMEOUT_SEC).
    Skips benchmarks already tried with this timeout or higher.
    Runs evaluations in parallel across a configurable number of worker processes (default 4 via env MQT_EVAL_WORKERS or kwargs).
    """
    logger.info(f"Starting evaluation for method: {method}")
    eval_dir = CIRCUITS_DIR
    circuit_files = sorted(eval_dir.glob("circuit_*.qpy"))
    logger.info(f"Found {len(circuit_files)} circuits to evaluate")

    # Determine timeout and workers
    timeout_sec = float(kwargs.get("timeout_sec", os.environ.get("MQT_EVAL_TIMEOUT_SEC", "1.0")))
    workers = int(kwargs.get("workers", os.environ.get("MQT_EVAL_WORKERS", "4")))

    kwargs = dict(kwargs)
    kwargs["timeout_sec"] = timeout_sec

    # Ensure timeout bookkeeping is up-to-date
    _backfill_timeout_columns()

    if workers <= 1:
        # Sequential fallback
        try:
            with Session(engine) as session:
                for circuit_file in circuit_files:
                    _evaluate_single_circuit(circuit_file, method, session, **kwargs)
            logger.info(f"Completed evaluation for method: {method}")
        except KeyboardInterrupt:
            logger.info("Evaluation cancelled by user")
            raise
        except Exception:
            logger.exception(f"Error during evaluation of {method}")
            raise
        return

    logger.info(f"Running evaluations in parallel with {workers} worker threads")
    try:
        args_iter = [(str(cf), method, kwargs) for cf in circuit_files]
        with ThreadPoolExecutor(max_workers=workers) as executor:
            futures = [executor.submit(_parallel_worker_packed, args) for args in args_iter]
            try:
                for fut in futures:
                    fut.result()
            except Exception:
                logger.exception(f"Worker error during parallel evaluation of {method}")
        logger.info(f"Completed evaluation for method: {method}")
    except KeyboardInterrupt:
        logger.info("Evaluation cancelled by user during parallel execution")
        raise
    except Exception:
        logger.exception(f"Error during parallel evaluation of {method}")
        raise


def reset_inconsistent_results() -> None:
    """Reset results with inconsistent gate counts and reset their timeouts so they are retried."""
    logger.info("Resetting inconsistent synthesis results and their timeouts...")
    with Session(engine) as session:
        cliff_results = (
            session.query(SynthesisResult)
            .filter(
                SynthesisResult.qmap_cliff_iter_gate_count.isnot(None),
                SynthesisResult.qmap_cliff_maxsat_gate_count.isnot(None),
                SynthesisResult.qmap_cliff_iter_gate_count != SynthesisResult.qmap_cliff_maxsat_gate_count,
            )
            .all()
        )

        circ_results = (
            session.query(SynthesisResult)
            .filter(
                SynthesisResult.qmap_circ_iter_gate_count.isnot(None),
                SynthesisResult.qmap_circ_maxsat_gate_count.isnot(None),
                SynthesisResult.qmap_circ_iter_gate_count != SynthesisResult.qmap_circ_maxsat_gate_count,
            )
            .all()
        )

        for result in cliff_results:
            # Clear existing results
            result.qmap_cliff_iter_runtime = None
            result.qmap_cliff_iter_gate_count = None
            result.qmap_cliff_maxsat_runtime = None
            result.qmap_cliff_maxsat_gate_count = None
            # Reset timeouts so these benchmarks are reattempted on next run
            result.qmap_cliff_iter_timeout_tried = 0
            result.qmap_cliff_maxsat_timeout_tried = 0

        for result in circ_results:
            # Clear existing results
            result.qmap_circ_iter_runtime = None
            result.qmap_circ_iter_gate_count = None
            result.qmap_circ_maxsat_runtime = None
            result.qmap_circ_maxsat_gate_count = None
            # Reset timeouts so these benchmarks are reattempted on next run
            result.qmap_circ_iter_timeout_tried = 0
            result.qmap_circ_maxsat_timeout_tried = 0

        session.commit()
    logger.info(
        f"Reset {len(cliff_results)} inconsistent cliff results and {len(circ_results)} inconsistent circuit results (timeouts reset)"
    )


def export_results() -> None:
    """Export database results to JSON file."""
    logger.info("Exporting synthesis results to JSON...")
    results = []
    with Session(engine) as session:
        results.extend(
            {
                "id": result.id,
                "num_qubits": result.num_qubits,
                "num_gates": result.num_gates,
                "instance": result.instance,
                "ag_runtime": result.ag_runtime,
                "ag_gate_count": result.ag_gate_count,
                "greedy_runtime": result.greedy_runtime,
                "greedy_gate_count": result.greedy_gate_count,
                "qmap_cliff_iter_runtime": result.qmap_cliff_iter_runtime,
                "qmap_cliff_iter_gate_count": result.qmap_cliff_iter_gate_count,
                "qmap_cliff_maxsat_runtime": result.qmap_cliff_maxsat_runtime,
                "qmap_cliff_maxsat_gate_count": result.qmap_cliff_maxsat_gate_count,
                "qmap_circ_iter_runtime": result.qmap_circ_iter_runtime,
                "qmap_circ_iter_gate_count": result.qmap_circ_iter_gate_count,
                "qmap_circ_maxsat_runtime": result.qmap_circ_maxsat_runtime,
                "qmap_circ_maxsat_gate_count": result.qmap_circ_maxsat_gate_count,
            }
            for result in session.query(SynthesisResult).all()
        )

    with (EVAL_ROOT / "synthesis_results.json").open("w", encoding="utf-8") as f:
        json.dump(results, f, indent=2)
    logger.info(f"Exported {len(results)} results to {(EVAL_ROOT / 'synthesis_results.json')}")


def _allowed_num_gates(num_qubits: int) -> set[int]:
    """Return the refined set of gate counts for a given number of qubits.

    Rationale:
    - Keep all settings where at least one QMAP method reached 100/100 in prior runs.
    - Reduce gate counts for larger qubits to improve throughput.
      For 5 qubits: cap around 20 gates → {1, 5, 10, 20}
      For 6 qubits: cap at 15 gates     → {1, 6, 12, 15}
    - For 1..4 qubits keep the original progression {1, q, 2q, 5q, 10q}.
    """
    q = num_qubits
    if q <= 4:
        return {1, q, 2 * q, 5 * q, 10 * q}
    if q == 5:
        return {1, 5, 10, 15, 20}
    if q == 6:
        return {1, 6, 9, 12, 15}
    # Fallback for other sizes (not used currently)
    return {1, q, 2 * q, 5 * q, 10 * q}


def prune_excess_benchmarks() -> None:
    """Remove circuits and DB entries outside the refined gate-count set.

    - Deletes eval/circuits/circuit_* files whose gate count is not in the allowed set from _allowed_num_gates(q).
    - Deletes synthesis_results DB rows for such gate counts (per num_qubits 1..6).
    """
    logger.info("Pruning excess benchmarks (circuits and DB records) based on refined gate counts...")
    eval_dir = CIRCUITS_DIR

    # Remove files not matching the refined gate set
    if eval_dir.exists():
        removed_files = 0
        for cf in eval_dir.glob("circuit_*.qpy"):
            try:
                parts = cf.stem.split("_")
                q = int(parts[1])
                g = int(parts[2])
            except Exception:  # noqa: S112
                # Unexpected filename - ignore
                continue
            if g not in _allowed_num_gates(q):
                try:
                    cf.unlink()
                    removed_files += 1
                except Exception:
                    logger.warning(f"Failed to remove file {cf}")
        if removed_files:
            logger.info(f"Removed {removed_files} circuit files outside refined gate set")
    else:
        # Ensure directory exists for subsequent generation
        eval_dir.mkdir(exist_ok=True, parents=True)

    # Remove DB entries not matching the refined gate set
    total_deleted = 0
    with Session(engine) as session:
        for q in [1, 2, 3, 4, 5, 6]:
            allowed = _allowed_num_gates(q)
            deleted = (
                session.query(SynthesisResult)
                .filter(
                    SynthesisResult.num_qubits == q,
                    ~SynthesisResult.num_gates.in_(list(allowed)),
                )
                .delete(synchronize_session=False)
            )
            total_deleted += int(deleted or 0)
        session.commit()
    if total_deleted:
        logger.info(f"Deleted {total_deleted} DB rows outside refined gate set")


if __name__ == "__main__":
    logger.info("Starting circuit synthesis evaluation")

    # Prune excess benchmarks (files and DB) according to refined gate sets
    prune_excess_benchmarks()

    # Initialize database and generate circuits (only refined gate sets)
    generate_test_circuits()

    # Reset inconsistent results
    reset_inconsistent_results()

    # Configure timeout (default 1.0s, can be overridden via env MQT_EVAL_TIMEOUT_SEC)
    timeout_sec = float(os.environ.get("MQT_EVAL_TIMEOUT_SEC", "1800."))
    # Configure number of parallel workers (default 4, can be overridden via env MQT_EVAL_WORKERS)
    workers = int(os.environ.get("MQT_EVAL_WORKERS", "6"))

    # Run evaluations (comment our any methods you don't want to run)'
    evaluate_circuits("ag", timeout_sec=timeout_sec, workers=workers)
    evaluate_circuits("greedy", timeout_sec=timeout_sec, workers=workers)

    num_qubits_limit = 4  # increase or decrease to regulate the benchmarks being run
    num_gates_limit = 300  # increase or decrease to regulate the benchmarks being run

    evaluate_circuits(
        "qmap_from_clifford_iter",
        num_qubits_limit=num_qubits_limit,
        num_gates_limit=num_gates_limit,
        timeout_sec=timeout_sec,
        workers=workers,
    )
    evaluate_circuits(
        "qmap_from_clifford_maxsat",
        num_qubits_limit=num_qubits_limit,
        num_gates_limit=num_gates_limit,
        timeout_sec=timeout_sec,
        workers=workers,
    )
    evaluate_circuits(
        "qmap_from_circuit_iter",
        num_qubits_limit=num_qubits_limit,
        num_gates_limit=num_gates_limit,
        timeout_sec=timeout_sec,
        workers=workers,
    )
    evaluate_circuits(
        "qmap_from_circuit_maxsat",
        num_qubits_limit=num_qubits_limit,
        num_gates_limit=num_gates_limit,
        timeout_sec=timeout_sec,
        workers=workers,
    )

    # Export results
    export_results()

    logger.info("Circuit synthesis evaluation completed")
