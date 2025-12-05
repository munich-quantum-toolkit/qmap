# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
# Copyright (c) 2025 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""Test the hybrid Neutral Atom synthesis mapping."""

from __future__ import annotations

from pathlib import Path

import numpy as np
import pytest
from mqt.core import load
from qiskit import QuantumCircuit

from mqt.qmap.hybrid_mapper import HybridSynthesisMapper, NeutralAtomHybridArchitecture

arch_dir = Path(__file__).parent.parent.parent / "hybridmap" / "architectures"
circuit_dir = Path(__file__).parent.parent.parent / "hybridmap" / "circuits"

qc1_qiskit = QuantumCircuit(3)
qc1_qiskit.h(0)
qc1_qiskit.cx(0, 1)
qc1_qiskit.cx(1, 2)
qc1 = load(qc1_qiskit)

qc2_qiskit = QuantumCircuit(3)
qc2_qiskit.cx(0, 2)
qc2_qiskit.cx(1, 2)
qc2 = load(qc2_qiskit)


@pytest.mark.parametrize(
    "arch_filename",
    [
        "rubidium_gate.json",
        "rubidium_hybrid.json",
        "rubidium_shuttling.json",
    ],
)
def test_hybrid_synthesis(arch_filename: str) -> None:
    """Test the hybrid Neutral Atom synthesis mapper evaluation of different circuits."""
    arch = NeutralAtomHybridArchitecture(str(arch_dir / arch_filename))

    synthesis_mapper = HybridSynthesisMapper(arch)
    synthesis_mapper.init_mapping(3)
    best_circuit = synthesis_mapper.evaluate_synthesis_steps(
        [qc1, qc2],
        also_map=True,
    )

    assert isinstance(best_circuit, list)
    assert len(best_circuit) == 2
    assert best_circuit[0] <= 1
    assert best_circuit[0] >= 0


@pytest.mark.parametrize(
    "arch_filename",
    [
        "rubidium_gate.json",
        "rubidium_hybrid.json",
        "rubidium_shuttling.json",
    ],
)
def test_hybrid_synthesis_input_output(arch_filename: str, tmp_path: Path) -> None:
    """Test printing and saving the produced circuits."""
    arch = NeutralAtomHybridArchitecture(str(arch_dir / arch_filename))
    synthesis_mapper = HybridSynthesisMapper(arch)
    synthesis_mapper.init_mapping(3)

    synthesis_mapper.append_with_mapping(qc1)
    synthesis_mapper.append_without_mapping(qc2)

    qasm = synthesis_mapper.get_mapped_qc_qasm()
    assert qasm is not None

    filename_mapped = tmp_path / f"{arch_filename}_mapped.qasm"
    synthesis_mapper.save_mapped_qc_qasm(str(filename_mapped))

    synthesis_mapper.convert_to_aod()
    qasm_aod = synthesis_mapper.get_mapped_qc_aod_qasm()
    assert qasm_aod is not None

    filename_mapped_aod = tmp_path / f"{arch_filename}_mapped_aod.qasm"
    synthesis_mapper.save_mapped_qc_aod_qasm(str(filename_mapped_aod))

    qasm_synth = synthesis_mapper.get_synthesized_qc_qasm()
    assert qasm_synth is not None

    filename_synth = tmp_path / f"{arch_filename}_synthesized.qasm"
    synthesis_mapper.save_synthesized_qc_qasm(str(filename_synth))


def test_adjacency_matrix() -> None:
    """Test the adjacency matrix of the hybrid Neutral Atom synthesis mapper."""
    arch = NeutralAtomHybridArchitecture(str(arch_dir / "rubidium_gate.json"))
    synthesis_mapper = HybridSynthesisMapper(arch)
    circ_size = 3
    synthesis_mapper.init_mapping(circ_size)
    synthesis_mapper.append_with_mapping(qc1)
    adj_mat = np.array(synthesis_mapper.get_circuit_adjacency_matrix())
    assert adj_mat is not None
    assert adj_mat.shape == (circ_size, circ_size)
    for i in range(circ_size):
        for j in range(circ_size):
            assert adj_mat[i, j] == adj_mat[j, i]


def help_create_arch(arch_filename: str) -> NeutralAtomHybridArchitecture:
    """Helper function to create a hybrid Neutral Atom architecture."""
    return NeutralAtomHybridArchitecture(str(arch_dir / arch_filename))


def help_create_mapper(arch_filename: str) -> HybridSynthesisMapper:
    """Helper function to create a hybrid synthesis mapper."""
    arch = help_create_arch(arch_filename)
    synthesis_mapper = HybridSynthesisMapper(arch)
    synthesis_mapper.init_mapping(3)
    return synthesis_mapper


def test_keep_alive() -> None:
    """Test the keep alive functionality of the hybrid Neutral Atom synthesis mapper."""
    synthesis_mapper = help_create_mapper("rubidium_gate.json")
    synthesis_mapper.append_with_mapping(qc1)
    _ = synthesis_mapper.get_circuit_adjacency_matrix()
