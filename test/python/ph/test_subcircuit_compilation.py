# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""Regression tests for subcircuit_compilation.compile_subcircuit().

Six scenarios: chip_dim in {8, 16} x phase_error in {0.0, 0.015, 0.030}, target_dim=4.
All tests run for each scenario via the parametrized `run_result` fixture.

Setup: ideal BS, IO transmissions sampled from Uniform[0.7, 1.0] (normalized),
       matching data_collection.collect_pipeline_results with base_seed=0.
       1 restart, 300 iterations, torch.manual_seed(0) immediately before compile_subcircuit().

Adjust the bounds in TestRunValueRanges based on observed results.
"""

import numpy as np
import pytest

pytest.importorskip("perceval")
torch = pytest.importorskip("torch")

from mqt.qmap.ph.baseline import embed_target_unitary_into_chip
from mqt.qmap.ph.graph import generate_beam_splitter_matrix
from mqt.qmap.ph.subcircuit_compilation import OptimizationConfig, RunResult, compile_subcircuit
from mqt.qmap.ph.unitary_to_phase_compilation import get_haar_random_unitary

_PERF_KEYS = {"compensated_weight_sum", "mapped_distribution", "system_yield", "tvd"}

_SCENARIOS = [
    pytest.param((8, 4, 0.000), id="chip8-target4-pe0.000"),
    pytest.param((8, 4, 0.015), id="chip8-target4-pe0.015"),
    pytest.param((8, 4, 0.030), id="chip8-target4-pe0.030"),
    pytest.param((16, 4, 0.000), id="chip16-target4-pe0.000"),
    pytest.param((16, 4, 0.015), id="chip16-target4-pe0.015"),
    pytest.param((16, 4, 0.030), id="chip16-target4-pe0.030"),
]


@pytest.fixture(scope="module", params=_SCENARIOS)
def run_result(request):
    """Run compile_subcircuit for one (chip_dim, target_dim, phase_error) scenario."""
    chip_dim, target_dim, phase_error = request.param

    bs = generate_beam_splitter_matrix(chip_size=chip_dim, ideal_bs=True)

    # Mirror data_collection.collect_pipeline_results: seeded Uniform[0.7, 1.0], normalized.
    base_seed = 0
    hw_rng = np.random.default_rng(base_seed + 10 * chip_dim)
    input_t = hw_rng.uniform(0.7, 1.0, size=chip_dim)
    input_t /= np.max(input_t)
    output_t = hw_rng.uniform(0.7, 1.0, size=chip_dim)
    output_t /= np.max(output_t)

    rng = torch.Generator().manual_seed(7)
    target_unitary = get_haar_random_unitary(target_dim, rng, dtype=torch.complex128)
    embedded = embed_target_unitary_into_chip(target_unitary.cpu().numpy(), chip_dim=chip_dim, target_dim=target_dim)

    torch.manual_seed(0)

    return compile_subcircuit(
        beam_splitter_reflectivities=bs,
        input_transmissions=input_t,
        output_transmissions=output_t,
        target_unitary=target_unitary,
        target_unitary_embedded=embedded,
        phase_error=phase_error,
        config=OptimizationConfig(num_restarts=1, max_iterations=300),
    )


class TestRunReturnStructure:
    """Tests verifying the structure and types of the RunResult returned by compile_subcircuit."""

    @staticmethod
    def test_returns_run_result(run_result):
        """Test that compile_subcircuit returns a RunResult instance."""
        assert isinstance(run_result, RunResult)

    @staticmethod
    def test_performance_dict_has_required_keys(run_result):
        """Test that the performance dict contains all required metric keys."""
        assert set(run_result.performance.keys()) >= _PERF_KEYS

    @staticmethod
    def test_baseline_performance_dict_has_required_keys(run_result):
        """Test that the baseline performance dict contains all required metric keys."""
        assert set(run_result.baseline_performance.keys()) >= _PERF_KEYS

    @staticmethod
    def test_losses_is_float(run_result):
        """Test that loss and baseline_loss are convertible to float."""
        assert isinstance(float(run_result.loss), float)
        assert isinstance(float(run_result.baseline_loss), float)

    @staticmethod
    def test_compute_times_are_positive(run_result):
        """Test that compute_time and baseline_compute_time are strictly positive."""
        assert run_result.compute_time > 0
        assert run_result.baseline_compute_time > 0

    @staticmethod
    def test_system_yield_in_unit_interval(run_result):
        """Test that system_yield values lie in [0, 1] for both compiled and baseline."""
        assert 0.0 <= float(run_result.performance["system_yield"]) <= 1.0
        assert 0.0 <= float(run_result.baseline_performance["system_yield"]) <= 1.0

    @staticmethod
    def test_tvd_in_unit_interval(run_result):
        """Test that TVD values lie in [0, 1] for both compiled and baseline."""
        assert 0.0 <= float(run_result.performance["tvd"]) <= 1.0
        assert 0.0 <= float(run_result.baseline_performance["tvd"]) <= 1.0

    @staticmethod
    def test_losses_non_negative(run_result):
        """Test that loss and baseline_loss are non-negative."""
        assert float(run_result.loss) >= 0.0
        assert float(run_result.baseline_loss) >= 0.0


class TestRunValueRanges:
    """Range-based regression checks.

    Tighten these bounds after observing typical results — they should be
    strict enough to catch semantic regressions but loose enough to tolerate
    minor numerical variation across runs.  These bounds must hold for all
    six scenarios, including phase_error=0.030 (the hardest case).
    """

    # Minimum acceptable system yield (routing + optimization must be useful)
    SYSTEM_YIELD_MIN = 0.5
    # Maximum acceptable TVD (distribution must be close to ideal)
    TVD_MAX = 0.2
    # Maximum acceptable final optimization loss (optimizer must converge)
    LOSSES_MAX = 1e-2

    @staticmethod
    def test_system_yield_above_minimum(run_result):
        """Test that the compiled system yield meets the minimum threshold."""
        assert float(run_result.performance["system_yield"]) >= TestRunValueRanges.SYSTEM_YIELD_MIN

    @staticmethod
    def test_baseline_yield_above_minimum(run_result):
        """Test that the baseline system yield meets the minimum threshold."""
        assert float(run_result.baseline_performance["system_yield"]) >= TestRunValueRanges.SYSTEM_YIELD_MIN

    @staticmethod
    def test_tvd_below_maximum(run_result):
        """Test that the compiled TVD is below the maximum threshold."""
        assert float(run_result.performance["tvd"]) <= TestRunValueRanges.TVD_MAX

    @staticmethod
    def test_baseline_tvd_below_maximum(run_result):
        """Test that the baseline TVD is below the maximum threshold."""
        assert float(run_result.baseline_performance["tvd"]) <= TestRunValueRanges.TVD_MAX

    @staticmethod
    def test_optimization_loss_below_maximum(run_result):
        """Test that the final optimization loss is below the convergence threshold."""
        assert float(run_result.loss) <= TestRunValueRanges.LOSSES_MAX

    @staticmethod
    def test_baseline_loss_below_maximum(run_result):
        """Test that the baseline loss is below the convergence threshold."""
        assert float(run_result.baseline_loss) <= TestRunValueRanges.LOSSES_MAX

    @staticmethod
    def test_proposed_yield_exceeds_baseline(run_result):
        """Test that the compiled system yield is at least as high as the baseline."""
        assert float(run_result.performance["system_yield"]) >= float(run_result.baseline_performance["system_yield"])
