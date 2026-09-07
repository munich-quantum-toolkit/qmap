# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""Import a Qiskit backend."""

from __future__ import annotations

from typing import TYPE_CHECKING

from ....sc import Architecture

if TYPE_CHECKING:
    from qiskit.providers import BackendV2
    from qiskit.transpiler import Target


def import_target(target: Target) -> Architecture.Properties:
    """Import a target from qiskit.transpiler.Target.

    Args:
        target: The target to import.

    Returns:
        The imported target as an Architecture.Properties object.
    """
    props = Architecture.Properties()
    if target.num_qubits is None:
        msg = "Cannot import an unbounded Qiskit target."
        raise ValueError(msg)
    props.num_qubits = target.num_qubits

    for i, qubit_props in enumerate(target.qubit_properties or ()):
        if qubit_props is None:
            continue
        if qubit_props.t1 is not None:
            props.set_t1(i, qubit_props.t1)
        if qubit_props.t2 is not None:
            props.set_t2(i, qubit_props.t2)
        if qubit_props.frequency is not None:
            props.set_frequency(i, qubit_props.frequency)

    for instruction, qargs in target.instructions:
        if instruction.name in {"reset", "delay"} or qargs is None:
            continue

        instruction_props = target[instruction.name][qargs]
        if instruction_props is None or instruction_props.error is None:
            continue
        if instruction.name == "measure":
            props.set_readout_error(qargs[0], instruction_props.error)
        elif len(qargs) == 1:
            props.set_single_qubit_error(qargs[0], instruction.name, instruction_props.error)
        elif len(qargs) == 2:
            props.set_two_qubit_error(qargs[0], qargs[1], instruction_props.error, instruction.name)

    return props


def import_backend(backend: BackendV2) -> Architecture:
    """Import a backend from qiskit.providers.BackendV2."""
    arch = Architecture()
    arch.name = str(backend.name)
    arch.num_qubits = backend.num_qubits
    arch.coupling_map = set(backend.coupling_map.get_edges())
    arch.properties = import_target(backend.target)
    arch.properties.name = str(backend.name)

    return arch
