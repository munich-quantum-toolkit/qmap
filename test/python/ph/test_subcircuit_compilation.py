# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""Regression tests for subcircuit_compilation.compile_subcircuit().

Twenty-four scenarios: chip_dim in {8, 16} x phase_error in {0.0, 0.015, 0.030}
x transmission range in {ones, [0.9,1], [0.8,1], [0.7,1]}, target_dim=4.
All tests run for each scenario via the parametrized `scenario_result` fixture.

Setup: ideal BS, torch.manual_seed(0) immediately before compile_subcircuit().
       1 restart, 300 iterations.

Each scenario in _SCENARIOS carries its own bounds (cr_min, tvd_max, losses_max).
Adjust any row's values directly in the _SCENARIOS table below the _Scenario class.
Tighten them after observing typical results — they should catch semantic
regressions while tolerating minor numerical variation across runs.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import cast

import numpy as np
import pytest

pytest.importorskip("perceval")
torch = pytest.importorskip("torch")

from mqt.qmap.ph.baseline import embed_target_unitary_into_chip
from mqt.qmap.ph.graph import generate_beam_splitter_matrix
from mqt.qmap.ph.subcircuit_compilation import OptimizationConfig, RunResult, compile_subcircuit
from mqt.qmap.ph.unitary_to_phase_compilation import get_haar_random_unitary

_PERF_KEYS = {"compensated_weight_sum", "mapped_distribution", "coincidence_rate", "tvd"}


@dataclass(frozen=True)
class _Scenario:
    """Parameters and expected bounds for a single compile_subcircuit run."""

    chip_dim: int
    target_dim: int
    phase_error: float
    t_low: float | None  # None → all-ones (lossless); otherwise Uniform[t_low, 1.0] normalised
    # Proposed compiler bounds
    coincidence_rate_min: float
    tvd_max: float
    # Baseline bounds (no routing — typically lower cr, similar tvd)
    baseline_coincidence_rate_min: float
    baseline_tvd_max: float
    # Shared optimizer-convergence bound (independent of routing and transmission)
    losses_max: float = field(default=1e-2)

    @property
    def id(self) -> str:
        t_tag = "t1.0" if self.t_low is None else f"t{self.t_low:.1f}"
        return f"chip{self.chip_dim}-target{self.target_dim}-pe{self.phase_error:.3f}-{t_tag}"


# fmt: off
# Each row is one scenario.
# Columns: chip_dim, target_dim, phase_error, t_low,
#          cr_min, tvd_max,                   ← proposed compiler
#          baseline_cr_min, baseline_tvd_max  ← baseline (no routing)
#
# t_low=None → perfect transmission (all ones); otherwise Uniform[t_low, 1.0], normalised.
# Bounds are conservative — tighten them after observing actual run values.
# phase_error=0 → tight tvd_max; phase_error>0 → loose tvd_max.
# cr_min drops with lower transmission; baseline cr_min is lower still (no routing benefit).
_SCENARIOS = [
    # ── chip_dim = 8 ──────────────────────────────────────────────────────────────────────
    # t = 1.0  (lossless)
    pytest.param(_Scenario(8,  4, 0.000, None, 0.90, 0.001, 0.80, 0.001), id="chip8-t1.0-pe0.000"),
    pytest.param(_Scenario(8,  4, 0.015, None, 0.85, 0.050, 0.75, 0.050), id="chip8-t1.0-pe0.015"),
    pytest.param(_Scenario(8,  4, 0.030, None, 0.80, 0.100, 0.70, 0.100), id="chip8-t1.0-pe0.030"),
    # t ~ Uniform[0.9, 1.0]
    pytest.param(_Scenario(8,  4, 0.000, 0.9, 0.70, 0.002, 0.60, 0.002), id="chip8-t0.9-pe0.000"),
    pytest.param(_Scenario(8,  4, 0.015, 0.9, 0.65, 0.050, 0.55, 0.050), id="chip8-t0.9-pe0.015"),
    pytest.param(_Scenario(8,  4, 0.030, 0.9, 0.60, 0.100, 0.50, 0.100), id="chip8-t0.9-pe0.030"),
    # t ~ Uniform[0.8, 1.0]
    pytest.param(_Scenario(8,  4, 0.000, 0.8, 0.40, 0.005, 0.30, 0.005), id="chip8-t0.8-pe0.000"),
    pytest.param(_Scenario(8,  4, 0.015, 0.8, 0.35, 0.050, 0.25, 0.050), id="chip8-t0.8-pe0.015"),
    pytest.param(_Scenario(8,  4, 0.030, 0.8, 0.30, 0.100, 0.20, 0.100), id="chip8-t0.8-pe0.030"),
    # t ~ Uniform[0.7, 1.0]
    pytest.param(_Scenario(8,  4, 0.000, 0.7, 0.30, 0.010, 0.20, 0.010), id="chip8-t0.7-pe0.000"),
    pytest.param(_Scenario(8,  4, 0.015, 0.7, 0.25, 0.050, 0.15, 0.050), id="chip8-t0.7-pe0.015"),
    pytest.param(_Scenario(8,  4, 0.030, 0.7, 0.20, 0.100, 0.10, 0.100), id="chip8-t0.7-pe0.030"),
    # ── chip_dim = 16 ─────────────────────────────────────────────────────────────────────
    # t = 1.0  (lossless)
    pytest.param(_Scenario(16, 4, 0.000, None, 0.90, 0.002, 0.80, 0.001), id="chip16-t1.0-pe0.000"),
    pytest.param(_Scenario(16, 4, 0.015, None, 0.85, 0.050, 0.75, 0.100), id="chip16-t1.0-pe0.015"),
    pytest.param(_Scenario(16, 4, 0.030, None, 0.80, 0.100, 0.70, 0.150), id="chip16-t1.0-pe0.030"),
    # t ~ Uniform[0.9, 1.0]
    pytest.param(_Scenario(16, 4, 0.000, 0.9, 0.70, 0.002, 0.60, 0.002), id="chip16-t0.9-pe0.000"),
    pytest.param(_Scenario(16, 4, 0.015, 0.9, 0.65, 0.050, 0.55, 0.100), id="chip16-t0.9-pe0.015"),
    pytest.param(_Scenario(16, 4, 0.030, 0.9, 0.60, 0.100, 0.50, 0.150), id="chip16-t0.9-pe0.030"),
    # t ~ Uniform[0.8, 1.0]
    pytest.param(_Scenario(16, 4, 0.000, 0.8, 0.40, 0.005, 0.30, 0.005), id="chip16-t0.8-pe0.000"),
    pytest.param(_Scenario(16, 4, 0.015, 0.8, 0.35, 0.050, 0.25, 0.100), id="chip16-t0.8-pe0.015"),
    pytest.param(_Scenario(16, 4, 0.030, 0.8, 0.30, 0.100, 0.20, 0.200), id="chip16-t0.8-pe0.030"),
    # t ~ Uniform[0.7, 1.0]
    pytest.param(_Scenario(16, 4, 0.000, 0.7, 0.30, 0.010, 0.20, 0.010), id="chip16-t0.7-pe0.000"),
    pytest.param(_Scenario(16, 4, 0.015, 0.7, 0.25, 0.050, 0.15, 0.100), id="chip16-t0.7-pe0.015"),
    pytest.param(_Scenario(16, 4, 0.030, 0.7, 0.20, 0.100, 0.10, 0.200), id="chip16-t0.7-pe0.030"),
]
# fmt: on

# Subset used for tests that only make sense when there are transmission losses.
_SCENARIOS_WITH_LOSS = [p for p in _SCENARIOS if cast("_Scenario", p.values[0]).t_low is not None]


@dataclass
class ScenarioResult:
    """Bundles the RunResult with the scenario that produced it."""

    result: RunResult
    scenario: _Scenario


@pytest.fixture(scope="module", params=_SCENARIOS)
def scenario_result(request) -> ScenarioResult:
    """Run compile_subcircuit for one scenario and return a ScenarioResult."""
    s: _Scenario = request.param

    bs = generate_beam_splitter_matrix(chip_size=s.chip_dim, ideal_bs=True)

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

    run_result = compile_subcircuit(
        beam_splitter_reflectivities=bs,
        input_transmissions=input_t,
        output_transmissions=output_t,
        target_unitary=target_unitary,
        target_unitary_embedded=embedded,
        phase_error=s.phase_error,
        config=OptimizationConfig(max_iterations=300),
    )

    return ScenarioResult(result=run_result, scenario=s)


class TestRunReturnStructure:
    """Tests verifying the structure and types of the RunResult returned by compile_subcircuit."""

    @staticmethod
    def test_returns_run_result(scenario_result) -> None:
        """Test that compile_subcircuit returns a RunResult instance."""
        assert isinstance(scenario_result.result, RunResult)

    @staticmethod
    def test_performance_dict_has_required_keys(scenario_result) -> None:
        """Test that the performance dict contains all required metric keys."""
        assert set(scenario_result.result.performance.keys()) >= _PERF_KEYS

    @staticmethod
    def test_baseline_performance_dict_has_required_keys(scenario_result) -> None:
        """Test that the baseline performance dict contains all required metric keys."""
        assert set(scenario_result.result.baseline_performance.keys()) >= _PERF_KEYS

    @staticmethod
    def test_losses_is_float(scenario_result) -> None:
        """Test that loss and baseline_loss are convertible to float."""
        assert isinstance(float(scenario_result.result.loss), float)
        assert isinstance(float(scenario_result.result.baseline_loss), float)

    @staticmethod
    def test_compute_times_are_positive(scenario_result) -> None:
        """Test that compute_time and baseline_compute_time are strictly positive."""
        assert scenario_result.result.compute_time > 0
        assert scenario_result.result.baseline_compute_time > 0

    @staticmethod
    def test_coincidence_rate_in_unit_interval(scenario_result) -> None:
        """Test that coincidence_rate values lie in [0, 1] for both compiled and baseline."""
        for cr in (
            float(scenario_result.result.performance["coincidence_rate"]),
            float(scenario_result.result.baseline_performance["coincidence_rate"]),
        ):
            assert cr >= 0.0
            assert cr <= 1.0 or cr == pytest.approx(1.0)

    @staticmethod
    def test_tvd_in_unit_interval(scenario_result) -> None:
        """Test that TVD values lie in [0, 1] for both compiled and baseline."""
        for tvd in (
            float(scenario_result.result.performance["tvd"]),
            float(scenario_result.result.baseline_performance["tvd"]),
        ):
            assert tvd >= 0.0
            assert tvd <= 1.0 or tvd == pytest.approx(1.0)

    @staticmethod
    def test_losses_non_negative(scenario_result) -> None:
        """Test that loss and baseline_loss are non-negative."""
        assert float(scenario_result.result.loss) >= 0.0
        assert float(scenario_result.result.baseline_loss) >= 0.0


class TestRunValueRanges:
    """Range-based regression checks with per-scenario bounds.

    coincidence_rate_min varies with t_low (transmission loss reduces detected photons).
    tvd_max is split by phase_error regime: tight (0.01) for phase_error=0,
    loose (0.05) for phase_error>0.  losses_max is uniform across all scenarios.

    Bounds are conservative — tighten them after observing typical results.
    """

    @staticmethod
    def test_coincidence_rate_above_minimum(scenario_result) -> None:
        """Test that the compiled coincidence rate meets the scenario's minimum threshold."""
        cr = float(scenario_result.result.performance["coincidence_rate"])
        assert cr >= scenario_result.scenario.coincidence_rate_min

    @staticmethod
    def test_baseline_coincidence_rate_above_minimum(scenario_result) -> None:
        """Test that the baseline coincidence rate meets the baseline's minimum threshold."""
        cr = float(scenario_result.result.baseline_performance["coincidence_rate"])
        assert cr >= scenario_result.scenario.baseline_coincidence_rate_min

    @staticmethod
    def test_tvd_below_maximum(scenario_result) -> None:
        """Test that the compiled TVD is below the scenario's maximum threshold."""
        tvd = float(scenario_result.result.performance["tvd"])
        assert tvd <= scenario_result.scenario.tvd_max

    @staticmethod
    def test_baseline_tvd_below_maximum(scenario_result) -> None:
        """Test that the baseline TVD is below the baseline's maximum threshold."""
        tvd = float(scenario_result.result.baseline_performance["tvd"])
        assert tvd <= scenario_result.scenario.baseline_tvd_max

    @staticmethod
    def test_optimization_loss_below_maximum(scenario_result) -> None:
        """Test that the final optimization loss is below the convergence threshold."""
        assert float(scenario_result.result.loss) <= scenario_result.scenario.losses_max

    @staticmethod
    def test_baseline_loss_below_maximum(scenario_result) -> None:
        """Test that the baseline loss is below the convergence threshold."""
        assert float(scenario_result.result.baseline_loss) <= scenario_result.scenario.losses_max


@pytest.mark.parametrize("scenario_result", _SCENARIOS_WITH_LOSS, indirect=True)
def test_proposed_coincidence_rate_exceeds_baseline(scenario_result):
    """Test that the compiled coincidence rate is at least as high as the baseline.

    Only parametrized for lossy scenarios (t_low is not None): routing steers
    photons to lower-loss modes, so the proposed compiler should outperform the
    fixed-placement baseline.  With perfect transmission all paths are equivalent
    and no routing advantage is expected, so those scenarios are excluded entirely.
    """
    assert float(scenario_result.result.performance["coincidence_rate"]) >= float(
        scenario_result.result.baseline_performance["coincidence_rate"]
    )
