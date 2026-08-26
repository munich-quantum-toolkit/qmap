# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""Pytest configuration and shared fixtures for the mqt.qmap.ph test suite."""

from __future__ import annotations

from types import SimpleNamespace

import numpy as np
import pytest


@pytest.fixture
def ideal_bs_chip4():
    """Return ideal 50/50 beam-splitter reflectivities for a 4-mode chip."""
    # chip_size=4: MZIs per layer [2, 1, 2, 1] -> 6 MZIs -> 12 in/out reflectivities.
    return [0.5] * 12


@pytest.fixture
def nonideal_bs_chip4():
    """Return a hand-crafted non-ideal BS list for a 4-mode chip.

    Layout: ``[in0, out0, in1, out1, ...]`` - 6 MZIs x 2 values = 12 entries.
    Values are chosen so that each MZI pair is unique, making wrong pairings
    detectable in tests.
    """
    return [0.40, 0.60, 0.30, 0.70, 0.45, 0.55, 0.35, 0.65, 0.48, 0.52, 0.42, 0.58]


@pytest.fixture
def ones_transmissions_chip4():
    """Return all-ones transmission list for a 4-mode chip."""
    return [1.0, 1.0, 1.0, 1.0]


# Geometry for the routing-layer-mapping regression fixtures (chip_dim=8, target_dim=4).
# The routing region spans chip layers 0-3; the computation zone spans chip layers 4-7.
# This chip is intentionally mirrored in eval/ph/tests/conftest.py, which drives the
# end-to-end version of the same regression: the compiler suite must stay self-contained
# (no eval import), so the synthetic chip is defined in each suite rather than shared.
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

    Used to regression-test the graph-layer -> chip-layer routing mapping (see
    ``get_edge_cost_for_graph_layer`` in ``graph.py``): the extreme contrast
    forces a unique optimal route, so a mapping error changes the route and, end to
    end, ejects photons out of the computation window. The near-0.5 beam splitters
    used by the other scenarios cannot catch this -- there every MZI is near-perfect
    at both bar and cross, so the mapping barely affects the routing cost.
    """
    return SimpleNamespace(
        bs=_build_extreme_routing_bs(seed=7),
        chip_dim=_EXTREME_CHIP_DIM,
        target_dim=_EXTREME_TARGET_DIM,
    )
