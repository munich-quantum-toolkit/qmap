# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
# Copyright (c) 2025 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""Test the hybrid Neutral Atom mapper."""

from __future__ import annotations

from pathlib import Path

import pytest
from mqt.core import load

from mqt.qmap.hybrid_mapper import HybridNAMapper, MapperParameters, NeutralAtomHybridArchitecture

arch_dir = Path(__file__).parent.parent.parent / "hybridmap" / "architectures"
circuit_dir = Path(__file__).parent.parent.parent / "hybridmap" / "circuits"


@pytest.mark.parametrize(
    "circuit_filename",
    [
        "dj_nativegates_rigetti_qiskit_opt3_10.qasm",
        "modulo_2.qasm",
        "multiply_2.qasm",
        "qft_nativegates_rigetti_qiskit_opt3_10.qasm",
        "random_nativegates_rigetti_qiskit_opt3_10.qasm",
    ],
)
@pytest.mark.parametrize(
    "arch_filename",
    [
        "rubidium_gate.json",
        "rubidium_hybrid.json",
        "rubidium_shuttling.json",
    ],
)
@pytest.mark.parametrize(
    ("lookahead_weight", "decay", "gate_shuttling_weight"), [(0.0, 0.1, 0.1), (0.0, 0.0, 0.1), (0.0, 1.0, 10)]
)
def test_hybrid_na_mapper(
    circuit_filename: str, arch_filename: str, lookahead_weight: float, decay: float, gate_shuttling_weight: float
) -> None:
    """Test the hybrid Neutral Atom mapper."""
    arch = NeutralAtomHybridArchitecture(str(arch_dir / arch_filename))

    params = MapperParameters()
    params.lookahead_weight_moves = lookahead_weight
    params.lookahead_weight_swaps = lookahead_weight
    params.decay = decay
    params.gate_weight = gate_shuttling_weight
    mapper = HybridNAMapper(arch, params=params)

    # Map directly from QASM file using the pybind-exposed convenience method
    mapper.map_qasm_file(str(circuit_dir / circuit_filename))
    results = mapper.schedule(create_animation_csv=False)
    # Validate QASM exports (mapped abstract and AOD-annotated)
    mapped_qasm = mapper.get_mapped_qc_qasm()
    # AOD QASM retrieval currently not exposed in Python API; just sanity check mapped_qasm
    assert mapped_qasm.strip()

    assert results["totalExecutionTime"] > 0
    assert results["totalIdleTime"] > 0
    assert results["totalFidelities"] > 0


def _nested_mapper_create() -> HybridNAMapper:
    """Create a nested Neutral Atom hybrid architecture."""
    arch = NeutralAtomHybridArchitecture(str(arch_dir / "rubidium_gate.json"))
    params = MapperParameters()
    return HybridNAMapper(arch, params=params)


def test_keep_alive() -> None:
    """Test the keep alive feature of the python bindings."""
    mapper = _nested_mapper_create()

    qc = load(circuit_dir / "dj_nativegates_rigetti_qiskit_opt3_10.qasm")

    mapper.map(qc)
    results = mapper.schedule(create_animation_csv=False)
    mapped_qasm = mapper.get_mapped_qc_qasm()
    assert mapped_qasm.strip()

    assert results["totalExecutionTime"] > 0
    assert results["totalIdleTime"] > 0
    assert results["totalFidelities"] > 0
