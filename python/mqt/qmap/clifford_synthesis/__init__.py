# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
# Copyright (c) 2025 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""Clifford synthesis module."""

from __future__ import annotations

from mqt.qmap.clifford_synthesis.clifford_synthesis import (
    CliffordSynthesizer,
    SynthesisConfiguration,
    SynthesisResults,
    Tableau,
)
from mqt.qmap.clifford_synthesis.util import optimize_clifford, synthesize_clifford

__all__ = [
    "CliffordSynthesizer",
    "SynthesisConfiguration",
    "SynthesisResults",
    "Tableau",
    "optimize_clifford",
    "synthesize_clifford",
]


def __dir__() -> list[str]:
    return __all__
