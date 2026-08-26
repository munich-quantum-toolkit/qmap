# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""Utilities for converting routing masks to phase-shifter parameter grids."""

from __future__ import annotations

import numpy as np
import torch

from .routing import MaskState


def get_effective_params_and_mask(
    num_modes: int,
    movement_mask: torch.Tensor,
    raw_params: torch.Tensor,
    optimize_routing_parameters: bool = False,
) -> tuple[torch.Tensor, torch.Tensor, torch.Tensor]:
    """Apply routing constraints to produce effective phase-shifter parameters.

    The function applies the following logic in order:

    1. Take the virtual phase-shifter states (``TOP_ONLY``/``BOT_ONLY``)
       directly from ``movement_mask``.  :func:`routing.route_to_movement_mask`
       assigns these structurally from the compute-zone geometry, so genuine
       compute MZI pairs stay trainable regardless of their current phase
       magnitudes.
    2. Resolve each MZI pair to a single routing state by priority
       (``BOT_ONLY`` > ``TOP_ONLY`` > ``CROSS`` > ``BAR`` > ``MZI``, i.e. the
       larger ``MaskState`` code).  Masks produced by the routing pipeline always
       assign both modes of a pair the same state, so this ordering only acts as
       a defensive tiebreak and does not affect the result in practice.

    Compute-zone MZI cells (``MaskState.MZI``) are always left as free,
    trainable parameters - the compute/routing distinction comes solely from the
    structural ``movement_mask``, never from the current phase magnitudes.

    When ``optimize_routing_parameters`` is ``True``, routing cells become
    trainable with a constrained offset so their relative phase relationship
    is preserved (cross: equal phases; bar: phases differ by pi).

    Args:
        num_modes: Number of spatial modes on the chip.
        movement_mask: Integer tensor of shape ``(num_modes, num_modes)``
            with state codes.
        raw_params: Float tensor of shape ``(num_modes, num_modes)`` with
            current unconstrained phase values.
        optimize_routing_parameters: If ``True``, routing MZI pairs expose
            a single trainable degree of freedom while the second mode is
            derived and gradient-masked.

    Returns:
        A tuple ``(effective_params, grad_mask, refined_mask)`` where
        *effective_params* are the physically constrained phase values,
        *grad_mask* indicates which entries contribute gradients (1.0) or
        are frozen (0.0), and *refined_mask* is the updated movement mask.
    """
    refined_mask = movement_mask.clone()
    effective_params = raw_params.clone()
    grad_mask = torch.ones_like(raw_params, dtype=torch.float32)
    num_layers = raw_params.shape[1]

    device = raw_params.device
    mask_used = movement_mask[:, :num_layers]

    # Per-cell geometry (broadcast to (num_modes, num_layers)).
    mode_col = torch.arange(num_modes, device=device).view(num_modes, 1)
    layer_row = torch.arange(num_layers, device=device).view(1, num_layers)
    even_layer = layer_row % 2 == 0
    even_mode = mode_col % 2 == 0
    first_mode = mode_col == 0
    last_mode = mode_col == num_modes - 1

    # Layer parity sets the pairing: even layers pair (0,1),(2,3),...; odd layers pair
    # (1,2),(3,4),... and leave the two edge modes (0 and num_modes-1) as single edges.
    is_single = (~even_layer) & (first_mode | last_mode)
    is_top = torch.where(even_layer, even_mode, (~even_mode) & (~last_mode))
    is_bot = torch.where(even_layer, ~even_mode, even_mode & (~first_mode))
    is_pair = is_top | is_bot

    # Each pair cell resolves to the higher-priority state of its two modes.  The MaskState
    # codes ARE the priority order (MZI=0 < BAR=1 < CROSS=2 < TOP_ONLY=3 < BOT_ONLY=4), so
    # "higher priority" is just the larger code.  A top cell's partner is mode+1, a bot cell's
    # is mode-1 (singles partner with themselves; harmless, since they are never read as a pair).
    partner_mode = mode_col.expand(num_modes, num_layers) + is_top.long() - is_bot.long()
    mask_partner = torch.gather(mask_used, 0, partner_mode)
    raw_partner = torch.gather(raw_params, 0, partner_mode)
    pair_state = torch.maximum(mask_used, mask_partner)

    st_cross = is_pair & (pair_state == MaskState.CROSS)
    st_bar = is_pair & (pair_state == MaskState.BAR)
    st_top_only = is_pair & (pair_state == MaskState.TOP_ONLY)
    st_bot_only = is_pair & (pair_state == MaskState.BOT_ONLY)
    single_active = is_single & ((mask_used == MaskState.CROSS) | (mask_used == MaskState.BAR))

    # effective_params: overwrite only the constrained cells; compute cells (MZI) and the
    # top-of-pair BAR/CROSS cells under optimization keep their raw value (the initial clone).
    set_partner_pi = (is_bot & st_top_only) | (is_top & st_bot_only)  # -> raw of the partner mode + pi
    if optimize_routing_parameters:
        set_zero = single_active
        set_pi = torch.zeros_like(is_single)
        set_partner = is_bot & st_cross  # -> raw of the partner (top) mode
        set_partner_pi |= is_bot & st_bar
    else:
        set_zero = single_active | (is_top & (st_cross | st_bar)) | (is_bot & st_cross)
        set_pi = is_bot & st_bar  # -> pi
        set_partner = torch.zeros_like(is_single)

    effective_params = torch.where(set_zero, torch.zeros_like(raw_params), effective_params)
    effective_params = torch.where(set_pi, torch.full_like(raw_params, np.pi), effective_params)
    effective_params = torch.where(set_partner, raw_partner, effective_params)
    effective_params = torch.where(set_partner_pi, raw_partner + np.pi, effective_params)

    # grad_mask: freeze (0.0) the derived and fixed-routing cells; free cells keep 1.0.
    grad_zero = single_active | (is_top & st_bot_only) | (is_bot & (st_cross | st_bar)) | (is_bot & st_top_only)
    if not optimize_routing_parameters:
        grad_zero |= is_top & (st_cross | st_bar)
    grad_mask = torch.where(grad_zero, torch.zeros_like(grad_mask), grad_mask)

    return effective_params, grad_mask, refined_mask


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

    grid_2d = torch.zeros((num_modes, num_modes), dtype=params_1d.dtype, device=params_1d.device)
    mask = torch.ones((num_modes, num_modes), dtype=torch.bool, device=params_1d.device)

    if exclude_edge_phase_shifters:
        mask[0, -1] = False
        mask[num_modes - 1, -1] = False

    grid_2d[mask] = params_1d
    return grid_2d
