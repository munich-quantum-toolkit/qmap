# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""Synthetic photonic hardware model for the paper evaluation.

This module is evaluation-only and is not part of the installed ``mqt.qmap.ph``
compiler. The compiler consumes *measured* beam-splitter reflectivities as input;
it never generates them. For the paper's experiments we instead synthesize a
plausible chip by drawing reflectivities from calibrated statistics.

The hard-coded constants in :func:`generate_beam_splitter_matrix`
(``target_mean = 0.552``, ``target_std = 0.038``, ``target_avg_abs_diff = 0.019``)
describe the global mean and standard deviation of the beam-splitter
reflectivities and the average absolute difference between the two reflectivities
within a single Mach-Zehnder interferometer (MZI). They stand in for a real
device's fabrication spread and are only meaningful for the synthetic evaluation.
"""

from __future__ import annotations

import numpy as np


def generate_beam_splitter_matrix(
    chip_size: int,
    ideal_bs: bool = False,
    rng: np.random.Generator | None = None,
) -> np.ndarray:
    """Generate beam splitter reflectivity values as a 1D array.

    Values are generated with controlled global statistics and controlled
    intra-MZI differences.  The array is ordered MZI-by-MZI, strictly
    aligned with the spatial mapping of the unitary builder:
    ``[MZI_0_in, MZI_0_out, MZI_1_in, MZI_1_out, ...]``.

    When ``ideal_bs`` is ``False`` the values are drawn so that
    (approximately, in finite samples):

    * global mean ~ 0.552
    * global std ~ 0.038
    * average absolute difference within each MZI ~ 0.019 (exponential distribution)

    Args:
        chip_size: Number of spatial modes on the chip.
        ideal_bs: If ``True``, return an array filled with the ideal
            reflectivity of 0.5.
        rng: NumPy random generator for reproducibility.  Accepts a
            :class:`numpy.random.Generator`, an integer seed, or ``None``
            (creates a new generator with an unpredictable seed).

    Returns:
        1D NumPy array of beam splitter reflectivities with length
        ``2 * total_mzis``, where ``total_mzis`` is the total number of
        MZIs across all layers.
    """
    target_mean = 0.552
    target_std = 0.038
    target_avg_abs_diff = 0.019

    num_mzi_layers = chip_size  # 2 * chip_size physical layers -> chip_size MZI layers

    group_sizes = []
    for layer_idx in range(num_mzi_layers):
        if layer_idx % 2 == 0:
            group_sizes.append(chip_size // 2)
        else:
            group_sizes.append(chip_size // 2 - 1)

    total_mzis = int(np.sum(group_sizes))

    if ideal_bs:
        return np.full(2 * total_mzis, 0.5)

    rng = np.random.default_rng(rng)

    # Intra-MZI differences follow an exponential distribution.
    deltas = rng.exponential(scale=target_avg_abs_diff, size=total_mzis)
    mean_delta = np.mean(deltas)
    if mean_delta > 0:
        deltas *= target_avg_abs_diff / mean_delta
    else:
        deltas[:] = target_avg_abs_diff

    diff_variance_component = np.mean((deltas / 2.0) ** 2)
    required_center_variance = max(target_std**2 - diff_variance_component, 0.0)

    centers_raw = rng.normal(loc=0.0, scale=1.0, size=total_mzis)
    centers_raw_var = np.var(centers_raw)
    if centers_raw_var > 0:
        centers = centers_raw * np.sqrt(required_center_variance / centers_raw_var)
    else:
        centers = np.zeros_like(centers_raw)
    centers += target_mean - np.mean(centers)

    signs = rng.choice([-1.0, 1.0], size=total_mzis)

    bs_values = np.zeros(2 * total_mzis)
    bs_values[0::2] = centers - signs * (deltas / 2.0)
    bs_values[1::2] = centers + signs * (deltas / 2.0)

    # Affine correction for exact global statistics.
    current_mean = np.mean(bs_values)
    current_std = np.std(bs_values)
    if current_std > 0:
        bs_values = (bs_values - current_mean) * (target_std / current_std) + target_mean
    else:
        bs_values[:] = target_mean

    # Re-adjust intra-pair differences.
    pair_abs_diffs = np.abs(bs_values[0::2] - bs_values[1::2])
    current_avg_abs_diff = np.mean(pair_abs_diffs)
    if current_avg_abs_diff > 0:
        ratio = target_avg_abs_diff / current_avg_abs_diff
        pair_means = 0.5 * (bs_values[0::2] + bs_values[1::2])
        pair_deltas = 0.5 * (bs_values[1::2] - bs_values[0::2]) * ratio
        bs_values[0::2] = pair_means - pair_deltas
        bs_values[1::2] = pair_means + pair_deltas

    # Final exact mean/std normalization.
    current_mean = np.mean(bs_values)
    current_std = np.std(bs_values)
    if current_std > 0:
        bs_values = (bs_values - current_mean) * (target_std / current_std) + target_mean
    else:
        bs_values[:] = target_mean

    return bs_values
