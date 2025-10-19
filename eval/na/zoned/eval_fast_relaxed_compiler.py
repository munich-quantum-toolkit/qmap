#!/usr/bin/env python3
# -*- coding: utf-8 -*-
from collections.abc import Iterator, Mapping
from typing import Any, Tuple
from math import sqrt, exp
import re
import json
import os
from qiskit import QuantumCircuit, transpile
from mqt.core import load
from mqt.core.ir import QuantumComputation
from mqt.bench.benchmark_generation import get_benchmark
from mqt.bench import BenchmarkLevel
from mqt.qmap.na.zoned import ZonedNeutralAtomArchitecture, RoutingAwareCompiler, RelaxedRoutingAwareCompiler, \
    FastRelaxedRoutingAwareCompiler

BENCHMARKS = {
    'graphstate': (BenchmarkLevel.INDEP, {20, 40, 60, 80, 100}),
    'qft': (BenchmarkLevel.INDEP, {100, 200, 500}),
    'qpeexact': (BenchmarkLevel.INDEP, {100, 200}),
    'wstate': (BenchmarkLevel.INDEP, {50, 100, 200, 500}),
}


def transpile_benchmark(benchmark: str, circuit: QuantumCircuit) -> QuantumCircuit:
    print(f'\033[32m[INFO]\033[0m Transpiling {benchmark}...', flush=True, end='')
    flattened = QuantumCircuit(circuit.num_qubits, circuit.num_clbits)
    flattened.compose(circuit, inplace=True)
    transpiled = transpile(flattened, basis_gates=['cz', 'id', 'u2', 'u1', 'u3'], optimization_level=3,
                           seed_transpiler=0)
    stripped = QuantumCircuit(*transpiled.qregs, *transpiled.cregs)
    for instr in transpiled.data:
        if instr.operation.name != 'measure' and instr.operation.name != 'barrier':
            stripped.append(instr)
    print(' Done')
    return stripped


def benchmarks() -> Iterator[Tuple[str, QuantumComputation]]:
    for benchmark, settings in BENCHMARKS.items():
        mode, limits = settings
        for qubits in limits:
            print(f'\033[32m[INFO]\033[0m Creating {benchmark} with {qubits} qubits...', flush=True, end='')
            circuit = get_benchmark(benchmark, mode, qubits)
            print(' Done')
            transpiled = transpile_benchmark(benchmark, circuit)
            qc = load(transpiled)
            yield benchmark, qc


def process_benchmark(compiler, compiler_name: str, setting_name: str, qc: QuantumComputation, benchmark_name: str):
    print(f'\033[32m[INFO]\033[0m Compiling {benchmark_name} with {compiler_name}...', flush=True, end='')
    code = compiler.compile(qc)
    print(' Done')
    print(f'\033[32m[INFO]\033[0m Evaluating {benchmark_name}...', flush=True, end='')
    evaluator.reset()
    evaluator.evaluate(benchmark_name, compiler_name, qc, setting_name, code, compiler.stats())
    evaluator.print_data()
    print(' Done')
    code = '\n'.join(line for line in code.splitlines() if not line.startswith('@+ u'))
    os.makedirs(f'out/{compiler_name}/{setting_name}', exist_ok=True)
    with open(f'out/{compiler_name}/{setting_name}/{benchmark_name}_{qc.num_qubits}.naviz', 'w') as f:
        f.write(code)


class Evaluator:
    def __init__(self, arch: Mapping[str, Any], filename: str):
        self.arch = arch
        self.filename = filename

        self.circuit_name = ''
        self.compiler = ''
        self.setting = ''
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

        self.min_two_qubit_gates = None
        self.max_two_qubit_gates = None
        self.sum_two_qubit_gates = 0

        self.last_op_is_shuttling = False
        self.last_op_is_store = False

        self.atom_locations = {}
        self.atom_busy_times = {}
        self.atom_busy_rearrangement_times = {}

    def reset(self):
        self.__init__(self.arch, self.filename)

    def _process_load(self, line: str, it: Iterator[str]):
        # Extract atoms from the load operation
        atoms = []
        match = re.match(r'@\+ load \[', line)
        if match:
            # Multi-line load
            for next_line in it:
                next_line = next_line.strip()
                if next_line == ']':
                    break
                assert next_line in self.atom_locations.keys(), f'Atom {next_line} not found in atom locations'
                atoms.append(next_line)
        else:
            # Single atom load
            match = re.match(r'@\+ load (\w+)', line)
            if match:
                assert match.group(
                    1) in self.atom_locations.keys(), f'Atom {match.group(1)} not found in atom locations'
                atoms.append(match.group(1))
        self._apply_load(atoms)

    def _process_move(self, line: str, it: Iterator[str]):
        # Extract atoms and coordinates from the move operation
        moves = []
        match = re.match(r'@\+ move \[', line)
        if match:
            # Multi-line move
            for next_line in it:
                next_line = next_line.strip()
                if next_line == ']':
                    break
                move_match = re.match(r'\((-?\d+\.\d+), (-?\d+\.\d+)\) (\w+)', next_line)
                if move_match:
                    x, y, atom = move_match.groups()
                    assert atom in self.atom_locations.keys(), f'Atom {atom} not found in atom locations'
                    moves.append((atom, (int(float(x)), int(float(y)))))
        else:
            # Single atom move
            match = re.match(r'@\+ move \((-?\d+\.\d+), (-?\d+\.\d+)\) (\w+)', line)
            if match:
                x, y, atom = match.groups()
                assert atom in self.atom_locations.keys(), f'Atom {atom} not found in atom locations'
                moves.append((atom, (int(float(x)), int(float(y)))))
        self._apply_move(moves)

    def _process_store(self, line: str, it: Iterator[str]):
        # Extract atoms from the store operation
        match = re.match(r'@\+ store \[', line)
        atoms = []
        if match:
            # Multi-line store
            for next_line in it:
                next_line = next_line.strip()
                if next_line == ']':
                    break
                assert next_line in self.atom_locations.keys(), f'Atom {next_line} not found in atom locations'
                atoms.append(next_line)
        else:
            # Single atom store
            match = re.match(r'@\+ store (\w+)', line)
            if match:
                assert match.group(
                    1) in self.atom_locations.keys(), f'Atom {match.group(1)} not found in atom locations'
                atoms.append(match.group(1))
        self._apply_store(atoms)

    def _process_cz(self):
        atoms = []
        y_min = self.arch['entanglement_zones'][0]['slms'][0]['location'][1]
        for atom, coord in self.atom_locations.items():
            if coord[1] >= y_min:  # atom is in the entanglement zone
                atoms.append(atom)
        assert len(atoms) % 2 == 0, f'Expected even number of atoms in entanglement zone, got {len(atoms)}'
        self._apply_cz(atoms)

    def _process_u(self, line: str, it: Iterator[str]):
        # Extract atoms from u operation
        atoms = []
        match = re.match(r'@\+ u( \d\.\d+){3} \[', line)
        if match:
            # Multi-line u
            for next_line in it:
                next_line = next_line.strip()
                if next_line == ']':
                    break
                assert next_line in self.atom_locations.keys(), f'Atom {next_line} not found in atom locations'
                atoms.append(next_line)
        else:
            # Single atom u
            match = re.match(r'@\+ u( \d\.\d+){3} (\w+)', line)
            if match:
                if match.group(2) not in self.atom_locations.keys():
                    self._apply_global_u()
                    return
                atoms.append(match.group(2))
        self._apply_u(atoms)

    def _process_rz(self, line: str, it: Iterator[str]):
        # Extract atoms from u operation
        atoms = []
        match = re.match(r'@\+ rz \d\.\d+ \[', line)
        if match:
            # Multi-line u
            for next_line in it:
                next_line = next_line.strip()
                if next_line == ']':
                    break
                assert next_line in self.atom_locations.keys(), f'Atom {next_line} not found in atom locations'
                atoms.append(next_line)
        else:
            # Single atom u
            match = re.match(r'@\+ rz \d\.\d+ (\w+)', line)
            if match:
                assert match.group(
                    1) in self.atom_locations.keys(), f'Atom {match.group(1)} not found in atom locations'
                atoms.append(match.group(1))
        self._apply_rz(atoms)

    def _apply_load(self, atoms: list[str]):
        if not self.last_op_is_shuttling:
            self.rearrangement_layer += 1
        self.last_op_is_shuttling = True
        self.last_op_is_store = False

        self.transfer_fidelity *= self.arch['operation_fidelity']['atom_transfer'] ** len(atoms)
        self.circuit_duration += self.arch['operation_duration']['atom_transfer']
        self.rearrangement_duration += self.arch['operation_duration']['atom_transfer']
        for atom in atoms:
            self.atom_busy_times[atom] += self.arch['operation_duration']['atom_transfer']
            self.atom_busy_rearrangement_times[atom] += self.arch['operation_duration']['atom_transfer']

    def _apply_move(self, moves: list[tuple[str, tuple[int, int]]]):
        self.last_op_is_shuttling = True
        # do not change this value to ignore intermediate moves between store operations
        # self.last_op_is_store = False

        max_distance = 0.0
        for atom, coord in moves:
            if atom in self.atom_locations.keys():
                distance = sqrt(
                    (coord[0] - self.atom_locations[atom][0]) ** 2 + (coord[1] - self.atom_locations[atom][1]) ** 2)
                max_distance = max(max_distance, distance)

        a = 0.00275
        rearrangement_time = sqrt(max_distance / a)
        self.circuit_duration += rearrangement_time
        self.rearrangement_duration += rearrangement_time
        # Update atom locations
        for atom, coord in moves:
            assert atom in self.atom_locations.keys(), f'Atom {atom} not found in atom locations'
            self.atom_locations[atom] = coord

    def _apply_store(self, atoms: list[str]):
        if not self.last_op_is_store:
            self.rearrangement_steps += 1
        self.last_op_is_shuttling = True
        self.last_op_is_store = True

        self.transfer_fidelity *= self.arch['operation_fidelity']['atom_transfer'] ** len(atoms)
        self.circuit_duration += self.arch['operation_duration']['atom_transfer']
        self.rearrangement_duration += self.arch['operation_duration']['atom_transfer']
        for atom in atoms:
            self.atom_busy_times[atom] += self.arch['operation_duration']['atom_transfer']
            self.atom_busy_rearrangement_times[atom] += self.arch['operation_duration']['atom_transfer']

    def _apply_cz(self, atoms: list[str]):
        self.last_op_is_shuttling = False
        self.last_op_is_store = False

        # assuming the compiler works correctly, if there are n atoms in the
        # entanglement zone, n/2 gates are executed and there is no idling atom
        self.two_qubit_gate_fidelity *= self.arch['operation_fidelity']['rydberg_gate'] ** (len(atoms) / 2)

        self.circuit_duration += self.arch['operation_duration']['rydberg_gate']
        for atom in atoms:
            self.atom_busy_times[atom] += self.arch['operation_duration']['single_qubit_gate']

        self.two_qubit_gate_layer += 1
        self.min_two_qubit_gates = min(self.min_two_qubit_gates, len(atoms) / 2) if self.min_two_qubit_gates else len(
            atoms) / 2
        self.sum_two_qubit_gates += len(atoms) / 2
        self.max_two_qubit_gates = max(self.max_two_qubit_gates, len(atoms) / 2) if self.max_two_qubit_gates else len(
            atoms) / 2

    def _apply_u(self, atoms: list[str]):
        self.last_op_is_shuttling = False
        self.last_op_is_store = False

        self.one_qubit_gate_fidelity *= self.arch['operation_fidelity']['single_qubit_gate'] ** len(atoms)

        self.circuit_duration += self.arch['operation_duration']['single_qubit_gate']
        for atom in atoms:
            self.atom_busy_times[atom] += self.arch['operation_duration']['single_qubit_gate']

    def _apply_global_u(self):
        self.last_op_is_shuttling = False
        self.last_op_is_store = False

        self.one_qubit_gate_fidelity *= self.arch['operation_fidelity']['single_qubit_gate'] ** len(self.atom_locations)

        self.circuit_duration += self.arch['operation_duration']['single_qubit_gate']
        for atom in self.atom_locations.keys():
            self.atom_busy_times[atom] += self.arch['operation_duration']['single_qubit_gate']

    def _apply_global_ry(self):
        self._apply_global_u()

    def _apply_rz(self, atoms: list[str]):
        self._apply_u(atoms)

    def evaluate(self, name: str, compiler: str, qc: QuantumComputation, setting: str, code: str,
                 stats: Mapping[str, Any]):
        self.circuit_name = name
        self.compiler = compiler
        self.setting = setting
        self.one_qubit_gates = sum(len(op.get_used_qubits()) == 1 for op in qc)
        self.two_qubit_gates = sum(len(op.get_used_qubits()) == 2 for op in qc)

        self.scheduling_time = stats['schedulingTime']
        self.reuse_analysis_time = stats['reuseAnalysisTime']
        self.placement_time = stats['layoutSynthesizerStatistics']['placementTime']
        self.routing_time = stats['layoutSynthesizerStatistics']['routingTime']
        self.code_generation_time = stats['codeGenerationTime']
        self.total_time = stats['totalTime']

        it = iter(code.splitlines())

        for line in it:
            match = re.match(r'atom\s+\((-?\d+\.\d+),\s*(-?\d+\.\d+)\)\s+(\w+)', line)
            if match:
                x, y, atom_name = match.groups()
                self.atom_locations[atom_name] = (int(float(x)), int(float(y)))
            else:
                # put line back on top of iterator
                it = iter([line] + list(it))
                break

        self.atom_busy_times = {atom: 0.0 for atom in self.atom_locations.keys()}
        self.atom_busy_rearrangement_times = {atom: 0.0 for atom in self.atom_locations.keys()}

        for line in it:
            if line.startswith('@+ load'):
                self._process_load(line, it)
            elif line.startswith('@+ move'):
                self._process_move(line, it)
            elif line.startswith('@+ store'):
                self._process_store(line, it)
            elif line.startswith('@+ cz'):
                self._process_cz()
            elif line.startswith('@+ u'):
                self._process_u(line, it)
            else:
                assert line.startswith('@+ rz'), f'Unrecognized operation: {line}'
                self._process_rz(line, it)

        # in contrast to the transfer fidelity, this fidelity includes decoherence
        # during rearrangement
        self.rearrangement_fidelity = self.transfer_fidelity
        for atom, busy_time in self.atom_busy_rearrangement_times.items():
            idle_time = self.rearrangement_duration - busy_time
            t_eff = self.arch['qubit_spec']['T']
            self.rearrangement_fidelity *= exp(-idle_time / t_eff)
        self.coherence_fidelity = 1.0
        for atom, busy_time in self.atom_busy_times.items():
            idle_time = self.circuit_duration - busy_time
            t_eff = self.arch['qubit_spec']['T']
            self.coherence_fidelity *= exp(-idle_time / t_eff)
        self.mean_two_qubit_gates = self.sum_two_qubit_gates / self.two_qubit_gate_layer

    def print_header(self):
        with open(self.filename, "w") as csv:
            csv.write(
                f'circuit_name,compiler,setting,one_qubit_gates,two_qubit_gates,scheduling_time,reuse_analysis_time,'
                f'placement_time,routing_time,code_generation_time,total_time,two_qubit_gate_layer,'
                f'min_two_qubit_gates,mean_two_qubit_gates,max_two_qubit_gates,one_qubit_gate_fidelity,'
                f'two_qubit_gate_fidelity,transfer_fidelity,coherence_fidelity,total_fidelity,'
                f'rearrangement_layer,rearrangement_steps,rearrangement_duration,rearrangement_fidelity,'
                f'circuit_duration\n')

    def print_data(self):
        with open(self.filename, "a") as csv:
            csv.write(
                f'{self.circuit_name},{self.compiler},{self.setting},{self.one_qubit_gates},{self.two_qubit_gates},'
                f'{self.scheduling_time},{self.reuse_analysis_time},{self.placement_time},{self.routing_time},'
                f'{self.code_generation_time},{self.total_time},{self.two_qubit_gate_layer},{self.min_two_qubit_gates},'
                f'{self.mean_two_qubit_gates},{self.max_two_qubit_gates},{self.one_qubit_gate_fidelity},'
                f'{self.two_qubit_gate_fidelity},{self.transfer_fidelity},{self.coherence_fidelity},'
                f'{self.one_qubit_gate_fidelity * self.two_qubit_gate_fidelity * self.transfer_fidelity * \
                   self.coherence_fidelity},'
                f'{self.rearrangement_layer},{self.rearrangement_steps},{self.rearrangement_duration},'
                f'{self.rearrangement_fidelity},{self.circuit_duration}\n')


if __name__ == '__main__':
    print(f'\033[32m[INFO]\033[0m Reading in architecture...', flush=True, end='')
    with open('full_architecture.json', 'r') as f:
        arch_dict = json.load(f)
    arch = ZonedNeutralAtomArchitecture.from_json_file('full_architecture.json')
    arch.to_namachine_file('arch.namachine')
    print(' Done')
    compiler_default = RoutingAwareCompiler(arch, log_level='error')
    relaxed_compiler_default = RelaxedRoutingAwareCompiler(arch, log_level='error')
    fast_compiler_default = FastRelaxedRoutingAwareCompiler(arch, trials=100, log_level='error')
    fast_compiler_half_deepening = FastRelaxedRoutingAwareCompiler(arch, deepening_value=0.1, trials=100, log_level='error')
    fast_compiler_no_deepening = FastRelaxedRoutingAwareCompiler(arch, deepening_value=0, trials=100, log_level='error')
    evaluator = Evaluator(arch_dict, 'results.csv')
    evaluator.print_header()
    for benchmark, qc in benchmarks():
        if not (benchmark == 'graphstate' and qc.num_qubits > 50) and not qc.num_qubits > 100:
            process_benchmark(compiler_default, 'RoutingAwareCompiler', 'default', qc, benchmark)
            process_benchmark(relaxed_compiler_default, 'RelaxedRoutingAwareCompiler', 'default', qc, benchmark)
        process_benchmark(fast_compiler_default, 'FastRelaxedRoutingAwareCompiler', 'default', qc, benchmark)
        process_benchmark(fast_compiler_half_deepening, 'FastRelaxedRoutingAwareCompiler', 'half_deepening', qc, benchmark)
        process_benchmark(fast_compiler_no_deepening, 'FastRelaxedRoutingAwareCompiler', 'no_deepening', qc, benchmark)
