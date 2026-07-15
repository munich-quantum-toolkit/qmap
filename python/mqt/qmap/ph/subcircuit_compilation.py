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
from typing import Any

import numpy as np
import torch

from .baseline import get_baseline_active_cols, get_baseline_input_ports
from .graph import construct_graph
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
class CompilationResult:
    """Output of a single :func:`compile_subcircuit` call.

    This is the end-user result of compiling a target unitary onto a physical
    chip.  It carries everything needed to drive the hardware: the phase-shifter
    values to program, and the input/output ports the photons enter and leave on.

    Attributes:
        phases: Effective phase-shifter parameters of shape
            ``(chip_dim, chip_dim)`` — rows are spatial modes, columns are MZI
            layers.  These are the values to program onto the chip.
        input_ports: Chip-wide binary input-state vector of length ``chip_dim``
            marking the modes into which photons are injected (dual-rail).
        output_ports: Physical mode indices of the computation zone where the
            output photons are measured.
        loss: Final fidelity loss of the proposed compiler optimisation.
        compute_time: Wall-clock seconds for the proposed compiler (routing
            + optimisation).
    """

    phases: torch.Tensor
    input_ports: list[int]
    output_ports: list[int]
    loss: float
    compute_time: float


@dataclass
class RunResult:
    """Output of a single :func:`evaluate_subcircuit` call.

    Attributes:
        performance: Metrics for the proposed compiler (coincidence rate, TVD,
            etc.) as returned by
            :func:`perceval_simulation.evaluate_chip_performance`.
        baseline_performance: Same metrics for the baseline strategy.
        loss: Final fidelity loss of the proposed compiler optimisation.
        baseline_loss: Final fidelity loss of the baseline optimisation.
        compute_time: Wall-clock seconds for the proposed compiler (routing
            + optimisation).
        baseline_compute_time: Wall-clock seconds for the baseline
            optimisation.
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

    Args:
        beam_splitter_reflectivities: 1D array of chip beam-splitter
            reflectivities, ordered MZI-by-MZI.
        input_transmissions: Per-mode input transmission coefficients, shape
            ``(chip_dim,)``.
        output_transmissions: Per-mode output transmission coefficients, shape
            ``(chip_dim,)``.
        target_unitary: Target unitary tensor of shape ``(target_dim, target_dim)``.
        chip_dim: Total number of spatial modes on the chip.
        target_dim: Dimension of the target unitary.

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
        beam_splitter_reflectivities: 1D array of chip beam-splitter reflectivities.
        movement_mask: ``(chip_dim, chip_dim)`` routing-state mask for the chosen route.
        config: Optimisation hyperparameters.
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

    losses = result["best_loss"]
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

    Args:
        target_unitary_embedded: Target unitary embedded into a
            ``(chip_dim, chip_dim)`` identity matrix.
        beam_splitter_reflectivities: Chip beam-splitter reflectivities as a tensor.
        config: Optimisation hyperparameters.
        chip_dim: Total number of spatial modes on the chip.
        baseline_active_cols: Even-indexed dual-rail input columns of the fixed
            baseline placement.

    Returns:
        A tuple of ``(best_loss, baseline_phase_shifter_params_2d)``, where
        ``best_loss`` is the loss of the returned (best) parameters.
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
        # Deliberately stricter than the proposed path's 1e-4: the baseline optimises the
        # full chip-sized embedding and keeps refining on smaller gains before early stopping.
        # Both thresholds are baked into the calibrated regression bounds, so aligning them
        # would shift the baseline metrics and is intentionally avoided.
        min_improvement=1e-6,
    )

    baseline_losses = baseline_result["best_loss"]
    baseline_phase_shifter_params = baseline_result["phase_shifter_params"].detach()

    baseline_phase_shifter_params_2d = reshape_flattened_params_to_grid(
        baseline_phase_shifter_params,
        chip_dim,
        exclude_edge_phase_shifters=config.exclude_edge_phase_shifters,
    )

    return baseline_losses, baseline_phase_shifter_params_2d


def _compute_ideal_distributions(target_unitary: torch.Tensor, target_dim: int) -> tuple[Any, Any]:
    """Compute ground-truth output distributions via ideal Perceval simulation.

    Args:
        target_unitary: Target unitary tensor of shape ``(target_dim, target_dim)``.
        target_dim: Dimension of the target unitary.

    Returns:
        A tuple of ``(ideal_probability_distribution, baseline_ideal_probability_distribution)``.
        Both entries are identical since proposed and baseline share the same target unitary
        and input state; the tuple is kept for a consistent call interface.
    """
    import perceval as pcvl  # noqa: PLC0415  (optional dependency, only needed for evaluation)
    from perceval import algorithm  # noqa: PLC0415

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
    config: OptimizationConfig | None = None,
) -> CompilationResult:
    """Compile a target unitary onto the chip and return the phases to program.

    ``chip_dim`` is derived from ``len(input_transmissions)`` and
    ``target_dim`` from ``target_unitary.shape[0]``.

    The compiler searches for the optimal photon routing through the chip
    (the path minimising overall photon loss, independent of the target
    operation) and then optimises the phase-shifter parameters for that
    placement.  No hardware simulation is performed, so this step is suitable
    for chips too large to simulate classically.

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
        config: Optimisation hyperparameters.  Defaults to
            :class:`OptimizationConfig` with all defaults when ``None``.

    Returns:
        A :class:`CompilationResult` containing the phase-shifter matrix to
        program, the input/output ports, the final fidelity loss, and the
        compilation compute time.
    """
    if config is None:
        config = OptimizationConfig()

    chip_dim = len(input_transmissions)
    target_dim = int(target_unitary.shape[0])

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

    return CompilationResult(
        phases=phase_shifter_params_including_routing,
        input_ports=converted_input_ports,
        output_ports=output_ports,
        loss=losses,
        compute_time=proposed_compute_time,
    )


def evaluate_subcircuit(
    compilation: CompilationResult,
    beam_splitter_reflectivities: np.ndarray,
    input_transmissions: np.ndarray,
    output_transmissions: np.ndarray,
    target_unitary: torch.Tensor,
    target_unitary_embedded: torch.Tensor,
    phase_error: float,
    config: OptimizationConfig | None = None,
    phase_noise_seed: np.random.Generator | int | None = None,
) -> RunResult:
    """Evaluate a compiled subcircuit against the baseline via Perceval simulation.

    Takes the output of :func:`compile_subcircuit` and simulates the resulting
    chip, comparing it to a fixed dual-rail baseline placement (first
    ``target_dim`` modes, no routing).  Both are simulated on the same hardware
    model (beam-splitter imperfections, phase noise, input/output transmission
    losses) and their output distributions are compared to an ideal Perceval
    simulation.

    This step requires a classical simulation of the chip and is intended for
    reproducing benchmark results; it is not needed to drive real hardware.

    Args:
        compilation: The :class:`CompilationResult` returned by
            :func:`compile_subcircuit` for the same target unitary and chip.
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
        phase_noise_seed: Source of randomness for the Gaussian phase noise.
            Accepts a :class:`numpy.random.Generator`, an integer seed, or
            ``None`` (default).  When ``None``, the proposed and baseline chips
            each draw fresh, non-reproducible noise from OS entropy.  When a
            generator or integer is given, a single generator is shared across
            both chips so the whole evaluation is reproducible while the two
            chips still see independent (sequential) noise draws.

    Returns:
        A :class:`RunResult` containing performance metrics, final losses, and
        compute times for both the proposed compiler and the baseline.
    """
    # Perceval is only required for this simulation-based evaluation step, so it is
    # imported lazily: compiling a subcircuit (compile_subcircuit) needs only torch.
    from .perceval_simulation import (  # noqa: PLC0415
        create_mzi_chip,
        evaluate_chip_performance,
        simulate_with_loss,
    )

    if config is None:
        config = OptimizationConfig()

    chip_dim = len(input_transmissions)
    target_dim = int(target_unitary.shape[0])

    # When a seed is supplied, share one generator across both create_mzi_chip
    # calls so the evaluation is reproducible; ``None`` preserves the original
    # behaviour of drawing fresh entropy per chip.
    noise_rng = None if phase_noise_seed is None else np.random.default_rng(phase_noise_seed)

    baseline_active_cols = get_baseline_active_cols(target_dim)
    baseline_input_ports = get_baseline_input_ports(baseline_active_cols, chip_dim)

    beam_splitter_reflectivities_t = torch.as_tensor(beam_splitter_reflectivities, dtype=torch.float64)

    baseline_start = time.time()
    baseline_losses, baseline_phase_shifter_params_2d = _run_baseline_optimization(
        target_unitary_embedded,
        beam_splitter_reflectivities_t,
        config,
        chip_dim,
        baseline_active_cols,
    )
    baseline_compute_time = time.time() - baseline_start

    ideal_prob_dist, baseline_ideal_prob_dist = _compute_ideal_distributions(target_unitary, target_dim)

    virtual_chip = create_mzi_chip(
        beam_splitter_reflectivities,
        compilation.phases,
        phase_error=phase_error,
        chip_size=chip_dim,
        exclude_edge_phase_shifters=config.exclude_edge_phase_shifters,
        rng=noise_rng,
    )
    baseline_virtual_chip = create_mzi_chip(
        beam_splitter_reflectivities,
        baseline_phase_shifter_params_2d,
        phase_error=phase_error,
        chip_size=chip_dim,
        exclude_edge_phase_shifters=config.exclude_edge_phase_shifters,
        rng=noise_rng,
    )

    _, probability_distribution = simulate_with_loss(
        virtual_chip,
        chip_dim,
        input_state=compilation.input_ports,
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
        target_modes=compilation.output_ports,
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
        loss=compilation.loss,
        baseline_loss=baseline_losses,
        compute_time=compilation.compute_time,
        baseline_compute_time=baseline_compute_time,
    )
