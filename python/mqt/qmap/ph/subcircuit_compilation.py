# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""Photonic MZI-mesh subcircuit compiler."""

from __future__ import annotations

import time
from dataclasses import dataclass
from typing import TYPE_CHECKING, Any

import perceval as pcvl
import torch
from perceval import algorithm

from .baseline import get_baseline_active_cols, get_baseline_input_ports
from .graph import construct_graph
from .perceval_simulation import create_mzi_chip, evaluate_chip_performance, simulate_with_loss
from .routing import (
    convert_input_ports,
    convert_output_ports,
    get_best_route,
    get_input_ports_for_computation_zone,
    infer_input_computation_and_output_ports,
    route_to_movement_mask,
)
from .routing_to_phases import get_effective_params_and_mask, reshape_flattened_params_to_grid
from .unitary_to_phase_compilation import optimize_unitary_subcircuit_parameters

if TYPE_CHECKING:
    import numpy as np


@dataclass
class OptimizationConfig:
    """Hyperparameters for the phase-shifter optimisation.

    Attributes:
        lr: Initial Adam learning rate.
        threshold: Fidelity-loss value below which optimisation terminates
            early.
        max_iterations: Maximum gradient steps.
        exclude_edge_phase_shifters: If ``True``, the two corner phase
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
class RunResult:
    """Output of a single :func:`compile_subcircuit` call.

    Attributes:
        performance: Metrics for the proposed compiler (coincidence rate, TVD,
            etc.) as returned by
            :func:`perceval_simulation.evaluate_chip_performance`.
        baseline_performance: Same metrics for the baseline strategy.
        loss: Final fidelity loss of the proposed compiler optimisation.
        baseline_loss: Final fidelity loss of the baseline optimisation.
        compute_time: Wall-clock seconds for the proposed compiler (routing
            + optimisation + simulation).
        baseline_compute_time: Wall-clock seconds for the baseline
            optimisation + simulation.
    """

    performance: dict[str, Any]
    baseline_performance: dict[str, Any]
    loss: float
    baseline_loss: float
    compute_time: float
    baseline_compute_time: float


def _setup_routing(
    beam_splitter_reflectivities: np.ndarray,
    input_transmissions: np.ndarray,
    output_transmissions: np.ndarray,
    target_unitary: torch.Tensor,
    chip_dim: int,
    target_dim: int,
) -> tuple[Any, list[int], list[int], list[int], list[int], torch.Tensor]:
    """Find the best photon route and derive port assignments and an adjusted target unitary.

    Returns:
        A tuple of ``(movement_mask, input_ports, output_ports,
        active_cols_computation_zone, converted_input_ports, target_unitary_opt)``.
    """
    graph, _, layers = construct_graph(
        chip_dim=chip_dim,
        target_dim=target_dim,
        input_transmission=input_transmissions,
        beam_splitter_reflectivities=beam_splitter_reflectivities,
        output_transmission=output_transmissions,
    )

    best_node_sequence, _ = get_best_route(graph, layers)

    movement_mask = route_to_movement_mask(best_node_sequence, chip_dim=chip_dim, target_dim=target_dim)

    input_ports, output_ports, active_cols_computation_zone = infer_input_computation_and_output_ports(
        best_node_sequence, target_dim
    )
    converted_input_ports = convert_input_ports(input_ports, chip_dim)
    convert_output_ports(output_ports, chip_dim)
    input_ports_for_computation_zone = get_input_ports_for_computation_zone(active_cols_computation_zone, target_dim)

    # When photons enter on odd columns, apply a swap permutation to the target
    # so the optimiser sees the correct column ordering.
    if input_ports_for_computation_zone[0] == 0:
        permutation_matrix = torch.zeros((target_dim, target_dim), dtype=torch.complex128)
        for i in range(target_dim):
            if i % 2 == 0:
                permutation_matrix[i, i + 1] = 1
            else:
                permutation_matrix[i, i - 1] = 1
        target_unitary_opt = target_unitary @ permutation_matrix
    else:
        target_unitary_opt = target_unitary

    return (
        movement_mask,
        input_ports,
        output_ports,
        active_cols_computation_zone,
        converted_input_ports,
        target_unitary_opt,
    )


def _run_proposed_optimization(
    target_unitary_opt: torch.Tensor,
    beam_splitter_reflectivities: np.ndarray,
    movement_mask: Any,
    config: OptimizationConfig,
    chip_dim: int,
    input_ports: list[int],
    active_cols_computation_zone: list[int],
    output_ports: list[int],
) -> tuple[float, torch.Tensor]:
    """Optimize phase-shifter parameters for the proposed routing path.

    Returns:
        A tuple of ``(final_loss, phase_shifter_params_including_routing)``.
    """
    result = optimize_unitary_subcircuit_parameters(
        target_unitary=target_unitary_opt,
        beam_splitter_reflectivities=beam_splitter_reflectivities,
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

    losses = result["losses"][-1] if result["losses"] else float("inf")
    phase_shifter_params = result["phase_shifter_params"].detach()

    phase_shifter_params_2d = reshape_flattened_params_to_grid(
        phase_shifter_params,
        chip_dim,
        exclude_edge_phase_shifters=config.exclude_edge_phase_shifters,
    )

    params_including_routing, _, _ = get_effective_params_and_mask(
        chip_dim,
        movement_mask,
        phase_shifter_params_2d,
        optimize_routing_parameters=config.optimize_routing_parameters,
    )

    return losses, params_including_routing


def _run_baseline_optimization(
    target_unitary_embedded: torch.Tensor,
    beam_splitter_reflectivities: torch.Tensor,
    config: OptimizationConfig,
    chip_dim: int,
    baseline_active_cols: list[int],
) -> tuple[float, torch.Tensor]:
    """Optimize phase-shifter parameters for the baseline dual-rail placement.

    Returns:
        A tuple of ``(final_loss, baseline_phase_shifter_params_2d)``.
    """
    baseline_result = optimize_unitary_subcircuit_parameters(
        target_unitary=target_unitary_embedded,
        beam_splitter_reflectivities=beam_splitter_reflectivities,
        lr=config.lr,
        threshold=config.threshold,
        active_cols=baseline_active_cols,
        max_iterations=config.max_iterations,
        baseline=True,
        exclude_edge_phase_shifters=config.exclude_edge_phase_shifters,
        early_stop_patience=50,
        min_improvement=1e-6,
    )

    baseline_losses = baseline_result["losses"][-1] if baseline_result["losses"] else float("inf")
    baseline_phase_shifter_params = baseline_result["phase_shifter_params"].detach()

    baseline_phase_shifter_params_2d = reshape_flattened_params_to_grid(
        baseline_phase_shifter_params,
        chip_dim,
        exclude_edge_phase_shifters=config.exclude_edge_phase_shifters,
    )

    return baseline_losses, baseline_phase_shifter_params_2d


def _compute_ideal_distributions(target_unitary: torch.Tensor, target_dim: int) -> tuple[Any, Any]:
    """Compute ground-truth output distributions via ideal Perceval simulation.

    Returns:
        A tuple of ``(ideal_probability_distribution, baseline_ideal_probability_distribution)``.
        Both entries are identical since proposed and baseline share the same target unitary
        and input state; the tuple is kept for a consistent call interface.
    """
    pcvl_u = pcvl.Unitary(pcvl.MatrixN(target_unitary))

    ground_truth_processor = pcvl.Processor(m_circuit=target_dim, backend="SLOS")
    ground_truth_processor.add(mode_mapping=list(range(target_dim)), component=pcvl_u)
    ground_truth_processor.with_input(pcvl.BasicState([1, 0] * (target_dim // 2)))

    ideal_prob_dist = algorithm.Sampler(ground_truth_processor).probs()["results"]
    return ideal_prob_dist, ideal_prob_dist


def compile_subcircuit(
    beam_splitter_reflectivities: np.ndarray,
    input_transmissions: np.ndarray,
    output_transmissions: np.ndarray,
    target_unitary: torch.Tensor,
    target_unitary_embedded: torch.Tensor,
    phase_error: float,
    config: OptimizationConfig | None = None,
) -> RunResult:
    """Compile a target unitary onto the chip and evaluate against the baseline.

    ``chip_dim`` is derived from ``len(input_transmissions)`` and
    ``target_dim`` from ``target_unitary.shape[0]``.

    The proposed compiler searches for the optimal photon routing through the
    chip before optimising the phase-shifter parameters for the found
    placement.  The baseline uses a fixed dual-rail placement on the first
    ``target_dim`` modes with no routing step.

    Both approaches are evaluated on the same hardware model (beam-splitter
    imperfections, optional phase noise, input/output transmission losses) and
    their output distributions are compared to an ideal Perceval simulation.

    Args:
        beam_splitter_reflectivities: 1D array of chip beam-splitter
            reflectivities as produced by
            :func:`graph.generate_beam_splitter_matrix`.
        input_transmissions: Per-mode input transmission coefficients, shape
            ``(chip_dim,)``.  Its length determines ``chip_dim``.
        output_transmissions: Per-mode output transmission coefficients, shape
            ``(chip_dim,)``.
        target_unitary: Target unitary tensor of shape ``(target_dim, target_dim)``.
            Its first dimension determines ``target_dim``.
        target_unitary_embedded: ``target_unitary`` embedded into a ``(chip_dim, chip_dim)``
            identity matrix (used by the baseline optimiser).
        phase_error: Standard deviation of Gaussian phase noise applied to
            each phase shifter during Perceval simulation.
        config: Optimisation hyperparameters.  Defaults to
            :class:`OptimizationConfig` with all defaults when ``None``.

    Returns:
        A :class:`RunResult` containing performance metrics, final losses, and
        compute times for both the proposed compiler and the baseline.
    """
    if config is None:
        config = OptimizationConfig()

    chip_dim = len(input_transmissions)
    target_dim = int(target_unitary.shape[0])

    baseline_active_cols = get_baseline_active_cols(target_dim)
    baseline_input_ports = get_baseline_input_ports(baseline_active_cols, chip_dim)

    proposed_start = time.time()
    (
        movement_mask,
        input_ports,
        output_ports,
        active_cols_computation_zone,
        converted_input_ports,
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

    beam_splitter_reflectivities = torch.as_tensor(beam_splitter_reflectivities, dtype=torch.float64)

    baseline_start = time.time()
    baseline_losses, baseline_phase_shifter_params_2d = _run_baseline_optimization(
        target_unitary_embedded,
        beam_splitter_reflectivities,
        config,
        chip_dim,
        baseline_active_cols,
    )
    baseline_compute_time = time.time() - baseline_start

    ideal_prob_dist, baseline_ideal_prob_dist = _compute_ideal_distributions(target_unitary, target_dim)

    virtual_chip = create_mzi_chip(
        beam_splitter_reflectivities,
        phase_shifter_params_including_routing,
        phase_error=phase_error,
        chip_size=chip_dim,
        exclude_edge_phase_shifters=config.exclude_edge_phase_shifters,
    )
    baseline_virtual_chip = create_mzi_chip(
        beam_splitter_reflectivities,
        baseline_phase_shifter_params_2d,
        phase_error=phase_error,
        chip_size=chip_dim,
        exclude_edge_phase_shifters=config.exclude_edge_phase_shifters,
    )

    _, probability_distribution = simulate_with_loss(
        virtual_chip,
        chip_dim,
        input_state=converted_input_ports,
        input_transmissions=input_transmissions,
        output_transmissions=output_transmissions,
    )
    _, baseline_probability_distribution = simulate_with_loss(
        baseline_virtual_chip,
        chip_dim,
        input_state=baseline_input_ports,
        input_transmissions=input_transmissions,
        output_transmissions=output_transmissions,
    )

    performance_dict = evaluate_chip_performance(
        raw_results=probability_distribution,
        ideal_baseline=ideal_prob_dist,
        target_modes=output_ports,
        required_photons=target_dim // 2,
        output_transmissions=output_transmissions,
    )
    baseline_performance_dict = evaluate_chip_performance(
        raw_results=baseline_probability_distribution,
        ideal_baseline=baseline_ideal_prob_dist,
        target_modes=list(range(target_dim)),
        required_photons=target_dim // 2,
        output_transmissions=output_transmissions,
    )

    return RunResult(
        performance=performance_dict,
        baseline_performance=baseline_performance_dict,
        loss=losses,
        baseline_loss=baseline_losses,
        compute_time=proposed_compute_time,
        baseline_compute_time=baseline_compute_time,
    )
