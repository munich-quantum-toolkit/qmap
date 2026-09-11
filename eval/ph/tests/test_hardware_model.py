# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""Tests for the synthetic hardware model (``eval/ph/hardware_model.py``)."""

from __future__ import annotations

import numpy as np
from hardware_model import generate_beam_splitter_matrix


class TestGenerateBeamSplitterMatrix:
    """Tests for generate_beam_splitter_matrix."""

    @staticmethod
    def test_ideal_returns_all_half() -> None:
        """Test that ideal mode returns all 0.5 reflectivities."""
        bs = generate_beam_splitter_matrix(chip_size=4, ideal_bs=True)
        assert np.allclose(bs, 0.5)

    @staticmethod
    def test_ideal_correct_size_chip4() -> None:
        """Test that a 4-mode chip yields 12 beam-splitter values."""
        # chip_size=4: MZIs per layer [2, 1, 2, 1] -> 6 total -> 12 BS values
        bs = generate_beam_splitter_matrix(chip_size=4, ideal_bs=True)
        assert len(bs) == 12

    @staticmethod
    def test_ideal_correct_size_chip6() -> None:
        """Test that a 6-mode chip yields 30 beam-splitter values."""
        # chip_size=6: MZIs per layer [3, 2, 3, 2, 3, 2] -> 15 total -> 30 BS values
        bs = generate_beam_splitter_matrix(chip_size=6, ideal_bs=True)
        assert len(bs) == 30

    @staticmethod
    def test_random_has_correct_size_chip4() -> None:
        """Test that random mode also yields 12 values for a 4-mode chip."""
        bs = generate_beam_splitter_matrix(chip_size=4, ideal_bs=False, rng=np.random.default_rng(0))
        assert len(bs) == 12

    @staticmethod
    def test_random_values_in_unit_interval() -> None:
        """Test that randomly sampled reflectivities lie in [0, 1]."""
        bs = generate_beam_splitter_matrix(chip_size=4, ideal_bs=False, rng=np.random.default_rng(0))
        assert np.all(bs >= 0.0)
        assert np.all(bs <= 1.0)
