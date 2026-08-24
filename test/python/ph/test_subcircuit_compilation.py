# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""Regression tests for subcircuit_compilation.compile_subcircuit / evaluate_subcircuit.

Twenty-four scenarios: chip_dim in {8, 16} x phase_error in {0.0, 0.015, 0.030}
x transmission range in {ones, [0.9,1], [0.8,1], [0.7,1]}, target_dim=4.
All tests run for each scenario via the parametrized `scenario_result` fixture.

Setup: non-ideal (statistically distributed) beam splitters seeded per chip size,
       torch.manual_seed(0) immediately before compile_subcircuit(), then
       evaluate_subcircuit() on its result.  1 restart, 300 iterations.

Each scenario in _SCENARIOS carries its own bounds (cr_min, tvd_max, losses_max).
Adjust any row's values directly in the _SCENARIOS table below the _Scenario class.
Tighten them after observing typical results - they should catch semantic
regressions while tolerating minor numerical variation across runs.
"""

from __future__ import annotations

import pathlib
import sys
from dataclasses import dataclass, field
from typing import cast

import numpy as np
import pytest

pytest.importorskip("perceval")
torch = pytest.importorskip("torch")

# generate_beam_splitter_matrix is the synthetic hardware model in eval/ph/
# (paper-reproduction code, not part of the installable package).
sys.path.insert(0, str(pathlib.Path(__file__).parents[3] / "eval" / "ph"))

from hardware_model import generate_beam_splitter_matrix

from mqt.qmap.ph.baseline import embed_target_unitary_into_chip
from mqt.qmap.ph.subcircuit_compilation import (
    CompilationResult,
    OptimizationConfig,
    RunResult,
    compile_subcircuit,
    evaluate_subcircuit,
)
from mqt.qmap.ph.unitary_to_phase_compilation import get_haar_random_unitary

_PERF_KEYS = {"compensated_weight_sum", "mapped_distribution", "coincidence_rate", "tvd"}


@dataclass(frozen=True)
class _Scenario:
    """Parameters and expected bounds for a single compile_subcircuit run."""

    chip_dim: int
    target_dim: int
    phase_error: float
    t_low: float | None  # None -> all-ones (lossless); otherwise Uniform[t_low, 1.0] normalized
    # Proposed compiler bounds
    coincidence_rate_min: float
    tvd_max: float
    # Baseline bounds (no routing - typically lower cr, similar tvd)
    baseline_coincidence_rate_min: float
    baseline_tvd_max: float
    # Shared optimizer-convergence bound (independent of routing and transmission).
    # Observed losses are <= 1.9e-3; 5e-3 leaves headroom for cross-platform/torch
    # variation while still flagging a failure to converge.
    losses_max: float = field(default=5e-3)

    @property
    def id(self) -> str:
        t_tag = "t1.0" if self.t_low is None else f"t{self.t_low:.1f}"
        return f"chip{self.chip_dim}-target{self.target_dim}-pe{self.phase_error:.3f}-{t_tag}"


# fmt: off
# Each row is one scenario.
# Columns: chip_dim, target_dim, phase_error, t_low,
#          cr_min, tvd_max,                   <- proposed compiler
#          baseline_cr_min, baseline_tvd_max  <- baseline (no routing)
#
# t_low=None -> perfect transmission (all ones); otherwise Uniform[t_low, 1.0], normalized.
# Bounds calibrated against observed runs with NON-IDEAL (statistically distributed) beam
# splitters and DETERMINISTIC phase noise (phase_noise_seed=0 in the fixture).  Because
# every random input is now seeded, tvd_max/cr_min sit only a small margin beyond the
# observed values (about +0.015 on tvd, -0.025 on cr) - tight enough to catch semantic
# regressions, with headroom only for cross-platform/torch numerical variation.
# tvd grows with phase_error; cr is transmission-dominated and the proposed cr_min sits
# above the baseline's, reflecting the routing advantage under transmission loss.
_SCENARIOS = [
    # -- chip_dim = 8 ----------------------------------------------------------------------
    # t = 1.0  (lossless)
    pytest.param(_Scenario(8,  4, 0.000, None, 0.97, 0.008, 0.97, 0.005), id="chip8-t1.0-pe0.000"),
    pytest.param(_Scenario(8,  4, 0.015, None, 0.97, 0.045, 0.97, 0.035), id="chip8-t1.0-pe0.015"),
    pytest.param(_Scenario(8,  4, 0.030, None, 0.97, 0.065, 0.97, 0.050), id="chip8-t1.0-pe0.030"),
    # t ~ Uniform[0.9, 1.0]
    pytest.param(_Scenario(8,  4, 0.000, 0.9, 0.86, 0.008, 0.84, 0.005), id="chip8-t0.9-pe0.000"),
    pytest.param(_Scenario(8,  4, 0.015, 0.9, 0.86, 0.045, 0.84, 0.035), id="chip8-t0.9-pe0.015"),
    pytest.param(_Scenario(8,  4, 0.030, 0.9, 0.86, 0.065, 0.84, 0.050), id="chip8-t0.9-pe0.030"),
    # t ~ Uniform[0.8, 1.0]
    pytest.param(_Scenario(8,  4, 0.000, 0.8, 0.79, 0.008, 0.71, 0.005), id="chip8-t0.8-pe0.000"),
    pytest.param(_Scenario(8,  4, 0.015, 0.8, 0.79, 0.035, 0.71, 0.035), id="chip8-t0.8-pe0.015"),
    pytest.param(_Scenario(8,  4, 0.030, 0.8, 0.79, 0.050, 0.71, 0.050), id="chip8-t0.8-pe0.030"),
    # t ~ Uniform[0.7, 1.0]
    pytest.param(_Scenario(8,  4, 0.000, 0.7, 0.71, 0.008, 0.59, 0.005), id="chip8-t0.7-pe0.000"),
    pytest.param(_Scenario(8,  4, 0.015, 0.7, 0.71, 0.035, 0.59, 0.035), id="chip8-t0.7-pe0.015"),
    pytest.param(_Scenario(8,  4, 0.030, 0.7, 0.71, 0.050, 0.59, 0.050), id="chip8-t0.7-pe0.030"),
    # -- chip_dim = 16 ---------------------------------------------------------------------
    # t = 1.0  (lossless)
    pytest.param(_Scenario(16, 4, 0.000, None, 0.97, 0.008, 0.97, 0.005), id="chip16-t1.0-pe0.000"),
    pytest.param(_Scenario(16, 4, 0.015, None, 0.97, 0.045, 0.97, 0.050), id="chip16-t1.0-pe0.015"),
    pytest.param(_Scenario(16, 4, 0.030, None, 0.97, 0.065, 0.97, 0.080), id="chip16-t1.0-pe0.030"),
    # t ~ Uniform[0.9, 1.0]
    pytest.param(_Scenario(16, 4, 0.000, 0.9, 0.85, 0.008, 0.85, 0.005), id="chip16-t0.9-pe0.000"),
    pytest.param(_Scenario(16, 4, 0.015, 0.9, 0.85, 0.035, 0.85, 0.050), id="chip16-t0.9-pe0.015"),
    pytest.param(_Scenario(16, 4, 0.030, 0.9, 0.85, 0.050, 0.85, 0.080), id="chip16-t0.9-pe0.030"),
    # t ~ Uniform[0.8, 1.0]
    pytest.param(_Scenario(16, 4, 0.000, 0.8, 0.75, 0.008, 0.73, 0.005), id="chip16-t0.8-pe0.000"),
    pytest.param(_Scenario(16, 4, 0.015, 0.8, 0.75, 0.035, 0.73, 0.050), id="chip16-t0.8-pe0.015"),
    pytest.param(_Scenario(16, 4, 0.030, 0.8, 0.75, 0.050, 0.73, 0.080), id="chip16-t0.8-pe0.030"),
    # t ~ Uniform[0.7, 1.0]
    pytest.param(_Scenario(16, 4, 0.000, 0.7, 0.66, 0.008, 0.62, 0.005), id="chip16-t0.7-pe0.000"),
    pytest.param(_Scenario(16, 4, 0.015, 0.7, 0.66, 0.035, 0.62, 0.050), id="chip16-t0.7-pe0.015"),
    pytest.param(_Scenario(16, 4, 0.030, 0.7, 0.66, 0.050, 0.62, 0.080), id="chip16-t0.7-pe0.030"),
]
# fmt: on

# Subset used for tests that only make sense when there are transmission losses.
_SCENARIOS_WITH_LOSS = [p for p in _SCENARIOS if cast("_Scenario", p.values[0]).t_low is not None]


@dataclass
class ScenarioResult:
    """Bundles the CompilationResult and RunResult with the scenario that produced it."""

    result: RunResult
    compilation: CompilationResult
    scenario: _Scenario


@pytest.fixture(scope="module", params=_SCENARIOS)
def scenario_result(request) -> ScenarioResult:
    """Run compile_subcircuit for one scenario and return a ScenarioResult."""
    s: _Scenario = request.param

    # Non-ideal beam splitters (statistically distributed reflectivities): this is
    # the realistic regime the compiler must handle, and it strongly influences
    # performance.  Seed by chip_dim so every scenario on the same chip size sees
    # the same physical beam-splitter layout, deterministically.
    bs_rng = np.random.default_rng(2 * s.chip_dim)
    bs = generate_beam_splitter_matrix(chip_size=s.chip_dim, ideal_bs=False, rng=bs_rng)

    hw_rng = np.random.default_rng(10 * s.chip_dim)
    if s.t_low is None:
        input_t = np.ones(s.chip_dim)
        output_t = np.ones(s.chip_dim)
    else:
        input_t = hw_rng.uniform(s.t_low, 1.0, size=s.chip_dim)
        input_t /= np.max(input_t)
        output_t = hw_rng.uniform(s.t_low, 1.0, size=s.chip_dim)
        output_t /= np.max(output_t)

    rng = torch.Generator().manual_seed(10)
    target_unitary = get_haar_random_unitary(s.target_dim, rng, dtype=torch.complex128)
    embedded = embed_target_unitary_into_chip(
        target_unitary.cpu().numpy(), chip_dim=s.chip_dim, target_dim=s.target_dim
    )

    torch.manual_seed(0)

    # compile_subcircuit/evaluate_subcircuit take plain lists; the NumPy arrays above
    # exist only to compute the statistics/normalization.
    bs_list = bs.tolist()
    input_t_list = input_t.tolist()
    output_t_list = output_t.tolist()

    config = OptimizationConfig(max_iterations=300)
    compilation = compile_subcircuit(
        beam_splitter_reflectivities=bs_list,
        input_transmissions=input_t_list,
        output_transmissions=output_t_list,
        target_unitary=target_unitary,
        config=config,
    )
    run_result = evaluate_subcircuit(
        compilation,
        beam_splitter_reflectivities=bs_list,
        input_transmissions=input_t_list,
        output_transmissions=output_t_list,
        target_unitary=target_unitary,
        target_unitary_embedded=embedded,
        phase_error=s.phase_error,
        config=config,
        # Fixed seed -> deterministic phase noise, so the tvd bounds below can be
        # tight rather than padded ceilings.
        phase_noise_seed=0,
    )

    return ScenarioResult(result=run_result, compilation=compilation, scenario=s)


class TestCompilationResult:
    """Tests verifying the structure and types of the CompilationResult from compile_subcircuit."""

    @staticmethod
    def test_returns_compilation_result(scenario_result) -> None:
        """Test that compile_subcircuit returns a CompilationResult instance."""
        assert isinstance(scenario_result.compilation, CompilationResult)

    @staticmethod
    def test_phases_is_column_major_list(scenario_result) -> None:
        """Test that phases is a flat list of chip_dim**2 values."""
        chip_dim = scenario_result.scenario.chip_dim
        phases = scenario_result.compilation.phases
        assert isinstance(phases, list)
        assert len(phases) == chip_dim**2

    @staticmethod
    def test_phases_are_finite(scenario_result) -> None:
        """Test that all phase values are finite real numbers."""
        assert np.all(np.isfinite(scenario_result.compilation.phases))

    @staticmethod
    def test_input_ports_are_valid_mode_indices(scenario_result) -> None:
        """Test that input_ports is a list of distinct in-range mode indices.

        One index per injected photon (``target_dim // 2``), consistent with the
        index-based ``output_ports``.
        """
        input_ports = scenario_result.compilation.input_ports
        chip_dim = scenario_result.scenario.chip_dim
        assert len(input_ports) == scenario_result.scenario.target_dim // 2
        assert all(0 <= p < chip_dim for p in input_ports)
        assert len(set(input_ports)) == len(input_ports)

    @staticmethod
    def test_output_ports_length_matches_target_dim(scenario_result) -> None:
        """Test that the output-port list has length target_dim."""
        assert len(scenario_result.compilation.output_ports) == scenario_result.scenario.target_dim

    @staticmethod
    def test_compilation_loss_non_negative(scenario_result) -> None:
        """Test that the compilation loss is non-negative."""
        assert float(scenario_result.compilation.loss) >= 0.0

    @staticmethod
    def test_compilation_compute_time_positive(scenario_result) -> None:
        """Test that the compilation compute time is strictly positive."""
        assert scenario_result.compilation.compute_time > 0

    @staticmethod
    def test_run_result_reuses_compilation_loss_and_time(scenario_result) -> None:
        """Test that evaluate_subcircuit propagates the compilation loss and compute time."""
        assert scenario_result.result.proposed.loss == scenario_result.compilation.loss
        assert scenario_result.result.proposed.compute_time == scenario_result.compilation.compute_time


class TestRunReturnStructure:
    """Tests verifying the structure and types of the RunResult returned by evaluate_subcircuit."""

    @staticmethod
    def test_returns_run_result(scenario_result) -> None:
        """Test that compile_subcircuit returns a RunResult instance."""
        assert isinstance(scenario_result.result, RunResult)

    @staticmethod
    def test_performance_dict_has_required_keys(scenario_result) -> None:
        """Test that the performance dict contains all required metric keys."""
        assert set(scenario_result.result.proposed.performance.keys()) >= _PERF_KEYS

    @staticmethod
    def test_baseline_performance_dict_has_required_keys(scenario_result) -> None:
        """Test that the baseline performance dict contains all required metric keys."""
        assert set(scenario_result.result.baseline.performance.keys()) >= _PERF_KEYS

    @staticmethod
    def test_losses_is_float(scenario_result) -> None:
        """Test that loss and baseline_loss are convertible to float."""
        assert isinstance(float(scenario_result.result.proposed.loss), float)
        assert isinstance(float(scenario_result.result.baseline.loss), float)

    @staticmethod
    def test_compute_times_are_positive(scenario_result) -> None:
        """Test that compute_time and baseline_compute_time are strictly positive."""
        assert scenario_result.result.proposed.compute_time > 0
        assert scenario_result.result.baseline.compute_time > 0

    @staticmethod
    def test_coincidence_rate_in_unit_interval(scenario_result) -> None:
        """Test that coincidence_rate values lie in [0, 1] for both compiled and baseline."""
        for cr in (
            float(scenario_result.result.proposed.performance["coincidence_rate"]),
            float(scenario_result.result.baseline.performance["coincidence_rate"]),
        ):
            assert cr >= 0.0
            assert cr <= 1.0 or cr == pytest.approx(1.0)

    @staticmethod
    def test_tvd_in_unit_interval(scenario_result) -> None:
        """Test that TVD values lie in [0, 1] for both compiled and baseline."""
        for tvd in (
            float(scenario_result.result.proposed.performance["tvd"]),
            float(scenario_result.result.baseline.performance["tvd"]),
        ):
            assert tvd >= 0.0
            assert tvd <= 1.0 or tvd == pytest.approx(1.0)

    @staticmethod
    def test_losses_non_negative(scenario_result) -> None:
        """Test that loss and baseline_loss are non-negative."""
        assert float(scenario_result.result.proposed.loss) >= 0.0
        assert float(scenario_result.result.baseline.loss) >= 0.0


class TestRunValueRanges:
    """Range-based regression checks with per-scenario bounds.

    coincidence_rate_min varies with t_low (transmission loss reduces detected photons).
    tvd_max grows with phase_error (about 0.008 at phase_error=0, up to 0.065-0.080 at 0.030).
    losses_max is uniform across all scenarios.

    All random inputs are seeded (beam splitters, optimizer, and phase noise), so the
    bounds sit just above the observed values with headroom only for cross-platform and
    torch-version numerical variation.
    """

    @staticmethod
    def test_coincidence_rate_above_minimum(scenario_result) -> None:
        """Test that the compiled coincidence rate meets the scenario's minimum threshold."""
        cr = float(scenario_result.result.proposed.performance["coincidence_rate"])
        assert cr >= scenario_result.scenario.coincidence_rate_min

    @staticmethod
    def test_baseline_coincidence_rate_above_minimum(scenario_result) -> None:
        """Test that the baseline coincidence rate meets the baseline's minimum threshold."""
        cr = float(scenario_result.result.baseline.performance["coincidence_rate"])
        assert cr >= scenario_result.scenario.baseline_coincidence_rate_min

    @staticmethod
    def test_tvd_below_maximum(scenario_result) -> None:
        """Test that the compiled TVD is below the scenario's maximum threshold."""
        tvd = float(scenario_result.result.proposed.performance["tvd"])
        assert tvd <= scenario_result.scenario.tvd_max

    @staticmethod
    def test_baseline_tvd_below_maximum(scenario_result) -> None:
        """Test that the baseline TVD is below the baseline's maximum threshold."""
        tvd = float(scenario_result.result.baseline.performance["tvd"])
        assert tvd <= scenario_result.scenario.baseline_tvd_max

    @staticmethod
    def test_optimization_loss_below_maximum(scenario_result) -> None:
        """Test that the final optimization loss is below the convergence threshold."""
        assert float(scenario_result.result.proposed.loss) <= scenario_result.scenario.losses_max

    @staticmethod
    def test_baseline_loss_below_maximum(scenario_result) -> None:
        """Test that the baseline loss is below the convergence threshold."""
        assert float(scenario_result.result.baseline.loss) <= scenario_result.scenario.losses_max


@pytest.mark.parametrize("scenario_result", _SCENARIOS_WITH_LOSS, indirect=True)
def test_proposed_coincidence_rate_exceeds_baseline(scenario_result) -> None:
    """Test that the compiled coincidence rate is at least as high as the baseline.

    Only parametrized for lossy scenarios (t_low is not None): routing steers
    photons to lower-loss modes, so the proposed compiler should outperform the
    fixed-placement baseline.  With perfect transmission all paths are equivalent
    and no routing advantage is expected, so those scenarios are excluded entirely.
    """
    assert float(scenario_result.result.proposed.performance["coincidence_rate"]) >= float(
        scenario_result.result.baseline.performance["coincidence_rate"]
    )


def test_extreme_routing_coincidence_rate(extreme_routing_chip) -> None:
    """End-to-end regression for the routing layer mapping, via the coincidence rate.

    On a chip with extreme routing beam splitters and an ideal (universal)
    computation zone, the correct graph-layer -> chip-layer mapping routes photons
    cleanly into the computation window, so with perfect input/output ports and no
    phase noise the coincidence rate is ~1.0 and the target is realized (tvd ~ 0).

    A mapping error (for example the ``+1`` sign bug in
    ``get_edge_fidelity_odd_graph_layer``) picks a route that, executed on the
    extreme hardware, ejects photons clean out of the computation window; they are
    not lost (the mesh is unitary) but land in undetected modes, so the coincidence
    rate collapses (observed < 0.15). The near-0.5 beam splitters used by the other
    scenarios cannot expose this: there every route realizes the target equally, so
    the coincidence rate stays ~1.0 regardless of the mapping.
    """
    chip = extreme_routing_chip
    perfect_transmissions = [1.0] * chip.chip_dim

    rng = torch.Generator().manual_seed(10)
    target_unitary = get_haar_random_unitary(chip.target_dim, rng, dtype=torch.complex128)
    embedded = embed_target_unitary_into_chip(
        target_unitary.cpu().numpy(), chip_dim=chip.chip_dim, target_dim=chip.target_dim
    )

    torch.manual_seed(0)
    config = OptimizationConfig(max_iterations=300)
    compilation = compile_subcircuit(
        beam_splitter_reflectivities=chip.bs,
        input_transmissions=perfect_transmissions,
        output_transmissions=perfect_transmissions,
        target_unitary=target_unitary,
        config=config,
    )
    result = evaluate_subcircuit(
        compilation,
        beam_splitter_reflectivities=chip.bs,
        input_transmissions=perfect_transmissions,
        output_transmissions=perfect_transmissions,
        target_unitary=target_unitary,
        target_unitary_embedded=embedded,
        phase_error=0.0,
        config=config,
        phase_noise_seed=0,
    )

    # Correct mapping -> window on modes [2, 3, 4, 5]; the +1 sign bug yields [0, 1, 2, 3].
    assert compilation.output_ports == [2, 3, 4, 5]
    # Photons stay in the computation window -> coincidence rate ~ 1.0 (the bug drops it < 0.15),
    # and the universal computation zone realizes the target -> tvd ~ 0.
    assert float(result.proposed.performance["coincidence_rate"]) >= 0.95
    assert float(result.proposed.performance["tvd"]) <= 0.01
