# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""Perceval-based chip simulation and performance evaluation."""

from __future__ import annotations

from typing import TYPE_CHECKING, Any

import numpy as np
import perceval as pcvl
from perceval.components import BS, PS

if TYPE_CHECKING:
    import torch


def create_mzi_chip(
    bs_list: np.ndarray,
    ps_matrix: torch.Tensor | np.ndarray,
    phase_error: float | None,
    chip_size: int,
    exclude_edge_phase_shifters: bool = False,
    rng: np.random.Generator | int | None = None,
) -> pcvl.Circuit:
    """Build a Perceval circuit representing a staggered MZI mesh chip.

    The chip alternates between full layers (MZIs on mode pairs 0-1, 2-3, …)
    and half layers (MZIs on pairs 1-2, 3-4, …), matching the layout
    assumed by the unitary builder.  Gaussian phase noise is optionally
    added to model fabrication imperfections.

    Args:
        bs_list: 1D array of beam splitter reflectivities ordered MZI-by-MZI
            as produced by :func:`graph.generate_beam_splitter_matrix`.
        ps_matrix: 2D array of phase-shifter values with shape
            ``(chip_size, chip_size)``.  Rows are spatial modes, columns are
            MZI layers.
        phase_error: Standard deviation of zero-mean Gaussian noise added to
            each phase-shifter value.  Pass ``None`` for a noiseless circuit.
        chip_size: Total number of spatial modes (equals the number of MZI
            layers).
        exclude_edge_phase_shifters: If ``True``, omit the phase shifters on
            modes 0 and ``chip_size - 1`` in the last odd layer.
        rng: Source of randomness for the Gaussian phase noise.  Accepts a
            :class:`numpy.random.Generator`, an integer seed, or ``None``
            (default) which draws fresh, non-reproducible noise from OS
            entropy.  Pass a seeded generator or integer for reproducible
            noise.  Ignored when ``phase_error`` is ``None``.

    Returns:
        A :class:`perceval.Circuit` of size ``chip_size`` implementing the
        full MZI mesh.
    """
    circuit = pcvl.Circuit(chip_size, name="Quantum_MZI_Chip")
    mzi_layers = chip_size
    bs_idx = 0

    if phase_error is not None:
        ps_matrix = np.asarray(ps_matrix, dtype=np.float64)
        noise = np.random.default_rng(rng).normal(loc=0.0, scale=phase_error, size=ps_matrix.shape)
        ps_matrix += noise

    for layer in range(mzi_layers):
        is_full_layer = layer % 2 == 0
        is_last_layer = layer == mzi_layers - 1
        mzi_count = chip_size // 2 if is_full_layer else chip_size // 2 - 1
        layer_bs_start_idx = bs_idx

        for mzi in range(mzi_count):
            top_mode = mzi * 2 if is_full_layer else mzi * 2 + 1
            in_idx = layer_bs_start_idx + mzi * 2
            circuit.add(top_mode, BS(BS.r_to_theta(bs_list[in_idx])))

        for mode in range(chip_size):
            is_uncoupled = not is_full_layer and (mode == 0 or mode == chip_size - 1)
            if is_last_layer and is_uncoupled and exclude_edge_phase_shifters:
                continue
            circuit.add(mode, PS(phi=ps_matrix[mode][layer]))

        for mzi in range(mzi_count):
            top_mode = mzi * 2 if is_full_layer else mzi * 2 + 1
            out_idx = layer_bs_start_idx + mzi * 2 + 1
            circuit.add(top_mode, BS(BS.r_to_theta(bs_list[out_idx])))

        bs_idx += mzi_count * 2

    return circuit


def simulate_with_loss(
    circuit: pcvl.Circuit,
    chip_dim: int,
    input_state: list[int],
    input_transmissions: np.ndarray | list[float] | None = None,
    output_transmissions: np.ndarray | list[float] | None = None,
) -> tuple[pcvl.Processor, dict]:
    """Simulate a circuit inside a lossy processor and return the output distribution.

    Fibre-to-chip (input) and chip-to-detector (output) losses are modelled
    as per-mode loss channels wrapping the circuit.

    Args:
        circuit: A Perceval circuit representing the photonic chip.
        chip_dim: Total number of spatial modes.
        input_state: Binary occupancy list of length ``chip_dim`` used as the
            input :class:`perceval.BasicState`.
        input_transmissions: Per-mode input transmission coefficients.  When
            provided, a loss channel ``LC(1 - t)`` is prepended to each mode.
        output_transmissions: Per-mode output transmission coefficients.
            When provided, a loss channel ``LC(1 - t)`` is appended to each
            mode.

    Returns:
        A tuple ``(processor, probability_distribution)`` where *processor*
        is the configured :class:`perceval.Processor` and
        *probability_distribution* is the raw BSDistribution mapping output
        states to probabilities.
    """
    processor = pcvl.Processor("SLOS", chip_dim)

    if isinstance(input_transmissions, (list, np.ndarray)):
        for mode in range(chip_dim):
            processor.add(mode, pcvl.LC(1 - input_transmissions[mode]))

    processor.add(0, circuit)

    if isinstance(output_transmissions, (list, np.ndarray)):
        for mode in range(chip_dim):
            processor.add(mode, pcvl.LC(1 - output_transmissions[mode]))

    processor.with_input(pcvl.BasicState(input_state))
    processor.min_detected_photons_filter(0)

    sampler = pcvl.algorithm.Sampler(processor)
    return processor, sampler.probs()["results"]


def evaluate_chip_performance(
    raw_results: dict,
    ideal_baseline: dict,
    target_modes: list[int],
    required_photons: int,
    output_transmissions: np.ndarray | list[float] | None = None,
    apply_output_transmission_correction: bool = True,
) -> dict[str, Any]:
    """Evaluate coincidence rate and TVD of a simulated chip against the ideal distribution.

    Photon events are first filtered to those where all ``required_photons``
    land in the computation zone (``target_modes``).  The surviving
    probability mass gives the coincidence rate.  The conditional distribution
    is then compared to the ideal distribution via Total Variation Distance
    (TVD).

    When ``apply_output_transmission_correction`` is ``True``, each
    surviving probability is divided by the product of per-mode output
    transmissions raised to the per-mode photon count, compensating for
    detector efficiency before computing TVD.

    Args:
        raw_results: Unmapped probability distribution from the Perceval
            processor, keyed by full-chip :class:`perceval.BasicState`.
        ideal_baseline: Ideal probability distribution over the computation-
            zone states, keyed by :class:`perceval.BasicState`.
        target_modes: Indices of the spatial modes belonging to the
            computation zone.
        required_photons: Number of photons that must land in
            ``target_modes`` for an event to count as a success.
        output_transmissions: Per-mode output transmission coefficients used
            for probability correction.  Ignored when
            ``apply_output_transmission_correction`` is ``False``.
        apply_output_transmission_correction: Whether to correct
            probabilities for detector losses before computing TVD.

    Returns:
        A dictionary with the following keys:

        * ``"coincidence_rate"`` — fraction of events where all photons are in
          the computation zone.
        * ``"tvd"`` — Total Variation Distance between the corrected
          conditional distribution and the ideal distribution (1.0 if no
          photons survive).
        * ``"mapped_distribution"`` — corrected conditional distribution
          keyed by computation-zone :class:`perceval.BasicState`.
        * ``"compensated_weight_sum"`` — total corrected probability mass
          before normalisation.
    """
    coincidence_rate = 0.0
    mapped_dist: dict = {}
    compensated_weight_sum = 0.0

    for full_state, prob in raw_results.items():
        target_photons = [full_state[m] for m in target_modes]

        if sum(target_photons) != required_photons:
            continue

        coincidence_rate += prob
        corrected_prob = prob

        if apply_output_transmission_correction and isinstance(output_transmissions, (list, np.ndarray)):
            correction = 1.0
            for local_idx, mode in enumerate(target_modes):
                t = float(output_transmissions[mode])
                n = int(target_photons[local_idx])
                if t <= 0.0:
                    correction = 0.0
                    break
                if n > 0:
                    correction *= t**n
            corrected_prob = prob / correction if correction > 0.0 else 0.0

        compensated_weight_sum += corrected_prob
        sub_state = pcvl.BasicState(target_photons)
        mapped_dist[sub_state] = mapped_dist.get(sub_state, 0.0) + corrected_prob

    tvd = 1.0
    if compensated_weight_sum > 0:
        norm_sim = {s: p / compensated_weight_sum for s, p in mapped_dist.items()}
        baseline_total = sum(ideal_baseline.values())
        norm_ideal = {s: p / baseline_total for s, p in ideal_baseline.items()}
        all_states = set(norm_sim) | set(norm_ideal)
        tvd = 0.5 * sum(abs(norm_sim.get(s, 0.0) - norm_ideal.get(s, 0.0)) for s in all_states)

    return {
        "coincidence_rate": coincidence_rate,
        "tvd": tvd,
        "mapped_distribution": mapped_dist,
        "compensated_weight_sum": compensated_weight_sum,
    }
