# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
# Copyright (c) 2025 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""Python bindings for Clifford synthesis module."""

from typing import Any, ClassVar, overload

from mqt.core.ir import QuantumComputation

class TargetMetric:
    __members__: ClassVar[dict[TargetMetric, int]] = ...  # read-only
    depth: ClassVar[TargetMetric] = ...
    gates: ClassVar[TargetMetric] = ...
    two_qubit_gates: ClassVar[TargetMetric] = ...

    @overload
    def __init__(self, value: int) -> None: ...
    @overload
    def __init__(self, arg0: str) -> None: ...
    @overload
    def __init__(self, arg0: TargetMetric) -> None: ...
    def __eq__(self, other: object) -> bool: ...
    def __getstate__(self) -> int: ...
    def __hash__(self) -> int: ...
    def __index__(self) -> int: ...
    def __int__(self) -> int: ...
    def __ne__(self, other: object) -> bool: ...
    def __setstate__(self, state: int) -> None: ...
    @property
    def name(self) -> str: ...
    @property
    def value(self) -> int: ...

class Verbosity:
    __members__: ClassVar[dict[Verbosity, int]] = ...  # read-only
    none: ClassVar[Verbosity] = ...
    fatal: ClassVar[Verbosity] = ...
    error: ClassVar[Verbosity] = ...
    warning: ClassVar[Verbosity] = ...
    info: ClassVar[Verbosity] = ...
    debug: ClassVar[Verbosity] = ...
    verbose: ClassVar[Verbosity] = ...

    @overload
    def __init__(self, value: int) -> None: ...
    @overload
    def __init__(self, arg0: str) -> None: ...
    @overload
    def __init__(self, arg0: Verbosity) -> None: ...
    def __eq__(self, other: object) -> bool: ...
    def __getstate__(self) -> int: ...
    def __hash__(self) -> int: ...
    def __index__(self) -> int: ...
    def __int__(self) -> int: ...
    def __ne__(self, other: object) -> bool: ...
    def __setstate__(self, state: int) -> None: ...
    @property
    def name(self) -> str: ...
    @property
    def value(self) -> int: ...

class SynthesisConfiguration:
    """Class representing the configuration for the Clifford synthesis tehcniques."""

    dump_intermediate_results: bool
    gate_limit_factor: float
    initial_timestep_limit: int
    intermediate_results_path: str
    minimize_gates_after_depth_optimization: bool
    minimize_gates_after_two_qubit_gate_optimization: bool
    solver_parameters: dict[str, bool | int | float | str]
    target_metric: TargetMetric
    try_higher_gate_limit_for_two_qubit_gate_optimization: bool
    use_maxsat: bool
    use_symmetry_breaking: bool
    verbosity: Verbosity
    heuristic: bool
    split_size: int
    linear_search: bool

    def __init__(self) -> None: ...
    def json(self) -> dict[str, Any]: ...

class SynthesisResults:
    """Class representing the results of the Clifford synthesis techniques."""
    def __init__(self) -> None: ...
    def sat(self) -> bool: ...
    def unsat(self) -> bool: ...
    @property
    def circuit(self) -> str: ...
    @property
    def depth(self) -> int: ...
    @property
    def gates(self) -> int: ...
    @property
    def runtime(self) -> float: ...
    @property
    def single_qubit_gates(self) -> int: ...
    @property
    def solver_calls(self) -> int: ...
    @property
    def tableau(self) -> str: ...
    @property
    def two_qubit_gates(self) -> int: ...

class Tableau:
    """Class representing a Clifford tableau."""
    @overload
    def __init__(self, n: int, include_stabilizers: bool = False) -> None: ...
    @overload
    def __init__(self, description: str) -> None: ...
    @overload
    def __init__(self, stabilizers: str, destabilizers: str) -> None: ...

class CliffordSynthesizer:
    """The main class for the Clifford synthesis techniques."""
    @overload
    def __init__(self, initial_tableau: Tableau, target_tableau: Tableau) -> None: ...
    @overload
    def __init__(self, target_tableau: Tableau) -> None: ...
    @overload
    def __init__(self, qc: QuantumComputation, use_destabilizers: bool) -> None: ...
    @overload
    def __init__(self, initial_tableau: Tableau, qc: QuantumComputation) -> None: ...
    def synthesize(self, config: SynthesisConfiguration = ...) -> None: ...
    @property
    def results(self) -> SynthesisResults: ...
    @property
    def result_circuit(self) -> QuantumComputation: ...
