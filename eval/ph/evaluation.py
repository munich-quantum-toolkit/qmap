# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""Perceval-based evaluation of the photonic compiler against a fixed baseline.

This module reproduces the paper's benchmark: it takes a compiled subcircuit
(the compiler's :class:`~mqt.qmap.ph.subcircuit_compilation.CompilationResult`)
and simulates it on Perceval, comparing it to a fixed dual-rail baseline
placement. It lives under ``eval/ph`` because it depends on Perceval, which is
not a dependency of the installed ``mqt.qmap.ph`` compiler.
"""

from __future__ import annotations

import time
from dataclasses import dataclass
from typing import Any

import numpy as np
import perceval as pcvl
import torch
from baseline import get_baseline_active_cols
from perceval import algorithm
from perceval_simulation import (
    SIMULATION_BACKEND,
    create_mzi_chip,
    evaluate_chip_performance,
    simulate_with_loss,
)

from mqt.qmap.ph.routing import convert_input_ports
from mqt.qmap.ph.subcircuit_compilation import CompilationResult, OptimizationConfig
from mqt.qmap.ph.unitary_to_phase_compilation import optimize_unitary_subcircuit_parameters


@dataclass
class RunMetrics:
    """Metrics for a single evaluated strategy (proposed compiler or baseline).

    Attributes:
        performance: Metrics (coincidence rate, TVD, etc.) as returned by
            :func:`perceval_simulation.evaluate_chip_performance`.
        loss: Final fidelity loss of the optimization.
        compute_time: Wall-clock seconds for the optimization (routing +
            optimization for the proposed compiler).
    """

    performance: dict[str, Any]
    loss: float
    compute_time: float


@dataclass
class RunResult:
    """Output of a single :func:`evaluate_subcircuit` call.

    Attributes:
        proposed: :class:`RunMetrics` for the proposed compiler.
        baseline: :class:`RunMetrics` for the fixed dual-rail baseline strategy.
    """

    proposed: RunMetrics
    baseline: RunMetrics


def _run_baseline_optimization(
    target_unitary_embedded: torch.Tensor,
    beam_splitter_reflectivities: torch.Tensor,
    config: OptimizationConfig,
    baseline_active_cols: list[int],
) -> tuple[float, torch.Tensor]:
    """Optimize phase-shifter parameters for the baseline dual-rail placement.

    Args:
        target_unitary_embedded: Target unitary embedded into a
            ``(chip_dim, chip_dim)`` identity matrix.  Its own dimension
            determines the optimizer's grid size (``chip_dim``), since no
            ``movement_mask`` is passed for the baseline.
        beam_splitter_reflectivities: Chip beam-splitter reflectivities as a tensor.
        config: Optimization hyperparameters.
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
        # Deliberately stricter than the proposed path's 1e-4: the baseline optimizes the
        # full chip-sized embedding and keeps refining on smaller gains before early stopping.
        # Both thresholds are baked into the calibrated regression bounds, so aligning them
        # would shift the baseline metrics and is intentionally avoided.
        min_improvement=1e-6,
    )

    baseline_losses = baseline_result.best_loss
    # phase_shifter_params is already the (chip_dim, chip_dim) grid the optimizer trains natively.
    baseline_phase_shifter_params_2d = baseline_result.phase_shifter_params.detach()

    return baseline_losses, baseline_phase_shifter_params_2d


def _compute_ideal_distribution(target_unitary: torch.Tensor, target_dim: int) -> dict:
    """Compute the ground-truth output distribution via ideal Perceval simulation.

    Used as the reference for both the proposed and baseline strategies: both
    share the same target unitary and input state, so a single ideal
    distribution is the correct comparison point for either.

    Args:
        target_unitary: Target unitary tensor of shape ``(target_dim, target_dim)``.
        target_dim: Dimension of the target unitary.

    Returns:
        The ideal probability distribution over computation-zone states, keyed
        by :class:`perceval.BasicState`.
    """
    pcvl_u = pcvl.Unitary(pcvl.MatrixN(target_unitary))

    ground_truth_processor = pcvl.Processor(m_circuit=target_dim, backend=SIMULATION_BACKEND)
    ground_truth_processor.add(mode_mapping=list(range(target_dim)), component=pcvl_u)
    ground_truth_processor.with_input(pcvl.BasicState([1, 0] * (target_dim // 2)))

    return algorithm.Sampler(ground_truth_processor).probs()["results"]


def evaluate_subcircuit(
    compilation: CompilationResult,
    beam_splitter_reflectivities: list[float],
    input_transmissions: list[float],
    output_transmissions: list[float],
    target_unitary: torch.Tensor,
    target_unitary_embedded: torch.Tensor,
    phase_error: float,
    config: OptimizationConfig | None = None,
    phase_noise_seed: np.random.Generator | int | None = None,
) -> RunResult:
    """Evaluate a compiled subcircuit against the baseline via Perceval simulation.

    Takes the output of :func:`~mqt.qmap.ph.subcircuit_compilation.compile_subcircuit`
    and simulates the resulting chip, comparing it to a fixed dual-rail baseline
    placement (first ``target_dim`` modes, no routing). Both are simulated on the
    same hardware model (beam-splitter imperfections, phase noise, input/output
    transmission losses) and their output distributions are compared to an ideal
    Perceval simulation.

    Args:
        compilation: The :class:`~mqt.qmap.ph.subcircuit_compilation.CompilationResult`
            returned by :func:`~mqt.qmap.ph.subcircuit_compilation.compile_subcircuit`
            for the same target unitary and chip.
        beam_splitter_reflectivities: Flat list of measured chip beam-splitter
            reflectivities ordered MZI-by-MZI as in/out pairs, layer by layer.
        input_transmissions: Per-mode input transmission coefficients, a list
            of length ``chip_dim``. Its length determines ``chip_dim``.
        output_transmissions: Per-mode output transmission coefficients, a
            list of length ``chip_dim``.
        target_unitary: Target unitary tensor of shape ``(target_dim, target_dim)``.
            Its first dimension determines ``target_dim``.
        target_unitary_embedded: ``target_unitary`` embedded into a ``(chip_dim, chip_dim)``
            identity matrix (used by the baseline optimizer).
        phase_error: Standard deviation of Gaussian phase noise applied to
            each phase shifter during Perceval simulation.
        config: Optimization hyperparameters. Defaults to
            :class:`~mqt.qmap.ph.subcircuit_compilation.OptimizationConfig` with
            all defaults when ``None``.
        phase_noise_seed: Source of randomness for the Gaussian phase noise.
            Accepts a :class:`numpy.random.Generator`, an integer seed, or
            ``None`` (default). When ``None``, the proposed and baseline chips
            each draw fresh, non-reproducible noise from OS entropy. When a
            generator or integer is given, a single generator is shared across
            both chips so the whole evaluation is reproducible while the two
            chips still see independent (sequential) noise draws.

    Returns:
        A :class:`RunResult` containing performance metrics, final losses, and
        compute times for both the proposed compiler and the baseline.

    Note:
        This step requires a classical simulation of the chip and is intended
        for reproducing benchmark results; it is not needed to drive real
        hardware.
    """
    if config is None:
        config = OptimizationConfig()

    chip_dim = len(input_transmissions)
    target_dim = int(target_unitary.shape[0])

    # When a seed is supplied, share one generator across both create_mzi_chip
    # calls so the evaluation is reproducible; ``None`` preserves the original
    # behavior of drawing fresh entropy per chip.
    noise_rng = None if phase_noise_seed is None else np.random.default_rng(phase_noise_seed)

    baseline_active_cols = get_baseline_active_cols(target_dim)

    beam_splitter_reflectivities_tensor = torch.as_tensor(beam_splitter_reflectivities, dtype=torch.float64)

    baseline_start = time.time()
    baseline_losses, baseline_phase_shifter_params_2d = _run_baseline_optimization(
        target_unitary_embedded,
        beam_splitter_reflectivities_tensor,
        config,
        baseline_active_cols,
    )
    baseline_compute_time = time.time() - baseline_start

    ideal_prob_dist = _compute_ideal_distribution(target_unitary, target_dim)

    # phases is a column-major flat list; rebuild the (chip_dim, chip_dim) grid the simulator expects.
    phases_grid = torch.tensor(compilation.phases, dtype=torch.float64).reshape(chip_dim, chip_dim).t()
    virtual_chip = create_mzi_chip(
        beam_splitter_reflectivities,
        phases_grid,
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
        # input_ports are physical mode indices; Perceval needs an occupancy vector.
        input_state=convert_input_ports(compilation.input_ports, chip_dim),
        input_transmissions=input_transmissions,
        output_transmissions=output_transmissions,
    )
    _, baseline_probability_distribution = simulate_with_loss(
        baseline_virtual_chip,
        chip_dim,
        # baseline_active_cols are physical mode indices; Perceval needs an occupancy vector.
        input_state=convert_input_ports(baseline_active_cols, chip_dim),
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
        ideal_baseline=ideal_prob_dist,
        target_modes=list(range(target_dim)),
        required_photons=target_dim // 2,
        output_transmissions=output_transmissions,
    )

    return RunResult(
        proposed=RunMetrics(
            performance=performance_dict,
            loss=compilation.loss,
            compute_time=compilation.compute_time,
        ),
        baseline=RunMetrics(
            performance=baseline_performance_dict,
            loss=baseline_losses,
            compute_time=baseline_compute_time,
        ),
    )
