# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

import enum

import mqt.core.ir

class ZonedNeutralAtomArchitecture:
    """Class representing a zoned neutral atom architecture."""

    @staticmethod
    def from_json_file(filename: str) -> ZonedNeutralAtomArchitecture:
        """Create an architecture from a JSON file.

        Args:
            filename: The path to the JSON file

        Returns:
            The architecture

        Raises:
            ValueError: if the file does not exist or is not a valid JSON file
        """

    @staticmethod
    def from_json_string(json: str) -> ZonedNeutralAtomArchitecture:
        """Create an architecture from a JSON string.

        Args:
            json: The JSON string

        Returns:
            The architecture

        Raises:
            ValueError: If the string is not a valid JSON string
        """

    def to_namachine_file(self, filename: str) -> None:
        """Write the architecture to a .namachine file.

        Args:
            filename: The path to the .namachine file
        """

    def to_namachine_string(self) -> str:
        """Get the architecture as a .namachine string.

        Returns:
            The architecture as a .namachine string
        """

class PlacementMethod(enum.Enum):
    """Enumeration of the available placement methods for the heuristic placer."""

    astar = 0
    """A-star algorithm."""

    ids = 1
    """Iterative diving search."""

class RoutingMethod(enum.Enum):
    """Enumeration of the available routing methods for the independent set router."""

    strict = 0
    """
    Strict routing, i.e., the relative order of atoms must be maintained throughout a movement.
    """

    relaxed = 1
    """
    Relaxed routing, i.e., the relative order of atoms may change throughout a movement by applying offsets during pick-up and drop-off.
    """

class RoutingAgnosticCompiler:
    """Routing-agnostic zoned neutral atom compiler."""

    def __init__(
        self,
        arch: ZonedNeutralAtomArchitecture,
        log_level: str = "I",
        max_filling_factor: float = 0.9,
        use_window: bool = True,
        window_size: int = 10,
        dynamic_placement: bool = True,
        routing_method: RoutingMethod = ...,
        prefer_split: float = 1.0,
        warn_unsupported_gates: bool = True,
    ) -> None:
        """Create a routing-agnostic compiler for the given architecture and configurations.

        Args:
            arch: The zoned neutral atom architecture
            log_level: The log level for the compiler, possible values are "debug"/"D", "info"/"I", "warning"/"W", "error"/"E", and "critical"/"C"
            max_filling_factor: The maximum filling factor for the entanglement zone, i.e., it sets the limit for the maximum number of entangling gates that are scheduled in parallel
            use_window: Whether to use a window for the placer
            window_size: The size of the window for the placer
            dynamic_placement: Whether to use dynamic placement for the placer
            routing_method: The routing method that should be used for the independent set router
            prefer_split: The threshold factor for group merging decisions during routing.
            warn_unsupported_gates: Whether to warn about unsupported gates in the code generator
        """

    @staticmethod
    def from_json_string(arch: ZonedNeutralAtomArchitecture, json: str) -> RoutingAgnosticCompiler:
        """Create a compiler for the given architecture and with configurations from a JSON string.

        Args:
            arch: The zoned neutral atom architecture
            json: The JSON string

        Returns:
            The initialized compiler

        Raises:
            ValueError: If the string is not a valid JSON string
        """

    def compile(self, qc: mqt.core.ir.QuantumComputation) -> str:
        """Compile a quantum circuit for the zoned neutral atom architecture.

        Args:
            qc: The quantum circuit

        Returns:
            The compilation result as a string in the .naviz format.
        """

    def stats(self) -> dict[str, float]:
        """Get the statistics of the last compilation as a JSON-style dictionary.

        Returns:
            The statistics as a JSON-style dictionary
        """

class RoutingAwareCompiler:
    """Routing-aware zoned neutral atom compiler."""

    def __init__(
        self,
        arch: ZonedNeutralAtomArchitecture,
        log_level: str = "I",
        max_filling_factor: float = 0.9,
        use_window: bool = True,
        window_min_width: int = 16,
        window_ratio: float = 1.0,
        window_share: float = 0.8,
        placement_method: PlacementMethod = ...,
        deepening_factor: float = ...,
        deepening_value: float = 0.0,
        lookahead_factor: float = ...,
        reuse_level: float = 5.0,
        max_nodes: int = 10000000,
        trials: int = 4,
        queue_capacity: int = 100,
        routing_method: RoutingMethod = ...,
        prefer_split: float = 1.0,
        warn_unsupported_gates: bool = True,
    ) -> None:
        """Create a routing-aware compiler for the given architecture and configurations.

        Args:
            arch: The zoned neutral atom architecture
            log_level: The log level for the compiler, possible values are "debug"/"D", "info"/"I", "warning"/"W", "error"/"E", and "critical"/"C"
            max_filling_factor: The maximum filling factor for the entanglement zone, i.e., it sets the limit for the maximum number of entangling gates that are scheduled in parallel
            use_window: Whether to use a window for the placer
            window_min_width: The minimum width of the window for the placer
            window_ratio: The ratio between the height and the width of the window
            window_share: The share of free sites in the window in relation to the number of atoms to be moved in this step
            placement_method: The placement method that should be used for the heuristic placer
            deepening_factor: Controls the impact of the term in the heuristic of the A* search that resembles the standard deviation of the differences between the current and target sites of the atoms to be moved in every orientation
            deepening_value: Is added to the sum of standard deviations before it is multiplied with the number of unplaced nodes and :attr:`deepening_factor`
            lookahead_factor: Controls the lookahead's influence that considers the distance of atoms to their interaction partner in the next layer
            reuse_level: The reuse level that corresponds to the estimated extra fidelity loss due to the extra trap transfers when the atom is not reused and instead moved to the storage zone and back to the entanglement zone
            max_nodes: The maximum number of nodes that are considered in the A* search.
                If this number is exceeded, the search is aborted and an error is raised.
                In the current implementation, one node roughly consumes 120 Byte.
                Hence, allowing 50,000,000 nodes results in memory consumption of about 6 GB plus the size of the rest of the data structures.
            trials: The number of restarts during IDS.
            queue_capacity: The maximum capacity of the priority queue used during IDS.
            routing_method: The routing method that should be used for the independent set router
            prefer_split: The threshold factor for group merging decisions during routing.
            warn_unsupported_gates: Whether to warn about unsupported gates in the code generator
        """

    @staticmethod
    def from_json_string(arch: ZonedNeutralAtomArchitecture, json: str) -> RoutingAwareCompiler:
        """Create a compiler for the given architecture and configurations from a JSON string.

        Args:
            arch: The zoned neutral atom architecture
            json: The JSON string

        Returns:
            The initialized compiler

        Raises:
            ValueError: If the string is not a valid JSON string
        """

    def compile(self, qc: mqt.core.ir.QuantumComputation) -> str:
        """Compile a quantum circuit for the zoned neutral atom architecture.

        Args:
            qc: The quantum circuit

        Returns:
            The compilation result as a string in the .naviz format.
        """

    def stats(self) -> dict[str, float]:
        """Get the statistics of the last compilation.

        Returns:
            The statistics as a dictionary
        """
