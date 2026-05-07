# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""Baseline reference strategy for the photonic compiler."""

from __future__ import annotations

from typing import TYPE_CHECKING

import torch

if TYPE_CHECKING:
    import numpy as np


def embed_target_unitary_into_chip(target_unitary: np.ndarray, chip_dim: int, target_dim: int) -> torch.Tensor:
    """Embed a target unitary into the top-left block of a chip-sized identity matrix.

    Args:
        target_unitary: Complex unitary of shape ``(target_dim, target_dim)``.
        chip_dim: Total number of spatial modes on the chip.
        target_dim: Dimension of the target unitary.

    Returns:
        A ``(chip_dim, chip_dim)`` complex tensor that equals the identity
        everywhere except the top-left ``(target_dim, target_dim)`` block,
        which is replaced by ``target_unitary``.
    """
    embedded = torch.eye(chip_dim, dtype=torch.complex128)
    for i in range(target_dim):
        for j in range(target_dim):
            embedded[i, j] = target_unitary[i, j]
    return embedded


def get_baseline_active_cols(target_dim: int) -> list[int]:
    """Return the even-indexed column indices used by the baseline strategy.

    The baseline places photons on every other mode (dual-rail encoding),
    so only even column indices are active.

    Args:
        target_dim: Dimension of the target unitary.

    Returns:
        List of even indices ``[0, 2, 4, …, target_dim - 2]``.
    """
    return [i for i in range(target_dim) if i % 2 == 0]


def get_baseline_input_ports(active_cols: list[int], chip_dim: int) -> list[int]:
    """Build a binary input-port vector for the baseline from active column indices.

    Args:
        active_cols: Column indices that carry a photon (``1``); all others are ``0``.
        chip_dim: Total number of spatial modes on the chip.

    Returns:
        A list of length ``chip_dim`` with ``1`` at each active column and ``0``
        elsewhere.
    """
    input_ports = [0] * chip_dim
    for i in active_cols:
        input_ports[i] = 1
    return input_ports
