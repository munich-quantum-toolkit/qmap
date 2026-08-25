# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""Pytest configuration for the eval/ph evaluation tests.

The evaluation code in ``eval/ph`` is paper-reproduction code, not part of the
installable ``mqt.qmap`` package. Add ``eval/ph`` to ``sys.path`` so these tests
can import the evaluation modules (``hardware_model``, ``baseline``,
``evaluation``, ``random_unitary``, ``data_collection``) by bare name.
"""

from __future__ import annotations

import pathlib
import sys
from types import SimpleNamespace

import numpy as np
import pytest

sys.path.insert(0, str(pathlib.Path(__file__).parents[1]))


# Geometry for the extreme-routing chip (chip_dim=8, target_dim=4). This fixture
# intentionally mirrors the identically-named one in test/python/ph/conftest.py:
# both suites are self-contained (the compiler suite must not import eval code),
# so the synthetic chip is defined in each. There the compiler suite uses it for
# the routing-level regression; here it drives the end-to-end coincidence-rate
# regression in test_subcircuit_compilation.py.
_EXTREME_CHIP_DIM = 8
_EXTREME_TARGET_DIM = 4
# Operation each MZI in a given routing chip layer performs perfectly. "bar" ->
# equal reflectivities (perfect straight-through, poor cross); "cross" ->
# complementary reflectivities summing to 1 (perfect swap, poor bar). This
# (bar, bar, cross, cross) pattern forces a unique optimal route that bars through
# layers 0-1 and crosses through layers 2-3, moving the photon window to modes
# [2, 3, 4, 5]. Reproducing it depends on the graph-layer -> chip-layer mapping.
_EXTREME_ROUTING_PROFILES = ("bar", "bar", "cross", "cross")


def _mzi_counts_per_layer(chip_dim: int) -> list[int]:
    """MZIs per chip layer: full (even) layers pair every mode, half (odd) layers skip the edge modes."""
    return [chip_dim // 2 if layer % 2 == 0 else chip_dim // 2 - 1 for layer in range(chip_dim)]


def _build_extreme_routing_bs(seed: int) -> list[float]:
    """Beam-splitter reflectivities: extreme+distinct in the routing region, ideal 0.5 in the compute zone.

    Routing chip layers 0-3 follow ``_EXTREME_ROUTING_PROFILES`` with random-but-
    distinct magnitudes far from 0.5, so each MZI is near-perfect at exactly one of
    bar/cross and clearly poor at the other -- the strong contrast that forces a
    unique route. Computation-zone layers 4-7 are ideal (0.5), a universal
    interferometer that can realize any target. Ordering matches
    :func:`generate_beam_splitter_matrix` (layer by layer, MZI by MZI, r_in/r_out).
    """
    rng = np.random.default_rng(seed)
    values: list[float] = []
    for layer, count in enumerate(_mzi_counts_per_layer(_EXTREME_CHIP_DIM)):
        profile = _EXTREME_ROUTING_PROFILES[layer] if layer < len(_EXTREME_ROUTING_PROFILES) else "ideal"
        for _ in range(count):
            if profile == "ideal":
                values.extend((0.5, 0.5))
                continue
            magnitude = float(rng.uniform(0.03, 0.15))  # far from 0.5, distinct per MZI
            if profile == "bar":
                values.extend((magnitude, magnitude))  # equal -> perfect BAR
            else:  # "cross"
                values.extend((magnitude, 1.0 - magnitude))  # avg 0.5 -> perfect CROSS
    return values


@pytest.fixture
def extreme_routing_chip():
    """A chip_dim=8 chip with extreme routing beam splitters and an ideal computation zone.

    Drives the end-to-end routing-mapping regression: the extreme contrast forces a
    unique optimal route, so a graph-layer -> chip-layer mapping error changes the
    route and ejects photons out of the computation window, collapsing the
    coincidence rate. See ``test_subcircuit_compilation.test_extreme_routing_coincidence_rate``.
    """
    return SimpleNamespace(
        bs=_build_extreme_routing_bs(seed=7),
        chip_dim=_EXTREME_CHIP_DIM,
        target_dim=_EXTREME_TARGET_DIM,
    )
