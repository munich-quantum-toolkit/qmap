# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

import enum
from collections.abc import Mapping
from typing import Any, overload

import mqt.core.ir

class TargetMetric(enum.Enum):
    gates = 0
    """Optimize gate count."""

    two_qubit_gates = 1
    """Optimize two-qubit gate count."""

    depth = 2
    """Optimize circuit depth."""

class Verbosity(enum.Enum):
    none = 0
    """No output."""

    fatal = 1
    """Only show fatal errors."""

    error = 2
    """Show errors."""

    warning = 3
    """Show warnings."""

    info = 4
    """Show general information."""

    debug = 5
    """Show additional debug information."""

    verbose = 6
    """Show all information."""

class SynthesisConfiguration:
    """Class representing the configuration for the Clifford synthesis techniques."""

    def __init__(self) -> None: ...
    @property
    def initial_timestep_limit(self) -> int:
        """Initial timestep limit for the Clifford synthesis. Defaults to `0`, which implies that the initial timestep limit is determined automatically."""

    @initial_timestep_limit.setter
    def initial_timestep_limit(self, arg: int, /) -> None: ...
    @property
    def minimal_timesteps(self) -> int:
        """Minimal timestep considered for the Clifford synthesis. This option limits the lower bound of the interval in which the binary search method looks for solutions. Set this if you know a lower bound for the circuit depth. Defaults to `0`, which implies that no lower bound for depth is known."""

    @minimal_timesteps.setter
    def minimal_timesteps(self, arg: int, /) -> None: ...
    @property
    def use_maxsat(self) -> bool:
        """Use MaxSAT to solve the synthesis problem or to rely on the binary search scheme for finding the optimum. Defaults to `False`."""

    @use_maxsat.setter
    def use_maxsat(self, arg: bool, /) -> None: ...
    @property
    def linear_search(self) -> bool:
        """Use linear search instead of binary search scheme for finding the optimum. Defaults to `False`."""

    @linear_search.setter
    def linear_search(self, arg: bool, /) -> None: ...
    @property
    def target_metric(self) -> TargetMetric:
        """Target metric for the Clifford synthesis. Defaults to `gates`."""

    @target_metric.setter
    def target_metric(self, arg: TargetMetric, /) -> None: ...
    @property
    def use_symmetry_breaking(self) -> bool:
        """Use symmetry breaking clauses to speed up the synthesis process. Defaults to `True`."""

    @use_symmetry_breaking.setter
    def use_symmetry_breaking(self, arg: bool, /) -> None: ...
    @property
    def dump_intermediate_results(self) -> bool:
        """Dump intermediate results of the synthesis process. Defaults to `False`."""

    @dump_intermediate_results.setter
    def dump_intermediate_results(self, arg: bool, /) -> None: ...
    @property
    def intermediate_results_path(self) -> str:
        """Path to the directory where intermediate results should be dumped. Defaults to `./`. The path needs to include a path separator at the end."""

    @intermediate_results_path.setter
    def intermediate_results_path(self, arg: str, /) -> None: ...
    @property
    def verbosity(self) -> Verbosity:
        """Verbosity level for the synthesis process. Defaults to 'warning'."""

    @verbosity.setter
    def verbosity(self, arg: Verbosity, /) -> None: ...
    @property
    def solver_parameters(self) -> dict[str, bool | int | float | str]:
        """Parameters to be passed to Z3 as dict[str, bool | int | float | str]."""

    @solver_parameters.setter
    def solver_parameters(self, arg: Mapping[str, bool | int | float | str], /) -> None: ...
    @property
    def minimize_gates_after_depth_optimization(self) -> bool:
        """Depth optimization might produce a circuit with more gates than necessary. This option enables an additional run of the synthesizer to minimize the overall number of gates. Defaults to `False`."""

    @minimize_gates_after_depth_optimization.setter
    def minimize_gates_after_depth_optimization(self, arg: bool, /) -> None: ...
    @property
    def try_higher_gate_limit_for_two_qubit_gate_optimization(self) -> bool:
        """When optimizing two-qubit gates, the synthesizer might fail to find an optimal solution for a certain timestep limit, but there might be a better solution for some higher timestep limit. This option enables an additional run of the synthesizer with a higher gate limit. Defaults to `False`."""

    @try_higher_gate_limit_for_two_qubit_gate_optimization.setter
    def try_higher_gate_limit_for_two_qubit_gate_optimization(self, arg: bool, /) -> None: ...
    @property
    def gate_limit_factor(self) -> float:
        """Factor by which the gate limit is increased when trying to find a better solution for the two-qubit gate optimization. Defaults to `1.1`."""

    @gate_limit_factor.setter
    def gate_limit_factor(self, arg: float, /) -> None: ...
    @property
    def minimize_gates_after_two_qubit_gate_optimization(self) -> bool:
        """Two-qubit gate optimization might produce a circuit with more gates than necessary. This option enables an additional run of the synthesizer to minimize the overall number of gates. Defaults to `False`."""

    @minimize_gates_after_two_qubit_gate_optimization.setter
    def minimize_gates_after_two_qubit_gate_optimization(self, arg: bool, /) -> None: ...
    @property
    def heuristic(self) -> bool:
        """Use heuristic to synthesize the circuit. This method synthesizes shallow intermediate circuits and combines them. Defaults to `False`."""

    @heuristic.setter
    def heuristic(self, arg: bool, /) -> None: ...
    @property
    def split_size(self) -> int:
        """Size of subcircuits used in heuristic. Defaults to `5`."""

    @split_size.setter
    def split_size(self, arg: int, /) -> None: ...
    @property
    def n_threads_heuristic(self) -> int:
        """Maximum number of threads used for the heuristic optimizer. Defaults to the number of available threads on the system."""

    @n_threads_heuristic.setter
    def n_threads_heuristic(self, arg: int, /) -> None: ...
    def json(self) -> dict[str, Any]:
        """Returns a JSON-style dictionary of all the information present in the :class:`.Configuration`."""

class SynthesisResults:
    """Class representing the results of the Clifford synthesis techniques."""

    def __init__(self) -> None: ...
    @property
    def gates(self) -> int:
        """Returns the number of gates in the circuit."""

    @property
    def single_qubit_gates(self) -> int:
        """Returns the number of single-qubit gates in the synthesized circuit."""

    @property
    def two_qubit_gates(self) -> int:
        """Returns the number of two-qubit gates in the synthesized circuit."""

    @property
    def depth(self) -> int:
        """Returns the depth of the synthesized circuit."""

    @property
    def runtime(self) -> float:
        """Returns the runtime of the synthesis in seconds."""

    @property
    def solver_calls(self) -> int:
        """Returns the number of calls to the SAT solver."""

    @property
    def circuit(self) -> str:
        """Returns the synthesized circuit as a qasm string."""

    @property
    def tableau(self) -> str:
        """Returns a string representation of the synthesized circuit's tableau."""

    def sat(self) -> bool:
        """Returns `True` if the synthesis was successful."""

    def unsat(self) -> bool:
        """Returns `True` if the synthesis was unsuccessful."""

class Tableau:
    """Class representing a Clifford tableau."""

    @overload
    def __init__(self, n: int, include_destabilizers: bool = False) -> None:
        """Creates a tableau for an n-qubit Clifford."""

    @overload
    def __init__(self, tableau: str) -> None:
        """Constructs a tableau from a string description. This can either be a semicolon separated binary matrix or a list of Pauli strings."""

    @overload
    def __init__(self, stabilizers: str, destabilizers: str) -> None:
        """Constructs a tableau from two lists of Pauli strings, the stabilizers and destabilizers."""

class CliffordSynthesizer:
    """The main class for the Clifford synthesis techniques."""

    @overload
    def __init__(self, initial_tableau: Tableau, target_tableau: Tableau) -> None:
        """Constructs a synthesizer for two tableaus representing the initial and target state."""

    @overload
    def __init__(self, target_tableau: Tableau) -> None:
        """Constructs a synthesizer for a tableau representing the target state."""

    @overload
    def __init__(self, qc: mqt.core.ir.QuantumComputation, use_destabilizers: bool) -> None:
        """Constructs a synthesizer for a quantum computation representing the target state."""

    @overload
    def __init__(self, initial_tableau: Tableau, qc: mqt.core.ir.QuantumComputation) -> None:
        """Constructs a synthesizer for a quantum computation representing the target state that starts in an initial state represented by a tableau."""

    def synthesize(self, config: SynthesisConfiguration = ...) -> None:
        """Runs the synthesis with the given configuration."""

    @property
    def results(self) -> SynthesisResults:
        """Returns the results of the synthesis."""

    @property
    def result_circuit(self) -> mqt.core.ir.QuantumComputation:
        """Returns the synthesized circuit as a :class:`~mqt.core.ir.QuantumComputation` object."""
