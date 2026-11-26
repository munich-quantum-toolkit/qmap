# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""Test the compilation of circuits."""

from __future__ import annotations

import locale
import math
from pathlib import Path

import pytest
from mqt.core import load
from mqt.core.dd import build_unitary
from mqt.core.ir import QuantumComputation
from mqt.core.ir.operations import Control, OpType, StandardOperation
from mqt.core.ir.symbolic import Expression, Term, Variable
from mqt.qcec import verify
from qiskit import QuantumCircuit
from qiskit.quantum_info import Operator

from mqt.qmap.plugins.qiskit.sc import compile_
from mqt.qmap.sc import (
    Arch,
    Architecture,
    CommanderGrouping,
    Configuration,
    Encoding,
    Heuristic,
    InitialLayout,
    Layering,
    LookaheadHeuristic,
    Method,
    SwapReduction,
    map_,
)
from mqt.qmap.visualization import SearchVisualizer


@pytest.fixture
def example_circuit() -> QuantumCircuit:
    """Return a simple example circuit."""
    qc = QuantumCircuit(3)
    qc.h(0)
    qc.cx(0, 1)
    qc.cx(1, 2)
    qc.measure_all()
    return qc


def test_either_arch_or_calibration(example_circuit: QuantumCircuit) -> None:
    """Test that either arch or calibration must be provided."""
    with pytest.raises(ValueError, match="Either arch or calibration must be specified"):
        compile_(example_circuit, arch=None, calibration=None)


def test_circuit_name_is_preserved(example_circuit: QuantumCircuit) -> None:
    """Test that mapping preserves the input circuit name."""
    example_circuit.name = "my_bell_chain"

    mapped, _ = compile_(example_circuit, arch=Arch.IBM_QX4, method=Method.exact)

    assert mapped.name == example_circuit.name


@pytest.mark.parametrize(
    ("method", "layout"),
    [
        (Method.heuristic, InitialLayout.dynamic),
        (Method.heuristic, InitialLayout.identity),
        (Method.exact, InitialLayout.dynamic),
    ],
)
@pytest.mark.parametrize("identity_gate", [False, True])
def test_layout_with_idle_qubit(method: Method, layout: InitialLayout, identity_gate: bool) -> None:
    """Keep the idle logical wire in the layout, including when routing moves it."""
    qc = QuantumCircuit(3)
    qc.h(0)
    if identity_gate:
        qc.id(1)
    qc.cx(0, 2)

    line = Architecture(3, {(0, 1), (1, 0), (1, 2), (2, 1)})
    config = Configuration()
    config.method = method
    config.initial_layout = layout
    config.add_measurements_to_mapped_circuit = False
    circuit = load(qc)
    mapped, _ = map_(circuit, line, config)

    assert sorted(mapped.output_permutation[q] for q in range(qc.num_qubits)) == [0, 1, 2]
    assert verify(circuit, mapped).considered_equivalent()


@pytest.mark.parametrize("method", [Method.heuristic, Method.exact])
def test_compact_circuit_on_narrow_device(method: Method) -> None:
    """Place a high-index input wire on a device that fits only the active circuit."""
    qc = QuantumCircuit(4)
    qc.h(3)
    compact = QuantumCircuit(1)
    compact.h(0)

    mapped, _ = compile_(
        qc, arch=Architecture(2, {(0, 1), (1, 0)}), method=method, add_measurements_to_mapped_circuit=False
    )
    assert verify(compact, mapped).considered_equivalent()


@pytest.mark.parametrize("method", [Method.heuristic, Method.exact])
def test_empty_circuit_on_narrow_device(method: Method) -> None:
    """A fully compacted circuit has no outputs to measure."""
    mapped, _ = compile_(QuantumCircuit(4), arch=Architecture(2, {(0, 1), (1, 0)}), method=method)
    assert mapped.num_clbits == 0
    assert mapped.size() == 0


@pytest.mark.parametrize("method", [Method.heuristic, Method.exact])
def test_route_controlled_gates(method: Method) -> None:
    """Route all three qubit pairs on a line and preserve the circuit operator."""
    circuit = QuantumCircuit.from_qasm_str(
        'OPENQASM 2.0; include "qelib1.inc"; qreg q[3];'
        "cz q[0],q[1]; cy q[1],q[2]; ch q[2],q[0];"
        "crz(0.3) q[0],q[1]; cp(0.2) q[1],q[2];"
        "cu1(0.4) q[2],q[0]; cu3(0.3,0.2,0.1) q[0],q[2]; cx q[1],q[0];"
    )
    edges = {(0, 1), (1, 0), (1, 2), (2, 1)}
    mapped, results = compile_(
        circuit,
        Architecture(3, edges),
        method=method,
        initial_layout=InitialLayout.identity,
        iterative_bidirectional_routing_passes=1,
        add_measurements_to_mapped_circuit=False,
    )

    assert Operator.from_circuit(mapped).equiv(Operator(circuit))
    for instruction in mapped.data:
        if instruction.operation.num_qubits == 2:
            assert tuple(mapped.find_bit(q).index for q in instruction.qubits) in edges
    assert results.input_.cnots == 1
    counts = mapped.count_ops()
    assert counts["cz"] == counts["cy"] == 1
    assert results.output.cnots == counts.get("cx", 0) + 3 * counts.get("swap", 0)


@pytest.mark.parametrize(
    ("method", "swap_reduction"),
    [
        (Method.heuristic, SwapReduction.coupling_limit),
        (Method.exact, SwapReduction.coupling_limit),
        (Method.exact, SwapReduction.increasing),
    ],
)
def test_controlled_gate_direction(method: Method, swap_reduction: SwapReduction) -> None:
    """Route opposite CRZ gates through the same one-way coupling."""
    circuit = QuantumCircuit(2)
    circuit.crz(0.3, 0, 1)
    circuit.cx(0, 1)
    circuit.crz(0.2, 1, 0)
    circuit.cx(0, 1)
    mapped, _ = compile_(
        circuit,
        Architecture(2, {(0, 1)}),
        method=method,
        layering=Layering.disjoint_2q_blocks,
        swap_reduction=swap_reduction,
        add_measurements_to_mapped_circuit=False,
    )
    assert Operator.from_circuit(mapped).equiv(Operator(circuit))
    for instruction in mapped.data:
        if instruction.operation.name in {"crz", "cx"}:
            assert [mapped.find_bit(q).index for q in instruction.qubits] == [0, 1]


@pytest.mark.parametrize("control", [0, 1])
def test_exact_counts_repeated_cnot_reversals_in_block(control: int) -> None:
    """Choose the direction that reverses one CNOT in a mixed block."""
    circuit = QuantumCircuit(2)
    target = 1 - control
    circuit.cx(control, target)
    circuit.rxx(0.37, 1, 0)
    circuit.cx(target, control)
    circuit.cx(control, target)
    mapped, results = compile_(
        circuit,
        Architecture(2, {(0, 1)}),
        method=Method.exact,
        layering=Layering.disjoint_2q_blocks,
        post_mapping_optimizations=False,
        add_measurements_to_mapped_circuit=False,
    )

    assert results.input_.layers == 1
    assert results.output.swaps == 0
    assert results.output.direction_reverse == 1
    assert Operator.from_circuit(mapped).equiv(Operator(circuit))


def test_exact_keeps_solution_when_later_subset_is_unsatisfiable() -> None:
    """Only the triangle supports all three interactions with zero SWAPs."""
    circuit = QuantumCircuit(4)
    circuit.crz(0.3, 0, 1)
    circuit.crz(0.4, 1, 2)
    circuit.crz(0.5, 2, 0)
    circuit.cx(1, 0)
    mapped, results = compile_(
        circuit,
        Architecture(4, {(0, 1), (1, 2), (2, 0), (2, 3)}),
        method=Method.exact,
        swap_reduction=SwapReduction.custom,
        swap_limit=0,
        add_measurements_to_mapped_circuit=False,
    )

    assert results.timeout is False
    assert results.output.swaps == 0
    assert Operator.from_circuit(mapped).equiv(Operator(circuit))


@pytest.mark.parametrize("method", [Method.heuristic, Method.exact])
@pytest.mark.parametrize("polarity", [Control.Type.Pos, Control.Type.Neg])
def test_controlled_phase_symmetry(method: Method, polarity: Control.Type) -> None:
    """A positive controlled phase exchanges operands on a one-way edge."""
    circuit = QuantumComputation(2)
    circuit.cz(Control(0, polarity), 1)
    circuit.cp(0.37, Control(1, polarity), 0)
    config = Configuration()
    config.method = method
    config.layering = Layering.disjoint_2q_blocks
    config.add_measurements_to_mapped_circuit = False

    mapped, results = map_(circuit, Architecture(2, {(0, 1)}), config)

    assert Operator(build_unitary(mapped)).equiv(Operator(build_unitary(circuit)))
    assert results.output.swaps == int(polarity == Control.Type.Neg)
    for op in mapped:
        if op.controls:
            assert op.controls == {Control(0, polarity)}
            assert op.targets == [1]


@pytest.mark.parametrize("method", [Method.heuristic, Method.exact])
@pytest.mark.parametrize(
    ("gate", "params", "can_exchange"),
    [
        (OpType.swap, [], True),
        (OpType.iswap, [], True),
        (OpType.iswapdg, [], True),
        (OpType.peres, [], False),
        (OpType.peresdg, [], False),
        (OpType.dcx, [], False),
        (OpType.ecr, [], False),
        (OpType.rxx, [0.23], True),
        (OpType.ryy, [0.31], True),
        (OpType.rzz, [0.43], True),
        (OpType.rzx, [0.59], False),
        (OpType.xx_minus_yy, [0.67, 0.13], True),
        (OpType.xx_plus_yy, [0.71, 0.17], True),
    ],
)
def test_route_two_target_gates(method: Method, gate: OpType, params: list[float], can_exchange: bool) -> None:
    """Check both operand orders of each native Core two-target gate."""
    circuit = QuantumComputation(2)
    circuit.append(StandardOperation([0, 1], gate, params))
    circuit.h(0)
    circuit.append(StandardOperation([1, 0], gate, params))
    config = Configuration()
    config.method = method
    config.layering = Layering.disjoint_2q_blocks
    config.post_mapping_optimizations = False
    config.add_measurements_to_mapped_circuit = False

    mapped, results = map_(circuit, Architecture(2, {(0, 1)}), config)

    assert Operator(build_unitary(mapped)).equiv(Operator(build_unitary(circuit)))
    assert results.output.swaps == int(not can_exchange)
    native_gates = [op for op in mapped if op.type_ == gate]
    assert len(native_gates) == 2
    assert all(op.targets == [0, 1] for op in native_gates)


@pytest.mark.parametrize("method", [Method.heuristic, Method.exact])
def test_symbolic_operands(method: Method) -> None:
    """Keep a negative control and negate the symbolic phase of a reversed XX+YY."""
    beta = Variable("beta")
    circuit = QuantumComputation(2)
    circuit.crz(Expression(Term(beta)), Control(0, Control.Type.Neg), 1)
    circuit.xx_plus_yy(0.41, Expression(Term(beta)), 1, 0)
    config = Configuration()
    config.method = method
    config.add_measurements_to_mapped_circuit = False
    mapped, results = map_(circuit, Architecture(2, {(0, 1)}), config)

    assert results.output.swaps == 0
    assert mapped.variables == {beta}
    bound = mapped.instantiate({beta: 0.19})
    assert bound[0].controls == {Control(0, Control.Type.Neg)}
    assert bound[1].targets == [0, 1]
    assert bound[1].parameter == [0.41, -0.19]
    assert Operator(build_unitary(bound)).equiv(Operator(build_unitary(circuit.instantiate({beta: 0.19}))))


def test_input_and_routing_swaps() -> None:
    """Keep deferred single-qubit gates on the correct wire across both kinds of SWAP."""
    circuit = QuantumCircuit(3)
    circuit.h(1)
    circuit.swap(1, 2)
    circuit.rzz(0.37, 2, 0)
    circuit.rxx(0.41, 0, 1)
    mapped, results = compile_(
        circuit,
        Architecture(3, {(0, 1), (1, 0), (1, 2), (2, 1)}),
        post_mapping_optimizations=False,
        add_measurements_to_mapped_circuit=False,
    )

    assert results.output.swaps >= 1
    assert mapped.count_ops()["swap"] == results.output.swaps + 1
    assert Operator.from_circuit(mapped).equiv(Operator(circuit))


@pytest.mark.parametrize("method", [Method.heuristic, Method.exact])
@pytest.mark.parametrize(
    ("rxx_error", "h_error", "cnot_calibrated"),
    [(0.08, 0.03, False), (0.08, None, False), (None, 0.03, True)],
)
def test_native_gate_calibration(
    method: Method, rxx_error: float | None, h_error: float | None, cnot_calibrated: bool
) -> None:
    """Estimate fidelity when all emitted gates have calibration."""
    properties = Architecture.Properties()
    if cnot_calibrated:
        properties.set_two_qubit_error(0, 1, 0.02, "cx")
    if rxx_error is not None:
        properties.set_two_qubit_error(0, 1, rxx_error, "rxx")
    if h_error is not None:
        for qubit in range(2):
            properties.set_single_qubit_error(qubit, "h", h_error)
    circuit = QuantumCircuit(2)
    circuit.h(0)
    circuit.rxx(0.3, 1, 0)
    mapped, results = compile_(
        circuit, Architecture(2, {(0, 1)}, properties), method=method, add_measurements_to_mapped_circuit=False
    )

    assert Operator.from_circuit(mapped).equiv(Operator(circuit))
    assert results.output.gates == 2
    assert results.output.single_qubit_gates == 1
    assert results.output.cnots == 0
    if rxx_error is None or h_error is None:
        assert math.isnan(results.output.total_fidelity)
    else:
        assert results.output.total_fidelity == pytest.approx((1.0 - rxx_error) * (1.0 - h_error))


@pytest.mark.parametrize("swap_error", [0.08, None])
def test_swap_calibration(swap_error: float | None) -> None:
    """Price SWAP from native or CX calibration supplied through live properties."""
    architecture = Architecture(2, {(0, 1)})
    architecture.properties.set_two_qubit_error(0, 1, 0.02, "cx")
    if swap_error is not None:
        architecture.properties.set_two_qubit_error(0, 1, swap_error, "swap")
    circuit = QuantumCircuit(2)
    circuit.h(0)
    circuit.swap(1, 0)
    _, results = compile_(
        circuit,
        architecture,
        heuristic=Heuristic.fidelity_best_location,
        lookahead_heuristic=LookaheadHeuristic.none,
        add_measurements_to_mapped_circuit=False,
    )

    expected = 0.98**3 if swap_error is None else 1.0 - swap_error
    assert results.output.total_fidelity == pytest.approx(expected)
    assert results.input_.cnots == results.output.cnots == 3
    assert results.output.swaps == 0


@pytest.mark.parametrize(
    ("forward_error", "swap_error", "expected_swaps"),
    [(0.55, 0.02, 1), (0.55, 0.90, 0), (0.55, None, 0), (None, None, None)],
)
def test_native_fidelity_steers_routing(
    forward_error: float | None, swap_error: float | None, expected_swaps: int | None
) -> None:
    """Trade native SWAP error against the error of a directional gate."""
    properties = Architecture.Properties()
    for qubit in range(3):
        properties.set_single_qubit_error(qubit, "h", 0.03)
    if forward_error is not None:
        properties.set_two_qubit_error(0, 1, forward_error, "rzx")
    properties.set_two_qubit_error(1, 0, 0.05, "rzx")
    if swap_error is not None:
        properties.set_two_qubit_error(1, 0, swap_error, "swap")
    if expected_swaps is None:
        # This calibrated SWAP cannot move the first RZX operand.
        properties.set_two_qubit_error(1, 2, 0.02, "swap")

    circuit = QuantumComputation(3)
    circuit.h(2)
    circuit.rzx(0.3, 0, 1)
    architecture = Architecture(3, {(0, 1), (1, 0), (1, 2), (2, 1)}, properties)
    config = Configuration()
    config.initial_layout = InitialLayout.identity
    config.heuristic = Heuristic.fidelity_best_location
    config.lookahead_heuristic = LookaheadHeuristic.none
    config.add_measurements_to_mapped_circuit = False
    if expected_swaps is None:
        with pytest.raises(ValueError, match=r"(?i)calibrat"):
            map_(circuit, architecture, config)
        return

    mapped, results = map_(circuit, architecture, config)
    assert Operator(build_unitary(mapped)).equiv(Operator(build_unitary(circuit)))
    assert results.output.swaps == expected_swaps
    expected_fidelity = 0.97 * (0.98 * 0.95 if expected_swaps else 0.45)
    assert results.output.total_fidelity == pytest.approx(expected_fidelity)
    assert mapped[-1].targets == ([1, 0] if expected_swaps else [0, 1])


@pytest.mark.parametrize("partial_cx", [False, True])
def test_fidelity_orients_symmetric_gate_with_one_calibrated_direction(partial_cx: bool) -> None:
    """Transpose a symmetric gate without routing when just one order has a rate."""
    properties = Architecture.Properties()
    properties.set_two_qubit_error(1, 0, 0.04, "rxx")
    if partial_cx:
        properties.set_two_qubit_error(0, 1, 0.01, "cx")
    circuit = QuantumCircuit(2)
    circuit.rxx(0.3, 0, 1)
    mapped, results = compile_(
        circuit,
        Architecture(2, {(0, 1), (1, 0)}, properties),
        initial_layout=InitialLayout.identity,
        heuristic=Heuristic.fidelity_best_location,
        lookahead_heuristic=LookaheadHeuristic.none,
        add_measurements_to_mapped_circuit=False,
    )

    assert Operator.from_circuit(mapped).equiv(Operator(circuit))
    assert [mapped.find_bit(q).index for q in mapped.data[0].qubits] == [1, 0]
    assert results.output.swaps == 0
    assert results.output.total_fidelity == pytest.approx(0.96)


def test_fidelity_routes_block_with_disjoint_calibrated_directions(tmp_path: Path) -> None:
    """Route a block whose gates need two placements, with no reverse CX rate."""
    properties = Architecture.Properties()
    properties.set_two_qubit_error(0, 1, 0.10, "rzx")
    properties.set_two_qubit_error(0, 1, 0.03, "cx")
    properties.set_two_qubit_error(1, 0, 0.05, "crz")
    properties.set_two_qubit_error(0, 1, 0.02, "swap")
    circuit = QuantumCircuit(2)
    circuit.rzx(0.3, 0, 1)
    circuit.cx(0, 1)
    circuit.crz(0.4, 0, 1)
    mapped, results = compile_(
        circuit,
        Architecture(2, {(0, 1), (1, 0)}, properties),
        layering=Layering.disjoint_2q_blocks,
        heuristic=Heuristic.fidelity_best_location,
        lookahead_heuristic=LookaheadHeuristic.none,
        visualizer=SearchVisualizer(str(tmp_path)),
        add_measurements_to_mapped_circuit=False,
    )

    assert Operator.from_circuit(mapped).equiv(Operator(circuit))
    assert results.output.swaps == 1
    assert results.output.total_fidelity == pytest.approx(0.90 * 0.97 * 0.98 * 0.95)


# test that all available architecture enumerations can be properly used
@pytest.mark.parametrize(
    "arch",
    [
        Arch.IBM_QX4,
        Arch.IBM_QX5,
        Arch.IBMQ_Yorktown,
        Arch.IBMQ_London,
        Arch.IBMQ_Bogota,
        Arch.IBMQ_Tokyo,
        Arch.Rigetti_Agave,
        Arch.Rigetti_Aspen,
    ],
)
def test_available_architectures(example_circuit: QuantumCircuit, arch: Arch) -> None:
    """Test that the available architecture enums can be properly used."""
    example_circuit_mapped, results = compile_(example_circuit, arch=arch)
    assert results.timeout is False

    result = verify(example_circuit, example_circuit_mapped)
    assert result.considered_equivalent() is True


def test_architecture_from_file(example_circuit: QuantumCircuit) -> None:
    """Test that architectures from files can be properly used."""
    with Path("test_architecture.arch").open("w+", encoding=locale.getpreferredencoding(False)) as f:
        f.write("3\n0 1\n0 2\n1 2\n")

    example_circuit_mapped, results = compile_(example_circuit, arch="test_architecture.arch")
    assert results.timeout is False

    result = verify(example_circuit, example_circuit_mapped)
    assert result.considered_equivalent() is True


def test_architecture_from_python(example_circuit: QuantumCircuit) -> None:
    """Test that architectures from python can be properly used."""
    arch = Architecture(3, {(0, 1), (0, 2), (1, 2)})
    example_circuit_mapped, results = compile_(example_circuit, arch=arch)
    assert results.timeout is False

    result = verify(example_circuit, example_circuit_mapped)
    assert result.considered_equivalent() is True


def test_calibration_from_file(example_circuit: QuantumCircuit) -> None:
    """Test that calibrations from files can be properly used."""
    with Path("test_calibration.cal").open("w+", encoding=locale.getpreferredencoding(False)) as f:
        f.write("Header\n")
        f.write('Q0,0,0,0,1e-2,1e-4,"0_1: 1e-2, 0_2: 1e-2"\n')
        f.write('Q1,0,0,0,1e-2,1e-4,"1_2: 1e-2"\n')
        f.write("Q2,0,0,0,1e-2,1e-4, \n")

    example_circuit_mapped, results = compile_(example_circuit, arch=None, calibration="test_calibration.cal")
    assert results.timeout is False

    result = verify(example_circuit, example_circuit_mapped)
    assert result.considered_equivalent() is True


def test_parameters(example_circuit: QuantumCircuit) -> None:
    """Test that parameters to compile are properly parsed and passed to the backend."""
    properties = Architecture.Properties()
    properties.set_single_qubit_error(0, "x", 0.01)
    properties.set_single_qubit_error(1, "x", 0.01)
    properties.set_single_qubit_error(2, "x", 0.01)
    properties.set_two_qubit_error(0, 1, 0.02, "cx")
    properties.set_two_qubit_error(1, 0, 0.02, "cx")
    properties.set_two_qubit_error(1, 2, 0.02, "cx")
    properties.set_two_qubit_error(2, 1, 0.02, "cx")
    arch = Architecture(3, {(0, 1), (1, 0), (1, 2), (2, 1)}, properties)
    _, results = compile_(
        example_circuit,
        arch=arch,
        method=Method.exact,
        encoding=Encoding.commander,
        commander_grouping=CommanderGrouping.fixed3,
        swap_reduction=SwapReduction.coupling_limit,
        include_wcnf=False,
        use_subsets=True,
        subgraph=None,
        add_measurements_to_mapped_circuit=True,
    )
    assert results.configuration.method == Method.exact
    assert results.configuration.encoding == Encoding.commander
    assert results.configuration.commander_grouping == CommanderGrouping.fixed3
    assert results.configuration.swap_reduction == SwapReduction.coupling_limit
    assert results.configuration.include_wcnf is False
    assert results.configuration.use_subsets is True
    assert results.configuration.subgraph == set()
    assert results.configuration.add_measurements_to_mapped_circuit is True

    with SearchVisualizer() as visualizer:
        _, results = compile_(
            example_circuit,
            arch=arch,
            method=Method.heuristic,
            heuristic=Heuristic.gate_count_max_distance,
            initial_layout=InitialLayout.dynamic,
            iterative_bidirectional_routing_passes=1,
            layering=Layering.individual_gates,
            automatic_layer_splits_node_limit=5000,
            lookahead_heuristic=LookaheadHeuristic.gate_count_max_distance,
            lookaheads=15,
            lookahead_factor=0.5,
            pre_mapping_optimizations=True,
            post_mapping_optimizations=True,
            verbose=True,
            debug=True,
            visualizer=visualizer,
        )
        assert results.configuration.method == Method.heuristic
        assert results.configuration.heuristic == Heuristic.gate_count_max_distance
        assert results.configuration.lookahead_heuristic == LookaheadHeuristic.gate_count_max_distance
        assert results.configuration.initial_layout == InitialLayout.dynamic
        assert results.configuration.iterative_bidirectional_routing is True
        assert results.configuration.iterative_bidirectional_routing_passes == 1
        assert results.configuration.layering == Layering.individual_gates
        assert results.configuration.automatic_layer_splits is True
        assert results.configuration.automatic_layer_splits_node_limit == 5000
        assert results.configuration.lookaheads == 15
        assert results.configuration.lookahead_factor == pytest.approx(0.5)
        assert results.configuration.pre_mapping_optimizations is True
        assert results.configuration.post_mapping_optimizations is True
        assert results.configuration.verbose is True
        assert results.configuration.debug is True
        assert results.configuration.data_logging_path == visualizer.data_logging_path

    _, results = compile_(
        example_circuit,
        arch=arch,
        method=Method.heuristic,
        heuristic=Heuristic.fidelity_best_location,
        initial_layout=InitialLayout.identity,
        iterative_bidirectional_routing_passes=None,
        layering=Layering.disjoint_qubits,
        automatic_layer_splits_node_limit=None,
        lookahead_heuristic=None,
        pre_mapping_optimizations=False,
        post_mapping_optimizations=False,
        verbose=False,
        debug=False,
    )
    assert results.configuration.method == Method.heuristic
    assert results.configuration.heuristic == Heuristic.fidelity_best_location
    assert results.configuration.lookahead_heuristic == LookaheadHeuristic.none
    assert results.configuration.initial_layout == InitialLayout.identity
    assert results.configuration.iterative_bidirectional_routing is False
    assert results.configuration.layering == Layering.disjoint_qubits
    assert results.configuration.automatic_layer_splits is False
    assert results.configuration.lookaheads == 0
    assert results.configuration.pre_mapping_optimizations is False
    assert results.configuration.post_mapping_optimizations is False
    assert results.configuration.verbose is False
    assert results.configuration.debug is False
    assert not results.configuration.data_logging_path
