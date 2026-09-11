# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""Tests for the Perceval-simulation helpers (``eval/ph/perceval_simulation.py``)."""

from __future__ import annotations

import pytest

pytest.importorskip("perceval")

from perceval_simulation import convert_input_ports


class TestConvertInputPorts:
    """Tests for convert_input_ports (index list -> Perceval occupancy vector)."""

    @staticmethod
    def test_first_mode_active() -> None:
        """Test that active port 0 on a 4-mode chip gives [1, 0, 0, 0]."""
        # input_ports=[0] on a 4-mode chip: mode 0 gets photon, mode 1 skipped
        result = convert_input_ports([0], chip_dim=4)
        assert result == [1, 0, 0, 0]

    @staticmethod
    def test_third_mode_active() -> None:
        """Test that active port 2 on a 4-mode chip gives [0, 0, 1, 0]."""
        # input_ports=[2]: mode 2 gets photon, mode 3 skipped
        result = convert_input_ports([2], chip_dim=4)
        assert result == [0, 0, 1, 0]

    @staticmethod
    def test_no_active_modes() -> None:
        """Test that no active ports yields an all-zero vector."""
        result = convert_input_ports([], chip_dim=4)
        assert result == [0, 0, 0, 0]

    @staticmethod
    def test_total_length_matches_chip_dim() -> None:
        """Test that the result length equals chip_dim."""
        result = convert_input_ports([0], chip_dim=6)
        assert len(result) == 6
