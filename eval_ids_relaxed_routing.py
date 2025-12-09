#!/usr/bin/env python3
# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
# Copyright (c) 2025 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""Script for evaluating the fast relaxed compiler."""

from __future__ import annotations

import json
import pathlib
import queue
import re
from math import exp, sqrt
from multiprocessing import get_context
from typing import TYPE_CHECKING

from mqt.bench import BenchmarkLevel, get_benchmark
from mqt.core import load
from qiskit import QuantumCircuit, transpile

from mqt.qmap.na.zoned import (
    FastRelaxedRoutingAwareCompiler,
    FastRoutingAwareCompiler,
    RoutingAwareCompiler,
    ZonedNeutralAtomArchitecture,
)

if TYPE_CHECKING:
    from collections.abc import Callable, Iterable, Iterator, Mapping
    from multiprocessing import Queue
    from typing import Any, ParamSpec, TypeVar

    from mqt.core.ir import QuantumComputation

    T = TypeVar("T", bound=FastRelaxedRoutingAwareCompiler | RoutingAwareCompiler | FastRoutingAwareCompiler)
    P = ParamSpec("P")
    R = ParamSpec("R")


def _proc_target(q: Queue, func: Callable[P, R], args: P.args, kwargs: P.kwargs) -> None:
    """Target function for the process to run the given function and put the result in the queue.

    Args:
        q: The queue to put the result in.
        func: The function to run.
        args: The positional arguments to pass to the function.
        kwargs: The keyword arguments to pass to the function.
    """
    try:
        q.put(("ok", func(*args, **kwargs)))
    except Exception as e:
        q.put(("err", e))


def run_with_process_timeout(func: Callable[P, R], timeout: float, *args: P.args, **kwargs: P.kwargs) -> R:
    """Run a function in a separate process and timeout after the given timeout.

    Args:
        func: The function to run.
        timeout: The timeout in seconds.
        *args: The positional arguments to pass to the function.
        **kwargs: The keyword arguments to pass to the function.

    Returns:
        The result of the function.

    Raises:
        TimeoutError: If the function times out.
        Exception: If the function raises an exception.
    """
    ctx = get_context("fork")  # use fork so bound methods don't need to be pickled on macOS/Unix
    q = ctx.Queue()
    p = ctx.Process(target=_proc_target, args=(q, func, args, kwargs))
    p.start()
    try:
        status, payload = q.get(block=True, timeout=timeout)
    except queue.Empty as e:
        msg = f"Timed out after {timeout}s"
        raise TimeoutError(msg) from e
    finally:
        if p.is_alive():
            p.terminate()
            p.join(2)
            if p.is_alive():
                p.kill()
                p.join()
    if status == "ok":
        return payload
    if (
        status == "err"
        and isinstance(payload, Exception)
        and payload.args
        and isinstance(payload.args[0], str)
        and "Maximum number of nodes reached" in payload.args[0]
    ):
        msg = "Out of memory"
        raise MemoryError(msg)
    raise payload


def transpile_benchmark(benchmark: str, circuit: QuantumCircuit) -> QuantumCircuit:
    """Transpile the given benchmark circuit to the native gate set.

    Args:
        benchmark: Name of the benchmark.
        circuit: The benchmark circuit to transpile.

    Returns:
        The transpiled benchmark circuit.
    """
    print(f"\033[32m[INFO]\033[0m Transpiling {benchmark}...")
    flattened = QuantumCircuit(circuit.num_qubits, circuit.num_clbits)
    flattened.compose(circuit, inplace=True)
    transpiled = transpile(
        flattened, basis_gates=["cz", "id", "u2", "u1", "u3"], optimization_level=3, seed_transpiler=0
    )
    stripped = QuantumCircuit(*transpiled.qregs, *transpiled.cregs)
    for instr in transpiled.data:
        if instr.operation.name not in {"measure", "barrier"}:
            stripped.append(instr)
    print("\033[32m[INFO]\033[0m Done")
    return stripped


def benchmarks(
    benchmark_dict: Iterable[tuple[str, tuple[BenchmarkLevel, Iterable[int]]]],
) -> Iterator[tuple[str, QuantumComputation]]:
    """Yields the benchmark names and their circuits."""
    for benchmark, settings in benchmark_dict:
        mode, limits = settings
        for qubits in limits:
            print(f"\033[32m[INFO]\033[0m Creating {benchmark} with {qubits} qubits...")
            circuit = get_benchmark(benchmark, mode, qubits)
            print("\033[32m[INFO]\033[0m Done")
            transpiled = transpile_benchmark(benchmark, circuit)
            qc = load(transpiled)
            yield benchmark, qc


def _compile_wrapper(
    compiler: FastRelaxedRoutingAwareCompiler | RoutingAwareCompiler | FastRoutingAwareCompiler, qc: QuantumComputation
) -> tuple[str, Mapping[str, Any]]:
    """Compile and return the compiled code and stats.

    Args:
        compiler: The compiler to use.
        qc: The circuit to compile.

    Returns:
        The compiled code and stats.
    """
    return compiler.compile(qc), compiler.stats()


def process_benchmark(
    compiler: FastRelaxedRoutingAwareCompiler | RoutingAwareCompiler | FastRoutingAwareCompiler,
    setting_name: str,
    qc: QuantumComputation,
    benchmark_name: str,
    evaluator: Evaluator,
    use_cached: bool = False,
) -> bool:
    """Compile and evaluate the given benchmark circuit.

    Args:
        compiler: The compiler to use.
        setting_name: Name of the compiler setting.
        qc: The benchmark circuit to compile.
        benchmark_name: Name of the benchmark.
        evaluator: The evaluator to use.
        use_cached: Whether to use the cached results.
    """
    compiler_name = type(compiler).__name__
    if not use_cached:
        print(f"\033[32m[INFO]\033[0m Compiling {benchmark_name} with {qc.num_qubits} qubits with {compiler_name}...")
        try:
            code, stats = run_with_process_timeout(_compile_wrapper, TIMEOUT, compiler, qc)
        except TimeoutError as e:
            print(f"\033[31m[ERROR]\033[0m Failed ({e})")
            evaluator.print_timeout(benchmark_name, qc, compiler_name, setting_name)
            return False
        except MemoryError as e:
            print(f"\033[31m[ERROR]\033[0m Failed ({e})")
            evaluator.print_memout(benchmark_name, qc, compiler_name, setting_name)
            return False
        except RuntimeError as e:
            print(f"\033[31m[ERROR]\033[0m Failed ({e})")
            return False

        code = "\n".join(line for line in code.splitlines() if not line.startswith("@+ u"))
        pathlib.Path(f"out/{compiler_name}/{setting_name}").mkdir(exist_ok=True, parents=True)
        with pathlib.Path(f"out/{compiler_name}/{setting_name}/{benchmark_name}_{qc.num_qubits}.naviz").open(
            "w", encoding="utf-8"
        ) as f:
            f.write(code)
        print("\033[32m[INFO]\033[0m Done")
    else:
        print(
            f"\033[32m[INFO]\033[0m Reading cached {benchmark_name} with {qc.num_qubits} qubits with {compiler_name}..."
        )
        code = ""
        path = pathlib.Path(f"out/{compiler_name}/{setting_name}/{benchmark_name}_{qc.num_qubits}.naviz")
        if not path.exists():
            evaluator.print_memout(benchmark_name, qc, compiler_name, setting_name)
            print("\033[31m[ERROR]\033[0m Abort")
            return False
        with path.open("r", encoding="utf-8") as f:
            code = f.read()
        stats = {
            "schedulingTime": 0.0,
            "reuseAnalysisTime": 0.0,
            "layoutSynthesizerStatistics": {
                "placementTime": 0.0,
                "routingTime": 0.0,
            },
            "codeGenerationTime": 0.0,
            "totalTime": 0.0,
        }
        print("\033[32m[INFO]\033[0m Done")

    print(f"\033[32m[INFO]\033[0m Evaluating {benchmark_name} with {qc.num_qubits} qubits...")
    evaluator.reset()
    evaluator.evaluate(benchmark_name, compiler_name, qc, setting_name, code, stats)
    evaluator.print_data()
    print("\033[32m[INFO]\033[0m Done")
    return True


class Evaluator:
    """Class for evaluating compiled circuits.

    Attributes:
        arch: The architecture dictionary.
        filename: The output CSV filename.
        circuit_name: Name of the circuit.
        compiler: Name of the compiler.
        setting: Compiler setting name.
        one_qubit_gates: Number of one-qubit gates.
        two_qubit_gates: Number of two-qubit gates.
        scheduling_time: Time taken for scheduling.
        reuse_analysis_time: Time taken for reuse analysis.
        placement_time: Time taken for placement.
        routing_time: Time taken for routing.
        code_generation_time: Time taken for code generation.
        total_time: Total compilation time.
        one_qubit_gate_fidelity: Fidelity of one-qubit gates.
        two_qubit_gate_fidelity: Fidelity of two-qubit gates.
        transfer_fidelity: Fidelity of atom transfer operations.
        rearrangement_fidelity: Fidelity of rearrangement operations.
        coherence_fidelity: Fidelity due to coherence.
        circuit_duration: Total duration of the circuit.
        rearrangement_duration: Duration of rearrangement operations.
        two_qubit_gate_layer: Number of two-qubit gate layers.
        mean_two_qubit_gates: Mean number of two-qubit gates per layer.
        rearrangement_layer: Number of rearrangement layers.
        rearrangement_steps: Number of rearrangement steps.
        min_two_qubit_gates: Minimum number of two-qubit gates in a layer.
        max_two_qubit_gates: Maximum number of two-qubit gates in a layer.
        sum_two_qubit_gates: Sum of two-qubit gates across layers.
        last_op_is_shuttling: Flag indicating if the last operation was shuttling.
        last_op_is_store: Flag indicating if the last operation was a store operation.
        atom_locations: Dictionary of atom locations.
        atom_busy_times: Dictionary of atom busy times.
        atom_busy_rearrangement_times: Dictionary of atom busy times during rearrangement.
    """

    def __init__(self, arch: Mapping[str, Any], filename: str) -> None:
        """Initialize the Evaluator.

        Args:
            arch: The architecture dictionary.
            filename: The output CSV filename.
        """
        self.arch = arch
        self.filename = filename

        self.circuit_name = ""
        self.num_qubits = 0
        self.compiler = ""
        self.setting = ""
        self.one_qubit_gates = 0
        self.two_qubit_gates = 0

        self.scheduling_time = 0
        self.reuse_analysis_time = 0
        self.placement_time = 0
        self.routing_time = 0
        self.code_generation_time = 0
        self.total_time = 0

        self.one_qubit_gate_fidelity = 1.0
        self.two_qubit_gate_fidelity = 1.0
        self.transfer_fidelity = 1.0
        self.rearrangement_fidelity = 1.0
        self.coherence_fidelity = 1.0

        self.circuit_duration = 0.0
        self.rearrangement_duration = 0.0

        self.two_qubit_gate_layer = 0
        self.mean_two_qubit_gates = 0.0
        self.rearrangement_layer = 0
        self.rearrangement_steps = 0
        self.rearrangement_distance = 0
        self.num_loads = 0
        self.num_stores = 0

        self.min_two_qubit_gates = None
        self.max_two_qubit_gates = None
        self.sum_two_qubit_gates = 0

        self.last_op_is_shuttling = False
        self.last_op_is_store = False

        self.atom_locations = {}
        self.atom_busy_times = {}
        self.atom_busy_rearrangement_times = {}

    def reset(self) -> None:
        """Reset the Evaluator."""
        self.circuit_name = ""
        self.num_qubits = 0
        self.compiler = ""
        self.setting = ""
        self.one_qubit_gates = 0
        self.two_qubit_gates = 0

        self.scheduling_time = 0
        self.reuse_analysis_time = 0
        self.placement_time = 0
        self.routing_time = 0
        self.code_generation_time = 0
        self.total_time = 0

        self.one_qubit_gate_fidelity = 1.0
        self.two_qubit_gate_fidelity = 1.0
        self.transfer_fidelity = 1.0
        self.rearrangement_fidelity = 1.0
        self.coherence_fidelity = 1.0

        self.circuit_duration = 0.0
        self.rearrangement_duration = 0.0

        self.two_qubit_gate_layer = 0
        self.mean_two_qubit_gates = 0.0
        self.rearrangement_layer = 0
        self.rearrangement_steps = 0
        self.rearrangement_distance = 0
        self.num_loads = 0
        self.num_stores = 0

        self.min_two_qubit_gates = None
        self.max_two_qubit_gates = None
        self.sum_two_qubit_gates = 0

        self.last_op_is_shuttling = False
        self.last_op_is_store = False

        self.atom_locations = {}
        self.atom_busy_times = {}
        self.atom_busy_rearrangement_times = {}

    def _process_load(self, line: str, it: Iterator[str]) -> None:
        """Process a load operation.

        Args:
            line: The current line being processed.
            it: An iterator over the remaining lines.
        """
        # Extract atoms from the load operation
        atoms = []
        match = re.match(r"@\+ load \[", line)
        if match:
            # Multi-line load
            for next_line in it:
                next_line_stripped = next_line.strip()
                if next_line_stripped == "]":
                    break
                assert next_line_stripped in self.atom_locations, (
                    f"Atom {next_line_stripped} not found in atom locations"
                )
                atoms.append(next_line_stripped)
        else:
            # Single atom load
            match = re.match(r"@\+ load (\w+)", line)
            if match:
                assert match.group(1) in self.atom_locations, f"Atom {match.group(1)} not found in atom locations"
                atoms.append(match.group(1))
        self._apply_load(atoms)

    def _process_move(self, line: str, it: Iterator[str]) -> None:
        """Process a move operation.

        Args:
            line: The current line being processed.
            it: An iterator over the remaining lines.
        """
        # Extract atoms and coordinates from the move operation
        moves = []
        match = re.match(r"@\+ move \[", line)
        if match:
            # Multi-line move
            for next_line in it:
                next_line_stripped = next_line.strip()
                if next_line_stripped == "]":
                    break
                move_match = re.match(r"\((-?\d+\.\d+), (-?\d+\.\d+)\) (\w+)", next_line_stripped)
                if move_match:
                    x, y, atom = move_match.groups()
                    assert atom in self.atom_locations, f"Atom {atom} not found in atom locations"
                    moves.append((atom, (int(float(x)), int(float(y)))))
        else:
            # Single atom move
            match = re.match(r"@\+ move \((-?\d+\.\d+), (-?\d+\.\d+)\) (\w+)", line)
            if match:
                x, y, atom = match.groups()
                assert atom in self.atom_locations, f"Atom {atom} not found in atom locations"
                moves.append((atom, (int(float(x)), int(float(y)))))
        self._apply_move(moves)

    def _process_store(self, line: str, it: Iterator[str]) -> None:
        """Process a store operation.

        Args:
            line: The current line being processed.
            it: An iterator over the remaining lines.
        """
        # Extract atoms from the store operation
        match = re.match(r"@\+ store \[", line)
        atoms = []
        if match:
            # Multi-line store
            for next_line in it:
                next_line_stripped = next_line.strip()
                if next_line_stripped == "]":
                    break
                assert next_line_stripped in self.atom_locations, (
                    f"Atom {next_line_stripped} not found in atom locations"
                )
                atoms.append(next_line_stripped)
        else:
            # Single atom store
            match = re.match(r"@\+ store (\w+)", line)
            if match:
                assert match.group(1) in self.atom_locations, f"Atom {match.group(1)} not found in atom locations"
                atoms.append(match.group(1))
        self._apply_store(atoms)

    def _process_cz(self) -> None:
        """Process a cz operation."""
        atoms = []
        y_min = self.arch["entanglement_zones"][0]["slms"][0]["location"][1]
        for atom, coord in self.atom_locations.items():
            if coord[1] >= y_min:  # atom is in the entanglement zone
                atoms.append(atom)
        assert len(atoms) % 2 == 0, f"Expected even number of atoms in entanglement zone, got {len(atoms)}"
        self._apply_cz(atoms)

    def _process_u(self, line: str, it: Iterator[str]) -> None:
        """Process a u operation.

        Args:
            line: The current line being processed.
            it: An iterator over the remaining lines.
        """
        # Extract atoms from u operation
        atoms = []
        match = re.match(r"@\+ u( \d\.\d+){3} \[", line)
        if match:
            # Multi-line u
            for next_line in it:
                next_line_stripped = next_line.strip()
                if next_line_stripped == "]":
                    break
                assert next_line_stripped in self.atom_locations, (
                    f"Atom {next_line_stripped} not found in atom locations"
                )
                atoms.append(next_line_stripped)
        else:
            # Single atom u
            match = re.match(r"@\+ u( \d\.\d+){3} (\w+)", line)
            if match:
                if match.group(2) not in self.atom_locations:
                    self._apply_global_u()
                    return
                atoms.append(match.group(2))
        self._apply_u(atoms)

    def _process_rz(self, line: str, it: Iterator[str]) -> None:
        """Process a rz operation.

        Args:
            line: The current line being processed.
            it: An iterator over the remaining lines.
        """
        # Extract atoms from u operation
        atoms = []
        match = re.match(r"@\+ rz \d\.\d+ \[", line)
        if match:
            # Multi-line u
            for next_line in it:
                next_line_stripped = next_line.strip()
                if next_line_stripped == "]":
                    break
                assert next_line_stripped in self.atom_locations, (
                    f"Atom {next_line_stripped} not found in atom locations"
                )
                atoms.append(next_line_stripped)
        else:
            # Single atom u
            match = re.match(r"@\+ rz \d\.\d+ (\w+)", line)
            if match:
                assert match.group(1) in self.atom_locations, f"Atom {match.group(1)} not found in atom locations"
                atoms.append(match.group(1))
        self._apply_rz(atoms)

    def _apply_load(self, atoms: list[str]) -> None:
        """Apply a load operation.

        Args:
            atoms: List of atoms to load.
        """
        if not self.last_op_is_shuttling:
            self.rearrangement_layer += 1
        self.last_op_is_shuttling = True
        self.last_op_is_store = False

        self.num_loads += 1
        self.transfer_fidelity *= self.arch["operation_fidelity"]["atom_transfer"] ** len(atoms)
        self.circuit_duration += self.arch["operation_duration"]["atom_transfer"]
        self.rearrangement_duration += self.arch["operation_duration"]["atom_transfer"]
        for atom in atoms:
            self.atom_busy_times[atom] += self.arch["operation_duration"]["atom_transfer"]
            self.atom_busy_rearrangement_times[atom] += self.arch["operation_duration"]["atom_transfer"]

    def _apply_move(self, moves: list[tuple[str, tuple[int, int]]]) -> None:
        """Apply a move operation.

        Args:
            moves: List of tuples containing atom names and their target coordinates.
        """
        self.last_op_is_shuttling = True
        # do not change this value to ignore intermediate moves between store operations
        # self.last_op_is_store = False

        max_distance = 0.0
        for atom, coord in moves:
            if atom in self.atom_locations:
                distance = sqrt(
                    (coord[0] - self.atom_locations[atom][0]) ** 2 + (coord[1] - self.atom_locations[atom][1]) ** 2
                )
                self.rearrangement_distance += distance
                max_distance = max(max_distance, distance)

        t_d_max = 200
        d_max = 110
        jerk = 32 * d_max / t_d_max**3  # 0.00044
        v_max = d_max / t_d_max * 2  # 1.1

        if max_distance <= d_max:
            rearrangement_time = 2 * (4 * max_distance / jerk) ** (1 / 3)
        else:
            rearrangement_time = t_d_max + (max_distance - d_max) / v_max
        self.circuit_duration += rearrangement_time
        self.rearrangement_duration += rearrangement_time
        # Update atom locations
        for atom, coord in moves:
            assert atom in self.atom_locations, f"Atom {atom} not found in atom locations"
            self.atom_locations[atom] = coord

    def _apply_store(self, atoms: list[str]) -> None:
        """Apply a store operation.

        Args:
            atoms: List of atoms to store.
        """
        if not self.last_op_is_store:
            self.rearrangement_steps += 1
        self.last_op_is_shuttling = True
        self.last_op_is_store = True

        self.num_stores += 1
        self.transfer_fidelity *= self.arch["operation_fidelity"]["atom_transfer"] ** len(atoms)
        self.circuit_duration += self.arch["operation_duration"]["atom_transfer"]
        self.rearrangement_duration += self.arch["operation_duration"]["atom_transfer"]
        for atom in atoms:
            self.atom_busy_times[atom] += self.arch["operation_duration"]["atom_transfer"]
            self.atom_busy_rearrangement_times[atom] += self.arch["operation_duration"]["atom_transfer"]

    def _apply_cz(self, atoms: list[str]) -> None:
        """Apply a cz operation.

        Args:
            atoms: List of atoms involved in the cz operation.
        """
        self.last_op_is_shuttling = False
        self.last_op_is_store = False

        # assuming the compiler works correctly, if there are n atoms in the
        # entanglement zone, n/2 gates are executed and there is no idling atom
        self.two_qubit_gate_fidelity *= self.arch["operation_fidelity"]["rydberg_gate"] ** (len(atoms) / 2)

        self.circuit_duration += self.arch["operation_duration"]["rydberg_gate"]
        for atom in atoms:
            self.atom_busy_times[atom] += self.arch["operation_duration"]["single_qubit_gate"]

        self.two_qubit_gate_layer += 1
        self.min_two_qubit_gates = (
            min(self.min_two_qubit_gates, len(atoms) / 2) if self.min_two_qubit_gates else len(atoms) / 2
        )
        self.sum_two_qubit_gates += len(atoms) / 2
        self.max_two_qubit_gates = (
            max(self.max_two_qubit_gates, len(atoms) / 2) if self.max_two_qubit_gates else len(atoms) / 2
        )

    def _apply_u(self, atoms: list[str]) -> None:
        """Apply a u operation.

        Args:
            atoms: List of atoms involved in the u operation.
        """
        self.last_op_is_shuttling = False
        self.last_op_is_store = False

        self.one_qubit_gate_fidelity *= self.arch["operation_fidelity"]["single_qubit_gate"] ** len(atoms)

        self.circuit_duration += self.arch["operation_duration"]["single_qubit_gate"]
        for atom in atoms:
            self.atom_busy_times[atom] += self.arch["operation_duration"]["single_qubit_gate"]

    def _apply_global_u(self) -> None:
        """Apply a global u operation."""
        self.last_op_is_shuttling = False
        self.last_op_is_store = False

        self.one_qubit_gate_fidelity *= self.arch["operation_fidelity"]["single_qubit_gate"] ** len(self.atom_locations)

        self.circuit_duration += self.arch["operation_duration"]["single_qubit_gate"]
        for atom in self.atom_locations:
            self.atom_busy_times[atom] += self.arch["operation_duration"]["single_qubit_gate"]

    def _apply_global_ry(self) -> None:
        """Apply a global rydberg gate operation."""
        self._apply_global_u()

    def _apply_rz(self, atoms: list[str]) -> None:
        """Apply a rz operation.

        Args:
            atoms: List of atoms involved in the rz operation.
        """
        self._apply_u(atoms)

    def evaluate(
        self, name: str, compiler: str, qc: QuantumComputation, setting: str, code: str, stats: Mapping[str, Any]
    ) -> None:
        """Evaluate a circuit.

        Args:
            name: Name of the circuit.
            compiler: Name of the compiler.
            qc: The quantum circuit.
            setting: Compiler setting name.
            code: The compiled code.
            stats: Compilation statistics.
        """
        self.circuit_name = name
        self.num_qubits = qc.num_qubits
        self.compiler = compiler
        self.setting = setting
        self.one_qubit_gates = sum(len(op.get_used_qubits()) == 1 for op in qc)
        self.two_qubit_gates = sum(len(op.get_used_qubits()) == 2 for op in qc)

        self.scheduling_time = stats["schedulingTime"]
        self.reuse_analysis_time = stats["reuseAnalysisTime"]
        self.placement_time = stats["layoutSynthesizerStatistics"]["placementTime"]
        self.routing_time = stats["layoutSynthesizerStatistics"]["routingTime"]
        self.code_generation_time = stats["codeGenerationTime"]
        self.total_time = stats["totalTime"]

        it = iter(code.splitlines())

        for line in it:
            match = re.match(r"atom\s+\((-?\d+\.\d+),\s*(-?\d+\.\d+)\)\s+(\w+)", line)
            if match:
                x, y, atom_name = match.groups()
                self.atom_locations[atom_name] = (int(float(x)), int(float(y)))
            else:
                # put line back on top of iterator
                it = iter([line, *list(it)])
                break

        self.atom_busy_times = dict.fromkeys(self.atom_locations.keys(), 0.0)
        self.atom_busy_rearrangement_times = dict.fromkeys(self.atom_locations.keys(), 0.0)

        for line in it:
            if line.startswith("@+ load"):
                self._process_load(line, it)
            elif line.startswith("@+ move"):
                self._process_move(line, it)
            elif line.startswith("@+ store"):
                self._process_store(line, it)
            elif line.startswith("@+ cz"):
                self._process_cz()
            elif line.startswith("@+ u"):
                self._process_u(line, it)
            else:
                assert line.startswith("@+ rz"), f"Unrecognized operation: {line}"
                self._process_rz(line, it)

        # in contrast to the transfer fidelity, this fidelity includes decoherence
        # during rearrangement
        self.rearrangement_fidelity = self.transfer_fidelity
        for busy_time in self.atom_busy_rearrangement_times.values():
            idle_time = self.rearrangement_duration - busy_time
            t_eff = self.arch["qubit_spec"]["T"]
            self.rearrangement_fidelity *= exp(-idle_time / t_eff)
        self.coherence_fidelity = 1.0
        for busy_time in self.atom_busy_times.values():
            idle_time = self.circuit_duration - busy_time
            t_eff = self.arch["qubit_spec"]["T"]
            self.coherence_fidelity *= exp(-idle_time / t_eff)
        self.mean_two_qubit_gates = self.sum_two_qubit_gates / self.two_qubit_gate_layer

    def print_header(self) -> None:
        """Print the header of the CSV file."""
        with pathlib.Path(self.filename).open("w", encoding="utf-8") as csv:
            csv.write(
                "circuit_name,num_qubits,compiler,setting,status,one_qubit_gates,two_qubit_gates,scheduling_time,"
                "reuse_analysis_time,placement_time,routing_time,code_generation_time,total_time,two_qubit_gate_layer,"
                "min_two_qubit_gates,mean_two_qubit_gates,max_two_qubit_gates,one_qubit_gate_fidelity,"
                "two_qubit_gate_fidelity,transfer_fidelity,coherence_fidelity,total_fidelity,"
                "rearrangement_layer,rearrangement_steps,rearrangement_duration,rearrangement_fidelity,"
                "circuit_duration,num_loads,num_stores,rearrangement_distance\n"
            )

    def print_data(self) -> None:
        """Print the data of the CSV file."""
        total_fidelity = (
            self.one_qubit_gate_fidelity
            * self.two_qubit_gate_fidelity
            * self.transfer_fidelity
            * self.coherence_fidelity
        )
        with pathlib.Path(self.filename).open("a", encoding="utf-8") as csv:
            csv.write(
                f"{self.circuit_name},{self.num_qubits},{self.compiler},{self.setting},ok,{self.one_qubit_gates},"
                f"{self.two_qubit_gates},{self.scheduling_time},{self.reuse_analysis_time},{self.placement_time},"
                f"{self.routing_time},{self.code_generation_time},{self.total_time},{self.two_qubit_gate_layer},"
                f"{self.min_two_qubit_gates},{self.mean_two_qubit_gates},{self.max_two_qubit_gates},"
                f"{self.one_qubit_gate_fidelity},{self.two_qubit_gate_fidelity},{self.transfer_fidelity},"
                f"{self.coherence_fidelity},{total_fidelity},{self.rearrangement_layer},{self.rearrangement_steps},"
                f"{self.rearrangement_duration},{self.rearrangement_fidelity},{self.circuit_duration},{self.num_loads},"
                f"{self.num_stores},{self.rearrangement_distance}\n"
            )

    def print_timeout(self, circuit_name: str, qc: QuantumComputation, compiler: str, setting: str) -> None:
        """Print the data of the CSV file.

        Args:
            circuit_name: Name of the circuit.
            qc: The quantum circuit.
            compiler: Name of the compiler.
            setting: Compiler setting name.
        """
        with pathlib.Path(self.filename).open("a", encoding="utf-8") as csv:
            csv.write(f"{circuit_name},{qc.num_qubits},{compiler},{setting},timeout,,,,,,,,,,,,,,,,,,,,,,,,,\n")

    def print_memout(self, circuit_name: str, qc: QuantumComputation, compiler: str, setting: str) -> None:
        """Print the data of the CSV file.

        Args:
            circuit_name: Name of the circuit.
            qc: The quantum circuit.
            compiler: Name of the compiler.
            setting: Compiler setting name.
        """
        with pathlib.Path(self.filename).open("a", encoding="utf-8") as csv:
            csv.write(f"{circuit_name},{qc.num_qubits},{compiler},{setting},memout,,,,,,,,,,,,,,,,,,,,,,,,,\n")


def main() -> None:
    """Main function for evaluating the fast relaxed compiler."""
    print("\033[32m[INFO]\033[0m Reading in architecture...")
    with pathlib.Path("square_architecture.json").open(encoding="utf-8") as f:
        arch_dict = json.load(f)
    arch = ZonedNeutralAtomArchitecture.from_json_file("square_architecture.json")
    arch.to_namachine_file("arch.namachine")
    print("\033[32m[INFO]\033[0m Done")
    RoutingAwareCompiler(
        arch,
        use_window=True,
        window_min_width=16,
        window_ratio=1.0,
        window_share=0.8,
        deepening_factor=0.9,
        deepening_value=8.0,
        lookahead_factor=0.2,
        reuse_level=5.0,
        max_nodes=int(1e7),
        log_level=LOG_LEVEL,
        warn_unsupported_gates=False,
    )
    FastRoutingAwareCompiler(
        arch,
        use_window=True,
        window_min_width=16,
        window_ratio=1.0,
        window_share=0.8,
        deepening_factor=0.9,
        deepening_value=8.0,
        lookahead_factor=0.2,
        reuse_level=5.0,
        trials=TRIALS,
        queue_capacity=100,
        log_level=LOG_LEVEL,
        warn_unsupported_gates=False,
    )
    FastRoutingAwareCompiler(
        arch,
        use_window=True,
        window_min_width=16,
        window_ratio=1.0,
        window_share=0.8,
        deepening_factor=0.02,
        deepening_value=1.0,
        lookahead_factor=0.4,
        reuse_level=5.0,
        trials=TRIALS,
        queue_capacity=100,
        log_level=LOG_LEVEL,
        warn_unsupported_gates=False,
    )
    FastRoutingAwareCompiler(
        arch,
        use_window=True,
        window_min_width=16,
        window_ratio=1.0,
        window_share=0.8,
        deepening_factor=0.01,
        deepening_value=1.0,
        lookahead_factor=0.1,
        reuse_level=5.0,
        trials=TRIALS,
        queue_capacity=100,
        log_level=LOG_LEVEL,
        warn_unsupported_gates=False,
    )
    FastRoutingAwareCompiler(
        arch,
        use_window=True,
        window_min_width=16,
        window_ratio=1.0,
        window_share=0.8,
        deepening_factor=0.02,
        deepening_value=1.0,
        lookahead_factor=0.1,
        reuse_level=5.0,
        trials=TRIALS,
        queue_capacity=100,
        log_level=LOG_LEVEL,
        warn_unsupported_gates=False,
    )
    FastRoutingAwareCompiler(
        arch,
        use_window=True,
        window_min_width=16,
        window_ratio=1.0,
        window_share=0.8,
        deepening_factor=0.02,
        deepening_value=0.0,
        lookahead_factor=0.1,
        reuse_level=5.0,
        trials=TRIALS,
        queue_capacity=100,
        log_level=LOG_LEVEL,
        warn_unsupported_gates=False,
    )
    FastRoutingAwareCompiler(
        arch,
        use_window=True,
        window_min_width=16,
        window_ratio=1.0,
        window_share=0.8,
        deepening_factor=0.0,
        deepening_value=0.0,
        lookahead_factor=0.2,
        reuse_level=5.0,
        trials=TRIALS,
        queue_capacity=100,
        log_level=LOG_LEVEL,
        warn_unsupported_gates=False,
    )
    FastRoutingAwareCompiler(
        arch,
        use_window=True,
        window_min_width=16,
        window_ratio=1.0,
        window_share=0.8,
        deepening_factor=0.02,
        deepening_value=0.0,
        lookahead_factor=0.5,
        reuse_level=5.0,
        trials=TRIALS,
        queue_capacity=100,
        log_level=LOG_LEVEL,
        warn_unsupported_gates=False,
    )
    FastRoutingAwareCompiler(
        arch,
        use_window=True,
        window_min_width=16,
        window_ratio=1.0,
        window_share=0.8,
        deepening_factor=0.0,
        deepening_value=0.0,
        lookahead_factor=0.4,
        reuse_level=5.0,
        trials=TRIALS,
        queue_capacity=100,
        log_level=LOG_LEVEL,
        warn_unsupported_gates=False,
    )
    FastRoutingAwareCompiler(
        arch,
        use_window=True,
        window_min_width=16,
        window_ratio=1.0,
        window_share=0.8,
        deepening_factor=0.02,
        deepening_value=0.0,
        lookahead_factor=0.45,
        reuse_level=5.0,
        trials=TRIALS,
        queue_capacity=100,
        log_level=LOG_LEVEL,
        warn_unsupported_gates=False,
    )
    fast_relaxed_compiler_less_split = FastRelaxedRoutingAwareCompiler(
        arch,
        use_window=True,
        window_min_width=16,
        window_ratio=1.0,
        window_share=0.8,
        deepening_factor=0.01,
        deepening_value=0.0,
        lookahead_factor=0.4,
        reuse_level=5.0,
        trials=TRIALS,
        queue_capacity=100,
        prefer_split=0.0,
        log_level=LOG_LEVEL,
        warn_unsupported_gates=False,
    )
    FastRelaxedRoutingAwareCompiler(
        arch,
        use_window=True,
        window_min_width=16,
        window_ratio=1.0,
        window_share=0.8,
        deepening_factor=0.01,
        deepening_value=0.0,
        lookahead_factor=0.4,
        reuse_level=5.0,
        trials=TRIALS,
        queue_capacity=100,
        prefer_split=1.0,
        log_level=LOG_LEVEL,
        warn_unsupported_gates=False,
    )
    FastRelaxedRoutingAwareCompiler(
        arch,
        use_window=True,
        window_min_width=16,
        window_ratio=1.0,
        window_share=0.8,
        deepening_factor=0.01,
        deepening_value=0.0,
        lookahead_factor=0.4,
        reuse_level=5.0,
        trials=TRIALS,
        queue_capacity=100,
        prefer_split=2.0,
        log_level=LOG_LEVEL,
        warn_unsupported_gates=False,
    )
    FastRelaxedRoutingAwareCompiler(
        arch,
        use_window=True,
        window_min_width=16,
        window_ratio=1.0,
        window_share=0.8,
        deepening_factor=0.01,
        deepening_value=0.0,
        lookahead_factor=0.4,
        reuse_level=5.0,
        trials=TRIALS,
        queue_capacity=100,
        prefer_split=4.0,
        log_level=LOG_LEVEL,
        warn_unsupported_gates=False,
    )
    evaluator = Evaluator(arch_dict, "results.csv")
    # evaluator.print_header()

    # Seed graphstate with seed = 0 (!)
    for benchmark, qc in benchmarks([
        ("graphstate", (BenchmarkLevel.INDEP, [60, 80, 100, 120, 140, 160, 180, 200, 500, 1000, 2000, 5000])),
        ("qft", (BenchmarkLevel.INDEP, [500, 1000])),
        ("qpeexact", (BenchmarkLevel.INDEP, [500, 1000])),
        ("wstate", (BenchmarkLevel.INDEP, [500, 1000])),
        ("qaoa", (BenchmarkLevel.INDEP, [50, 100, 150, 200])),
        ("vqe_two_local", (BenchmarkLevel.INDEP, [50, 100, 150, 200])),
    ]):
        qc.qasm3(f"in/{benchmark}_n{qc.num_qubits}.qasm")
        # process_benchmark(compiler_default, "default", qc, benchmark, evaluator, use_cached=CACHE)
        # process_benchmark(fast_compiler_default, "default", qc, benchmark, evaluator, use_cached=CACHE)
        # process_benchmark(fast_compiler_half_deepening, "double_deepening4", qc, benchmark, evaluator, use_cached=CACHE)
        # process_benchmark(fast_compiler_no_deepening, "half_lookahead6", qc, benchmark, evaluator, use_cached=CACHE)
        # process_benchmark(
        #     fast_compiler_double_lookahead, "half_lookahead5", qc, benchmark, evaluator, use_cached=CACHE
        # )
        # process_benchmark(
        #     fast_compiler_half_lookahead, "half_lookahead4", qc, benchmark, evaluator, use_cached=CACHE
        # )
        # process_benchmark(fast_compiler_zero_deep, "zero_deep", qc, benchmark, evaluator, use_cached=CACHE)
        # process_benchmark(fast_compiler_almost_zero_deep, "almost_zero_deep3", qc, benchmark, evaluator, use_cached=CACHE)
        # process_benchmark(fast_compiler_less_deep, "less_deep5", qc, benchmark, evaluator, use_cached=CACHE)
        # process_benchmark(fast_compiler_least_deep, "least_deep4", qc, benchmark, evaluator, use_cached=CACHE)
        # process_benchmark(fast_relaxed_compiler_default, "default3", qc, benchmark, evaluator, use_cached=CACHE)
        process_benchmark(fast_relaxed_compiler_less_split, "no_split", qc, benchmark, evaluator, use_cached=CACHE)
        # process_benchmark(fast_relaxed_compiler_more_split, "more_split3", qc, benchmark, evaluator, use_cached=CACHE)
        # process_benchmark(
        #     fast_relaxed_compiler_even_more_split, "even_more_split3", qc, benchmark, evaluator, use_cached=CACHE
        # )


LOG_LEVEL = "error"
TIMEOUT = 15 * 60
TRIALS = 4
CACHE = False

if __name__ == "__main__":
    main()
