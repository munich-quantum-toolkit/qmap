"""
Tests for data_collection.py.

collect_pipeline_results is kept intentionally small (1 setup, 1 unitary,
1 repeat, few iterations) so that the smoke test runs in seconds rather than
minutes.
"""

import numpy as np
import pytest

pd = pytest.importorskip("pandas")
pytest.importorskip("perceval")
torch = pytest.importorskip("torch")

from mqt.qmap.ph.data_collection import Setup, build_setup_grid, collect_pipeline_results
from mqt.qmap.ph.subcircuit_compilation import OptimizationConfig


class TestBuildSetupGrid:
    def test_filters_target_larger_than_chip(self):
        setups = build_setup_grid([4], [6])
        assert setups == []

    def test_filters_odd_chip_dim(self):
        setups = build_setup_grid([5], [2])
        assert setups == []

    def test_filters_odd_target_dim(self):
        setups = build_setup_grid([4], [3])
        assert setups == []

    def test_valid_single_setup(self):
        setups = build_setup_grid([4], [2])
        assert len(setups) == 1
        assert setups[0] == Setup(num_modes=4, target_dim=2)

    def test_valid_multiple_setups(self):
        setups = build_setup_grid([4, 6], [2, 4])
        # (4,2), (4,4), (6,2), (6,4) — all even; (6,6) excluded because target==chip allowed
        assert Setup(num_modes=4, target_dim=2) in setups
        assert Setup(num_modes=4, target_dim=4) in setups
        assert Setup(num_modes=6, target_dim=2) in setups
        assert Setup(num_modes=6, target_dim=4) in setups

    def test_produces_setup_dataclass_instances(self):
        setups = build_setup_grid([4], [2])
        assert all(isinstance(s, Setup) for s in setups)


class TestCollectPipelineResults:
    """Smoke test: verify shape, column names, and basic value ranges."""

    @pytest.fixture(scope="class")
    def smoke_result(self):
        torch.manual_seed(0)
        np.random.seed(0)

        setups = build_setup_grid([4], [2])
        return collect_pipeline_results(
            setups=setups,
            config=OptimizationConfig(num_restarts=1, max_iterations=50),
            num_unitaries_per_setup=1,
            repeats_per_unitary=1,
            phase_errors=[0.0],
            ideal_beam_splitters=True,
        )

    def test_returns_dataframe(self, smoke_result):
        assert isinstance(smoke_result, pd.DataFrame)

    def test_one_row_per_setup_and_phase_error(self, smoke_result):
        # 1 setup × 1 phase_error → 1 row
        assert len(smoke_result) == 1

    def test_expected_columns_present(self, smoke_result):
        required = {
            "num_modes", "target_dim", "phase_error",
            "avg_tvd", "avg_system_yield",
            "avg_baseline_tvd", "avg_baseline_system_yield",
            "tvd_difference", "system_yield_difference",
        }
        assert required.issubset(smoke_result.columns)

    def test_system_yield_in_unit_interval(self, smoke_result):
        assert (smoke_result["avg_system_yield"] >= 0.0).all()
        assert (smoke_result["avg_system_yield"] <= 1.0).all()

    def test_tvd_non_negative(self, smoke_result):
        assert (smoke_result["avg_tvd"] >= 0.0).all()

    def test_num_modes_and_target_dim_match_setup(self, smoke_result):
        assert smoke_result["num_modes"].iloc[0] == 4
        assert smoke_result["target_dim"].iloc[0] == 2
