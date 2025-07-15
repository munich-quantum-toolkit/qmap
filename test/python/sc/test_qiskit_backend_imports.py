# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
# Copyright (c) 2025 Munich Quantum Software Company GmbH
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
from qiskit.providers.fake_provider import GenericBackendV2

from mqt.qmap.plugins.qiskit.sc import compile  # noqa: A004


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
    qc, results = compile(example_circuit, arch=backend)
    assert results.timeout is False
    assert verify(example_circuit, qc).considered_equivalent()


def test_architecture_from_v2_target(example_circuit: QuantumCircuit, backend: GenericBackendV2) -> None:
    """Test that circuits can be mapped by simply providing the target (the BackendV2 way)."""
    qc, results = compile(example_circuit, arch=None, calibration=backend.target)
    assert results.timeout is False
    assert verify(example_circuit, qc).considered_equivalent()
