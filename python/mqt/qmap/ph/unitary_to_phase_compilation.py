# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""Unitary-to-phase compilation via gradient-based optimization."""

from __future__ import annotations

import logging
from dataclasses import dataclass

import torch

from .mesh import mzi_top_modes
from .routing_to_phases import apply_routing_transform, precompute_routing_transform, reshape_flattened_params_to_grid

logger = logging.getLogger(__name__)

TWO_PI = 2 * torch.pi


def build_unitary_selected_columns_from_components(
    num_modes: int,
    beam_splitter_params: torch.Tensor,
    phase_shifter_params: torch.Tensor,
    column_indices: list[int] | torch.Tensor,
    exclude_edge_phase_shifters: bool = False,
) -> torch.Tensor:
    """Build selected columns of the chip unitary without constructing the full matrix.

    Propagates only the selected input state vectors through the MZI mesh, so it
    is faster when ``len(column_indices) << num_modes`` and never forms an
    ``N x N`` component matrix.  Selecting every column
    (``column_indices=list(range(num_modes))``) yields the full chip unitary.

    Args:
        num_modes: Number of spatial modes on the chip.
        beam_splitter_params: 1D tensor of beam-splitter reflectivities ordered
            MZI-by-MZI as in/out pairs, layer by layer.
        phase_shifter_params: Phase grid of shape ``(num_modes, num_modes)``.
        column_indices: Indices of the columns to compute.
        exclude_edge_phase_shifters: If ``True``, corner phase shifters are
            omitted.

    Returns:
        Complex tensor of shape ``(num_modes, len(column_indices))``
        containing the selected columns of the full chip unitary.
    """
    ps_grid = phase_shifter_params
    if exclude_edge_phase_shifters:
        ps_grid = ps_grid.clone()
        ps_grid[0, -1] = ps_grid[-1, -1] = 0.0

    device = ps_grid.device
    col_idx = torch.as_tensor(column_indices, dtype=torch.long, device=device)
    reflectivities = beam_splitter_params.to(device=device, dtype=torch.float64)
    a = torch.sqrt(reflectivities)
    b = 1j * torch.sqrt(1 - reflectivities)
    splitters = torch.stack((a, b, b, a), dim=-1).reshape(-1, 2, 2, 2)
    phases = torch.exp(1j * ps_grid)
    u = torch.eye(num_modes, dtype=torch.complex128, device=device)[:, col_idx]

    mzi_offset = 0
    for layer in range(num_modes):
        tops = mzi_top_modes(num_modes, layer)
        start, stop = tops.start, tops.start + 2 * len(tops)
        layer_splitters = splitters[mzi_offset : mzi_offset + len(tops)]
        paired = u[start:stop].reshape(len(tops), 2, len(col_idx))
        paired = layer_splitters[:, 0] @ paired
        paired = phases[start:stop, layer].reshape(-1, 2, 1) * paired
        paired = layer_splitters[:, 1] @ paired
        u = torch.cat((
            phases[:start, layer, None] * u[:start],
            paired.flatten(0, 1),
            phases[stop:, layer, None] * u[stop:],
        ))
        mzi_offset += len(tops)

    return u


def fidelity_loss(
    effective_unitary: torch.Tensor,
    target_unitary: torch.Tensor,
) -> torch.Tensor:
    r"""Compute the normalized fidelity loss between two unitaries.

    Loss is defined as
    :math:`1 - |\mathrm{Tr}(U_\mathrm{tgt}^\dagger U_\mathrm{eff})|^2 / N^2`,
    where :math:`N` is the number of compared columns.  A loss of 0.0 indicates
    a perfect match (up to global phase).

    Args:
        effective_unitary: Selected columns and output rows of the chip unitary.
        target_unitary: Target unitary with compatible shape.

    Returns:
        Scalar tensor holding the fidelity loss in ``[0, 1]``.
    """
    n = effective_unitary.shape[1]
    overlap = (target_unitary.conj() * effective_unitary).sum()
    fidelity = overlap.abs() ** 2 / (n * n)
    return 1.0 - fidelity


@dataclass
class OptimizationResult:
    """Result of an :func:`optimize_unitary_subcircuit_parameters` run.

    Attributes:
        phase_shifter_params: Best ``(num_modes_opt, num_modes_opt)`` parameter
            grid (mod 2pi), where ``num_modes_opt`` is ``target_dim`` when
            ``movement_mask`` is ``None`` or ``movement_mask.shape[0]``
            otherwise. Routing constraints are already applied. Excluded
            corner phase shifters have zero phase.
        best_loss: Loss of ``phase_shifter_params`` (the minimum over all
            steps), matching the returned parameters rather than the final step.
    """

    phase_shifter_params: torch.Tensor
    best_loss: float


def optimize_unitary_subcircuit_parameters(
    target_unitary: torch.Tensor,
    beam_splitter_reflectivities: torch.Tensor,
    movement_mask: torch.Tensor | None = None,
    lr: float = 0.05,
    threshold: float = 1e-5,
    active_cols: list[int] | None = None,
    active_cols_target: list[int] | None = None,
    verbose: bool = False,
    max_iterations: int = 10000,
    output_rows: list[int] | None = None,
    exclude_edge_phase_shifters: bool = False,
    optimize_routing_parameters: bool = True,
    early_stop_patience: int = 50,
    min_improvement: float = 1e-4,
) -> OptimizationResult:
    """Optimize phase-shifter parameters to approximate a target unitary.

    Runs an Adam optimizer with optional learning-rate scheduling.

    Args:
        target_unitary: Target unitary tensor of shape ``(target_dim, target_dim)``.
        beam_splitter_reflectivities: 1D tensor of chip beam-splitter
            reflectivities.  Treated as fixed (no gradient).
        movement_mask: Integer tensor of shape ``(num_modes, num_modes)``
            encoding routing constraints.  When ``None``, the full chip is
            treated as a computation zone.
        lr: Initial Adam learning rate.
        threshold: Loss value below which optimization is considered
            successful and terminates early.
        active_cols: Physical input column indices to inject photons into.
        active_cols_target: Column indices within the computation zone
            corresponding to ``active_cols``.
        verbose: If ``True``, log progress (INFO level) every 100 iterations.
        max_iterations: Exact upper bound on the number of gradient steps. The
            loop evaluates the initial parameters and the parameters after each
            step, so ``0`` performs no step and returns the initial state, ``1``
            performs exactly one step, and so on.
        output_rows: Output rows to compare. Defaults to all rows.
        exclude_edge_phase_shifters: If ``True``, the two corner phase
            shifters are excluded from the parameter set.
        optimize_routing_parameters: If ``True``, routing cells contribute a
            single trainable degree of freedom.
        early_stop_patience: Number of consecutive steps without improvement
            before optimization is terminated early.
        min_improvement: Minimum absolute loss decrease required to reset the
            patience counter.

    Returns:
        An :class:`OptimizationResult` with the best parameter grid and its loss.
    """
    if max_iterations < 0:
        msg = "max_iterations must be nonnegative."
        raise ValueError(msg)
    target_unitary = target_unitary.to(dtype=torch.complex128)
    target_dim = target_unitary.shape[0]

    num_modes_opt = target_dim if movement_mask is None else movement_mask.shape[0]

    param_count = num_modes_opt**2 - 2 if exclude_edge_phase_shifters else num_modes_opt**2

    column_indices = active_cols if active_cols is not None else list(range(num_modes_opt))
    if active_cols is not None:
        target_cols = active_cols_target if active_cols_target is not None else active_cols
        target_unitary = target_unitary[:, target_cols]

    init_flat = TWO_PI * torch.rand(param_count, dtype=torch.float64)
    phase_shifter_params = reshape_flattened_params_to_grid(
        init_flat, num_modes_opt, exclude_edge_phase_shifters=exclude_edge_phase_shifters
    ).requires_grad_(True)
    routing_transform = (
        None if movement_mask is None else precompute_routing_transform(movement_mask, optimize_routing_parameters)
    )

    optimizer = torch.optim.Adam([phase_shifter_params], lr=lr)
    scheduler = torch.optim.lr_scheduler.ReduceLROnPlateau(
        optimizer,
        mode="min",
        factor=0.5,
        patience=50,
        min_lr=1e-7,
    )

    best_loss = float("inf")
    best_params = phase_shifter_params.detach().clone()
    patience_ref_loss = float("inf")
    no_improve_steps = 0

    for index in range(max_iterations + 1):
        ps_for_build = (
            phase_shifter_params
            if routing_transform is None
            else apply_routing_transform(phase_shifter_params, routing_transform)
        )

        u_model = build_unitary_selected_columns_from_components(
            num_modes_opt,
            beam_splitter_reflectivities,
            ps_for_build,
            column_indices=column_indices,
            exclude_edge_phase_shifters=exclude_edge_phase_shifters,
        )
        if output_rows is not None:
            u_model = u_model[output_rows]
        loss = fidelity_loss(u_model, target_unitary)

        loop_loss = loss.item()

        if loop_loss < best_loss:
            best_loss = loop_loss
            best_params = ps_for_build.detach().clone()

        if loop_loss < patience_ref_loss - min_improvement:
            patience_ref_loss = loop_loss
            no_improve_steps = 0
        else:
            no_improve_steps += 1

        if verbose and index % 100 == 0:
            logger.info("Iteration %d: loss=%.6e", index, loop_loss)

        if loop_loss <= threshold or index >= max_iterations:
            break
        if early_stop_patience > 0 and no_improve_steps >= early_stop_patience:
            break

        optimizer.zero_grad()
        loss.backward()

        optimizer.step()
        scheduler.step(loop_loss)

    return OptimizationResult(
        phase_shifter_params=torch.remainder(best_params, TWO_PI),
        best_loss=best_loss,
    )
