# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""Utilities for converting routing masks to phase-shifter parameter grids."""

from __future__ import annotations

from dataclasses import dataclass

import torch

from .mesh import mzi_top_modes
from .routing import MaskState


@dataclass(frozen=True)
class RoutingTransform:
    """Static routing constraints for a phase grid.

    Attributes:
        source_mode: Raw phase source for each cell, including paired modes.
        offset: Fixed phase offset, either zero or pi.
        use_raw: Whether the effective phase includes a raw phase.
    """

    source_mode: torch.Tensor
    offset: torch.Tensor
    use_raw: torch.Tensor


def precompute_routing_transform(
    movement_mask: torch.Tensor,
    optimize_routing_parameters: bool = False,
) -> RoutingTransform:
    """Precompute phase sources and offsets from the mesh and routing states.

    A bar pair has phases differing by pi; a cross pair has equal phases.
    Routing pairs share one trainable phase when optimization is enabled.
    Virtual phase shifters use the raw phase of their active mode. Compute
    cells remain independent. Mixed pairs take the higher MaskState code.

    Args:
        movement_mask: Integer grid of routing states, indexed by mode and layer.
        optimize_routing_parameters: Whether routing pairs have a trainable phase.

    Returns:
        Phase sources, offsets, and selectors to reuse during optimization.
    """
    num_modes, num_layers = movement_mask.shape
    device = movement_mask.device
    mode_col = torch.arange(num_modes, device=device).view(num_modes, 1)
    is_top = torch.zeros_like(movement_mask, dtype=torch.bool)
    is_bot = torch.zeros_like(is_top)
    for layer in range(num_layers):
        tops = mzi_top_modes(num_modes, layer)
        is_top[tops.start : tops.stop : tops.step, layer] = True
        is_bot[tops.start + 1 : tops.stop + 1 : tops.step, layer] = True
    is_pair = is_top | is_bot

    partner_mode = mode_col.expand(num_modes, num_layers) + is_top.long() - is_bot.long()
    pair_state = torch.maximum(movement_mask, torch.gather(movement_mask, 0, partner_mode))
    bar = is_pair & (pair_state == MaskState.BAR)
    cross = is_pair & (pair_state == MaskState.CROSS)
    top_only = is_pair & (pair_state == MaskState.TOP_ONLY)
    bot_only = is_pair & (pair_state == MaskState.BOT_ONLY)

    derived = (is_bot & top_only) | (is_top & bot_only)
    fixed = ~is_pair & ((movement_mask == MaskState.BAR) | (movement_mask == MaskState.CROSS))
    if optimize_routing_parameters:
        derived |= is_bot & (bar | cross)
    else:
        fixed |= bar | cross

    offset = torch.zeros_like(movement_mask, dtype=torch.float64)
    offset[(is_bot & (bar | top_only)) | (is_top & bot_only)] = torch.pi
    return RoutingTransform(
        source_mode=torch.where(derived, partner_mode, mode_col),
        offset=offset,
        use_raw=~fixed,
    )


def apply_routing_transform(raw_params: torch.Tensor, transform: RoutingTransform) -> torch.Tensor:
    """Apply routing constraints while preserving gradients to their raw sources.

    Args:
        raw_params: Current phase grid.
        transform: Precomputed routing constraints.

    Returns:
        Effective phases. Autograd freezes constants and accumulates gradients
        from derived phases into their sources.
    """
    source = torch.gather(raw_params, 0, transform.source_mode)
    return torch.where(transform.use_raw, source, 0.0) + transform.offset.to(raw_params.dtype)


def reshape_flattened_params_to_grid(
    params_1d: torch.Tensor,
    num_modes: int,
    exclude_edge_phase_shifters: bool = False,
) -> torch.Tensor:
    """Inflate a 1D parameter vector into a 2D phase-shifter grid.

    When ``exclude_edge_phase_shifters`` is ``True``, the top-right and
    bottom-right corner positions are absent from ``params_1d`` and are
    padded with zero in the output grid.

    Args:
        params_1d: Flat parameter tensor of size ``num_modes**2`` (or
            ``num_modes**2 - 2`` when edge phase shifters are excluded).
        num_modes: Number of spatial modes on the chip.
        exclude_edge_phase_shifters: If ``True``, the two corner entries are
            absent from ``params_1d``.

    Returns:
        Float tensor of shape ``(num_modes, num_modes)`` with parameters
        placed at valid grid positions and zeros at excluded corners.

    Raises:
        ValueError: If the size of ``params_1d`` does not match the expected
            count for the given ``num_modes`` and ``exclude_edge_phase_shifters``
            setting.
    """
    expected_size = num_modes**2 - 2 if exclude_edge_phase_shifters else num_modes**2

    if params_1d.numel() != expected_size:
        msg = f"Size mismatch: expected {expected_size} parameters for {num_modes} modes, but got {params_1d.numel()}."
        raise ValueError(msg)

    if not exclude_edge_phase_shifters:
        return params_1d.reshape(num_modes, num_modes)

    grid_2d = torch.zeros((num_modes, num_modes), dtype=params_1d.dtype, device=params_1d.device)
    mask = torch.ones((num_modes, num_modes), dtype=torch.bool, device=params_1d.device)

    mask[0, -1] = False
    mask[num_modes - 1, -1] = False

    grid_2d[mask] = params_1d
    return grid_2d
