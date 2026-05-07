# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""Tests for the photonic compiler baseline module."""

import numpy as np
import pytest

torch = pytest.importorskip("torch")

from mqt.qmap.ph.baseline import embed_target_unitary_into_chip, get_baseline_active_cols, get_baseline_input_ports


class TestGetBaselineActiveCols:
    """Tests for get_baseline_active_cols."""

    @staticmethod
    def test_dim2_returns_only_zero():
        """Test that target_dim=2 yields only column 0."""
        assert get_baseline_active_cols(2) == [0]

    @staticmethod
    def test_dim4_returns_even_indices():
        """Test that target_dim=4 yields columns [0, 2]."""
        assert get_baseline_active_cols(4) == [0, 2]

    @staticmethod
    def test_dim6_returns_even_indices():
        """Test that target_dim=6 yields columns [0, 2, 4]."""
        assert get_baseline_active_cols(6) == [0, 2, 4]

    @staticmethod
    def test_length_is_half_target_dim():
        """Test that the number of active columns equals target_dim // 2."""
        for dim in (2, 4, 6, 8):
            cols = get_baseline_active_cols(dim)
            assert len(cols) == dim // 2

    @staticmethod
    def test_all_returned_indices_are_even():
        """Test that all returned column indices are even."""
        for dim in (2, 4, 6, 8):
            assert all(c % 2 == 0 for c in get_baseline_active_cols(dim))


class TestEmbedTargetUnitaryIntoChip:
    """Tests for embed_target_unitary_into_chip."""

    @staticmethod
    def test_2x2_identity_embedded_into_4x4():
        """Test that embedding a 2x2 identity into a 4x4 chip yields the 4x4 identity."""
        u = np.eye(2, dtype=complex)
        result = embed_target_unitary_into_chip(u, chip_dim=4, target_dim=2)
        expected = torch.eye(4, dtype=torch.complex128)
        assert torch.allclose(result, expected)

    @staticmethod
    def test_target_block_is_correctly_placed():
        """Test that the target unitary values appear in the top-left block."""
        u = np.array([[1 + 2j, 3 + 4j], [5 + 6j, 7 + 8j]])
        result = embed_target_unitary_into_chip(u, chip_dim=4, target_dim=2)

        assert result[0, 0] == pytest.approx(1 + 2j)
        assert result[0, 1] == pytest.approx(3 + 4j)
        assert result[1, 0] == pytest.approx(5 + 6j)
        assert result[1, 1] == pytest.approx(7 + 8j)

    @staticmethod
    def test_identity_block_preserved_outside_target():
        """Test that the identity is preserved in the rows/cols outside the target block."""
        u = np.eye(2, dtype=complex) * 2  # non-identity to make the test meaningful
        result = embed_target_unitary_into_chip(u, chip_dim=4, target_dim=2)

        # Rows/cols 2 and 3 should still be the identity
        assert result[2, 2] == pytest.approx(1.0)
        assert result[3, 3] == pytest.approx(1.0)
        assert result[2, 3] == pytest.approx(0.0)
        assert result[3, 2] == pytest.approx(0.0)

    @staticmethod
    def test_output_shape():
        """Test that the embedded matrix has shape (chip_dim, chip_dim)."""
        u = np.eye(2, dtype=complex)
        result = embed_target_unitary_into_chip(u, chip_dim=6, target_dim=2)
        assert result.shape == (6, 6)


class TestGetBaselineInputPorts:
    """Tests for get_baseline_input_ports."""

    @staticmethod
    def test_active_cols_zero_on_chip4():
        """Test that active col 0 on a 4-mode chip gives [1, 0, 0, 0]."""
        result = get_baseline_input_ports([0], chip_dim=4)
        assert result == [1, 0, 0, 0]

    @staticmethod
    def test_active_cols_zero_and_two_on_chip4():
        """Test that active cols [0, 2] on a 4-mode chip gives [1, 0, 1, 0]."""
        result = get_baseline_input_ports([0, 2], chip_dim=4)
        assert result == [1, 0, 1, 0]

    @staticmethod
    def test_length_equals_chip_dim():
        """Test that the result length equals chip_dim."""
        result = get_baseline_input_ports([0], chip_dim=6)
        assert len(result) == 6

    @staticmethod
    def test_no_active_cols_returns_all_zeros():
        """Test that no active columns yields an all-zero vector."""
        result = get_baseline_input_ports([], chip_dim=4)
        assert result == [0, 0, 0, 0]
