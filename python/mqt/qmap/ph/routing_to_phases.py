"""Utilities for converting routing masks to phase-shifter parameter grids."""

from __future__ import annotations

import numpy as np
import torch

from .routing import MaskState


def infer_routing_states(
    num_modes: int,
    movement_mask: torch.Tensor,
    embedded_params: torch.Tensor,
    tolerance: float = 1e-6,
) -> torch.Tensor:
    """Refine a movement mask by detecting virtual phase-shifter cells.

    An MZI cell whose top (bottom) parameter is non-zero while the other is
    effectively zero is promoted to ``MaskState.TOP_ONLY`` (``MaskState.BOT_ONLY``).

    Args:
        num_modes: Number of spatial modes on the chip.
        movement_mask: Integer tensor of shape ``(num_modes, num_modes)``
            containing initial state codes.
        embedded_params: Float tensor of shape ``(num_modes, num_modes)``
            with current phase values laid out on the chip grid.
        tolerance: Absolute threshold below which a parameter is treated as
            zero.

    Returns:
        Refined movement mask with the same shape as ``movement_mask``.
    """
    refined_mask = movement_mask.clone()
    num_layers = movement_mask.shape[1]

    for layer in range(num_layers):
        if layer % 2 == 0:
            mzi_pairs = [(i, i + 1) for i in range(0, num_modes - 1, 2)]
        else:
            mzi_pairs = [(i, i + 1) for i in range(1, num_modes - 1, 2)]

        for top, bot in mzi_pairs:
            if movement_mask[top, layer].item() != MaskState.MZI:
                continue

            has_top = abs(embedded_params[top, layer].item()) > tolerance
            has_bot = abs(embedded_params[bot, layer].item()) > tolerance

            if has_top and not has_bot:
                refined_mask[top, layer] = MaskState.TOP_ONLY
                refined_mask[bot, layer] = MaskState.TOP_ONLY
            elif has_bot and not has_top:
                refined_mask[top, layer] = MaskState.BOT_ONLY
                refined_mask[bot, layer] = MaskState.BOT_ONLY

    return refined_mask


def get_effective_params_and_mask(
    num_modes: int,
    movement_mask: torch.Tensor,
    raw_params: torch.Tensor,
    optimize_routing_parameters: bool = False,
) -> tuple[torch.Tensor, torch.Tensor, torch.Tensor]:
    """Apply routing constraints to produce effective phase-shifter parameters.

    The function applies the following logic in order:

    1. Infer virtual phase-shifter states from ``raw_params``.
    2. Apply priority rules (``MaskState.BAR`` > ``MaskState.MZI`` > ``MaskState.CROSS``).
    3. Apply smart defaults: an MZI cell with both parameters at zero is
       set to bar state (0, π) to seal the photonic block.

    When ``optimize_routing_parameters`` is ``True``, routing cells become
    trainable with a constrained offset so their relative phase relationship
    is preserved (cross: equal phases; bar: phases differ by π).

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
    priority_map = {
        MaskState.MZI: 0,
        MaskState.BAR: 1,
        MaskState.CROSS: 2,
        MaskState.TOP_ONLY: 3,
        MaskState.BOT_ONLY: 4,
    }

    refined_mask = infer_routing_states(num_modes, movement_mask, raw_params)
    effective_params = raw_params.clone()
    grad_mask = torch.ones_like(raw_params, dtype=torch.float32)
    num_layers = raw_params.shape[1]

    for layer in range(num_layers):
        if layer % 2 == 0:
            mzi_pairs = [(i, i + 1) for i in range(0, num_modes - 1, 2)]
            single_edges: list[int] = []
        else:
            mzi_pairs = [(i, i + 1) for i in range(1, num_modes - 1, 2)]
            single_edges = [0, num_modes - 1]

        for mode in single_edges:
            if refined_mask[mode, layer].item() in (MaskState.CROSS, MaskState.BAR):
                effective_params[mode, layer] = 0.0
                grad_mask[mode, layer] = 0.0

        for top, bot in mzi_pairs:
            s_top = refined_mask[top, layer].item()
            s_bot = refined_mask[bot, layer].item()
            state = s_top if priority_map[s_top] >= priority_map[s_bot] else s_bot

            if state == MaskState.CROSS:
                if optimize_routing_parameters:
                    effective_params[top, layer] = raw_params[top, layer]
                    effective_params[bot, layer] = raw_params[top, layer]
                    grad_mask[top, layer] = 1.0
                    grad_mask[bot, layer] = 0.0
                else:
                    effective_params[top, layer] = 0.0
                    effective_params[bot, layer] = 0.0
                    grad_mask[top, layer] = 0.0
                    grad_mask[bot, layer] = 0.0

            elif state == MaskState.BAR:
                if optimize_routing_parameters:
                    effective_params[top, layer] = raw_params[top, layer]
                    effective_params[bot, layer] = raw_params[top, layer] + np.pi
                    grad_mask[top, layer] = 1.0
                    grad_mask[bot, layer] = 0.0
                else:
                    effective_params[top, layer] = 0.0
                    effective_params[bot, layer] = np.pi
                    grad_mask[top, layer] = 0.0
                    grad_mask[bot, layer] = 0.0

            elif state == MaskState.MZI:
                p_top = raw_params[top, layer].item()
                p_bot = raw_params[bot, layer].item()
                if abs(p_top) < 1e-9 and abs(p_bot) < 1e-9:
                    # Empty compute MZI → bar state to seal the block.
                    effective_params[top, layer] = 0.0
                    effective_params[bot, layer] = np.pi
                    grad_mask[top, layer] = 0.0
                    grad_mask[bot, layer] = 0.0

            elif state == MaskState.TOP_ONLY:
                effective_params[bot, layer] = raw_params[top, layer] + np.pi
                grad_mask[bot, layer] = 0.0

            elif state == MaskState.BOT_ONLY:
                effective_params[top, layer] = raw_params[bot, layer] + np.pi
                grad_mask[top, layer] = 0.0

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
        msg = (
            f"Size mismatch: expected {expected_size} parameters for "
            f"{num_modes} modes, but got {params_1d.numel()}."
        )
        raise ValueError(msg)

    grid_2d = torch.zeros((num_modes, num_modes), dtype=params_1d.dtype, device=params_1d.device)
    mask = torch.ones((num_modes, num_modes), dtype=torch.bool, device=params_1d.device)

    if exclude_edge_phase_shifters:
        mask[0, -1] = False
        mask[num_modes - 1, -1] = False

    grid_2d[mask] = params_1d
    return grid_2d
