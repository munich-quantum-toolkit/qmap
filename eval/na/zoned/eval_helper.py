#!/usr/bin/env -S uv run --script --quiet
# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""Helper functions to evaluate the routing-aware zoned neutral atom compiler."""

from __future__ import annotations

import pathlib
import queue
import re
from itertools import chain
from math import sqrt
from multiprocessing import get_context
from typing import TYPE_CHECKING

from mqt.bench import get_benchmark
from mqt.core import load
from qiskit import QuantumCircuit, transpile

if TYPE_CHECKING:
    from collections.abc import Callable, Iterable, Iterator, Mapping
    from multiprocessing import Queue
    from typing import Any, ParamSpec, TypeVar

    from mqt.bench import BenchmarkLevel
    from mqt.core.ir import QuantumComputation

    from mqt.qmap.na.zoned import PlacementAndRoutingAwareCompiler, RoutingAwareCompiler

    P = ParamSpec("P")
    R = TypeVar("R")


"""Timeout for running the benchmark in seconds."""
TIMEOUT = 15 * 60  # sec


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
    # "fork" context avoids pickling bound methods but is Unix/macOS only.
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
            circuit = get_benchmark(benchmark, mode, qubits)
            transpiled = transpile_benchmark(benchmark, circuit)
            qc = load(transpiled)
            yield benchmark, qc


def _compile_wrapper(
    compiler: RoutingAwareCompiler | PlacementAndRoutingAwareCompiler, qc: QuantumComputation
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
    compiler: RoutingAwareCompiler | PlacementAndRoutingAwareCompiler,
    setting_name: str,
    qc: QuantumComputation,
    benchmark_name: str,
    evaluator: Evaluator,
) -> bool:
    """Compile and evaluate the given benchmark circuit.

    Args:
        compiler: The compiler to use.
        setting_name: Name of the compiler setting.
        qc: The benchmark circuit to compile.
        benchmark_name: Name of the benchmark.
        evaluator: The evaluator to use.

    Returns:
        True if compilation succeeded, False otherwise.
    """
    compiler_name = type(compiler).__name__
    print(f"\033[32m[INFO]\033[0m Compiling {benchmark_name} with {qc.num_qubits} qubits with {compiler_name}...")
    try:
        code, stats = run_with_process_timeout(_compile_wrapper, TIMEOUT, compiler, qc)
    except TimeoutError as e:
        print(f"\033[31m[ERROR]\033[0m Failed ({e})")
        evaluator.print_timeout(benchmark_name, qc, setting_name)
        return False
    except MemoryError as e:
        print(f"\033[31m[ERROR]\033[0m Failed ({e})")
        evaluator.print_memout(benchmark_name, qc, setting_name)
        return False
    except RuntimeError as e:
        print(f"\033[31m[ERROR]\033[0m Failed ({e})")
        evaluator.print_error(benchmark_name, qc, setting_name)
        return False

    code = "\n".join(line for line in code.splitlines() if not line.startswith("@+ u"))
    pathlib.Path(f"out/{compiler_name}/{setting_name}").mkdir(exist_ok=True, parents=True)
    pathlib.Path(f"out/{compiler_name}/{setting_name}/{benchmark_name}_{qc.num_qubits}.naviz").write_text(
        code, encoding="utf-8"
    )
    print("\033[32m[INFO]\033[0m Done")

    print(f"\033[32m[INFO]\033[0m Evaluating {benchmark_name} with {qc.num_qubits} qubits...")
    evaluator.reset()
    evaluator.evaluate(benchmark_name, qc, setting_name, code, stats)
    evaluator.print_data()
    print("\033[32m[INFO]\033[0m Done")
    return True


class Evaluator:
    """Class for evaluating compiled circuits.

    Attributes:
        arch: The architecture dictionary.
        filename: The output CSV filename.
        circuit_name: Name of the circuit.
        setting: Compiler setting name.
        num_qubits: Number of qubits.
        two_qubit_gates: Number of two-qubit gates.
        scheduling_time: Time taken for scheduling.
        reuse_analysis_time: Time taken for reuse analysis.
        placement_time: Time taken for placement.
        routing_time: Time taken for routing.
        code_generation_time: Time taken for code generation.
        total_time: Total compilation time.
        rearrangement_duration: Duration of rearrangement operations.
        two_qubit_gate_layer: Number of two-qubit gate layers.
        max_two_qubit_gates: Maximum number of two-qubit gates in a layer.
        atom_locations: Dictionary of atom locations.
    """

    def __init__(self, arch: Mapping[str, Any], filename: str) -> None:
        """Initialize the Evaluator.

        Args:
            arch: The architecture dictionary.
            filename: The output CSV filename.
        """
        self.arch = arch
        self.filename = filename

        self.reset()

    def reset(self) -> None:
        """Reset the Evaluator."""
        self.circuit_name = ""
        self.num_qubits = 0
        self.setting = ""
        self.two_qubit_gates = 0

        self.scheduling_time = 0
        self.reuse_analysis_time = 0
        self.placement_time = 0
        self.routing_time = 0
        self.code_generation_time = 0
        self.total_time = 0

        self.rearrangement_duration = 0.0
        self.two_qubit_gate_layer = 0
        self.max_two_qubit_gates = 0

        self.atom_locations = {}

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
                if next_line_stripped not in self.atom_locations:
                    msg = f"Atom {next_line_stripped} not found in atom locations"
                    raise ValueError(msg)
                atoms.append(next_line_stripped)
        else:
            # Single atom load
            match = re.match(r"@\+ load (\w+)", line)
            if match:
                atom = match.group(1)
                if atom not in self.atom_locations:
                    msg = f"Atom {atom} not found in atom locations"
                    raise ValueError(msg)
                atoms.append(atom)
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

    def _apply_load(self, _: list[str]) -> None:
        """Apply a load operation.

        Args:
            _: List of atoms to load.
        """
        self.rearrangement_duration += self.arch["operation_duration"]["atom_transfer"]

    def _apply_move(self, moves: list[tuple[str, tuple[int, int]]]) -> None:
        """Apply a move operation.

        Args:
            moves: List of tuples containing atom names and their target coordinates.
        """
        max_distance = 0.0
        for atom, coord in moves:
            if atom in self.atom_locations:
                distance = sqrt(
                    (coord[0] - self.atom_locations[atom][0]) ** 2 + (coord[1] - self.atom_locations[atom][1]) ** 2
                )
                max_distance = max(max_distance, distance)

        # Movement timing model parameters (units: um, us)
        t_d_max = 200  # Time to traverse max distance (us)
        d_max = 110  # Maximum distance for cubic profile (um)
        jerk = 32 * d_max / t_d_max**3  # 0.00044, Jerk constant (um/us³)
        v_max = d_max / t_d_max * 2  # = 1.1, Maximum velocity (um/us)

        if max_distance <= d_max:
            rearrangement_time = 2 * (4 * max_distance / jerk) ** (1 / 3)
        else:
            rearrangement_time = t_d_max + (max_distance - d_max) / v_max
        self.rearrangement_duration += rearrangement_time
        # Update atom locations
        for atom, coord in moves:
            assert atom in self.atom_locations, f"Atom {atom} not found in atom locations"
            self.atom_locations[atom] = coord

    def _apply_store(self, _: list[str]) -> None:
        """Apply a store operation.

        Args:
            _: List of atoms to store.
        """
        self.rearrangement_duration += self.arch["operation_duration"]["atom_transfer"]

    def _apply_cz(self, atoms: list[str]) -> None:
        """Apply a cz operation.

        Args:
            atoms: List of atoms involved in the cz operation.
        """
        self.two_qubit_gate_layer += 1
        self.max_two_qubit_gates = max(self.max_two_qubit_gates, len(atoms) // 2)

    def _apply_u(self, atoms: list[str]) -> None:
        """Apply an u operation.

        Args:
            atoms: List of atoms involved in the u operation.
        """

    def _apply_global_u(self) -> None:
        """Apply a global u operation."""

    def _apply_global_ry(self) -> None:
        """Apply a global rydberg gate operation."""
        self._apply_global_u()

    def _apply_rz(self, atoms: list[str]) -> None:
        """Apply a rz operation.

        Args:
            atoms: List of atoms involved in the rz operation.
        """
        self._apply_u(atoms)

    def evaluate(self, name: str, qc: QuantumComputation, setting: str, code: str, stats: Mapping[str, Any]) -> None:
        """Evaluate a circuit.

        Args:
            name: Name of the circuit.
            qc: The quantum circuit.
            setting: Compiler setting name.
            code: The compiled code.
            stats: Compilation statistics.
        """
        self.circuit_name = name
        self.num_qubits = qc.num_qubits
        self.setting = setting
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
                it = chain([line], it)
                break

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
            elif line.startswith("@+ rz"):
                self._process_rz(line, it)
            else:
                msg = f"Unrecognized operation: {line}"
                raise ValueError(msg)

    def print_header(self) -> None:
        """Print the header of the CSV file."""
        pathlib.Path(self.filename).write_text(
            "circuit_name,num_qubits,setting,status,two_qubit_gates,scheduling_time,reuse_analysis_time,"
            "placement_time,routing_time,code_generation_time,total_time,two_qubit_gate_layer,max_two_qubit_gates,"
            "rearrangement_duration\n",
            encoding="utf-8",
        )

    def print_data(self) -> None:
        """Print the data of the CSV file."""
        with pathlib.Path(self.filename).open("a", encoding="utf-8") as csv:
            csv.write(
                f"{self.circuit_name},{self.num_qubits},{self.setting},ok,{self.two_qubit_gates},"
                f"{self.scheduling_time},{self.reuse_analysis_time},{self.placement_time},"
                f"{self.routing_time},{self.code_generation_time},{self.total_time},{self.two_qubit_gate_layer},"
                f"{self.max_two_qubit_gates},{self.rearrangement_duration}\n"
            )

    def print_timeout(self, circuit_name: str, qc: QuantumComputation, setting: str) -> None:
        """Print the data of the CSV file.

        Args:
            circuit_name: Name of the circuit.
            qc: The quantum circuit.
            setting: Compiler setting name.
        """
        with pathlib.Path(self.filename).open("a", encoding="utf-8") as csv:
            csv.write(f"{circuit_name},{qc.num_qubits},{setting},timeout,,,,,,,,,\n")

    def print_memout(self, circuit_name: str, qc: QuantumComputation, setting: str) -> None:
        """Print the data of the CSV file.

        Args:
            circuit_name: Name of the circuit.
            qc: The quantum circuit.
            setting: Compiler setting name.
        """
        with pathlib.Path(self.filename).open("a", encoding="utf-8") as csv:
            csv.write(f"{circuit_name},{qc.num_qubits},{setting},memout,,,,,,,,,\n")

    def print_error(self, circuit_name: str, qc: QuantumComputation, setting: str) -> None:
        """Print the data of the CSV file.

        Args:
            circuit_name: Name of the circuit.
            qc: The quantum circuit.
            setting: Compiler setting name.
        """
        with pathlib.Path(self.filename).open("a", encoding="utf-8") as csv:
            csv.write(f"{circuit_name},{qc.num_qubits},{setting},error,,,,,,,,,\n")
