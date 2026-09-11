# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""Pairing rule for the staggered MZI mesh."""


def mzi_top_modes(num_modes: int, layer: int) -> range:
    """Return the upper mode of each MZI in a layer, in beam-splitter order.

    Args:
        num_modes: Number of spatial modes.
        layer: Zero-based physical layer index.

    Returns:
        Upper modes of adjacent pairs. Odd layers leave the edge modes uncoupled.
    """
    return range(layer % 2, num_modes - 1, 2)
