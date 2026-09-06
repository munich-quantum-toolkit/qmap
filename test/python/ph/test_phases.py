# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""Tests for the routing-to-phases conversion module."""

import math

import pytest

torch = pytest.importorskip("torch")

from mqt.qmap.ph.routing import MaskState
from mqt.qmap.ph.routing_to_phases import (
    apply_routing_transform,
    get_effective_params_and_mask,
    precompute_routing_transform,
    reshape_flattened_params_to_grid,
)


class TestReshapeFlattenedParamsToGrid:
    """Tests for reshape_flattened_params_to_grid."""

    @staticmethod
    def test_no_exclude_sequential_fill() -> None:
        """Test that a flat parameter vector is reshaped into an (N, N) grid without excluded corners."""
        params = torch.arange(16, dtype=torch.float64)
        grid = reshape_flattened_params_to_grid(params, num_modes=4, exclude_edge_phase_shifters=False)
        assert grid.shape == (4, 4)
        assert torch.equal(grid, params.reshape(4, 4))

    @staticmethod
    def test_exclude_edge_zeroes_corners() -> None:
        """Test that excluded corners are set to zero in the output grid."""
        params = torch.arange(14, dtype=torch.float64)
        grid = reshape_flattened_params_to_grid(params, num_modes=4, exclude_edge_phase_shifters=True)
        assert grid.shape == (4, 4)
        assert not grid[0, 3].item()  # top-right corner
        assert not grid[3, 3].item()  # bottom-right corner

    @staticmethod
    def test_exclude_edge_fills_remaining_14_positions() -> None:
        """Test that excluding corners leaves exactly 14 active positions filled with ones."""
        params = torch.ones(14, dtype=torch.float64)
        grid = reshape_flattened_params_to_grid(params, num_modes=4, exclude_edge_phase_shifters=True)
        # Exactly 2 zeros (the corners), 14 ones
        assert (~grid.bool()).sum().item() == 2
        assert grid.bool().sum().item() == 14

    @staticmethod
    def test_wrong_size_raises_value_error() -> None:
        """Test that an incorrectly sized parameter vector raises ValueError."""
        with pytest.raises(ValueError, match="Size mismatch"):
            reshape_flattened_params_to_grid(torch.zeros(10), num_modes=4)

    @staticmethod
    def test_wrong_size_exclude_raises_value_error() -> None:
        """Test that a 16-element vector raises ValueError when corner exclusion expects 14."""
        with pytest.raises(ValueError, match="Size mismatch"):
            reshape_flattened_params_to_grid(torch.zeros(16), num_modes=4, exclude_edge_phase_shifters=True)


class TestGetEffectiveParamsAndMask:
    """Tests for get_effective_params_and_mask."""

    @staticmethod
    def _bar_mask(chip_dim) -> torch.Tensor:
        return torch.ones((chip_dim, chip_dim), dtype=torch.int)

    @staticmethod
    def _cross_mask(chip_dim) -> torch.Tensor:
        return torch.full((chip_dim, chip_dim), MaskState.CROSS, dtype=torch.int)

    def test_all_bar_mask_forces_zero_pi_no_optimize(self) -> None:
        """Test that a full bar mask sets even-layer MZI pairs to (0, pi) when routing optimization is disabled."""
        chip_dim = 4
        mask = self._bar_mask(chip_dim)
        raw = torch.zeros((chip_dim, chip_dim), dtype=torch.float64)

        eff, _grad = get_effective_params_and_mask(chip_dim, mask, raw, optimize_routing_parameters=False)

        # Even layers: each MZI pair -> (top=0, bot=pi)
        for layer in range(0, chip_dim, 2):
            for top in range(0, chip_dim - 1, 2):
                assert eff[top, layer].item() == pytest.approx(0.0)
                assert eff[top + 1, layer].item() == pytest.approx(math.pi)

    def test_all_bar_mask_zeros_gradients_no_optimize(self) -> None:
        """Test that a full bar mask zeros all gradients when routing optimization is disabled."""
        chip_dim = 4
        mask = self._bar_mask(chip_dim)
        raw = torch.zeros((chip_dim, chip_dim), dtype=torch.float64)

        _, grad = get_effective_params_and_mask(chip_dim, mask, raw, optimize_routing_parameters=False)

        assert not grad.any()

    def test_all_cross_mask_forces_both_zero_no_optimize(self) -> None:
        """Test that a full cross mask forces effective params to zero when routing optimization is disabled."""
        chip_dim = 4
        mask = self._cross_mask(chip_dim)
        raw = torch.zeros((chip_dim, chip_dim), dtype=torch.float64)

        eff, grad = get_effective_params_and_mask(chip_dim, mask, raw, optimize_routing_parameters=False)

        assert not eff.any()
        assert not grad.any()

    @staticmethod
    def test_mzi_zone_passes_through_nonzero_params() -> None:
        """Test that MZI-zone parameters pass through unchanged when routing optimization is disabled."""
        chip_dim = 4
        mask = torch.zeros((chip_dim, chip_dim), dtype=torch.int)  # all MaskState.MZI
        raw = torch.ones((chip_dim, chip_dim), dtype=torch.float64)

        eff, _grad = get_effective_params_and_mask(chip_dim, mask, raw, optimize_routing_parameters=False)

        # Non-zero MZI params should pass through unchanged
        assert torch.allclose(eff, raw)

    @staticmethod
    def test_mzi_zone_zero_params_stay_trainable() -> None:
        """Test that a compute MZI with both phases near zero keeps its (0, 0) phases and gradients.

        The compute/routing distinction comes from the structural mask, not from
        transient phase magnitudes, so an all-zero compute MZI must not be sealed
        to a bar state (0, pi) or have its gradients frozen.
        """
        chip_dim = 4
        mask = torch.zeros((chip_dim, chip_dim), dtype=torch.int)  # all MaskState.MZI
        raw = torch.zeros((chip_dim, chip_dim), dtype=torch.float64)

        eff, grad = get_effective_params_and_mask(chip_dim, mask, raw, optimize_routing_parameters=False)

        assert not eff.any()  # phases remain (0, 0), not overwritten to (0, pi)
        assert grad.all()  # every compute cell stays trainable

    def test_returns_two_tensors(self) -> None:
        """Test that get_effective_params_and_mask returns a tuple of two tensors."""
        chip_dim = 4
        mask = self._bar_mask(chip_dim)
        raw = torch.zeros((chip_dim, chip_dim), dtype=torch.float64)

        result = get_effective_params_and_mask(chip_dim, mask, raw)
        assert len(result) == 2


class TestRoutingTransform:
    """Tests for the precompute/apply split of the routing-to-phase conversion."""

    @staticmethod
    def test_precompute_apply_matches_wrapper() -> None:
        """Test that precompute + apply reproduces the one-shot wrapper exactly."""
        chip_dim = 8
        mask = torch.full((chip_dim, chip_dim), MaskState.CROSS, dtype=torch.int)
        raw = torch.rand((chip_dim, chip_dim), dtype=torch.float64)

        eff_ref, grad_ref = get_effective_params_and_mask(chip_dim, mask, raw, optimize_routing_parameters=True)
        transform = precompute_routing_transform(chip_dim, mask, raw.shape[1], optimize_routing_parameters=True)
        eff, grad = apply_routing_transform(raw, transform)

        assert torch.equal(eff, eff_ref)
        assert torch.equal(grad, grad_ref)

    @staticmethod
    def test_transform_is_phase_independent() -> None:
        """Test that one precomputed transform serves any phase values (the point of the split).

        The grad mask is constant, and applying the transform to two different phase
        grids matches computing each from scratch - so it is safe to precompute once
        and reuse across optimizer iterations.
        """
        chip_dim = 8
        mask = torch.ones((chip_dim, chip_dim), dtype=torch.int)  # all BAR
        transform = precompute_routing_transform(chip_dim, mask, chip_dim, optimize_routing_parameters=False)

        raw_a = torch.rand((chip_dim, chip_dim), dtype=torch.float64)
        raw_b = torch.rand((chip_dim, chip_dim), dtype=torch.float64)
        eff_a, grad_a = apply_routing_transform(raw_a, transform)
        eff_b, grad_b = apply_routing_transform(raw_b, transform)

        assert torch.equal(grad_a, grad_b)  # grad mask does not depend on the phases
        assert torch.equal(eff_a, get_effective_params_and_mask(chip_dim, mask, raw_a)[0])
        assert torch.equal(eff_b, get_effective_params_and_mask(chip_dim, mask, raw_b)[0])
