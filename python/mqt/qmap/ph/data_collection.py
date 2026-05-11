# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""Batch data collection and aggregation for the photonic compiler pipeline."""

# ------------------------------------------------------------------------------
# Setup used for QCE26 paper submission.
# Future structure of the code will focus on the compilation,
# not the simulation and data collection.
# ------------------------------------------------------------------------------

from __future__ import annotations

import pathlib
from dataclasses import dataclass
from itertools import product
from typing import TYPE_CHECKING

import numpy as np
import pandas as pd
import torch

from .baseline import embed_target_unitary_into_chip
from .graph import generate_beam_splitter_matrix
from .subcircuit_compilation import OptimizationConfig, compile_subcircuit
from .unitary_to_phase_compilation import get_haar_random_unitary

if TYPE_CHECKING:
    from collections.abc import Iterable


@dataclass(frozen=True)
class Setup:
    """A single (chip_size, target_dim) hardware configuration."""

    num_modes: int
    target_dim: int


def build_setup_grid(
    num_modes_list: Iterable[int],
    target_dims_list: Iterable[int],
) -> list[Setup]:
    """Build the Cartesian product of mode counts and target dimensions.

    Configurations are filtered to include only even values for both
    ``num_modes`` and ``target_dim``, and only cases where
    ``target_dim <= num_modes``.

    Args:
        num_modes_list: Candidate chip mode counts.
        target_dims_list: Candidate target unitary dimensions.

    Returns:
        List of valid :class:`Setup` instances.
    """
    setups = []
    for num_modes, target_dim in product(num_modes_list, target_dims_list):
        if target_dim > num_modes:
            continue
        if num_modes % 2 != 0 or target_dim % 2 != 0:
            continue
        setups.append(Setup(num_modes=num_modes, target_dim=target_dim))
    return setups


def _mean_std(values: list[float]) -> tuple[float, float]:
    arr = np.asarray(values, dtype=np.float64)
    return float(arr.mean()), float(arr.std(ddof=0))


def collect_pipeline_results(
    setups: list[Setup],
    config: OptimizationConfig | None = None,
    num_unitaries_per_setup: int = 10,
    repeats_per_unitary: int = 3,
    phase_errors: Iterable[float] = (0.01,),
    base_seed: int = 0,
    input_losses: bool = False,
    output_losses: bool = False,
    ideal_beam_splitters: bool = False,
    custom_bs_data: dict[int, np.ndarray] | None = None,
) -> pd.DataFrame:
    """Collect and aggregate TVD and coincidence-rate metrics over a parameter sweep.

    For each ``(setup, phase_error)`` combination the function averages over:

    1. ``repeats_per_unitary`` independent runs (different phase initialisations).
    2. ``num_unitaries_per_setup`` random target unitaries.

    Hardware parameters (beam-splitter reflectivities and transmission
    coefficients) are sampled once per unique ``num_modes`` value and reused
    across all ``target_dim`` values and unitaries for that chip size.

    Args:
        setups: List of :class:`Setup` instances to evaluate.
        config: Optimisation hyperparameters shared across all runs.  Defaults
            to :class:`OptimizationConfig` with all defaults when ``None``.
        num_unitaries_per_setup: Number of Haar-random target unitaries to
            sample per ``(setup, phase_error)`` combination.
        repeats_per_unitary: Number of repeated optimisation runs per unitary.
            Each run uses a different PyTorch random seed for initialisation.
        phase_errors: Phase-noise standard deviations to sweep over.
        base_seed: Base integer seed used to derive hardware-parameter RNG
            seeds per chip size (``base_seed + 10 * num_modes``).
        input_losses: If ``True``, sample per-mode input transmissions from
            ``Uniform(0.7, 1.0)`` and normalise so the maximum is 1.0.
            If ``False``, use all-ones (lossless inputs).
        output_losses: Analogous to ``input_losses`` for output modes.
        ideal_beam_splitters: If ``True``, use ideal 50/50 beam splitters
            instead of the statistically distributed model.
        custom_bs_data: Optional mapping from ``num_modes`` to a pre-loaded
            beam-splitter reflectivity array.  When a key is present it takes
            precedence over the generated distribution.

    Returns:
        Aggregated :class:`pandas.DataFrame` with one row per
        ``(num_modes, target_dim, phase_error)`` group, containing mean TVD,
        coincidence rate, compute times, and signed differences between the
        proposed compiler and the baseline.

    Raises:
        ValueError: If ``repeats_per_unitary`` is less than 1.
    """
    if config is None:
        config = OptimizationConfig()

    rows: list[dict] = []
    phase_errors_list = list(phase_errors)

    if repeats_per_unitary < 1:
        msg = "repeats_per_unitary must be >= 1"
        raise ValueError(msg)

    # Precompute hardware parameters once per num_modes.
    hardware_cache: dict[int, tuple[np.ndarray, np.ndarray, np.ndarray]] = {}
    for num_modes in sorted({s.num_modes for s in setups}):
        np_rng = np.random.default_rng(base_seed + 10 * num_modes)

        if custom_bs_data is not None and num_modes in custom_bs_data:
            bs = custom_bs_data[num_modes]
        else:
            bs = generate_beam_splitter_matrix(chip_size=num_modes, ideal_bs=ideal_beam_splitters, rng=np_rng)

        in_t: np.ndarray
        if input_losses:
            raw_in = np_rng.uniform(0.7, 1.0, size=num_modes)
            in_t = raw_in / np.max(raw_in)
        else:
            in_t = np.ones(num_modes)

        out_t: np.ndarray
        if output_losses:
            raw_out = np_rng.uniform(0.7, 1.0, size=num_modes)
            out_t = raw_out / np.max(raw_out)
        else:
            out_t = np.ones(num_modes)

        hardware_cache[num_modes] = (bs, in_t, out_t)

    for setup in setups:
        num_modes = setup.num_modes
        target_dim = setup.target_dim
        beam_splitter_reflectivities, input_transmissions, output_transmissions = hardware_cache[num_modes]

        for phase_error in phase_errors_list:
            for unitary_index in range(num_unitaries_per_setup):
                unitary_seed = target_dim + unitary_index
                target_unitary = get_haar_random_unitary(
                    target_dim,
                    torch.Generator().manual_seed(unitary_seed),
                    dtype=torch.complex128,
                )
                target_unitary_embedded = embed_target_unitary_into_chip(
                    target_unitary.cpu().numpy(),
                    chip_dim=num_modes,
                    target_dim=target_dim,
                )

                normal_coincidence_rates: list[float] = []
                normal_tvds: list[float] = []
                baseline_coincidence_rates: list[float] = []
                baseline_tvds: list[float] = []
                normal_losses: list[float] = []
                baseline_losses: list[float] = []
                normal_compute_times: list[float] = []
                baseline_compute_times: list[float] = []

                for repeat_idx in range(repeats_per_unitary):
                    torch.manual_seed(unitary_seed * 1000 + repeat_idx)

                    result = compile_subcircuit(
                        beam_splitter_reflectivities=beam_splitter_reflectivities,
                        input_transmissions=input_transmissions,
                        output_transmissions=output_transmissions,
                        target_unitary=target_unitary,
                        target_unitary_embedded=target_unitary_embedded,
                        phase_error=phase_error,
                        config=config,
                    )

                    normal_coincidence_rates.append(float(result.performance["coincidence_rate"]))
                    normal_tvds.append(float(result.performance["tvd"]))
                    baseline_coincidence_rates.append(float(result.baseline_performance["coincidence_rate"]))
                    baseline_tvds.append(float(result.baseline_performance["tvd"]))
                    normal_losses.append(float(result.loss))
                    baseline_losses.append(float(result.baseline_loss))
                    normal_compute_times.append(float(result.compute_time))
                    baseline_compute_times.append(float(result.baseline_compute_time))

                mean_normal_coincidence_rate, _ = _mean_std(normal_coincidence_rates)
                mean_normal_tvd, _ = _mean_std(normal_tvds)
                mean_baseline_coincidence_rate, _ = _mean_std(baseline_coincidence_rates)
                mean_baseline_tvd, _ = _mean_std(baseline_tvds)
                mean_loss, _ = _mean_std(normal_losses)
                mean_baseline_loss, _ = _mean_std(baseline_losses)
                mean_compute_time, _ = _mean_std(normal_compute_times)
                mean_baseline_compute_time, _ = _mean_std(baseline_compute_times)

                rows.append({
                    "Input Losses": input_losses,
                    "Output Losses": output_losses,
                    "Ideal Beam Splitters": ideal_beam_splitters,
                    "num_modes": num_modes,
                    "target_dim": target_dim,
                    "unitary_index": unitary_index,
                    "unitary_seed": unitary_seed,
                    "phase_error": phase_error,
                    "repeats_per_unitary": repeats_per_unitary,
                    "avg_coincidence_rate": mean_normal_coincidence_rate,
                    "avg_baseline_coincidence_rate": mean_baseline_coincidence_rate,
                    "avg_baseline_tvd": mean_baseline_tvd,
                    "avg_tvd": mean_normal_tvd,
                    "tvd_difference": mean_normal_tvd - mean_baseline_tvd,
                    "coincidence_rate_difference": mean_normal_coincidence_rate - mean_baseline_coincidence_rate,
                    "avg_loss": mean_loss,
                    "avg_baseline_loss": mean_baseline_loss,
                    "avg_compute_time": mean_compute_time,
                    "avg_baseline_compute_time": mean_baseline_compute_time,
                })

    df = pd.DataFrame(rows)

    groupby_cols = [
        "num_modes",
        "target_dim",
        "phase_error",
        "Input Losses",
        "Output Losses",
        "Ideal Beam Splitters",
        "repeats_per_unitary",
    ]

    df_aggregated = df.groupby(groupby_cols, as_index=False).agg(
        avg_tvd=("avg_tvd", "mean"),
        avg_coincidence_rate=("avg_coincidence_rate", "mean"),
        avg_baseline_tvd=("avg_baseline_tvd", "mean"),
        avg_baseline_coincidence_rate=("avg_baseline_coincidence_rate", "mean"),
        avg_compute_time=("avg_compute_time", "mean"),
        avg_baseline_compute_time=("avg_baseline_compute_time", "mean"),
    )

    df_aggregated["tvd_difference"] = df_aggregated["avg_tvd"] - df_aggregated["avg_baseline_tvd"]
    df_aggregated["coincidence_rate_difference"] = (
        df_aggregated["avg_coincidence_rate"] - df_aggregated["avg_baseline_coincidence_rate"]
    )
    df_aggregated["compute_time_difference"] = (
        df_aggregated["avg_compute_time"] - df_aggregated["avg_baseline_compute_time"]
    )

    return df_aggregated


def export_results_table(df: pd.DataFrame, csv_path: str) -> None:
    """Write a results DataFrame to CSV.

    Args:
        df: DataFrame to export, typically produced by
            :func:`collect_pipeline_results`.
        csv_path: Destination path for the CSV file.  Parent directories are
            created automatically.
    """
    pathlib.Path(pathlib.Path(csv_path).parent or ".").mkdir(exist_ok=True, parents=True)
    df.to_csv(csv_path, index=False)
