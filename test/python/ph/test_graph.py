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
    determine_routing_fidelities,
    get_edge_fidelity_even_graph_layer,
    get_edge_fidelity_odd_graph_layer,
)


class TestBarFidelity:
    """Tests for bar_fidelity."""

    @staticmethod
    def test_ideal_bs_gives_one() -> None:
        """Test that ideal 50/50 beam splitters yield bar fidelity 1.0."""
        assert bar_fidelity([0.5, 0.5]) == pytest.approx(1.0)

    @staticmethod
    def test_fully_reflective_gives_one() -> None:
        """Test that fully reflective beam splitters yield bar fidelity 1.0."""
        # Both BSs reflect fully -> bar state transmits all light
        assert bar_fidelity([1.0, 1.0]) == pytest.approx(1.0)

    @staticmethod
    def test_fully_transmissive_gives_one() -> None:
        """Test that fully transmissive beam splitters yield bar fidelity 1.0."""
        # Both BSs transmit fully -> bar state also reaches fidelity 1
        assert bar_fidelity([0.0, 0.0]) == pytest.approx(1.0)

    @staticmethod
    def test_cross_reflectivities_gives_zero() -> None:
        """Test that cross-configured reflectivities yield bar fidelity 0.0."""
        # r0=1, r1=0 -> cross config -> bar fidelity should be 0
        assert bar_fidelity([1.0, 0.0]) == pytest.approx(0.0)

    @staticmethod
    def test_symmetric_value() -> None:
        """Test that bar_fidelity is symmetric under reflectivity swap."""
        # bar_fidelity is symmetric: swapping r0 and r1 gives the same result
        assert bar_fidelity([0.3, 0.7]) == pytest.approx(bar_fidelity([0.7, 0.3]))

    @staticmethod
    def test_returns_float_in_unit_interval() -> None:
        """Test that bar_fidelity returns a value in [0, 1]."""
        result = bar_fidelity([0.45, 0.55])
        assert 0.0 <= result <= 1.0


class TestCrossFidelity:
    """Tests for cross_fidelity."""

    @staticmethod
    def test_ideal_bs_gives_one() -> None:
        """Test that ideal 50/50 beam splitters yield cross fidelity 1.0."""
        assert cross_fidelity([0.5, 0.5]) == pytest.approx(1.0)

    @staticmethod
    def test_fully_reflective_gives_zero() -> None:
        """Test that fully reflective beam splitters yield cross fidelity 0.0."""
        assert cross_fidelity([1.0, 1.0]) == pytest.approx(0.0)

    @staticmethod
    def test_fully_transmissive_gives_zero() -> None:
        """Test that fully transmissive beam splitters yield cross fidelity 0.0."""
        assert cross_fidelity([0.0, 0.0]) == pytest.approx(0.0)

    @staticmethod
    def test_bar_reflectivities_gives_one() -> None:
        """Test that bar-configured reflectivities yield cross fidelity 1.0."""
        # r0=1, r1=0 -> cross state transmits all light
        assert cross_fidelity([1.0, 0.0]) == pytest.approx(1.0)

    @staticmethod
    def test_symmetric_value() -> None:
        """Test that cross_fidelity is symmetric under reflectivity swap."""
        assert cross_fidelity([0.3, 0.7]) == pytest.approx(cross_fidelity([0.7, 0.3]))

    @staticmethod
    def test_complementary_with_bar_at_ideal() -> None:
        """Test that bar and cross fidelity are equal at ideal 50/50 beam splitters."""
        # At ideal BS, both bar and cross fidelity are 1.0
        r = [0.5, 0.5]
        assert bar_fidelity(r) == pytest.approx(cross_fidelity(r))

    @staticmethod
    def test_returns_float_in_unit_interval() -> None:
        """Test that cross_fidelity returns a value in [0, 1]."""
        result = cross_fidelity([0.45, 0.55])
        assert 0.0 <= result <= 1.0


class TestDetermineRoutingFidelities:
    """Tests for determine_routing_fidelities."""

    @staticmethod
    def test_ideal_bs_all_bar_fidelities_are_one(ideal_bs_chip4) -> None:
        """Test that ideal beam splitters yield bar fidelity 1.0 for all MZIs."""
        bar_fids, _ = determine_routing_fidelities(ideal_bs_chip4, chip_dim=4)
        assert all(pytest.approx(1.0) == f for f in bar_fids)

    @staticmethod
    def test_ideal_bs_all_cross_fidelities_are_one(ideal_bs_chip4) -> None:
        """Test that ideal beam splitters yield cross fidelity 1.0 for all MZIs."""
        _, cross_fids = determine_routing_fidelities(ideal_bs_chip4, chip_dim=4)
        assert all(pytest.approx(1.0) == f for f in cross_fids)

    @staticmethod
    def test_correct_number_of_fidelities_chip4(ideal_bs_chip4) -> None:
        """Test that a 4-mode chip produces 6 bar and 6 cross fidelity values."""
        # chip_size=4: 4 layers -> MZIs [2, 1, 2, 1] -> 6 fidelity values each
        bar_fids, cross_fids = determine_routing_fidelities(ideal_bs_chip4, chip_dim=4)
        assert len(bar_fids) == 6
        assert len(cross_fids) == 6

    @staticmethod
    def test_nonideal_bs_bar_fidelities_match_correct_pairs(nonideal_bs_chip4) -> None:
        """Test that each bar fidelity is computed from the correct in/out pair.

        The BS array has layout ``[in0, out0, in1, out1, ...]``, so MZI k uses
        indices ``[2k, 2k+1]``.  Any wrong pairing would produce a different
        fidelity value because the per-MZI reflectivities are all distinct.
        """
        bar_fids, _ = determine_routing_fidelities(nonideal_bs_chip4, chip_dim=4)
        expected = [bar_fidelity([nonideal_bs_chip4[2 * k], nonideal_bs_chip4[2 * k + 1]]) for k in range(6)]
        assert bar_fids == pytest.approx(expected)

    @staticmethod
    def test_nonideal_bs_cross_fidelities_match_correct_pairs(nonideal_bs_chip4) -> None:
        """Test that each cross fidelity is computed from the correct in/out pair."""
        _, cross_fids = determine_routing_fidelities(nonideal_bs_chip4, chip_dim=4)
        expected = [cross_fidelity([nonideal_bs_chip4[2 * k], nonideal_bs_chip4[2 * k + 1]]) for k in range(6)]
        assert cross_fids == pytest.approx(expected)


class TestConstructGraph:
    """Tests for construct_graph."""

    @staticmethod
    def test_graph_has_nodes_and_edges(ideal_bs_chip4, ones_transmissions_chip4) -> None:
        """Test that the constructed graph has at least one node and one edge."""
        routing_graph = construct_graph(
            chip_dim=4,
            target_dim=2,
            input_transmission=ones_transmissions_chip4,
            output_transmission=ones_transmissions_chip4,
            beam_splitter_reflectivities=ideal_bs_chip4,
        )
        assert routing_graph.graph.num_nodes() > 0
        assert routing_graph.graph.num_edges() > 0

    @staticmethod
    def test_number_of_layers_chip4_target2(ideal_bs_chip4, ones_transmissions_chip4) -> None:
        """Test that chip_dim=4, target_dim=2 yields 5 routing layers."""
        # number_of_layers = chip_dim - target_dim + 3 = 5
        routing_graph = construct_graph(
            chip_dim=4,
            target_dim=2,
            input_transmission=ones_transmissions_chip4,
            output_transmission=ones_transmissions_chip4,
            beam_splitter_reflectivities=ideal_bs_chip4,
        )
        assert len(routing_graph.layers) == 5

    @staticmethod
    def test_chip_dim_equal_target_dim_raises(ideal_bs_chip4, ones_transmissions_chip4) -> None:
        """Test that chip_dim == target_dim is rejected before any node access.

        A chip no larger than the target has no routing room; construct_graph must
        fail immediately rather than indexing nonexistent nodes in the layer==1 branch.
        """
        with pytest.raises(ValueError, match="must be greater than target_dim"):
            construct_graph(
                chip_dim=4,
                target_dim=4,
                input_transmission=ones_transmissions_chip4,
                output_transmission=ones_transmissions_chip4,
                beam_splitter_reflectivities=ideal_bs_chip4,
            )

    @staticmethod
    def test_non_positive_target_dim_raises(ideal_bs_chip4, ones_transmissions_chip4) -> None:
        """Test that target_dim <= 0 (e.g. an empty unitary) is rejected.

        A non-positive target_dim passes the even check (0 is even) and the
        chip_dim comparison, so it needs its own guard before graph construction.
        """
        with pytest.raises(ValueError, match="target_dim must be positive"):
            construct_graph(
                chip_dim=4,
                target_dim=0,
                input_transmission=ones_transmissions_chip4,
                output_transmission=ones_transmissions_chip4,
                beam_splitter_reflectivities=ideal_bs_chip4,
            )

    @staticmethod
    def test_odd_chip_dim_minus_target_dim_raises() -> None:
        """Test that an odd chip_dim - target_dim is rejected before layer sizing.

        chip_dim=3, target_dim=2 passes the positivity, ordering, and even-target
        checks, but chip_dim - target_dim is odd, which would silently truncate the
        layer-node counts via integer division. It must raise instead.
        """
        with pytest.raises(ValueError, match="chip_dim - target_dim must be even"):
            construct_graph(
                chip_dim=3,
                target_dim=2,
                input_transmission=[1.0, 1.0, 1.0],
                output_transmission=[1.0, 1.0, 1.0],
                beam_splitter_reflectivities=[1.0, 1.0, 1.0],
            )


class TestEdgeFidelityLayerMapping:
    """Regression tests: an edge leaving graph layer L must read chip layer L - 1.

    With non-ideal beam splitters the routing cost of an edge must reflect the chip
    layer that the photon actually traverses.  The movement mask realizes the edge
    leaving graph layer L as chip layer L - 1, so the graph cost must read the same
    (preceding) layer.  Distinct per-chip-layer fidelities make an off-by-two read
    detectable; ideal beam splitters (all fidelities 1.0) would hide it.
    """

    @staticmethod
    def _fidelities_with_distinct_layers(chip_dim: int, per_layer: dict[int, float]) -> list[float]:
        """Build a flat per-MZI fidelity list where chip layer ``k`` has value ``per_layer[k]`` (default 1.0)."""
        fids: list[float] = []
        for layer in range(chip_dim):
            mzi_count = chip_dim // 2 if layer % 2 == 0 else chip_dim // 2 - 1
            fids.extend([per_layer.get(layer, 1.0)] * mzi_count)
        return fids

    @staticmethod
    def test_even_graph_layer_reads_preceding_chip_layer() -> None:
        """Test that an even graph-layer edge (L=2) reads chip layer L-1=1, not L+1=3."""
        chip_dim, target_dim = 8, 4
        # Chip layer 1 is the correct (preceding) layer; chip layer 3 is the wrong one.
        bar = TestEdgeFidelityLayerMapping._fidelities_with_distinct_layers(chip_dim, {1: 0.8, 3: 0.5})
        cross = TestEdgeFidelityLayerMapping._fidelities_with_distinct_layers(chip_dim, {})
        cost = get_edge_fidelity_even_graph_layer(2, 1, 1, bar, cross, chip_dim=chip_dim, target_dim=target_dim)
        # target_dim // 2 = 2 photons traverse two MZIs of chip layer 1 (fidelity 0.8 each).
        assert cost == pytest.approx(float(-np.log(0.8 * 0.8)))

    @staticmethod
    def test_odd_graph_layer_reads_preceding_chip_layer() -> None:
        """Test that an odd graph-layer edge (L=3) reads chip layer L-1=2, not L+1=4."""
        chip_dim, target_dim = 8, 4
        bar = TestEdgeFidelityLayerMapping._fidelities_with_distinct_layers(chip_dim, {2: 0.7, 4: 0.3})
        cross = TestEdgeFidelityLayerMapping._fidelities_with_distinct_layers(chip_dim, {})
        cost = get_edge_fidelity_odd_graph_layer(3, 1, 1, bar, cross, chip_dim=chip_dim, target_dim=target_dim)
        assert cost == pytest.approx(float(-np.log(0.7 * 0.7)))
