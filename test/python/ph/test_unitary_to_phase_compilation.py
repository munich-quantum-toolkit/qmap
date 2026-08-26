# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""Tests for unitary_to_phase_compilation.optimize_unitary_subcircuit_parameters."""

import pytest

torch = pytest.importorskip("torch")

from mqt.qmap.ph.unitary_to_phase_compilation import optimize_unitary_subcircuit_parameters


class TestOptimizeMinimumIterations:
    """Regression tests for the minimum-iteration guard."""

    @staticmethod
    def test_max_iterations_is_clamped_to_two() -> None:
        """Test that ``max_iterations`` is clamped to a floor of two steps.

        With a single raw iteration the loop would evaluate the initial parameters,
        take one optimizer step, and terminate before ever evaluating that step -
        returning the initial parameters.  The optimizer clamps ``max_iterations`` to
        a minimum of 2 so the step's result is evaluated too.  The clamp is not
        exposed as a field, but it is observable: ``max_iterations=1`` must produce
        exactly the same result as ``max_iterations=2`` (both run two steps), and a
        genuinely different result from ``max_iterations=3`` (which runs one more).
        """
        chip_dim = 4
        # chip_dim=4: MZIs per layer [2, 1, 2, 1] -> 6 MZIs -> 12 ideal reflectivities.
        bs = torch.as_tensor([0.5] * 12, dtype=torch.float64)
        # A generic, reproducible unitary target (Q factor of a seeded complex Gaussian).
        z = torch.randn(chip_dim, chip_dim, generator=torch.Generator().manual_seed(1), dtype=torch.complex128)
        target_unitary, _ = torch.linalg.qr(z)

        def run(max_iterations: int):
            # Seed immediately before each run so the random parameter init is identical.
            torch.manual_seed(0)
            return optimize_unitary_subcircuit_parameters(
                target_unitary=target_unitary,
                beam_splitter_reflectivities=bs,
                max_iterations=max_iterations,
            )

        clamped = run(1)
        floor = run(2)
        one_more = run(3)

        # max_iterations=1 is clamped up to 2, so it matches the max_iterations=2 run exactly.
        assert torch.equal(clamped.phase_shifter_params, floor.phase_shifter_params)
        assert clamped.best_loss == floor.best_loss
        # ...and the floor really is two steps: a third step changes the result.
        assert not torch.equal(floor.phase_shifter_params, one_more.phase_shifter_params)
