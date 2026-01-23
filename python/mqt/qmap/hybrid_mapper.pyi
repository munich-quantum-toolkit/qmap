# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

import enum
from collections.abc import Sequence

import mqt.core.ir

class InitialCoordinateMapping(enum.Enum):
    """Initial mapping between hardware qubits hardware coordinates."""

    trivial = 0
    """Trivial identity mapping."""

    random = 1
    """Random mapping."""

class InitialCircuitMapping(enum.Enum):
    """Initial mapping between circuit qubits and hardware qubits."""

    identity = 0
    """Identity mapping."""

    graph = 1
    """Graph matching mapping."""

class MapperParameters:
    """Parameters controlling the mapper behavior."""

    def __init__(self) -> None:
        """Create a MapperParameters instance with default values."""

    @property
    def lookahead_depth(self) -> int:
        """Depth of lookahead for mapping decisions."""

    @lookahead_depth.setter
    def lookahead_depth(self, arg: int, /) -> None: ...
    @property
    def lookahead_weight_swaps(self) -> float:
        """Weight assigned to swap operations during lookahead."""

    @lookahead_weight_swaps.setter
    def lookahead_weight_swaps(self, arg: float, /) -> None: ...
    @property
    def lookahead_weight_moves(self) -> float:
        """Weight assigned to move operations during lookahead."""

    @lookahead_weight_moves.setter
    def lookahead_weight_moves(self, arg: float, /) -> None: ...
    @property
    def decay(self) -> float:
        """Decay factor for gate blocking."""

    @decay.setter
    def decay(self, arg: float, /) -> None: ...
    @property
    def shuttling_time_weight(self) -> float:
        """Weight for shuttling time in cost evaluation."""

    @shuttling_time_weight.setter
    def shuttling_time_weight(self, arg: float, /) -> None: ...
    @property
    def dynamic_mapping_weight(self) -> float:
        """Weight for dynamic remapping (SWAPs or MOVEs) in cost evaluation."""

    @dynamic_mapping_weight.setter
    def dynamic_mapping_weight(self, arg: float, /) -> None: ...
    @property
    def gate_weight(self) -> float:
        """Weight for gate execution in cost evaluation."""

    @gate_weight.setter
    def gate_weight(self, arg: float, /) -> None: ...
    @property
    def shuttling_weight(self) -> float:
        """Weight for shuttling operations in cost evaluation."""

    @shuttling_weight.setter
    def shuttling_weight(self, arg: float, /) -> None: ...
    @property
    def seed(self) -> int:
        """Random seed for stochastic decisions (initial mapping, etc.)."""

    @seed.setter
    def seed(self, arg: int, /) -> None: ...
    @property
    def num_flying_ancillas(self) -> int:
        """Number of ancilla qubits to be used (0 or 1 for now)."""

    @num_flying_ancillas.setter
    def num_flying_ancillas(self, arg: int, /) -> None: ...
    @property
    def limit_shuttling_layer(self) -> int:
        """Maximum allowed shuttling layer (default: 10)."""

    @limit_shuttling_layer.setter
    def limit_shuttling_layer(self, arg: int, /) -> None: ...
    @property
    def max_bridge_distance(self) -> int:
        """Maximum distance for bridge operations."""

    @max_bridge_distance.setter
    def max_bridge_distance(self, arg: int, /) -> None: ...
    @property
    def use_pass_by(self) -> bool:
        """Enable or disable pass-by operations."""

    @use_pass_by.setter
    def use_pass_by(self, arg: bool, /) -> None: ...
    @property
    def verbose(self) -> bool:
        """Enable verbose logging for debugging."""

    @verbose.setter
    def verbose(self, arg: bool, /) -> None: ...
    @property
    def initial_coord_mapping(self) -> InitialCoordinateMapping:
        """Strategy for initial coordinate mapping."""

    @initial_coord_mapping.setter
    def initial_coord_mapping(self, arg: InitialCoordinateMapping, /) -> None: ...

class MapperStats:
    def __init__(self) -> None: ...
    @property
    def num_swaps(self) -> int:
        """Number of swap operations performed."""

    @num_swaps.setter
    def num_swaps(self, arg: int, /) -> None: ...
    @property
    def num_bridges(self) -> int:
        """Number of bridge operations performed."""

    @num_bridges.setter
    def num_bridges(self, arg: int, /) -> None: ...
    @property
    def num_f_ancillas(self) -> int:
        """Number of fresh ancilla qubits used."""

    @num_f_ancillas.setter
    def num_f_ancillas(self, arg: int, /) -> None: ...
    @property
    def num_moves(self) -> int:
        """Number of move operations performed."""

    @num_moves.setter
    def num_moves(self, arg: int, /) -> None: ...
    @property
    def num_pass_by(self) -> int:
        """Number of pass-by operations performed."""

    @num_pass_by.setter
    def num_pass_by(self, arg: int, /) -> None: ...

class NeutralAtomHybridArchitecture:
    def __init__(self, filename: str) -> None: ...
    def load_json(self, json_filename: str) -> None: ...
    @property
    def name(self) -> str:
        """Name of the architecture."""

    @name.setter
    def name(self, arg: str, /) -> None: ...
    @property
    def num_rows(self) -> int:
        """Number of rows in a rectangular grid SLM arrangement."""

    @property
    def num_columns(self) -> int:
        """Number of columns in a rectangular grid SLM arrangement."""

    @property
    def num_positions(self) -> int:
        """Total number of positions in a rectangular grid SLM arrangement."""

    @property
    def num_aods(self) -> int:
        """Number of independent 2D acousto-optic deflectors."""

    @property
    def num_qubits(self) -> int:
        """Number of atoms in the neutral atom quantum computer that can be used as qubits."""

    @property
    def inter_qubit_distance(self) -> float:
        """Distance between SLM traps in micrometers."""

    @property
    def interaction_radius(self) -> float:
        """Interaction radius in inter-qubit distances."""

    @property
    def blocking_factor(self) -> float:
        """Blocking factor for parallel Rydberg gates."""

    @property
    def naod_intermediate_levels(self) -> int:
        """Number of possible AOD positions between two SLM traps."""

    @property
    def decoherence_time(self) -> float:
        """Decoherence time in microseconds."""

    def compute_swap_distance(self, idx1: int, idx2: int) -> int:
        """Number of SWAP gates required between two positions."""

    def get_gate_time(self, s: str) -> float:
        """Execution time of certain gate in microseconds."""

    def get_gate_average_fidelity(self, s: str) -> float:
        """Average gate fidelity from [0,1]."""

    def get_nearby_coordinates(self, idx: int) -> set[int]:
        """Positions that are within the interaction radius of the passed position."""

class HybridNAMapper:
    """Neutral Atom Hybrid Mapper that can use both SWAP gates and AOD movements to map a quantum circuit to a neutral atom quantum computer."""

    def __init__(self, arch: NeutralAtomHybridArchitecture, params: MapperParameters) -> None:
        """Create Hybrid NA Mapper with mapper parameters."""

    def set_parameters(self, params: MapperParameters) -> None:
        """Set the parameters for the Hybrid NA Mapper."""

    def get_init_hw_pos(self) -> dict[int, int]:
        """Get the initial hardware positions, required to create an animation."""

    def map(self, qc: mqt.core.ir.QuantumComputation, initial_mapping: InitialCircuitMapping = ...) -> None:
        """Map a quantum circuit object to the neutral atom quantum computer."""

    def map_qasm_file(self, filename: str, initial_mapping: InitialCircuitMapping = ...) -> None:
        """Map a quantum circuit to the neutral atom quantum computer."""

    def get_stats(self) -> dict[str, float]:
        """Returns the statistics of the mapping."""

    def get_mapped_qc_qasm(self) -> str:
        """Returns the mapped circuit as an extended qasm2 string."""

    def get_mapped_qc_aod_qasm(self) -> str:
        """Returns the mapped circuit with AOD operations as an extended qasm2 string."""

    def save_mapped_qc_aod_qasm(self, filename: str) -> None:
        """Saves the mapped circuit with AOD operations as an extended qasm2 string."""

    def schedule(
        self, verbose: bool = False, create_animation_csv: bool = False, shuttling_speed_factor: float = 1.0
    ) -> dict[str, float]:
        """Schedule the mapped circuit."""

    def save_animation_files(self, filename: str) -> None:
        """Saves the animation files (.naviz and .namachine) for the scheduling."""

    def get_animation_viz(self) -> str:
        """Returns the .naviz event-log content for the last scheduling."""

class HybridSynthesisMapper:
    """Neutral Atom Mapper that can evaluate different synthesis steps to choose the best one."""

    def __init__(
        self, arch: NeutralAtomHybridArchitecture, params: MapperParameters = ..., buffer_size: int = 0
    ) -> None:
        """Create Hybrid Synthesis Mapper with mapper parameters."""

    def set_parameters(self, params: MapperParameters) -> None:
        """Set the parameters for the Hybrid Synthesis Mapper."""

    def init_mapping(self, n_qubits: int) -> None:
        """Initializes the synthesized and mapped circuits and mapping structures for the given number of qubits."""

    def get_mapped_qc_qasm(self) -> str:
        """Returns the mapped circuit as an extended qasm2 string."""

    def save_mapped_qc_qasm(self, filename: str) -> None:
        """Saves the mapped circuit as an extended qasm2 to a file."""

    def convert_to_aod(self) -> mqt.core.ir.QuantumComputation:
        """Converts the mapped circuit to native AOD movements."""

    def get_mapped_qc_aod_qasm(self) -> str:
        """Returns the mapped circuit with native AOD movements as an extended qasm2 string."""

    def save_mapped_qc_aod_qasm(self, filename: str) -> None:
        """Saves the mapped circuit with native AOD movements as an extended qasm2 to a file."""

    def get_synthesized_qc_qasm(self) -> str:
        """Returns the synthesized circuit with all gates but not mapped to the hardware as a qasm2 string."""

    def save_synthesized_qc_qasm(self, filename: str) -> None:
        """Saves the synthesized circuit with all gates but not mapped to the hardware as qasm2 to a file."""

    def append_with_mapping(self, qc: mqt.core.ir.QuantumComputation, complete_remap: bool = False) -> None:
        """Appends the given QuantumComputation to the synthesized QuantumComputation and maps the gates to the hardware."""

    def get_circuit_adjacency_matrix(self) -> list[list[int]]:
        """Returns the current circuit-qubit adjacency matrix used for mapping."""

    def evaluate_synthesis_steps(
        self,
        synthesis_steps: Sequence[mqt.core.ir.QuantumComputation],
        complete_remap: bool = False,
        also_map: bool = False,
    ) -> list[float]:
        """Evaluates the synthesis steps proposed by the ZX extraction. Returns a list of fidelities of the mapped synthesis steps."""

    def complete_remap(self, include_buffer: bool = True) -> None:
        """Remaps the QuantumComputation to the hardware."""

    def schedule(
        self, verbose: bool = False, create_animation_csv: bool = False, shuttling_speed_factor: float = 1.0
    ) -> dict[str, float]:
        """Schedule the mapped circuit."""
