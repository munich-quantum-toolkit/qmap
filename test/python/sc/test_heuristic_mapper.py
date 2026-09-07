# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""Test the heuristic mapper."""

from __future__ import annotations

import pytest
from mqt.qcec import verify
from qiskit import QuantumCircuit
from qiskit.providers.fake_provider import GenericBackendV2

from mqt.qmap.plugins.qiskit.sc import compile_
from mqt.qmap.sc import Heuristic, InitialLayout, Layering


@pytest.fixture
def backend() -> GenericBackendV2:
    """Return a test backend."""
    return GenericBackendV2(num_qubits=5, coupling_map=[[0, 1], [1, 0], [1, 2], [2, 1], [1, 3], [3, 1], [3, 4], [4, 3]])


def test_heuristic_no_swaps_trivial_layout(backend: GenericBackendV2) -> None:
    """Verify that the heuristic mapper works on a simple circuit that requires no swaps on a trivial initial layout."""
    qc = QuantumCircuit(3)
    qc.h(0)
    qc.cx(0, 1)
    qc.cx(1, 2)
    qc.measure_all()

    qc_mapped, results = compile_(qc, arch=backend)
    assert results.timeout is False
    # assert results.output.swaps == 0

    result = verify(qc, qc_mapped)
    assert result.considered_equivalent() is True


def test_heuristic_no_swaps_non_trivial_layout(backend: GenericBackendV2) -> None:
    """Verify that the heuristic mapper works on a simple circuit that requires a non-trivial layout to achieve no swaps."""
    qc = QuantumCircuit(4)
    qc.h(0)
    qc.cx(0, 1)
    qc.cx(0, 2)
    qc.cx(0, 3)
    qc.measure_all()

    qc_mapped, results = compile_(qc, arch=backend)

    assert results.timeout is False
    # assert results.output.swaps == 0

    result = verify(qc, qc_mapped)
    assert result.considered_equivalent() is True


def test_heuristic_non_trivial_swaps(backend: GenericBackendV2) -> None:
    """Verify that the heuristic mapper works on a simple circuit that requires at least a single SWAP."""
    qc = QuantumCircuit(3)
    qc.h(0)
    qc.cx(0, 1)
    qc.cx(1, 2)
    qc.cx(2, 0)
    qc.measure_all()

    qc_mapped, results = compile_(qc, arch=backend)

    assert results.timeout is False
    assert results.output.swaps == 1

    print("\n")
    print(qc_mapped)

    result = verify(qc, qc_mapped)
    print(result)

    assert result.considered_equivalent() is True


def test_heuristic_layout_with_deferred_single_qubit() -> None:
    """Verify that placing a single-only qubit preserves a bijective layout."""
    qc = QuantumCircuit(4)
    qc.h(3)
    qc.cx(0, 1)
    qc.cx(1, 2)
    qc.measure_all()
    backend = GenericBackendV2(num_qubits=5, coupling_map=[[0, 1], [1, 4], [4, 3], [3, 2]])

    qc_mapped, _ = compile_(qc, arch=backend)

    assert verify(qc, qc_mapped).considered_equivalent() is True


def test_heuristic_static_layout_is_bijective() -> None:
    """Verify that static placement preserves a bijective layout."""
    qc = QuantumCircuit(4)
    qc.h(0)
    qc.h(1)
    qc.cx(2, 3)
    qc.measure_all()
    backend = GenericBackendV2(num_qubits=5, coupling_map=[[0, 4], [4, 3], [3, 2], [2, 1]])

    qc_mapped, _ = compile_(qc, arch=backend, initial_layout=InitialLayout.static, layering=Layering.disjoint_qubits)

    assert verify(qc, qc_mapped).considered_equivalent() is True


def test_fidelity_aware_layout_is_bijective() -> None:
    """Verify that fidelity-aware single-qubit placement preserves a bijective layout."""
    qc = QuantumCircuit(4)
    qc.h(3)
    qc.cx(0, 1)
    qc.cx(1, 2)
    qc.measure_all()
    backend = GenericBackendV2(num_qubits=5, coupling_map=[[0, 1], [1, 4], [4, 3], [3, 2]], seed=1)

    qc_mapped, _ = compile_(
        qc,
        arch=backend,
        heuristic=Heuristic.fidelity_best_location,
        lookahead_heuristic=None,
    )

    assert verify(qc, qc_mapped).considered_equivalent() is True
