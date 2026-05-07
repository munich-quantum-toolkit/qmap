# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""Tests for data_collection.py.

collect_pipeline_results is kept intentionally small (1 setup, 1 unitary,
1 repeat, few iterations) so that the smoke test runs in seconds rather than
minutes.
"""

import pytest

pd = pytest.importorskip("pandas")
pytest.importorskip("perceval")
torch = pytest.importorskip("torch")

from mqt.qmap.ph.data_collection import Setup, build_setup_grid, collect_pipeline_results
from mqt.qmap.ph.subcircuit_compilation import OptimizationConfig


class TestBuildSetupGrid:
    """Tests for build_setup_grid."""

    @staticmethod
    def test_filters_target_larger_than_chip():
        """Test that setups where target_dim > num_modes are excluded."""
        setups = build_setup_grid([4], [6])
        assert setups == []

    @staticmethod
    def test_filters_odd_chip_dim():
        """Test that odd chip dimensions are excluded."""
        setups = build_setup_grid([5], [2])
        assert setups == []

    @staticmethod
    def test_filters_odd_target_dim():
        """Test that odd target dimensions are excluded."""
        setups = build_setup_grid([4], [3])
        assert setups == []

    @staticmethod
    def test_valid_single_setup():
        """Test that a single valid (num_modes, target_dim) pair produces one Setup."""
        setups = build_setup_grid([4], [2])
        assert len(setups) == 1
        assert setups[0] == Setup(num_modes=4, target_dim=2)

    @staticmethod
    def test_valid_multiple_setups():
        """Test that the Cartesian product of valid inputs produces all expected setups."""
        setups = build_setup_grid([4, 6], [2, 4])
        # (4,2), (4,4), (6,2), (6,4) — all even; (6,6) excluded because target==chip allowed
        assert Setup(num_modes=4, target_dim=2) in setups
        assert Setup(num_modes=4, target_dim=4) in setups
        assert Setup(num_modes=6, target_dim=2) in setups
        assert Setup(num_modes=6, target_dim=4) in setups

    @staticmethod
    def test_produces_setup_dataclass_instances():
        """Test that all returned items are Setup dataclass instances."""
        setups = build_setup_grid([4], [2])
        assert all(isinstance(s, Setup) for s in setups)


class TestCollectPipelineResults:
    """Smoke test: verify shape, column names, and basic value ranges."""

    @pytest.fixture(scope="class")
    @staticmethod
    def smoke_result():
        """Run a minimal pipeline sweep and return the aggregated DataFrame."""
        torch.manual_seed(0)

        setups = build_setup_grid([4], [2])
        return collect_pipeline_results(
            setups=setups,
            config=OptimizationConfig(num_restarts=1, max_iterations=50),
            num_unitaries_per_setup=1,
            repeats_per_unitary=1,
            phase_errors=[0.0],
            ideal_beam_splitters=True,
        )

    @staticmethod
    def test_returns_dataframe(smoke_result):
        """Test that collect_pipeline_results returns a DataFrame."""
        assert isinstance(smoke_result, pd.DataFrame)

    @staticmethod
    def test_one_row_per_setup_and_phase_error(smoke_result):
        """Test that the result has one row per (setup, phase_error) combination."""
        # 1 setup x 1 phase_error -> 1 row
        assert len(smoke_result) == 1

    @staticmethod
    def test_expected_columns_present(smoke_result):
        """Test that all required columns are present in the result."""
        required = {
            "num_modes",
            "target_dim",
            "phase_error",
            "avg_tvd",
            "avg_system_yield",
            "avg_baseline_tvd",
            "avg_baseline_system_yield",
            "tvd_difference",
            "system_yield_difference",
        }
        assert required.issubset(smoke_result.columns)

    @staticmethod
    def test_system_yield_in_unit_interval(smoke_result):
        """Test that system yield values lie in [0, 1]."""
        assert (smoke_result["avg_system_yield"] >= 0.0).all()
        assert (smoke_result["avg_system_yield"] <= 1.0).all()

    @staticmethod
    def test_tvd_non_negative(smoke_result):
        """Test that TVD values are non-negative."""
        assert (smoke_result["avg_tvd"] >= 0.0).all()

    @staticmethod
    def test_num_modes_and_target_dim_match_setup(smoke_result):
        """Test that num_modes and target_dim in the result match the requested setup."""
        assert smoke_result["num_modes"].iloc[0] == 4
        assert smoke_result["target_dim"].iloc[0] == 2
