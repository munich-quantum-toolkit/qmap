import math

import pytest

torch = pytest.importorskip("torch")

from mqt.qmap.ph.routing import MaskState
from mqt.qmap.ph.routing_to_phases import get_effective_params_and_mask, reshape_flattened_params_to_grid


class TestReshapeFlattenedParamsToGrid:
    def test_no_exclude_sequential_fill(self):
        params = torch.arange(16, dtype=torch.float64)
        grid = reshape_flattened_params_to_grid(params, num_modes=4, exclude_edge_phase_shifters=False)
        assert grid.shape == (4, 4)
        assert torch.equal(grid, params.reshape(4, 4))

    def test_exclude_edge_zeroes_corners(self):
        params = torch.arange(14, dtype=torch.float64)
        grid = reshape_flattened_params_to_grid(params, num_modes=4, exclude_edge_phase_shifters=True)
        assert grid.shape == (4, 4)
        assert grid[0, 3].item() == 0.0   # top-right corner
        assert grid[3, 3].item() == 0.0   # bottom-right corner

    def test_exclude_edge_fills_remaining_14_positions(self):
        params = torch.ones(14, dtype=torch.float64)
        grid = reshape_flattened_params_to_grid(params, num_modes=4, exclude_edge_phase_shifters=True)
        # Exactly 2 zeros (the corners), 14 ones
        assert (grid == 0.0).sum().item() == 2
        assert (grid == 1.0).sum().item() == 14

    def test_wrong_size_raises_value_error(self):
        with pytest.raises(ValueError):
            reshape_flattened_params_to_grid(torch.zeros(10), num_modes=4)

    def test_wrong_size_exclude_raises_value_error(self):
        with pytest.raises(ValueError):
            reshape_flattened_params_to_grid(torch.zeros(16), num_modes=4, exclude_edge_phase_shifters=True)


class TestGetEffectiveParamsAndMask:
    def _bar_mask(self, chip_dim):
        return torch.ones((chip_dim, chip_dim), dtype=torch.int)

    def _cross_mask(self, chip_dim):
        return torch.full((chip_dim, chip_dim), MaskState.CROSS, dtype=torch.int)

    def test_all_bar_mask_forces_zero_pi_no_optimize(self):
        chip_dim = 4
        mask = self._bar_mask(chip_dim)
        raw = torch.zeros((chip_dim, chip_dim), dtype=torch.float64)

        eff, grad, _ = get_effective_params_and_mask(chip_dim, mask, raw, optimize_routing_parameters=False)

        # Even layers: each MZI pair → (top=0, bot=pi)
        for layer in range(0, chip_dim, 2):
            for top in range(0, chip_dim - 1, 2):
                assert eff[top, layer].item() == pytest.approx(0.0)
                assert eff[top + 1, layer].item() == pytest.approx(math.pi)

    def test_all_bar_mask_zeros_gradients_no_optimize(self):
        chip_dim = 4
        mask = self._bar_mask(chip_dim)
        raw = torch.zeros((chip_dim, chip_dim), dtype=torch.float64)

        _, grad, _ = get_effective_params_and_mask(chip_dim, mask, raw, optimize_routing_parameters=False)

        assert torch.all(grad == 0.0)

    def test_all_cross_mask_forces_both_zero_no_optimize(self):
        chip_dim = 4
        mask = self._cross_mask(chip_dim)
        raw = torch.zeros((chip_dim, chip_dim), dtype=torch.float64)

        eff, grad, _ = get_effective_params_and_mask(chip_dim, mask, raw, optimize_routing_parameters=False)

        assert torch.all(eff == 0.0)
        assert torch.all(grad == 0.0)

    def test_mzi_zone_passes_through_nonzero_params(self):
        chip_dim = 4
        mask = torch.zeros((chip_dim, chip_dim), dtype=torch.int)  # all MaskState.MZI
        raw = torch.ones((chip_dim, chip_dim), dtype=torch.float64)

        eff, grad, _ = get_effective_params_and_mask(chip_dim, mask, raw, optimize_routing_parameters=False)

        # Non-zero MZI params should pass through unchanged
        assert torch.allclose(eff, raw)

    def test_returns_three_tensors(self):
        chip_dim = 4
        mask = self._bar_mask(chip_dim)
        raw = torch.zeros((chip_dim, chip_dim), dtype=torch.float64)

        result = get_effective_params_and_mask(chip_dim, mask, raw)
        assert len(result) == 3
