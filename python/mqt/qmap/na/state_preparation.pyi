# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
# Copyright (c) 2025 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""Python bindings module for MQT QMAP's Neutral Atom State Preparation."""

from typing import Any

from mqt.core.ir import QuantumComputation

class NAStatePreparationSolver:
    """The MQT QMAP's Neutral Atom State Preparation Solver.

    The neutral atom state preparation solver generates an optimal sequence of
    neutral atom operations for a given state preparation circuit.
    """

    def __init__(
        self,
        max_x: int,
        max_y: int,
        max_c: int,
        max_r: int,
        max_h_offset: int,
        max_v_offset: int,
        max_h_dist: int,
        max_v_dist: int,
        min_entangling_y: int,
        max_entangling_y: int,
    ) -> None:
        """Create a solver instance for the neutral atom state preparation problem.

        The solver is based on a 2D grid abstraction of the neutral atom quantum
        computer. The 2D plane is divided into interaction sites. Each interaction site
        is denoted by abstract x- and y-coordinates. The parameter `max_x` specifies the
        maximum x-coordinate, and `max_y` specifies the maximum y-coordinate. In the
        center of an interaction site, sits an SLM trap. Around that trap there are
        several possible discrete AOD positions arranged as a grid. The specific
        position of an atom within an interaction site is determined by the x- and
        y-offset from the SLM trap, i.e., those can be positive and negative. The
        maximum absolute value of the x- and y-offset is specified by `max_h_offset` and
        `max_v_offset`, respectively. Then, the parameter `max_c` specifies the maximum
        number of AOD columns, and `max_r` specifies the maximum number of AOD rows.
        Finally, in order to interact during a Rydberg stage, atoms must be located
        within a certain distance. The maximum horizontal and vertical distance between
        two atoms is specified by `max_h_dist` and `max_v_dist`, respectively. The
        parameter `min_entangling_y` specifies the minimum y-coordinate for entangling
        operations, and `max_entangling_y` specifies the maximum y-coordinate for
        entangling operations. Hence, y-coordinates outside of this range are located in
        the storage zone.

        Note:
            The solver can only handle a single storage zone below the entangling zone,
            i.e., in this case `min_entangling_y` must be zero and `max_entangling_y`
            must be less than `max_y`.

        Args:
            max_x: is the maximum discrete x-coordinate of the interaction sites
            max_y: is the maximum discrete y-coordinate of the interaction sites
            max_c: is the maximum number of AOD columns
            max_r: is the maximum number of AOD rows
            max_h_offset: is the maximum horizontal offset of the atoms
            max_v_offset: is the maximum vertical offset of the atoms
            max_h_dist: is the maximum horizontal distance between two atoms
            max_v_dist: is the maximum vertical distance between two atoms
            min_entangling_y: is the minimum y-coordinate for entangling operations
            max_entangling_y: is the maximum y-coordinate for entangling operations

        Raises:
            ValueError: if one of the parameters is invalid, e.g., is a negative
            value
        """

    class Result:
        """Neutral Atom State Preparation Solver Result."""
        def __init__(self) -> None:
            """Create a result object."""
        def json(self) -> dict[str, Any]:
            """Returns the result as a JSON string.

            Returns:
                the result as a JSON string
            """

    def solve(
        self,
        ops: list[tuple[int, int]],
        num_qubits: int,
        num_stages: int,
        num_transfers: int | None = ...,
        mind_ops_order: bool = ...,
        shield_idle_qubits: bool = ...,
    ) -> Result:
        """Solve the neutral atom state preparation problem.

        The solver generates an optimal sequence of neutral atom operations for a given
        state preparation circuit. The circuit is given as a list of operations, where
        each operation is a pair of qubits. The sequence is divided into stages. Each
        stage is either a Rydberg stage or a transfer stage. In a Rydberg stage,
        adjacent qubits in the entangling zone undergo an entangling gate. In a transfer
        stage, atoms can be stored from AOD into SLM traps and loaded from SLM traps
        into AOD. At the end of each stage, the atoms are shuttled to their next
        position. The number of stages is specified by `num_stages`. The number of
        transfers is fixed by `num_transfers` if given. If this parameter is not
        specified, then the solver will determine the optimal number of transfers. The
        parameter `mind_ops_order` specifies whether the order of the operations in the
        circuit should be preserved. The parameter `shield_idle_qubits` specifies
        whether idle qubits should be shielded from the entangling operations.

        Note:
            To retrieve the list of qubit pairs from a quantum circuit, use the function
            :func:`get_ops_for_solver`.


            The returned solver's result can either be directly exported to the JSON format
            by calling the method :func:`json` on the result object or the result object
            can be passed to the function :func:`generate_code` to generate code
            consisting of neutral atom operations.

        Args:
            ops: is the list of operations in the circuit
            num_qubits: is the number of qubits in the circuit
            num_stages: is the number of stages in the sequence
            num_transfers: (optional) is the number of transfers in the sequence
            mind_ops_order: is True if the order of the operations should be
                preserved
            shield_idle_qubits: is True if idle qubits should be shielded

        Returns:
            the result of the solver

        Raises:
            ValueError: if one of the numeral parameters is invalid, e.g., is a
            negative value
        """

def get_ops_for_solver(
    qc: QuantumComputation,
    operation_type: str,
    num_controls: int,
    quiet: bool = ...,
) -> list[tuple[int, int]]:
    """Extract entangling operations as list of qubit pairs from the circuit.

    Note:
        This function can only extract qubit pairs of two-qubit operations.
        I.e., the operands of the operation plus the controls must be equal to two.

    Args:
        qc: is the quantum circuit
        operation_type: is the type of operation to extract, e.g., "z" for CZ
            gates
        num_controls: is the number of controls the operation acts on, e.g., 1
            for CZ gates
        quiet: if True, suppresses warning when the circuit contains operations
            other than the specified operation type

    Returns:
        list of qubit pairs

    Raises:
        ValueError: if the circuit contains operations other than the specified
            operation type and quiet is False
        ValueError: if the operation has more than two operands including
            controls
    """

def generate_code(
    qc: QuantumComputation,
    result: NAStatePreparationSolver.Result,
    min_atom_dist: int = ...,
    no_interaction_radius: int = ...,
    zone_dist: int = ...,
) -> str:
    """Generate code for the given circuit using the solver's result.

    Some parameters of the abstraction from the 2D grid used for the solver
    must be provided again.

    Args:
        qc: is the quantum circuit
        result: is the result of the solver
        min_atom_dist: is the minimum distance between atoms
        no_interaction_radius: is the radius around an atom where no other atom
            can be placed during an entangling operation that should not interact with the
            atom
        zone_dist: is the distance between zones, i.e., the minimal distance
            between two atoms in different zones

    Raises:
        ValueError: if one of the numeral parameters is invalid, e.g., is a
            negative value
    """
