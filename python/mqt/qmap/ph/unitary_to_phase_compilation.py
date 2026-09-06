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
        phase_shifter_params: Phase-shifter parameter array.  Accepted shapes are
            ``(N, N)``, ``(N**2,)``, or ``(N**2 - 2,)`` (corner-excluded).
        column_indices: Indices of the columns to compute.
        exclude_edge_phase_shifters: If ``True``, corner phase shifters are
            omitted.

    Returns:
        Complex tensor of shape ``(num_modes, len(column_indices))``
        containing the selected columns of the full chip unitary.
    """
    n = num_modes
    n2 = n * n

    if isinstance(column_indices, torch.Tensor):
        col_idx = column_indices.to(dtype=torch.long)
    else:
        col_idx = torch.tensor(column_indices, dtype=torch.long)

    def to_grid(tensor: torch.Tensor) -> torch.Tensor:
        # The optimizer trains the parameters natively as an (n, n) grid, so pass
        # that through; also accept the flat (n**2,) / corner-excluded (n**2 - 2,)
        # forms, inflating them with the shared reshape helper.
        if tensor.shape == (n, n):
            return tensor
        flat = tensor.flatten()
        if flat.numel() not in {n2, n2 - 2}:
            msg = f"Invalid parameter size: {flat.numel()}. Expected {n2}."
            raise ValueError(msg)
        return reshape_flattened_params_to_grid(flat, n, exclude_edge_phase_shifters=flat.numel() == n2 - 2)

    ps_grid = to_grid(phase_shifter_params)

    bs_idx = 0
    u = torch.eye(num_modes, dtype=torch.complex128)[:, col_idx]

    def apply_ps_left(u_local: torch.Tensor, mode: int, phi: torch.Tensor) -> torch.Tensor:
        phase = torch.exp(1j * phi)
        scales = torch.ones((num_modes, 1), dtype=torch.complex128, device=u_local.device)
        scales[mode, 0] = phase
        return scales * u_local

    def apply_bs_left(u_local: torch.Tensor, mode: int, reflectivity: torch.Tensor) -> torch.Tensor:
        r = torch.as_tensor(reflectivity, dtype=torch.float64, device=u_local.device)
        a = torch.sqrt(r)
        b = 1j * torch.sqrt(1 - r)
        row_top = u_local[mode : mode + 1, :]
        row_bot = u_local[mode + 1 : mode + 2, :]
        new_top = a * row_top + b * row_bot
        new_bot = b * row_top + a * row_bot
        prefix = u_local[:mode, :]
        suffix = u_local[mode + 2 :, :]
        return torch.cat((prefix, new_top, new_bot, suffix), dim=0)

    for layer in range(n):
        if layer % 2 == 0:
            for i in range(0, num_modes - 1, 2):
                theta_in = beam_splitter_params[bs_idx]
                theta_out = beam_splitter_params[bs_idx + 1]
                phi1 = ps_grid[i, layer]
                phi2 = ps_grid[i + 1, layer]
                u = apply_bs_left(u, i, theta_in)
                u = apply_ps_left(u, i, phi1)
                u = apply_ps_left(u, i + 1, phi2)
                u = apply_bs_left(u, i, theta_out)
                bs_idx += 2
        else:
            is_last_layer = exclude_edge_phase_shifters and layer == n - 1

            if not is_last_layer:
                phi = ps_grid[0, layer]
                u = apply_ps_left(u, 0, phi)

            for j in range(1, num_modes - 1, 2):
                theta_in = beam_splitter_params[bs_idx]
                theta_out = beam_splitter_params[bs_idx + 1]
                phi1 = ps_grid[j, layer]
                phi2 = ps_grid[j + 1, layer]
                u = apply_bs_left(u, j, theta_in)
                u = apply_ps_left(u, j, phi1)
                u = apply_ps_left(u, j + 1, phi2)
                u = apply_bs_left(u, j, theta_out)
                bs_idx += 2

            if not is_last_layer:
                phi = ps_grid[num_modes - 1, layer]
                u = apply_ps_left(u, num_modes - 1, phi)

    return u


def fidelity_loss(
    effective_unitary: torch.Tensor,
    target_unitary: torch.Tensor,
    active_cols: list[int] | None = None,
    active_cols_target: list[int] | None = None,
    baseline_outputs: list[int] | None = None,
) -> torch.Tensor:
    r"""Compute the normalized fidelity loss between two unitaries.

    Loss is defined as
    :math:`1 - |\mathrm{Tr}(U_\mathrm{tgt}^\dagger U_\mathrm{eff})|^2 / N^2`,
    where :math:`N` is the number of compared columns.  A loss of 0.0 indicates
    a perfect match (up to global phase).

    Args:
        effective_unitary: Unitary produced by the chip, shape
            ``(num_modes, num_modes)`` or ``(num_modes, len(active_cols))``.
        target_unitary: Target unitary with compatible shape.
        active_cols: Column indices to select from ``effective_unitary`` before
            comparison.  When ``None``, all columns are used.
        active_cols_target: Column indices to select from ``target_unitary``.
            Defaults to ``active_cols`` when ``None``.
        baseline_outputs: Row indices to select from ``effective_unitary`` after
            column selection.  Used to restrict comparison to the computation
            zone rows.

    Returns:
        Scalar tensor holding the fidelity loss in ``[0, 1]``.
    """
    if active_cols is not None:
        effective_unitary = effective_unitary[:, active_cols]
        target_unitary = (
            target_unitary[:, active_cols_target] if active_cols_target is not None else target_unitary[:, active_cols]
        )

    if baseline_outputs is not None:
        effective_unitary = effective_unitary[baseline_outputs]

    n = effective_unitary.shape[1]
    overlap = torch.trace(target_unitary.conj().T @ effective_unitary)
    fidelity = overlap.abs() ** 2 / (n * n)
    return 1.0 - fidelity


@dataclass
class OptimizationResult:
    """Result of an :func:`optimize_unitary_subcircuit_parameters` run.

    Attributes:
        phase_shifter_params: Best ``(num_modes_opt, num_modes_opt)`` parameter
            grid (mod 2pi), where ``num_modes_opt`` is ``target_dim`` when
            ``movement_mask`` is ``None`` or ``movement_mask.shape[0]``
            otherwise.  When ``exclude_edge_phase_shifters`` is ``True``, the
            two excluded corner cells are frozen at their initial values.
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
    baseline: bool = False,
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
        baseline: If ``True``, restrict comparison to the first
            ``target_dim`` output rows (baseline mode).
        output_rows: Explicit list of output rows to compare; overrides the
            ``baseline`` default.
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
    target_dim = target_unitary.shape[0]

    if movement_mask is not None:
        if not isinstance(movement_mask, torch.Tensor):
            movement_mask = torch.tensor(movement_mask, dtype=torch.int64)
        else:
            movement_mask = movement_mask.to(dtype=torch.int64)
        num_modes_opt = movement_mask.shape[0]
    else:
        num_modes_opt = target_dim

    param_count = num_modes_opt**2 - 2 if exclude_edge_phase_shifters else num_modes_opt**2

    default_output_rows = list(range(target_dim)) if baseline else None
    compared_rows = output_rows if output_rows is not None else default_output_rows

    # Draw the same random sequence as a flat, corner-excluded vector always would (so
    # initial values are identical regardless of exclude_edge_phase_shifters), then scatter
    # it into a native (num_modes_opt, num_modes_opt) grid once. The parameter is trained
    # in this grid shape directly -- no per-iteration reshape is needed, since the unitary
    # builders already accept a 2D grid unchanged and the excluded corners (when present)
    # are permanently masked out below rather than physically absent from the tensor.
    init_flat = TWO_PI * torch.rand(param_count, dtype=torch.float64)
    init_flat = torch.remainder(init_flat, TWO_PI)
    phase_shifter_params = reshape_flattened_params_to_grid(
        init_flat, num_modes_opt, exclude_edge_phase_shifters=exclude_edge_phase_shifters
    ).detach()
    phase_shifter_params.requires_grad_(True)

    # Permanent gradient mask freezing the two excluded corners (row 0 / last row, last
    # column) so they never move, mirroring the routing grad-masking mechanism below.
    # Built once since it does not change across iterations.
    corner_grad_mask: torch.Tensor | None = None
    if exclude_edge_phase_shifters:
        corner_grad_mask = torch.ones((num_modes_opt, num_modes_opt), dtype=torch.float32)
        corner_grad_mask[0, -1] = 0.0
        corner_grad_mask[num_modes_opt - 1, -1] = 0.0

    # The routing constraints depend only on the movement mask and geometry, not on the
    # phases, so precompute them once here and re-apply the cheap phase-dependent part
    # (a gather + a few torch.where) each iteration.
    routing_transform = (
        None
        if movement_mask is None
        else precompute_routing_transform(
            num_modes_opt,
            movement_mask,
            phase_shifter_params.shape[1],
            optimize_routing_parameters=optimize_routing_parameters,
        )
    )

    optimizer = torch.optim.Adam([phase_shifter_params], lr=lr)
    scheduler = torch.optim.lr_scheduler.ReduceLROnPlateau(
        optimizer,
        mode="min",
        factor=0.5,
        patience=50,
        min_lr=1e-7,
    )

    loop_loss = float("inf")
    index = 0
    best_loss = float("inf")
    best_params = phase_shifter_params.detach().clone()
    patience_ref_loss = float("inf")
    no_improve_steps = 0

    while True:
        # Evaluate the current parameters (initial state, then the result of each
        # step) so the returned best is always over states that were measured.
        ps_for_build = phase_shifter_params
        grad_mask = corner_grad_mask

        if routing_transform is not None:
            effective_params, movement_grad_mask = apply_routing_transform(phase_shifter_params, routing_transform)
            ps_for_build = effective_params
            grad_mask = movement_grad_mask if grad_mask is None else grad_mask * movement_grad_mask.to(grad_mask.dtype)

        if active_cols is not None:
            u_model = build_unitary_selected_columns_from_components(
                num_modes_opt,
                beam_splitter_reflectivities,
                ps_for_build,
                column_indices=active_cols,
                exclude_edge_phase_shifters=exclude_edge_phase_shifters,
            )
            target_cols = active_cols_target if active_cols_target is not None else active_cols
            target_unitary_for_loss = target_unitary[:, target_cols]
            loss = fidelity_loss(
                effective_unitary=u_model,
                target_unitary=target_unitary_for_loss,
                active_cols=None,
                active_cols_target=None,
                baseline_outputs=compared_rows,
            )
        else:
            # The full unitary is the selected builder with every column.
            u_model = build_unitary_selected_columns_from_components(
                num_modes_opt,
                beam_splitter_reflectivities,
                ps_for_build,
                column_indices=list(range(num_modes_opt)),
                exclude_edge_phase_shifters=exclude_edge_phase_shifters,
            )
            loss = fidelity_loss(
                effective_unitary=u_model,
                target_unitary=target_unitary,
                active_cols=None,
                active_cols_target=None,
                baseline_outputs=compared_rows,
            )

        loop_loss = loss.item()

        if loop_loss < best_loss:
            best_loss = loop_loss
            best_params = phase_shifter_params.detach().clone()

        if loop_loss < patience_ref_loss - min_improvement:
            patience_ref_loss = loop_loss
            no_improve_steps = 0
        else:
            no_improve_steps += 1

        if verbose and index % 100 == 0:
            logger.info("Iteration %d: loss=%.6e", index, loop_loss)

        # Stop before taking a step whose result would not be evaluated: on
        # convergence, once the exact step budget is spent, or on early stop.
        if loop_loss <= threshold or index >= max_iterations:
            break
        if early_stop_patience > 0 and no_improve_steps >= early_stop_patience:
            break

        optimizer.zero_grad()
        loss.backward()

        if grad_mask is not None and phase_shifter_params.grad is not None:
            phase_shifter_params.grad.mul_(grad_mask.to(phase_shifter_params.grad.dtype))

        optimizer.step()
        scheduler.step(loop_loss)
        index += 1

    return OptimizationResult(
        phase_shifter_params=torch.remainder(best_params, TWO_PI),
        best_loss=best_loss,
    )
