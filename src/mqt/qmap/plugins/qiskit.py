"""Module for importing Qiskit backends."""

from __future__ import annotations

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from qiskit.providers import BackendV2
    from qiskit.transpiler import Target

from ..pyqmap import Architecture

__all__ = [
    "import_backend",
    "import_target",
]


def __dir__() -> list[str]:
    return __all__


def import_backend(backend: BackendV2) -> Architecture:
    """Import a backend from qiskit.providers.BackendV2."""
    arch = Architecture()
    arch.name = backend.name
    arch.num_qubits = backend.num_qubits
    arch.coupling_map = set(backend.coupling_map.get_edges())
    arch.properties = import_target(backend.target)
    arch.properties.name = backend.name

    return arch


def import_target(target: Target) -> Architecture.Properties:
    """Import a target from qiskit.transpiler.Target.

    Args:
        target: The target to import.

    Returns:
        The imported target as an Architecture.Properties object.
    """
    props = Architecture.Properties()
    props.num_qubits = len(target.qubit_properties)

    for i in range(props.num_qubits):
        qubit_props = target.qubit_properties[i]
        props.set_t1(i, qubit_props.t1)
        props.set_t2(i, qubit_props.t2)
        props.set_frequency(i, qubit_props.frequency)

    for instruction, qargs in target.instructions:
        if instruction.name in {"reset", "delay"}:
            continue

        instruction_props = target[instruction.name][qargs]
        if instruction.name == "measure":
            props.set_readout_error(qargs[0], instruction_props.error)
        elif len(qargs) == 1:
            props.set_single_qubit_error(qargs[0], instruction.name, instruction_props.error)
        elif len(qargs) == 2:
            props.set_two_qubit_error(qargs[0], qargs[1], instruction_props.error, instruction.name)

    return props
