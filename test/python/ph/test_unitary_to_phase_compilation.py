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


class TestMaxIterationsContract:
    """``max_iterations`` is an exact bound on the number of gradient steps."""

    @staticmethod
    def _run(max_iterations: int):
        """Optimize a fixed 4-mode target for exactly ``max_iterations`` steps."""
        # chip_dim=4: MZIs per layer [2, 1, 2, 1] -> 6 MZIs -> 12 ideal reflectivities.
        bs = torch.as_tensor([0.5] * 12, dtype=torch.float64)
        # A generic, reproducible unitary target (Q factor of a seeded complex Gaussian).
        z = torch.randn(4, 4, generator=torch.Generator().manual_seed(1), dtype=torch.complex128)
        target_unitary, _ = torch.linalg.qr(z)
        # Seed immediately before each run so the random parameter init is identical.
        torch.manual_seed(0)
        return optimize_unitary_subcircuit_parameters(
            target_unitary=target_unitary,
            beam_splitter_reflectivities=bs,
            max_iterations=max_iterations,
        )

    def test_zero_iterations_performs_no_step(self) -> None:
        """``max_iterations=0`` returns the (evaluated) initial state, taking no step."""
        # Two zero-step runs are identical, and a single step changes the result -
        # so zero really is a no-optimization run, not silently bumped to one.
        assert torch.equal(self._run(0).phase_shifter_params, self._run(0).phase_shifter_params)
        assert not torch.equal(self._run(0).phase_shifter_params, self._run(1).phase_shifter_params)

    def test_each_step_counts_exactly(self) -> None:
        """0, 1, and 2 steps each produce a distinct result (no clamp collapsing 0/1 onto 2)."""
        zero, one, two = self._run(0), self._run(1), self._run(2)
        # The old floor-of-two clamp forced run(1) == run(2); an exact bound must not.
        assert not torch.equal(zero.phase_shifter_params, one.phase_shifter_params)
        assert not torch.equal(one.phase_shifter_params, two.phase_shifter_params)

    def test_more_steps_never_worsen_best_loss(self) -> None:
        """The returned best loss is non-increasing in the step budget."""
        losses = [self._run(mi).best_loss for mi in (0, 1, 2, 3)]
        assert losses == sorted(losses, reverse=True)
