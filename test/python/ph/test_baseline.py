import numpy as np
import pytest

torch = pytest.importorskip("torch")

from mqt.qmap.ph.baseline import embed_target_unitary_into_chip, get_baseline_active_cols, get_baseline_input_ports


class TestGetBaselineActiveCols:
    def test_dim2_returns_only_zero(self):
        assert get_baseline_active_cols(2) == [0]

    def test_dim4_returns_even_indices(self):
        assert get_baseline_active_cols(4) == [0, 2]

    def test_dim6_returns_even_indices(self):
        assert get_baseline_active_cols(6) == [0, 2, 4]

    def test_length_is_half_target_dim(self):
        for dim in (2, 4, 6, 8):
            cols = get_baseline_active_cols(dim)
            assert len(cols) == dim // 2

    def test_all_returned_indices_are_even(self):
        for dim in (2, 4, 6, 8):
            assert all(c % 2 == 0 for c in get_baseline_active_cols(dim))


class TestEmbedTargetUnitaryIntoChip:
    def test_2x2_identity_embedded_into_4x4(self):
        U = np.eye(2, dtype=complex)
        result = embed_target_unitary_into_chip(U, chip_dim=4, target_dim=2)
        expected = torch.eye(4, dtype=torch.complex128)
        assert torch.allclose(result, expected)

    def test_target_block_is_correctly_placed(self):
        U = np.array([[1 + 2j, 3 + 4j], [5 + 6j, 7 + 8j]])
        result = embed_target_unitary_into_chip(U, chip_dim=4, target_dim=2)

        assert result[0, 0] == pytest.approx(1 + 2j)
        assert result[0, 1] == pytest.approx(3 + 4j)
        assert result[1, 0] == pytest.approx(5 + 6j)
        assert result[1, 1] == pytest.approx(7 + 8j)

    def test_identity_block_preserved_outside_target(self):
        U = np.eye(2, dtype=complex) * 2  # non-identity to make the test meaningful
        result = embed_target_unitary_into_chip(U, chip_dim=4, target_dim=2)

        # Rows/cols 2 and 3 should still be the identity
        assert result[2, 2] == pytest.approx(1.0)
        assert result[3, 3] == pytest.approx(1.0)
        assert result[2, 3] == pytest.approx(0.0)
        assert result[3, 2] == pytest.approx(0.0)

    def test_output_shape(self):
        U = np.eye(2, dtype=complex)
        result = embed_target_unitary_into_chip(U, chip_dim=6, target_dim=2)
        assert result.shape == (6, 6)


class TestGetBaselineInputPorts:
    def test_active_cols_zero_on_chip4(self):
        result = get_baseline_input_ports([0], chip_dim=4)
        assert result == [1, 0, 0, 0]

    def test_active_cols_zero_and_two_on_chip4(self):
        result = get_baseline_input_ports([0, 2], chip_dim=4)
        assert result == [1, 0, 1, 0]

    def test_length_equals_chip_dim(self):
        result = get_baseline_input_ports([0], chip_dim=6)
        assert len(result) == 6

    def test_no_active_cols_returns_all_zeros(self):
        result = get_baseline_input_ports([], chip_dim=4)
        assert result == [0, 0, 0, 0]
