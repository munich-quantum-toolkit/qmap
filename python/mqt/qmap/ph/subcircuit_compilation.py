# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""Photonic MZI-mesh subcircuit compiler."""

from __future__ import annotations

import math
import time
from dataclasses import dataclass
from typing import Any

import torch

from .graph import construct_graph
from .routing import (
    get_best_route,
    infer_input_computation_and_output_ports,
    route_to_movement_mask,
)
from .routing_to_phases import get_effective_params_and_mask
from .unitary_to_phase_compilation import optimize_unitary_subcircuit_parameters


@dataclass
class OptimizationConfig:
    """Hyperparameters for the phase-shifter optimization.

    Attributes:
        lr: Initial Adam learning rate.
        threshold: Fidelity-loss value below which optimization terminates
            early.
        max_iterations: Maximum gradient steps.
        exclude_edge_phase_shifters: If ``True``, the two edge phase
            shifters are excluded from the parameter set.
        optimize_routing_parameters: If ``True``, routing MZI cells
            contribute a single trainable degree of freedom.
    """

    lr: float = 0.05
    threshold: float = 1e-6
    max_iterations: int = 10000
    exclude_edge_phase_shifters: bool = False
    optimize_routing_parameters: bool = True


@dataclass
class CompilationResult:
    """Output of a single :func:`compile_subcircuit` call.

    This is the end-user result of compiling a target unitary onto a physical
    chip. It carries everything needed to drive the hardware: the phase-shifter
    values to program, and the input/output ports the photons enter and leave on.

    Attributes:
        phases: Flat list of ``chip_dim ** 2`` phase-shifter angles in
            column-major (layer-by-layer) order - every mode phase of layer 0,
            then every mode phase of layer 1, and so on. The value for spatial
            mode ``r`` in MZI layer ``c`` is at index ``c * chip_dim + r``.
            These are the values to program onto the chip.
        input_ports: Physical mode indices into which photons are injected (the
            lower mode of each dual-rail pair), length ``target_dim // 2``.
        output_ports: Physical mode indices of the computation zone where the
            output photons are measured, length ``target_dim``.
        loss: Final fidelity loss of the proposed compiler optimization.
        compute_time: Wall-clock seconds for the proposed compiler (routing
            + optimization).
    """

    phases: list[float]
    input_ports: list[int]
    output_ports: list[int]
    loss: float
    compute_time: float


def _validate_compile_inputs(
    beam_splitter_reflectivities: list[float],
    input_transmissions: list[float],
    output_transmissions: list[float],
    target_unitary: torch.Tensor,
) -> None:
    """Reject malformed public inputs before any routing work begins.

    Guards the public compiler boundary against bad characterization data and
    targets. Without these checks the pipeline silently accepts a short
    transmission vector, turns a negative transmission into a ``NaN`` edge cost,
    and raises an incidental ``IndexError`` for a short beam-splitter vector deep
    in graph construction. ``chip_dim`` is defined as ``len(input_transmissions)``
    and everything else is validated against it.

    Args:
        beam_splitter_reflectivities: Flat, MZI-ordered reflectivity list.
        input_transmissions: Per-mode input transmission coefficients (defines
            ``chip_dim``).
        output_transmissions: Per-mode output transmission coefficients.
        target_unitary: Target unitary tensor.

    Raises:
        ValueError: If any input has the wrong shape, a non-finite value, or a
            coefficient outside ``[0, 1]``.
    """
    if target_unitary.ndim != 2 or target_unitary.shape[0] != target_unitary.shape[1]:
        msg = f"target_unitary must be a square 2D matrix, got shape {tuple(target_unitary.shape)}."
        raise ValueError(msg)
    target_dim = int(target_unitary.shape[0])
    if target_dim == 0 or target_dim % 2 != 0:
        msg = f"target_unitary dimension must be a positive even number, got {target_dim}."
        raise ValueError(msg)
    if not torch.is_complex(target_unitary):
        msg = f"target_unitary must have a complex dtype, got {target_unitary.dtype}."
        raise ValueError(msg)
    if not bool(torch.isfinite(target_unitary).all()):
        msg = "target_unitary must contain only finite values."
        raise ValueError(msg)

    chip_dim = len(input_transmissions)
    if chip_dim == 0 or chip_dim % 2 != 0:
        msg = f"input_transmissions length (chip_dim) must be a positive even number, got {chip_dim}."
        raise ValueError(msg)
    if target_dim > chip_dim:
        msg = f"target dimension {target_dim} cannot exceed chip dimension {chip_dim}."
        raise ValueError(msg)

    for name, transmissions in (
        ("input_transmissions", input_transmissions),
        ("output_transmissions", output_transmissions),
    ):
        if len(transmissions) != chip_dim:
            msg = f"{name} must have length chip_dim={chip_dim}, got {len(transmissions)}."
            raise ValueError(msg)
        for value in transmissions:
            if not math.isfinite(value) or not 0.0 <= value <= 1.0:
                msg = f"{name} values must be finite and in [0, 1], got {value}."
                raise ValueError(msg)

    # 2 * total_mzis reduces to chip_dim * (chip_dim - 1) for an even-width chip.
    expected_bs = chip_dim * (chip_dim - 1)
    if len(beam_splitter_reflectivities) != expected_bs:
        msg = (
            f"beam_splitter_reflectivities must have length chip_dim*(chip_dim-1)={expected_bs} "
            f"for chip_dim={chip_dim}, got {len(beam_splitter_reflectivities)}."
        )
        raise ValueError(msg)
    for value in beam_splitter_reflectivities:
        if not math.isfinite(value) or not 0.0 <= value <= 1.0:
            msg = f"beam_splitter_reflectivities values must be finite and in [0, 1], got {value}."
            raise ValueError(msg)


def _validate_input_ports(input_ports: list[int], chip_dim: int) -> None:
    """Sanity-check input-port indices before they are stored or simulated.

    A quick guard against the two ways the indices could be malformed: an
    index outside the chip, or the same mode injected twice. The routing
    pipeline never produces either, so this only catches externally supplied
    garbage.

    Args:
        input_ports: Physical mode indices where photons are injected.
        chip_dim: Total number of spatial modes on the chip.

    Raises:
        ValueError: If any port is outside ``[0, chip_dim)`` or is repeated.
    """
    for port in input_ports:
        if not 0 <= port < chip_dim:
            msg = f"Input port {port} is out of range for chip_dim={chip_dim}."
            raise ValueError(msg)
    if len(set(input_ports)) != len(input_ports):
        msg = f"Input ports must be distinct, got {input_ports}."
        raise ValueError(msg)


def _setup_routing(
    beam_splitter_reflectivities: list[float],
    input_transmissions: list[float],
    output_transmissions: list[float],
    target_unitary: torch.Tensor,
    chip_dim: int,
    target_dim: int,
) -> tuple[Any, list[int], list[int], list[int], torch.Tensor]:
    """Find the best photon route and derive port assignments and an adjusted target unitary.

    Args:
        beam_splitter_reflectivities: List of chip beam-splitter
            reflectivities, ordered MZI-by-MZI.
        input_transmissions: Per-mode input transmission coefficients, a list
            of length ``chip_dim``.
        output_transmissions: Per-mode output transmission coefficients, a
            list of length ``chip_dim``.
        target_unitary: Target unitary tensor of shape ``(target_dim, target_dim)``.
        chip_dim: Total number of spatial modes on the chip.
        target_dim: Dimension of the target unitary.

    Returns:
        A tuple of ``(movement_mask, input_ports, output_ports,
        active_cols_computation_zone, target_unitary_opt)`` where ``input_ports``
        and ``output_ports`` are both physical mode-index lists.
    """
    routing_graph = construct_graph(
        chip_dim=chip_dim,
        target_dim=target_dim,
        input_transmission=input_transmissions,
        beam_splitter_reflectivities=beam_splitter_reflectivities,
        output_transmission=output_transmissions,
    )

    best_node_sequence, _ = get_best_route(routing_graph.graph, routing_graph.layers)

    movement_mask = route_to_movement_mask(best_node_sequence, chip_dim=chip_dim, target_dim=target_dim)

    input_ports, output_ports, active_cols_computation_zone = infer_input_computation_and_output_ports(
        best_node_sequence, target_dim
    )
    _validate_input_ports(input_ports, chip_dim)

    # When photons enter on odd columns (mode 0 is not an active input), swap each
    # adjacent pair of target columns (0<->1, 2<->3, ...) so the optimizer sees the
    # correct ordering. Indexing by `i ^ 1` does this without a permutation matmul.
    if 0 not in active_cols_computation_zone:
        target_unitary_opt = target_unitary[:, [i ^ 1 for i in range(target_dim)]]
    else:
        target_unitary_opt = target_unitary

    return (
        movement_mask,
        input_ports,
        output_ports,
        active_cols_computation_zone,
        target_unitary_opt,
    )


def _run_proposed_optimization(
    target_unitary_opt: torch.Tensor,
    beam_splitter_reflectivities: list[float],
    movement_mask: torch.Tensor,
    config: OptimizationConfig,
    chip_dim: int,
    input_ports: list[int],
    active_cols_computation_zone: list[int],
    output_ports: list[int],
) -> tuple[float, torch.Tensor]:
    """Optimize phase-shifter parameters for the proposed routing path.

    Args:
        target_unitary_opt: Target unitary, column-permuted when required to
            match the routed input-column ordering.
        beam_splitter_reflectivities: List of chip beam-splitter reflectivities.
        movement_mask: ``(chip_dim, chip_dim)`` routing-state mask for the chosen route.
        config: Optimization hyperparameters.
        chip_dim: Total number of spatial modes on the chip.
        input_ports: Physical input mode indices photons are injected into.
        active_cols_computation_zone: Computation-zone column indices
            corresponding to ``input_ports``.
        output_ports: Physical output mode indices of the computation zone.

    Returns:
        A tuple of ``(best_loss, phase_shifter_params_including_routing)``, where
        ``best_loss`` is the loss of the returned (best) parameters.
    """
    result = optimize_unitary_subcircuit_parameters(
        target_unitary=target_unitary_opt,
        beam_splitter_reflectivities=torch.as_tensor(beam_splitter_reflectivities, dtype=torch.float64),
        movement_mask=movement_mask,
        lr=config.lr,
        threshold=config.threshold,
        active_cols=input_ports,
        active_cols_target=active_cols_computation_zone,
        output_rows=output_ports,
        max_iterations=config.max_iterations,
        exclude_edge_phase_shifters=config.exclude_edge_phase_shifters,
        optimize_routing_parameters=config.optimize_routing_parameters,
        early_stop_patience=50,
        min_improvement=1e-4,
    )

    losses = result.best_loss
    # phase_shifter_params is already the (chip_dim, chip_dim) grid the optimizer trains natively.
    phase_shifter_params_2d = result.phase_shifter_params.detach()

    params_including_routing, _ = get_effective_params_and_mask(
        chip_dim,
        movement_mask,
        phase_shifter_params_2d,
        optimize_routing_parameters=config.optimize_routing_parameters,
    )

    return losses, params_including_routing


def compile_subcircuit(
    beam_splitter_reflectivities: list[float],
    input_transmissions: list[float],
    output_transmissions: list[float],
    target_unitary: torch.Tensor,
    config: OptimizationConfig | None = None,
) -> CompilationResult:
    """Compile a target unitary onto the chip and return the phases to program.

    ``chip_dim`` is derived from ``len(input_transmissions)`` and
    ``target_dim`` from ``target_unitary.shape[0]``.

    The compiler searches for the optimal photon routing through the chip
    (the path minimizing overall photon loss, independent of the target
    operation) and then optimizes the phase-shifter parameters for that
    placement.

    Args:
        beam_splitter_reflectivities: Flat list of measured chip beam-splitter
            reflectivities ordered MZI-by-MZI as in/out pairs, layer by layer.
        input_transmissions: Per-mode input transmission coefficients, a list
            of length ``chip_dim``. Its length determines ``chip_dim``.
        output_transmissions: Per-mode output transmission coefficients, a
            list of length ``chip_dim``.
        target_unitary: Target unitary tensor of shape ``(target_dim, target_dim)``.
            Its first dimension determines ``target_dim``.
        config: Optimization hyperparameters. Defaults to
            :class:`OptimizationConfig` with all defaults when ``None``.

    Returns:
        A :class:`CompilationResult` containing the phase-shifter matrix to
        program, the input/output ports, the final fidelity loss, and the
        compilation compute time.

    Raises:
        ValueError: If ``target_unitary`` is not a square, even-dimensioned,
            finite complex matrix with ``target_dim <= chip_dim``; if either
            transmission vector does not have exactly ``chip_dim`` finite values
            in ``[0, 1]``; or if ``beam_splitter_reflectivities`` does not have
            exactly ``chip_dim * (chip_dim - 1)`` finite values in ``[0, 1]``.

    Note:
        No hardware simulation is performed, so this step is suitable
        for chips too large to simulate classically.
    """
    if config is None:
        config = OptimizationConfig()

    _validate_compile_inputs(
        beam_splitter_reflectivities,
        input_transmissions,
        output_transmissions,
        target_unitary,
    )

    chip_dim = len(input_transmissions)
    target_dim = int(target_unitary.shape[0])

    proposed_start = time.time()
    (
        movement_mask,
        input_ports,
        output_ports,
        active_cols_computation_zone,
        target_unitary_opt,
    ) = _setup_routing(
        beam_splitter_reflectivities,
        input_transmissions,
        output_transmissions,
        target_unitary,
        chip_dim,
        target_dim,
    )
    losses, phase_shifter_params_including_routing = _run_proposed_optimization(
        target_unitary_opt,
        beam_splitter_reflectivities,
        movement_mask,
        config,
        chip_dim,
        input_ports,
        active_cols_computation_zone,
        output_ports,
    )
    proposed_compute_time = time.time() - proposed_start

    return CompilationResult(
        # Flatten column by column (layer by layer): phase at mode r, layer c -> index c * chip_dim + r.
        phases=phase_shifter_params_including_routing.t().flatten().tolist(),
        input_ports=input_ports,
        output_ports=output_ports,
        loss=losses,
        compute_time=proposed_compute_time,
    )
