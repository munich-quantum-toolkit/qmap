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


@pytest.mark.parametrize("state", list(MaskState))
@pytest.mark.parametrize("optimize_routing", [False, True])
def test_routing_constraints_and_gradients(state: MaskState, optimize_routing: bool) -> None:
    """Routing phases and their derivatives follow the physical pair constraints."""
    mask = torch.full((4, 4), state, dtype=torch.int)
    transform = precompute_routing_transform(mask, optimize_routing)
    weights = torch.arange(16, dtype=torch.float64).reshape(4, 4)
    for values in (torch.zeros((4, 4), dtype=torch.float64), weights / 10):
        raw = values.clone().requires_grad_(True)
        expected = raw.clone()
        for layer in range(4):
            if layer % 2 and state in {MaskState.BAR, MaskState.CROSS}:
                expected[0, layer] = expected[-1, layer] = 0
            for top in range(layer % 2, 3, 2):
                bottom = top + 1
                if state == MaskState.BAR:
                    expected[top, layer] = raw[top, layer] if optimize_routing else 0.0
                    expected[bottom, layer] = (raw[top, layer] if optimize_routing else 0.0) + math.pi
                elif state == MaskState.CROSS:
                    expected[top, layer] = expected[bottom, layer] = raw[top, layer] if optimize_routing else 0.0
                elif state == MaskState.TOP_ONLY:
                    expected[bottom, layer] = raw[top, layer] + math.pi
                elif state == MaskState.BOT_ONLY:
                    expected[top, layer] = raw[bottom, layer] + math.pi
        actual = apply_routing_transform(raw, transform)
        torch.testing.assert_close(actual, expected, atol=1e-12, rtol=1e-12)
        actual_grad = torch.autograd.grad((actual * weights).sum(), raw)[0]
        expected_grad = torch.autograd.grad((expected * weights).sum(), raw)[0]
        torch.testing.assert_close(actual_grad, expected_grad, atol=1e-12, rtol=1e-12)


def test_mixed_pair_uses_higher_priority_state() -> None:
    """A virtual bottom phase takes precedence over a bar state in the same pair."""
    mask = torch.zeros((4, 4), dtype=torch.int)
    mask[1, 1] = MaskState.BAR
    mask[2, 1] = MaskState.BOT_ONLY
    raw = torch.arange(16, dtype=torch.float64).reshape(4, 4)
    effective = apply_routing_transform(raw, precompute_routing_transform(mask))
    assert effective[1, 1] == pytest.approx(raw[2, 1] + math.pi)
    assert effective[2, 1] == raw[2, 1]
