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
import warnings
from dataclasses import dataclass
from itertools import product
from typing import TYPE_CHECKING

import numpy as np
import pandas as pd
import torch

from mqt.qmap.ph.baseline import embed_target_unitary_into_chip
from mqt.qmap.ph.graph import generate_beam_splitter_matrix
from mqt.qmap.ph.subcircuit_compilation import (
    OptimizationConfig,
    compile_subcircuit,
    evaluate_subcircuit,
)
from mqt.qmap.ph.unitary_to_phase_compilation import get_haar_random_unitary

if TYPE_CHECKING:
    from collections.abc import Iterable

# Directory holding the exact per-mode transmission coefficients used for the
# paper submission, so reviewers can reproduce the reported results.  Each file
# is named ``{num_modes}_mode_{input,output}_transmissions.txt`` with one
# transmission value per line.
_HARDWARE_DATA_DIR = pathlib.Path(__file__).parent / "hardware_data"


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


def _mean(values: list[float]) -> float:
    """Compute the mean of a list of floats.

    Args:
        values: Sequence of numeric values.

    Returns:
        The mean as a float.
    """
    return float(np.mean(values))


def _resolve_transmissions(
    num_modes: int,
    kind: str,
    hardware_data_dir: pathlib.Path,
    rng: np.random.Generator,
) -> np.ndarray:
    """Return per-mode transmission coefficients, preferring the paper's data.

    If ``hardware_data_dir`` contains ``{num_modes}_mode_{kind}_transmissions.txt``
    the exact paper values are loaded (used as-is; the files are already
    normalised so the maximum is 1.0).  If no such file exists, per-mode
    transmissions are instead sampled from ``Uniform(0.7, 1.0)`` and normalised
    so the maximum is 1.0, and a :class:`UserWarning` is emitted so the caller
    knows the run is not reproducing the paper's exact hardware.

    Args:
        num_modes: Chip mode count; selects the file
            ``{num_modes}_mode_{kind}_transmissions.txt``.
        kind: Either ``"input"`` or ``"output"``.
        hardware_data_dir: Directory holding the transmission text files.
        rng: Generator used for the random fallback when no file is present.

    Returns:
        1D array of ``num_modes`` transmission coefficients.

    Raises:
        ValueError: If the file exists but does not contain exactly
            ``num_modes`` values.
    """
    path = hardware_data_dir / f"{num_modes}_mode_{kind}_transmissions.txt"
    if path.is_file():
        values = np.loadtxt(path, dtype=float).reshape(-1)
        if values.size != num_modes:
            msg = f"File '{path}' contains {values.size} values but {num_modes} were expected."
            raise ValueError(msg)
        return values

    warnings.warn(
        f"No transmission file '{path}' for the {num_modes}-mode {kind} transmissions; "
        f"sampling random values from Uniform(0.7, 1.0) instead (not the paper's exact hardware).",
        stacklevel=2,
    )
    raw = rng.uniform(0.7, 1.0, size=num_modes)
    return raw / np.max(raw)


def _build_hardware_cache(
    setups: list[Setup],
    base_seed: int,
    *,
    input_losses: bool,
    output_losses: bool,
    ideal_beam_splitters: bool,
    custom_bs_data: dict[int, np.ndarray] | None,
    hardware_data_dir: pathlib.Path = _HARDWARE_DATA_DIR,
) -> dict[int, tuple[np.ndarray, np.ndarray, np.ndarray]]:
    """Build per-num_modes hardware parameter cache.

    Args:
        setups: All setups whose unique ``num_modes`` values are cached.
        base_seed: Base RNG seed; the seed for ``num_modes`` is
            ``base_seed + 10 * num_modes``.
        input_losses: When ``True``, load the paper's exact input transmissions
            for each ``num_modes``, falling back to random sampling (with a
            warning) if no file is present.
        output_losses: As ``input_losses`` but for the output transmissions.
        ideal_beam_splitters: Use ideal 50/50 beam splitters when ``True``.
        custom_bs_data: Pre-loaded reflectivity arrays keyed by ``num_modes``.
        hardware_data_dir: Directory holding the transmission text files.

    Returns:
        Mapping from ``num_modes`` to ``(bs, in_t, out_t)`` arrays.
    """
    hardware_cache: dict[int, tuple[np.ndarray, np.ndarray, np.ndarray]] = {}
    for num_modes in sorted({s.num_modes for s in setups}):
        np_rng = np.random.default_rng(base_seed + 10 * num_modes)

        if custom_bs_data is not None and num_modes in custom_bs_data:
            bs = custom_bs_data[num_modes]
        else:
            bs = generate_beam_splitter_matrix(chip_size=num_modes, ideal_bs=ideal_beam_splitters, rng=np_rng)

        if input_losses:
            in_t: np.ndarray = _resolve_transmissions(num_modes, "input", hardware_data_dir, np_rng)
        else:
            in_t = np.ones(num_modes)

        if output_losses:
            out_t: np.ndarray = _resolve_transmissions(num_modes, "output", hardware_data_dir, np_rng)
        else:
            out_t = np.ones(num_modes)

        hardware_cache[num_modes] = (bs, in_t, out_t)
    return hardware_cache


def _run_repeats(
    beam_splitter_reflectivities: np.ndarray,
    input_transmissions: np.ndarray,
    output_transmissions: np.ndarray,
    target_unitary: torch.Tensor,
    target_unitary_embedded: torch.Tensor,
    phase_error: float,
    config: OptimizationConfig,
    repeats_per_unitary: int,
    unitary_seed: int,
) -> dict[str, float]:
    """Compile and evaluate ``repeats_per_unitary`` times and return mean metrics.

    Args:
        beam_splitter_reflectivities: Chip beam-splitter reflectivity array.
        input_transmissions: Per-mode input transmission coefficients.
        output_transmissions: Per-mode output transmission coefficients.
        target_unitary: Target unitary tensor.
        target_unitary_embedded: Target unitary embedded into chip-sized identity.
        phase_error: Phase-noise standard deviation.
        config: Optimisation hyperparameters.
        repeats_per_unitary: Number of independent runs to average over.
        unitary_seed: Seed used to derive per-repeat PyTorch seeds.

    Returns:
        Dict of mean metric values across all repeats, keyed by the same names
        used in the ``rows`` dicts of :func:`collect_pipeline_results`.
    """
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

        compilation = compile_subcircuit(
            beam_splitter_reflectivities=beam_splitter_reflectivities,
            input_transmissions=input_transmissions,
            output_transmissions=output_transmissions,
            target_unitary=target_unitary,
            config=config,
        )
        result = evaluate_subcircuit(
            compilation,
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

    return {
        "avg_coincidence_rate": _mean(normal_coincidence_rates),
        "avg_tvd": _mean(normal_tvds),
        "avg_baseline_coincidence_rate": _mean(baseline_coincidence_rates),
        "avg_baseline_tvd": _mean(baseline_tvds),
        "avg_loss": _mean(normal_losses),
        "avg_baseline_loss": _mean(baseline_losses),
        "avg_compute_time": _mean(normal_compute_times),
        "avg_baseline_compute_time": _mean(baseline_compute_times),
    }


def collect_pipeline_results(
    setups: list[Setup],
    config: OptimizationConfig | None = None,
    num_unitaries_per_setup: int = 10,
    repeats_per_unitary: int = 3,
    phase_errors: Iterable[float] = (0.01,),
    base_seed: int = 0,
    *,
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
        input_losses: If ``True``, load the paper's exact per-mode input
            transmissions from ``hardware_data/{num_modes}_mode_input_transmissions.txt``.
            When no such file exists, per-mode transmissions are sampled from
            ``Uniform(0.7, 1.0)`` (normalised to max 1.0) and a ``UserWarning``
            is emitted.  If ``False``, use all-ones (lossless inputs).
        output_losses: Analogous to ``input_losses`` for output modes, loaded
            from ``hardware_data/{num_modes}_mode_output_transmissions.txt``.
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

    if repeats_per_unitary < 1:
        msg = "repeats_per_unitary must be >= 1"
        raise ValueError(msg)

    phase_errors_list = list(phase_errors)
    hardware_cache = _build_hardware_cache(
        setups,
        base_seed,
        input_losses=input_losses,
        output_losses=output_losses,
        ideal_beam_splitters=ideal_beam_splitters,
        custom_bs_data=custom_bs_data,
    )

    rows: list[dict] = []
    for setup in setups:
        num_modes = setup.num_modes
        target_dim = setup.target_dim
        beam_splitter_reflectivities, input_transmissions, output_transmissions = hardware_cache[num_modes]

        for phase_error in phase_errors_list:
            for unitary_index in range(num_unitaries_per_setup):
                unitary_seed = target_dim * 1000 + unitary_index
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

                means = _run_repeats(
                    beam_splitter_reflectivities,
                    input_transmissions,
                    output_transmissions,
                    target_unitary,
                    target_unitary_embedded,
                    phase_error,
                    config,
                    repeats_per_unitary,
                    unitary_seed,
                )

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
                    **means,
                    "tvd_difference": means["avg_tvd"] - means["avg_baseline_tvd"],
                    "coincidence_rate_difference": means["avg_coincidence_rate"]
                    - means["avg_baseline_coincidence_rate"],
                })

    groupby_cols = [
        "num_modes",
        "target_dim",
        "phase_error",
        "Input Losses",
        "Output Losses",
        "Ideal Beam Splitters",
        "repeats_per_unitary",
    ]
    agg_cols = [
        "avg_tvd",
        "avg_coincidence_rate",
        "avg_baseline_tvd",
        "avg_baseline_coincidence_rate",
        "avg_loss",
        "avg_baseline_loss",
        "avg_compute_time",
        "avg_baseline_compute_time",
        "tvd_difference",
        "coincidence_rate_difference",
        "compute_time_difference",
    ]

    if not rows:
        return pd.DataFrame(columns=groupby_cols + agg_cols)

    df = pd.DataFrame(rows)

    df_aggregated = df.groupby(groupby_cols, as_index=False).agg(
        avg_tvd=("avg_tvd", "mean"),
        avg_coincidence_rate=("avg_coincidence_rate", "mean"),
        avg_baseline_tvd=("avg_baseline_tvd", "mean"),
        avg_baseline_coincidence_rate=("avg_baseline_coincidence_rate", "mean"),
        avg_loss=("avg_loss", "mean"),
        avg_baseline_loss=("avg_baseline_loss", "mean"),
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
