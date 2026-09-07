# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""Test the Qiskit backend imports."""

from __future__ import annotations

import pytest
from mqt.qcec import verify
from qiskit import QuantumCircuit
from qiskit.circuit import Measure
from qiskit.circuit.library import CXGate, HGate
from qiskit.providers import BackendV2, Options, QubitProperties
from qiskit.providers.fake_provider import GenericBackendV2
from qiskit.transpiler import InstructionProperties, Target

from mqt.qmap.plugins.qiskit.sc import compile_, import_backend, import_target


class MinimalBackend(BackendV2):
    """Backend without calibration properties."""

    def __init__(self) -> None:
        """Initialize the backend."""
        super().__init__()
        target = Target(num_qubits=3)
        target.add_instruction(HGate(), {(0,): None, (1,): None, (2,): None})
        target.add_instruction(CXGate(), {(0, 1): None, (1, 2): None})
        target.add_instruction(Measure(), {(0,): None, (1,): None, (2,): None})
        self._target = target

    @property
    def target(self) -> Target:
        """Backend target."""
        return self._target

    @property
    def max_circuits(self) -> int | None:
        """Maximum number of circuits per job."""
        return None

    @classmethod
    def _default_options(cls) -> Options:
        """Return the default backend options."""
        return Options()

    def run(self, *args: object, **kwargs: object) -> None:
        """This backend is only used for compilation."""
        raise NotImplementedError


@pytest.fixture
def example_circuit() -> QuantumCircuit:
    """Return a simple circuit."""
    qc = QuantumCircuit(3)
    qc.h(0)
    qc.cx(0, 1)
    qc.cx(1, 2)
    qc.measure_all()
    return qc


@pytest.fixture
def backend() -> GenericBackendV2:
    """Return a test backend."""
    return GenericBackendV2(num_qubits=5, coupling_map=[[0, 1], [1, 0], [1, 2], [2, 1], [1, 3], [3, 1], [3, 4], [4, 3]])


def test_backend_v2(example_circuit: QuantumCircuit, backend: GenericBackendV2) -> None:
    """Test that circuits can be mapped to Qiskit BackendV1 instances providing the old basis_gates."""
    qc, results = compile_(example_circuit, arch=backend)
    assert results.timeout is False
    assert verify(example_circuit, qc).considered_equivalent()


def test_architecture_from_v2_target(example_circuit: QuantumCircuit, backend: GenericBackendV2) -> None:
    """Test that circuits can be mapped by simply providing the target (the BackendV2 way)."""
    qc, results = compile_(example_circuit, arch=None, calibration=backend.target)
    assert results.timeout is False
    assert verify(example_circuit, qc).considered_equivalent()


def test_backend_without_calibration_properties(example_circuit: QuantumCircuit) -> None:
    """Test importing a backend that does not provide calibration properties."""
    backend = MinimalBackend()
    mapped, results = compile_(example_circuit, arch=backend)

    architecture = import_backend(backend)
    assert architecture.num_qubits == 3
    assert architecture.coupling_map == {(0, 1), (1, 2)}
    assert architecture.properties.num_qubits == 3
    with pytest.raises(IndexError):
        architecture.properties.get_two_qubit_error(0, 1, "cx")
    assert results.timeout is False
    assert verify(example_circuit, mapped).considered_equivalent()


def test_partially_missing_target_properties() -> None:
    """Test that unavailable calibration values remain unavailable."""
    target = Target(
        num_qubits=2,
        qubit_properties=[QubitProperties(t1=1.0), QubitProperties()],
    )
    target.add_instruction(
        HGate(),
        {
            (0,): InstructionProperties(error=0.01),
            (1,): InstructionProperties(error=None),
        },
    )

    properties = import_target(target)
    assert properties.num_qubits == 2
    assert properties.get_t1(0) == pytest.approx(1.0)
    assert properties.get_single_qubit_error(0, "h") == pytest.approx(0.01)
    with pytest.raises(IndexError):
        properties.get_t2(0)
    with pytest.raises(IndexError):
        properties.get_frequency(0)
    with pytest.raises(IndexError):
        properties.get_t1(1)
    with pytest.raises(IndexError):
        properties.get_single_qubit_error(1, "h")


def test_unbounded_target() -> None:
    """Test that an unbounded target cannot be imported."""
    with pytest.raises(ValueError, match="unbounded Qiskit target"):
        import_target(Target(num_qubits=None))
