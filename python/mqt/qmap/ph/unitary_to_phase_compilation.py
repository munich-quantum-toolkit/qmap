"""Unitary-to-phase compilation via gradient-based optimisation."""

from __future__ import annotations

from typing import Any

import torch

from .routing_to_phases import get_effective_params_and_mask, reshape_flattened_params_to_grid

TWO_PI = 2 * torch.pi


def get_haar_random_unitary(
    num_modes: int,
    generator: torch.Generator | None = None,
    dtype: torch.dtype = torch.complex128,
) -> torch.Tensor:
    """Generate a Haar-random unitary matrix.

    Uses the QR decomposition method: draw a complex Gaussian matrix, QR-
    decompose it, and correct the phases of the diagonal of R to ensure
    uniformity over the Haar measure.

    Args:
        num_modes: Dimension of the unitary matrix.
        generator: Optional :class:`torch.Generator` for reproducible results.
        dtype: Complex dtype of the output tensor.

    Returns:
        Complex tensor of shape ``(num_modes, num_modes)`` representing a
        Haar-random unitary.
    """
    Z = torch.randn(num_modes, num_modes, generator=generator, dtype=dtype)
    Q, R = torch.linalg.qr(Z)
    r_diag = torch.diagonal(R)
    lambda_diag = r_diag / torch.abs(r_diag)
    return Q * lambda_diag.unsqueeze(0)


def unitary_individual_phase_shifter(
    num_modes: int,
    mode: int,
    phase: torch.Tensor,
    transmission: torch.Tensor,
) -> torch.Tensor:
    """Build a diagonal unitary for a single phase shifter with optional loss.

    Args:
        num_modes: Total number of spatial modes.
        mode: Index of the mode carrying the phase shifter.
        phase: Phase value in radians.
        transmission: Amplitude transmission factor (1.0 for lossless).

    Returns:
        Complex tensor of shape ``(num_modes, num_modes)`` representing the
        diagonal phase-shifter unitary.
    """
    D = torch.eye(num_modes, dtype=torch.complex128)
    D[mode, mode] = torch.exp(1j * phase) * transmission
    return D


def unitary_individual_beam_splitter(
    num_modes: int,
    mode: int,
    reflectivity: float | torch.Tensor,
) -> torch.Tensor:
    """Build a 2×2 beam-splitter unitary embedded in the full mode space.

    The beam splitter couples ``mode`` and ``mode + 1`` with the standard
    symmetric convention: diagonal elements are ``sqrt(r)`` and
    off-diagonal elements are ``i * sqrt(1 - r)``.

    Args:
        num_modes: Total number of spatial modes.
        mode: Index of the top mode (couples ``mode`` and ``mode + 1``).
        reflectivity: Power reflectivity in ``[0, 1]``.

    Returns:
        Complex tensor of shape ``(num_modes, num_modes)`` representing the
        beam-splitter unitary.
    """
    U = torch.eye(num_modes, dtype=torch.complex128)
    r = torch.as_tensor(reflectivity, dtype=torch.float64, device=U.device)
    U[mode, mode] = torch.sqrt(r)
    U[mode, mode + 1] = 1j * torch.sqrt(1 - r)
    U[mode + 1, mode] = 1j * torch.sqrt(1 - r)
    U[mode + 1, mode + 1] = torch.sqrt(r)
    return U


def build_unitary_from_components(
    num_modes: int,
    beam_splitter_params: torch.Tensor,
    phase_shifter_params: torch.Tensor,
    phase_shifter_transmissions: torch.Tensor,
    exclude_edge_phase_shifters: bool = False,
    layer_range: tuple[int, int] | None = None,
) -> torch.Tensor:
    """Build the full-chip unitary matrix from physical component parameters.

    Constructs the unitary by multiplying individual beam-splitter and
    phase-shifter unitaries layer by layer over the specified range of
    physical layers.

    Args:
        num_modes: Number of spatial modes on the chip.
        beam_splitter_params: 1D tensor of reflectivities ordered as produced
            by :func:`graph.generate_beam_splitter_matrix`.
        phase_shifter_params: Phase-shifter parameter array.  Accepted shapes
            are ``(N, N)``, ``(N**2,)``, or ``(N**2 - 2,)`` (corner-excluded).
        phase_shifter_transmissions: Transmission amplitudes with the same
            shape convention as ``phase_shifter_params``.
        exclude_edge_phase_shifters: If ``True``, the top-right and bottom-
            right corner phase shifters are omitted.
        layer_range: Optional ``(start, end)`` tuple selecting a subset of
            physical layers.  Defaults to all ``num_modes`` layers.

    Returns:
        Complex tensor of shape ``(num_modes, num_modes)`` representing the
        chip unitary.
    """
    N = num_modes
    N2 = N * N

    def to_grid(tensor: torch.Tensor, fill_value: float = 0.0) -> torch.Tensor:
        if tensor.shape == (N, N):
            return tensor
        flat = tensor.flatten()
        if flat.numel() == N2:
            return flat.reshape(N, N)
        if flat.numel() == N2 - 2:
            grid = torch.zeros((N, N), dtype=tensor.dtype, device=tensor.device)
            if fill_value != 0.0:
                grid.fill_(fill_value)
            mask = torch.ones((N, N), dtype=torch.bool, device=tensor.device)
            mask[0, -1] = False
            mask[N - 1, -1] = False
            grid[mask] = flat
            return grid
        raise ValueError(f"Invalid parameter size: {flat.numel()}. Expected {N2}.")

    ps_grid = to_grid(phase_shifter_params, fill_value=0.0)
    t_grid = to_grid(phase_shifter_transmissions, fill_value=1.0)

    start_layer = 0 if layer_range is None else layer_range[0]
    end_layer = N if layer_range is None else layer_range[1]

    bs_idx = 0
    for layer in range(start_layer):
        mzis_in_layer = N // 2 if layer % 2 == 0 else N // 2 - 1
        bs_idx += mzis_in_layer * 2

    U = torch.eye(num_modes, dtype=torch.complex128)

    for layer in range(start_layer, end_layer):
        if layer % 2 == 0:
            for i in range(0, num_modes - 1, 2):
                theta_in = beam_splitter_params[bs_idx]
                theta_out = beam_splitter_params[bs_idx + 1]
                phi1, t1 = ps_grid[i, layer], t_grid[i, layer]
                phi2, t2 = ps_grid[i + 1, layer], t_grid[i + 1, layer]
                U = (
                    unitary_individual_beam_splitter(num_modes, i, theta_out)
                    @ unitary_individual_phase_shifter(num_modes, i + 1, phi2, t2)
                    @ unitary_individual_phase_shifter(num_modes, i, phi1, t1)
                    @ unitary_individual_beam_splitter(num_modes, i, theta_in)
                    @ U
                )
                bs_idx += 2
        else:
            is_last_layer = exclude_edge_phase_shifters and layer == N - 1

            if not is_last_layer:
                phi, t = ps_grid[0, layer], t_grid[0, layer]
                U = unitary_individual_phase_shifter(num_modes, 0, phi, t) @ U

            for j in range(1, num_modes - 1, 2):
                theta_in = beam_splitter_params[bs_idx]
                theta_out = beam_splitter_params[bs_idx + 1]
                phi1, t1 = ps_grid[j, layer], t_grid[j, layer]
                phi2, t2 = ps_grid[j + 1, layer], t_grid[j + 1, layer]
                U = (
                    unitary_individual_beam_splitter(num_modes, j, theta_out)
                    @ unitary_individual_phase_shifter(num_modes, j + 1, phi2, t2)
                    @ unitary_individual_phase_shifter(num_modes, j, phi1, t1)
                    @ unitary_individual_beam_splitter(num_modes, j, theta_in)
                    @ U
                )
                bs_idx += 2

            if not is_last_layer:
                phi, t = ps_grid[num_modes - 1, layer], t_grid[num_modes - 1, layer]
                U = unitary_individual_phase_shifter(num_modes, num_modes - 1, phi, t) @ U

    return U


def build_unitary_selected_columns_from_components(
    num_modes: int,
    beam_splitter_params: torch.Tensor,
    phase_shifter_params: torch.Tensor,
    phase_shifter_transmissions: torch.Tensor,
    column_indices: list[int] | torch.Tensor,
    exclude_edge_phase_shifters: bool = False,
    layer_range: tuple[int, int] | None = None,
) -> torch.Tensor:
    """Build selected columns of the chip unitary without constructing the full matrix.

    Mathematically equivalent to :func:`build_unitary_from_components`
    followed by column slicing, but faster when
    ``len(column_indices) << num_modes`` because only the selected state
    vectors are propagated.

    Args:
        num_modes: Number of spatial modes on the chip.
        beam_splitter_params: 1D tensor of beam-splitter reflectivities.
        phase_shifter_params: Phase-shifter parameter array (see
            :func:`build_unitary_from_components`).
        phase_shifter_transmissions: Transmission amplitudes (same shape
            convention as ``phase_shifter_params``).
        column_indices: Indices of the columns to compute.
        exclude_edge_phase_shifters: If ``True``, corner phase shifters are
            omitted.
        layer_range: Optional ``(start, end)`` tuple for a layer subset.

    Returns:
        Complex tensor of shape ``(num_modes, len(column_indices))``
        containing the selected columns of the full chip unitary.
    """
    N = num_modes
    N2 = N * N

    if isinstance(column_indices, torch.Tensor):
        col_idx = column_indices.to(dtype=torch.long)
    else:
        col_idx = torch.tensor(column_indices, dtype=torch.long)

    def to_grid(tensor: torch.Tensor, fill_value: float = 0.0) -> torch.Tensor:
        if tensor.shape == (N, N):
            return tensor
        flat = tensor.flatten()
        if flat.numel() == N2:
            return flat.reshape(N, N)
        if flat.numel() == N2 - 2:
            grid = torch.zeros((N, N), dtype=tensor.dtype, device=tensor.device)
            if fill_value != 0.0:
                grid.fill_(fill_value)
            mask = torch.ones((N, N), dtype=torch.bool, device=tensor.device)
            mask[0, -1] = False
            mask[N - 1, -1] = False
            grid[mask] = flat
            return grid
        raise ValueError(f"Invalid parameter size: {flat.numel()}. Expected {N2}.")

    ps_grid = to_grid(phase_shifter_params, fill_value=0.0)
    t_grid = to_grid(phase_shifter_transmissions, fill_value=1.0)

    start_layer = 0 if layer_range is None else layer_range[0]
    end_layer = N if layer_range is None else layer_range[1]

    bs_idx = 0
    for layer in range(start_layer):
        mzis_in_layer = N // 2 if layer % 2 == 0 else N // 2 - 1
        bs_idx += mzis_in_layer * 2

    U = torch.eye(num_modes, dtype=torch.complex128)[:, col_idx]

    def apply_ps_left(U_local: torch.Tensor, mode: int, phi: torch.Tensor, t: torch.Tensor) -> torch.Tensor:
        phase = torch.exp(1j * phi) * t
        scales = torch.ones((num_modes, 1), dtype=torch.complex128, device=U_local.device)
        scales[mode, 0] = phase
        return scales * U_local

    def apply_bs_left(U_local: torch.Tensor, mode: int, reflectivity: torch.Tensor) -> torch.Tensor:
        r = torch.as_tensor(reflectivity, dtype=torch.float64, device=U_local.device)
        a = torch.sqrt(r)
        b = 1j * torch.sqrt(1 - r)
        row_top = U_local[mode : mode + 1, :]
        row_bot = U_local[mode + 1 : mode + 2, :]
        new_top = a * row_top + b * row_bot
        new_bot = b * row_top + a * row_bot
        prefix = U_local[:mode, :]
        suffix = U_local[mode + 2 :, :]
        return torch.cat((prefix, new_top, new_bot, suffix), dim=0)

    for layer in range(start_layer, end_layer):
        if layer % 2 == 0:
            for i in range(0, num_modes - 1, 2):
                theta_in = beam_splitter_params[bs_idx]
                theta_out = beam_splitter_params[bs_idx + 1]
                phi1, t1 = ps_grid[i, layer], t_grid[i, layer]
                phi2, t2 = ps_grid[i + 1, layer], t_grid[i + 1, layer]
                U = apply_bs_left(U, i, theta_in)
                U = apply_ps_left(U, i, phi1, t1)
                U = apply_ps_left(U, i + 1, phi2, t2)
                U = apply_bs_left(U, i, theta_out)
                bs_idx += 2
        else:
            is_last_layer = exclude_edge_phase_shifters and layer == N - 1

            if not is_last_layer:
                phi, t = ps_grid[0, layer], t_grid[0, layer]
                U = apply_ps_left(U, 0, phi, t)

            for j in range(1, num_modes - 1, 2):
                theta_in = beam_splitter_params[bs_idx]
                theta_out = beam_splitter_params[bs_idx + 1]
                phi1, t1 = ps_grid[j, layer], t_grid[j, layer]
                phi2, t2 = ps_grid[j + 1, layer], t_grid[j + 1, layer]
                U = apply_bs_left(U, j, theta_in)
                U = apply_ps_left(U, j, phi1, t1)
                U = apply_ps_left(U, j + 1, phi2, t2)
                U = apply_bs_left(U, j, theta_out)
                bs_idx += 2

            if not is_last_layer:
                phi, t = ps_grid[num_modes - 1, layer], t_grid[num_modes - 1, layer]
                U = apply_ps_left(U, num_modes - 1, phi, t)

    return U


def fidelity_loss(
    U_effective: torch.Tensor,
    U_target: torch.Tensor,
    active_cols: list[int] | None = None,
    active_cols_target: list[int] | None = None,
    baseline_outputs: list[int] | None = None,
) -> torch.Tensor:
    """Compute the normalised fidelity loss between two unitaries.

    Loss is defined as ``1 - |Tr(U_tgt† @ U_eff)|² / N²``, where *N* is
    the number of compared columns.  A loss of 0.0 indicates a perfect match
    (up to global phase).

    Args:
        U_effective: Unitary produced by the chip, shape
            ``(num_modes, num_modes)`` or ``(num_modes, len(active_cols))``.
        U_target: Target unitary with compatible shape.
        active_cols: Column indices to select from ``U_effective`` before
            comparison.  When ``None``, all columns are used.
        active_cols_target: Column indices to select from ``U_target``.
            Defaults to ``active_cols`` when ``None``.
        baseline_outputs: Row indices to select from ``U_effective`` after
            column selection.  Used to restrict comparison to the computation
            zone rows.

    Returns:
        Scalar tensor holding the fidelity loss in ``[0, 1]``.
    """
    if active_cols is not None:
        U_effective = U_effective[:, active_cols]
        U_target = (
            U_target[:, active_cols_target]
            if active_cols_target is not None
            else U_target[:, active_cols]
        )

    if baseline_outputs is not None:
        U_effective = U_effective[baseline_outputs]

    N = U_effective.shape[1]
    overlap = torch.trace(U_target.conj().T @ U_effective)
    fidelity = overlap.abs() ** 2 / (N * N)
    return 1.0 - fidelity


def get_computation_zone(
    all_bs_values: torch.Tensor,
    target_modes: list[int],
    chip_size: int,
) -> tuple[torch.Tensor, torch.Tensor]:
    """Extract beam-splitter reflectivities belonging to the computation zone.

    The computation zone occupies the last ``len(target_modes)`` MZI layers
    of the chip and is restricted to MZIs whose mode pairs both lie in
    ``target_modes``.

    Args:
        all_bs_values: 1D tensor of all chip beam-splitter reflectivities.
        target_modes: Spatial mode indices of the computation zone.
        chip_size: Total number of spatial modes on the chip.

    Returns:
        A tuple ``(values_tensor, indices_tensor)`` where *values_tensor*
        contains the extracted reflectivities and *indices_tensor* contains
        their positions within ``all_bs_values``.
    """
    if not isinstance(all_bs_values, torch.Tensor):
        all_bs_values = torch.tensor(all_bs_values, dtype=torch.float64)

    target_dim = len(target_modes)
    mzi_layers = chip_size
    start_mzi_layer = mzi_layers - target_dim

    computation_bs_indices: list[int] = []
    current_bs_idx = 0

    for layer in range(mzi_layers):
        is_full_layer = layer % 2 == 0
        mzi_count = chip_size // 2 if is_full_layer else chip_size // 2 - 1

        for mzi in range(mzi_count):
            top_mode = mzi * 2 if is_full_layer else mzi * 2 + 1
            bottom_mode = top_mode + 1

            if layer >= start_mzi_layer and top_mode in target_modes and bottom_mode in target_modes:
                computation_bs_indices.append(current_bs_idx)
                computation_bs_indices.append(current_bs_idx + 1)

            current_bs_idx += 2

    indices_tensor = torch.tensor(computation_bs_indices, dtype=torch.long)
    return all_bs_values[indices_tensor], indices_tensor


def optimize_unitary_subcircuit_parameters(
    U_target: torch.Tensor,
    beam_splitter_reflectivities: torch.Tensor | None = None,
    phase_shifter_transmissions: torch.Tensor | None = None,
    movement_mask: torch.Tensor | None = None,
    lr: float = 0.05,
    threshold: float = 1e-5,
    active_cols: list[int] | None = None,
    active_cols_target: list[int] | None = None,
    verbose: bool = False,
    num_restarts: int = 3,
    max_iterations: int = 10000,
    restart_perturbation: float = 0.5,
    baseline: bool = False,
    output_rows: list[int] | None = None,
    exclude_edge_phase_shifters: bool = False,
    optimize_routing_parameters: bool = True,
    early_stop_patience: int = 50,
    min_improvement: float = 1e-4,
) -> dict[str, Any]:
    """Optimise phase-shifter parameters to approximate a target unitary.

    Runs an Adam optimiser with optional learning-rate scheduling and warm
    restarts.  Each restart perturbs the best known parameters to escape
    local minima.

    Args:
        U_target: Target unitary tensor of shape ``(target_dim, target_dim)``.
        beam_splitter_reflectivities: 1D tensor of chip beam-splitter
            reflectivities.  Treated as fixed (no gradient).
        phase_shifter_transmissions: Transmission amplitudes for the phase
            shifters.  Defaults to all-ones (lossless).
        movement_mask: Integer tensor of shape ``(num_modes, num_modes)``
            encoding routing constraints.  When ``None``, the full chip is
            treated as a computation zone.
        lr: Initial Adam learning rate.
        threshold: Loss value below which optimisation is considered
            successful and terminates early.
        active_cols: Physical input column indices to inject photons into.
        active_cols_target: Column indices within the computation zone
            corresponding to ``active_cols``.
        verbose: If ``True``, print progress every 100 iterations.
        num_restarts: Number of optimisation restarts (must be ≥ 1).
        max_iterations: Maximum number of gradient steps per restart.
        restart_perturbation: Standard deviation of Gaussian noise added when
            warm-restarting from the best known parameters.
        baseline: If ``True``, restrict comparison to the first
            ``target_dim`` output rows (baseline mode).
        output_rows: Explicit list of output rows to compare; overrides the
            ``baseline`` default.
        exclude_edge_phase_shifters: If ``True``, the two corner phase
            shifters are excluded from the parameter set.
        optimize_routing_parameters: If ``True``, routing cells contribute a
            single trainable degree of freedom.
        early_stop_patience: Number of consecutive steps without improvement
            before a restart is terminated early.
        min_improvement: Minimum absolute loss decrease required to reset the
            patience counter.

    Returns:
        A dictionary with the following keys:

        * ``"phase_shifter_params"`` — best flat parameter tensor (mod 2π).
        * ``"beam_splitter_params"`` — ``beam_splitter_reflectivities``.
        * ``"phase_shifter_transmissions"`` — transmission tensor used.
        * ``"losses"`` — list of per-step loss values across all restarts.
        * ``"lrs"`` — learning-rate history of the best restart.
        * ``"iterations"`` — number of gradient steps in the best restart.
        * ``"num_restarts"`` — number of restarts actually executed.

    Raises:
        ValueError: If ``num_restarts`` is less than 1.
    """
    target_dim = U_target.shape[0]

    if movement_mask is not None:
        if not isinstance(movement_mask, torch.Tensor):
            movement_mask = torch.tensor(movement_mask, dtype=torch.int64)
        else:
            movement_mask = movement_mask.to(dtype=torch.int64)
        num_modes_opt = movement_mask.shape[0]
    else:
        num_modes_opt = target_dim

    param_count = num_modes_opt**2 - 2 if exclude_edge_phase_shifters else num_modes_opt**2

    if phase_shifter_transmissions is None:
        phase_shifter_transmissions = torch.ones(param_count, dtype=torch.float64)
    elif not isinstance(phase_shifter_transmissions, torch.Tensor):
        phase_shifter_transmissions = torch.tensor(phase_shifter_transmissions, dtype=torch.float64)
    else:
        phase_shifter_transmissions = phase_shifter_transmissions.to(dtype=torch.float64)

    default_output_rows = list(range(target_dim)) if baseline else None
    compared_rows = output_rows if output_rows is not None else default_output_rows

    def flatten_grid_with_corner_policy(
        grid_2d: torch.Tensor,
        n_modes: int,
        exclude_corners: bool,
    ) -> torch.Tensor:
        if not exclude_corners:
            return grid_2d.flatten()
        mask = torch.ones((n_modes, n_modes), dtype=torch.bool, device=grid_2d.device)
        mask[0, -1] = False
        mask[n_modes - 1, -1] = False
        return grid_2d[mask]

    if num_restarts < 1:
        raise ValueError("num_restarts must be an integer >= 1")

    best_loss = float("inf")
    best_params: torch.Tensor | None = None
    best_lrs: list[float] = []
    best_iterations = 0
    restarts_used = 0
    losses: list[float] = []

    for restart_idx in range(1, num_restarts + 1):
        restarts_used = restart_idx

        if restart_idx == 1 or best_params is None:
            phase_shifter_params = TWO_PI * torch.rand(param_count, dtype=torch.float64)
        else:
            phase_shifter_params = best_params.detach().clone()
            if restart_perturbation > 0:
                if movement_mask is not None:
                    phase_grid = reshape_flattened_params_to_grid(
                        phase_shifter_params,
                        num_modes_opt,
                        exclude_edge_phase_shifters=exclude_edge_phase_shifters,
                    )
                    _, restart_grad_mask_2d, _ = get_effective_params_and_mask(
                        num_modes_opt,
                        movement_mask,
                        phase_grid,
                        optimize_routing_parameters=optimize_routing_parameters,
                    )
                    restart_grad_mask_flat = flatten_grid_with_corner_policy(
                        restart_grad_mask_2d,
                        num_modes_opt,
                        exclude_edge_phase_shifters,
                    ).to(dtype=torch.float64)
                else:
                    restart_grad_mask_flat = torch.ones_like(phase_shifter_params, dtype=torch.float64)

                noise = restart_perturbation * torch.randn_like(phase_shifter_params)
                phase_shifter_params = phase_shifter_params + noise * restart_grad_mask_flat

        phase_shifter_params = torch.remainder(phase_shifter_params, TWO_PI)
        phase_shifter_params.requires_grad_(True)

        optimizer = torch.optim.Adam([phase_shifter_params], lr=lr)
        scheduler = torch.optim.lr_scheduler.ReduceLROnPlateau(
            optimizer,
            mode="min",
            factor=0.5,
            patience=50,
            min_lr=1e-7,
        )

        lrs: list[float] = []
        loop_loss = float("inf")
        index = 0
        best_restart_loss = float("inf")
        no_improve_steps = 0

        while loop_loss > threshold and index < max_iterations:
            ps_for_build = phase_shifter_params
            grad_mask_flat: torch.Tensor | None = None

            if movement_mask is not None:
                phase_grid = reshape_flattened_params_to_grid(
                    phase_shifter_params,
                    num_modes_opt,
                    exclude_edge_phase_shifters=exclude_edge_phase_shifters,
                )
                effective_params, grad_mask_2d, _ = get_effective_params_and_mask(
                    num_modes_opt,
                    movement_mask,
                    phase_grid,
                    optimize_routing_parameters=optimize_routing_parameters,
                )
                ps_for_build = effective_params
                grad_mask_flat = flatten_grid_with_corner_policy(
                    grad_mask_2d,
                    num_modes_opt,
                    exclude_edge_phase_shifters,
                )

            if active_cols is not None:
                U_model = build_unitary_selected_columns_from_components(
                    num_modes_opt,
                    beam_splitter_reflectivities,
                    ps_for_build,
                    phase_shifter_transmissions,
                    column_indices=active_cols,
                    exclude_edge_phase_shifters=exclude_edge_phase_shifters,
                )
                target_cols = active_cols_target if active_cols_target is not None else active_cols
                U_target_for_loss = U_target[:, target_cols]
                loss = fidelity_loss(
                    U_effective=U_model,
                    U_target=U_target_for_loss,
                    active_cols=None,
                    active_cols_target=None,
                    baseline_outputs=compared_rows,
                )
            else:
                U_model = build_unitary_from_components(
                    num_modes_opt,
                    beam_splitter_reflectivities,
                    ps_for_build,
                    phase_shifter_transmissions,
                    exclude_edge_phase_shifters=exclude_edge_phase_shifters,
                )
                loss = fidelity_loss(
                    U_effective=U_model,
                    U_target=U_target,
                    active_cols=None,
                    active_cols_target=None,
                    baseline_outputs=compared_rows,
                )

            loop_loss = loss.item()
            losses.append(loop_loss)

            if loop_loss < best_restart_loss - min_improvement:
                best_restart_loss = loop_loss
                no_improve_steps = 0
            else:
                no_improve_steps += 1

            if index % 100 == 0 and verbose:
                print(
                    f"Restart {restart_idx}/{num_restarts}, Iter {index}: "
                    f"Loss = {loop_loss:.6f}, LR = {optimizer.param_groups[0]['lr']:.6f}"
                )

            lrs.append(optimizer.param_groups[0]["lr"])

            optimizer.zero_grad()
            loss.backward()

            if grad_mask_flat is not None and phase_shifter_params.grad is not None:
                phase_shifter_params.grad.mul_(grad_mask_flat.to(phase_shifter_params.grad.dtype))

            optimizer.step()
            scheduler.step(loop_loss)
            index += 1

            if early_stop_patience > 0 and no_improve_steps >= early_stop_patience:
                if verbose:
                    print(
                        f"Restart {restart_idx}/{num_restarts} early-stopped at iter {index} "
                        f"(no improvement > {min_improvement:g} for {early_stop_patience} steps)."
                    )
                break

        if verbose:
            print(
                f"Restart {restart_idx}/{num_restarts} finished after {index} iterations "
                f"with loss {loop_loss:.6f}"
            )

        if loop_loss < best_loss:
            best_loss = loop_loss
            best_params = phase_shifter_params.detach().clone()
            best_lrs = lrs
            best_iterations = index

        if best_loss <= threshold:
            break

    return {
        "phase_shifter_params": torch.remainder(best_params, TWO_PI),
        "beam_splitter_params": beam_splitter_reflectivities,
        "phase_shifter_transmissions": phase_shifter_transmissions,
        "losses": losses,
        "lrs": best_lrs,
        "iterations": best_iterations,
        "num_restarts": restarts_used,
    }
