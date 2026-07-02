# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""Tests for the photonic MZI-mesh graph module."""

import numpy as np
import pytest

from mqt.qmap.ph.graph import (
    bar_fidelity,
    construct_graph,
    cross_fidelity,
    determine_routing_fidelitites,
    generate_beam_splitter_matrix,
)


class TestBarFidelity:
    """Tests for bar_fidelity."""

    @staticmethod
    def test_ideal_bs_gives_one():
        """Test that ideal 50/50 beam splitters yield bar fidelity 1.0."""
        assert bar_fidelity([0.5, 0.5]) == pytest.approx(1.0)

    @staticmethod
    def test_fully_reflective_gives_one():
        """Test that fully reflective beam splitters yield bar fidelity 1.0."""
        # Both BSs reflect fully → bar state transmits all light
        assert bar_fidelity([1.0, 1.0]) == pytest.approx(1.0)

    @staticmethod
    def test_fully_transmissive_gives_one():
        """Test that fully transmissive beam splitters yield bar fidelity 1.0."""
        # Both BSs transmit fully → bar state also reaches fidelity 1
        assert bar_fidelity([0.0, 0.0]) == pytest.approx(1.0)

    @staticmethod
    def test_cross_reflectivities_gives_zero():
        """Test that cross-configured reflectivities yield bar fidelity 0.0."""
        # r0=1, r1=0 → cross config → bar fidelity should be 0
        assert bar_fidelity([1.0, 0.0]) == pytest.approx(0.0)

    @staticmethod
    def test_symmetric_value():
        """Test that bar_fidelity is symmetric under reflectivity swap."""
        # bar_fidelity is symmetric: swapping r0 and r1 gives the same result
        assert bar_fidelity([0.3, 0.7]) == pytest.approx(bar_fidelity([0.7, 0.3]))

    @staticmethod
    def test_returns_float_in_unit_interval():
        """Test that bar_fidelity returns a value in [0, 1]."""
        result = bar_fidelity([0.45, 0.55])
        assert 0.0 <= result <= 1.0


class TestCrossFidelity:
    """Tests for cross_fidelity."""

    @staticmethod
    def test_ideal_bs_gives_one():
        """Test that ideal 50/50 beam splitters yield cross fidelity 1.0."""
        assert cross_fidelity([0.5, 0.5]) == pytest.approx(1.0)

    @staticmethod
    def test_fully_reflective_gives_zero():
        """Test that fully reflective beam splitters yield cross fidelity 0.0."""
        assert cross_fidelity([1.0, 1.0]) == pytest.approx(0.0)

    @staticmethod
    def test_fully_transmissive_gives_zero():
        """Test that fully transmissive beam splitters yield cross fidelity 0.0."""
        assert cross_fidelity([0.0, 0.0]) == pytest.approx(0.0)

    @staticmethod
    def test_bar_reflectivities_gives_one():
        """Test that bar-configured reflectivities yield cross fidelity 1.0."""
        # r0=1, r1=0 → cross state transmits all light
        assert cross_fidelity([1.0, 0.0]) == pytest.approx(1.0)

    @staticmethod
    def test_symmetric_value():
        """Test that cross_fidelity is symmetric under reflectivity swap."""
        assert cross_fidelity([0.3, 0.7]) == pytest.approx(cross_fidelity([0.7, 0.3]))

    @staticmethod
    def test_complementary_with_bar_at_ideal():
        """Test that bar and cross fidelity are equal at ideal 50/50 beam splitters."""
        # At ideal BS, both bar and cross fidelity are 1.0
        r = [0.5, 0.5]
        assert bar_fidelity(r) == pytest.approx(cross_fidelity(r))

    @staticmethod
    def test_returns_float_in_unit_interval():
        """Test that cross_fidelity returns a value in [0, 1]."""
        result = cross_fidelity([0.45, 0.55])
        assert 0.0 <= result <= 1.0


class TestGenerateBeamSplitterMatrix:
    """Tests for generate_beam_splitter_matrix."""

    @staticmethod
    def test_ideal_returns_all_half():
        """Test that ideal mode returns all 0.5 reflectivities."""
        bs = generate_beam_splitter_matrix(chip_size=4, ideal_bs=True)
        assert np.allclose(bs, 0.5)

    @staticmethod
    def test_ideal_correct_size_chip4():
        """Test that a 4-mode chip yields 12 beam-splitter values."""
        # chip_size=4: MZIs per layer [2, 1, 2, 1] -> 6 total -> 12 BS values
        bs = generate_beam_splitter_matrix(chip_size=4, ideal_bs=True)
        assert len(bs) == 12

    @staticmethod
    def test_ideal_correct_size_chip6():
        """Test that a 6-mode chip yields 30 beam-splitter values."""
        # chip_size=6: MZIs per layer [3, 2, 3, 2, 3, 2] -> 15 total -> 30 BS values
        bs = generate_beam_splitter_matrix(chip_size=6, ideal_bs=True)
        assert len(bs) == 30

    @staticmethod
    def test_random_has_correct_size_chip4():
        """Test that random mode also yields 12 values for a 4-mode chip."""
        bs = generate_beam_splitter_matrix(chip_size=4, ideal_bs=False, rng=np.random.default_rng(0))
        assert len(bs) == 12

    @staticmethod
    def test_random_values_in_unit_interval():
        """Test that randomly sampled reflectivities lie in [0, 1]."""
        bs = generate_beam_splitter_matrix(chip_size=4, ideal_bs=False, rng=np.random.default_rng(0))
        assert np.all(bs >= 0.0)
        assert np.all(bs <= 1.0)


class TestDetermineRoutingFidelities:
    """Tests for determine_routing_fidelitites."""

    @staticmethod
    def test_ideal_bs_all_bar_fidelities_are_one(ideal_bs_chip4):
        """Test that ideal beam splitters yield bar fidelity 1.0 for all MZIs."""
        bar_fids, _ = determine_routing_fidelitites(ideal_bs_chip4, chip_dim=4)
        assert all(pytest.approx(1.0) == f for f in bar_fids)

    @staticmethod
    def test_ideal_bs_all_cross_fidelities_are_one(ideal_bs_chip4):
        """Test that ideal beam splitters yield cross fidelity 1.0 for all MZIs."""
        _, cross_fids = determine_routing_fidelitites(ideal_bs_chip4, chip_dim=4)
        assert all(pytest.approx(1.0) == f for f in cross_fids)

    @staticmethod
    def test_correct_number_of_fidelities_chip4(ideal_bs_chip4):
        """Test that a 4-mode chip produces 6 bar and 6 cross fidelity values."""
        # chip_size=4: 4 layers -> MZIs [2, 1, 2, 1] -> 6 fidelity values each
        bar_fids, cross_fids = determine_routing_fidelitites(ideal_bs_chip4, chip_dim=4)
        assert len(bar_fids) == 6
        assert len(cross_fids) == 6

    @staticmethod
    def test_nonideal_bs_bar_fidelities_match_correct_pairs(nonideal_bs_chip4):
        """Test that each bar fidelity is computed from the correct in/out pair.

        The BS array has layout ``[in0, out0, in1, out1, …]``, so MZI k uses
        indices ``[2k, 2k+1]``.  Any wrong pairing would produce a different
        fidelity value because the per-MZI reflectivities are all distinct.
        """
        bar_fids, _ = determine_routing_fidelitites(nonideal_bs_chip4, chip_dim=4)
        expected = [bar_fidelity([nonideal_bs_chip4[2 * k], nonideal_bs_chip4[2 * k + 1]]) for k in range(6)]
        assert bar_fids == pytest.approx(expected)

    @staticmethod
    def test_nonideal_bs_cross_fidelities_match_correct_pairs(nonideal_bs_chip4):
        """Test that each cross fidelity is computed from the correct in/out pair."""
        _, cross_fids = determine_routing_fidelitites(nonideal_bs_chip4, chip_dim=4)
        expected = [cross_fidelity([nonideal_bs_chip4[2 * k], nonideal_bs_chip4[2 * k + 1]]) for k in range(6)]
        assert cross_fids == pytest.approx(expected)


class TestConstructGraph:
    """Tests for construct_graph."""

    @staticmethod
    def test_graph_has_nodes_and_edges(ideal_bs_chip4, ones_transmissions_chip4):
        """Test that the constructed graph has at least one node and one edge."""
        graph, _pos, _layers = construct_graph(
            chip_dim=4,
            target_dim=2,
            input_transmission=ones_transmissions_chip4,
            output_transmission=ones_transmissions_chip4,
            beam_splitter_reflectivities=ideal_bs_chip4,
        )
        assert graph.num_nodes() > 0
        assert graph.num_edges() > 0

    @staticmethod
    def test_number_of_layers_chip4_target2(ideal_bs_chip4, ones_transmissions_chip4):
        """Test that chip_dim=4, target_dim=2 yields 5 routing layers."""
        # number_of_layers = chip_dim - target_dim + 3 = 5
        _, _, layers = construct_graph(
            chip_dim=4,
            target_dim=2,
            input_transmission=ones_transmissions_chip4,
            output_transmission=ones_transmissions_chip4,
            beam_splitter_reflectivities=ideal_bs_chip4,
        )
        assert len(layers) == 5
