"""Test the exact mapper."""

from __future__ import annotations

import pytest
from qiskit import QuantumCircuit
from qiskit.providers.fake_provider import GenericBackendV2

from mqt import qmap
from mqt.qcec import verify


@pytest.fixture
def backend() -> GenericBackendV2:
    """Return a test backend."""
    return GenericBackendV2(num_qubits=5, coupling_map=[[0, 1], [1, 0], [1, 2], [2, 1], [1, 3], [3, 1], [3, 4], [4, 3]])


def test_exact_no_swaps_trivial_layout(backend: GenericBackendV2) -> None:
    """Verify that the exact mapper works on a simple circuit that requires no swaps on a trivial initial layout."""
    qc = QuantumCircuit(3)
    qc.h(0)
    qc.cx(0, 1)
    qc.cx(1, 2)
    qc.measure_all()

    qc_mapped, results = qmap.compile(qc, arch=backend, method="exact")
    assert results.timeout is False
    assert results.output.swaps == 0

    result = verify(qc, qc_mapped)
    assert result.considered_equivalent() is True


def test_exact_no_swaps_non_trivial_layout(backend: GenericBackendV2) -> None:
    """Verify that the exact mapper works on a simple circuit that requires a non-trivial layout to achieve no swaps."""
    qc = QuantumCircuit(4)
    qc.h(0)
    qc.cx(0, 1)
    qc.cx(0, 2)
    qc.cx(0, 3)
    qc.measure_all()

    qc_mapped, results = qmap.compile(qc, arch=backend, method="exact")

    assert results.timeout is False
    assert results.output.swaps == 0

    result = verify(qc, qc_mapped)
    assert result.considered_equivalent() is True


def test_exact_non_trivial_swaps(backend: GenericBackendV2) -> None:
    """Verify that the exact mapper works on a simple circuit that requires at least a single SWAP."""
    qc = QuantumCircuit(3)
    qc.h(0)
    qc.cx(0, 1)
    qc.cx(1, 2)
    qc.cx(2, 0)
    qc.measure_all()

    qc_mapped, results = qmap.compile(qc, arch=backend, method="exact")

    assert results.timeout is False
    assert results.output.swaps == 1

    result = verify(qc, qc_mapped)
    assert result.considered_equivalent() is True
