# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
# Copyright (c) 2025 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""Super conducting module."""

from __future__ import annotations

from mqt.qmap.sc.compile import compile  # noqa: A004
from mqt.qmap.sc.load_architecture import load_architecture
from mqt.qmap.sc.load_calibration import load_calibration
from mqt.qmap.sc.sc import (
    Arch,
    Architecture,
    CommanderGrouping,
    Configuration,
    EarlyTermination,
    Encoding,
    Heuristic,
    InitialLayout,
    Layering,
    LookaheadHeuristic,
    MappingResults,
    Method,
    SwapReduction,
    map,  # noqa: A004
)
from mqt.qmap.sc.subarchitectures import (
    SubarchitectureOrder,
    ibm_guadalupe_subarchitectures,
    rigetti_16_subarchitectures,
)

__all__ = [
    "Arch",
    "Architecture",
    "CommanderGrouping",
    "Configuration",
    "EarlyTermination",
    "Encoding",
    "Heuristic",
    "InitialLayout",
    "Layering",
    "LookaheadHeuristic",
    "MappingResults",
    "Method",
    "SubarchitectureOrder",
    "SwapReduction",
    "compile",
    "ibm_guadalupe_subarchitectures",
    "load_architecture",
    "load_calibration",
    "map",
    "rigetti_16_subarchitectures",
]


def __dir__() -> list[str]:
    return __all__
