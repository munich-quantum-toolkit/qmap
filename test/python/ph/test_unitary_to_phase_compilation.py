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

from mqt.qmap.ph.graph import generate_beam_splitter_matrix
from mqt.qmap.ph.unitary_to_phase_compilation import (
    get_haar_random_unitary,
    optimize_unitary_subcircuit_parameters,
)


class TestOptimizeMinimumIterations:
    """Regression tests for the minimum-iteration guard."""

    @staticmethod
    def test_single_iteration_still_evaluates_the_optimizer_step() -> None:
        """Test that ``max_iterations=1`` still evaluates the optimizer step's result.

        With a single raw iteration the loop would evaluate the initial parameters,
        take one optimizer step, and terminate before ever evaluating that step -
        returning the initial parameters.  The optimizer clamps ``max_iterations`` to
        a minimum of 2, so both the initial parameters and the step's result are
        evaluated (two recorded losses).
        """
        torch.manual_seed(0)
        chip_dim = 4
        bs = torch.as_tensor(generate_beam_splitter_matrix(chip_size=chip_dim, ideal_bs=True), dtype=torch.float64)
        target_unitary = get_haar_random_unitary(chip_dim, torch.Generator().manual_seed(1), dtype=torch.complex128)

        result = optimize_unitary_subcircuit_parameters(
            target_unitary=target_unitary,
            beam_splitter_reflectivities=bs,
            max_iterations=1,
        )

        # Clamped to 2 iterations: the initial params and the first step are both evaluated.
        assert result.iterations == 2
        assert len(result.losses) == 2
