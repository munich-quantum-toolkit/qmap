# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
# Copyright (c) 2025 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""Python bindings module for MQT QMAP's Zoned Neutral Atom Compiler."""

from mqt.core.ir import QuantumComputation

class ZonedNeutralAtomArchitecture:
    """Class representing a Zoned Neutral Atom Architecture."""

    @classmethod
    def from_json_file(cls, filename: str) -> ZonedNeutralAtomArchitecture:
        """Create an architecture from a JSON file.

        Args:
            filename: is the path to the JSON file

        Returns:
            the architecture

        Raises:
            ValueError: if the file does not exist or is not a valid JSON file
        """
    @classmethod
    def from_json_string(cls, json: str) -> ZonedNeutralAtomArchitecture:
        """Create an architecture from a JSON string.

        Args:
            json: is the JSON string

        Returns:
            the architecture

        Raises:
            ValueError: if the string is not a valid JSON
        """
    def to_namachine_file(self, filename: str) -> None:
        """Write the architecture to a .namachine file.

        Args:
            filename: is the path to the .namachine file
        """
    def to_namachine_string(self) -> str:
        """Get the architecture as a .namachine string.

        Returns:
            the architecture as a .namachine string
        """

class RoutingAgnosticCompiler:
    """MQT QMAP's routing-agnostic Zoned Neutral Atom Compiler."""

    def __init__(
        self,
        arch: ZonedNeutralAtomArchitecture,
        log_level: str = ...,
        use_window: bool = ...,
        window_size: int = ...,
        dynamic_placement: bool = ...,
        parking_offset: int = ...,
        warn_unsupported_gates: bool = ...,
    ) -> None:
        """Create a routing-agnostic compiler for the given architecture and configurations.

        Args:
            arch: is the zoned neutral atom architecture
            log_level: is the log level for the compiler, possible values are
                "DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"
            use_window: whether to use a window for the placer
            window_size: the size of the window for the placer
            dynamic_placement: whether to use dynamic placement for the placer
            parking_offset: the parking offset of the code generator
            warn_unsupported_gates: whether to warn about unsupported gates in the code generator
        """
    @classmethod
    def from_json_string(cls, arch: ZonedNeutralAtomArchitecture, json: str) -> RoutingAgnosticCompiler:
        """Create a routing-agnostic compiler for the given architecture and with configurations from a JSON string.

        Args:
            arch: is the zoned neutral atom architecture
            json: is the JSON string

        Returns:
            the initialized compiler

        Raises:
            ValueError: if the string is not a valid JSON
        """
    def compile(self, qc: QuantumComputation) -> str:
        """Compile a quantum circuit for the zoned neutral atom architecture.

        Args:
            qc: is the quantum circuit

        Returns:
            the compilations result as a string in the .naviz format.
        """
    def stats(self) -> dict[str, float]:
        """Get the statistics of the last compilation.

        Returns:
            the statistics as a dictionary
        """

class RoutingAwareCompiler:
    """MQT QMAP's routing-aware Zoned Neutral Atom Compiler."""

    def __init__(
        self,
        arch: ZonedNeutralAtomArchitecture,
        log_level: str = ...,
        use_window: bool = ...,
        window_min_width: int = ...,
        window_ratio: float = ...,
        window_share: float = ...,
        deepening_factor: float = ...,
        deepening_value: float = ...,
        lookahead_factor: float = ...,
        reuse_level: float = ...,
        max_nodes: int = ...,
        parking_offset: int = ...,
        warn_unsupported_gates: bool = ...,
    ) -> None:
        """Create a routing-aware compiler for the given architecture and configurations.

        Args:
            arch: is the zoned neutral atom architecture
            log_level: is the log level for the compiler, possible values are
                "DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"
            use_window: is a flag whether to use a window for the placer
            window_min_width: is the minimum width of the window for the placer
            window_ratio: is the ratio between the height and the width of the window
            window_share: is the share of free sites in the window in relation to the
                number of atoms to be moved in this step
            deepening_factor: controls the impact of the term in the heuristic of the
                A* search that resembles the standard deviation of the differences
                between the current and target sites of the atoms to be moved in every
                orientation
            deepening_value: is added to the sum of standard deviations before it is
                multiplied with the number of unplaced nodes and :attr:`deepening_factor`
            lookahead_factor: controls the lookahead's influence that considers the
                distance of atoms to their interaction partner in the next layer
            reuse_level: is the reuse level that corresponds to the estimated extra
                fidelity loss due to the extra trap transfers when the atom is not
                reused and instead moved to the storage zone and back to the
                entanglement zone
            max_nodes: is the maximum number of nodes that are considered in the A*
                search. If this number is exceeded, the search is aborted and an error
                is raised. In the current implementation, one node roughly consumes 120
                Byte. Hence, allowing 50,000,000 nodes results in memory consumption of
                about 6 GB plus the size of the rest of the data structures.
            parking_offset: is the parking offset of the code generator
            warn_unsupported_gates: is a flag whether to warn about unsupported gates
                in the code generator
        """
    @classmethod
    def from_json_string(cls, arch: ZonedNeutralAtomArchitecture, json: str) -> RoutingAwareCompiler:
        """Create a routing-aware compiler for the given architecture and configurations from a JSON string.

        Args:
            arch: is the zoned neutral atom architecture
            json: is the JSON string

        Returns:
            the initialized compiler

        Raises:
            ValueError: if the string is not a valid JSON
        """
    def compile(self, qc: QuantumComputation) -> str:
        """Compile a quantum circuit for the zoned neutral atom architecture.

        Args:
            qc: is the quantum circuit

        Returns:
            the compilations result as a string in the .naviz format.
        """
    def stats(self) -> dict[str, float]:
        """Get the statistics of the last compilation.

        Returns:
            the statistics as a dictionary
        """
