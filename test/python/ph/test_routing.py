# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""Tests for the photonic MZI-mesh routing module."""

import pytest

torch = pytest.importorskip("torch")

from mqt.qmap.ph.graph import construct_graph
from mqt.qmap.ph.routing import (
    MaskState,
    convert_input_ports,
    convert_output_ports,
    get_best_route,
    get_input_ports_for_computation_zone,
    infer_input_computation_and_output_ports,
    route_to_movement_mask,
)


class TestInferInputComputationAndOutputPorts:
    """Tests for infer_input_computation_and_output_ports."""

    @staticmethod
    def test_straight_route_first_position() -> None:
        """Test that a straight route through position 0 yields input port 0, output ports [0,1], and active col 0."""
        # Source -> input 0 -> ... -> compute 0 -> sink
        input_ports, output_ports, active_cols = infer_input_computation_and_output_ports([0, 0, 0, 0, 0], target_dim=2)
        assert input_ports == [0]
        assert output_ports == [0, 1]
        assert active_cols == [0]

    @staticmethod
    def test_route_at_second_input_position() -> None:
        """Test that a route through input position 1 yields input port 1, output ports [2,3], and active col 1."""
        # Source -> input 1 -> intermediate nodes -> compute at odd index -> sink
        input_ports, output_ports, active_cols = infer_input_computation_and_output_ports([0, 1, 1, 1, 0], target_dim=2)
        assert input_ports == [2]
        assert output_ports == [0, 1]
        assert active_cols == [1]

    @staticmethod
    def test_active_cols_even_for_even_computation_index() -> None:
        """Test that an even computation index yields only even active columns."""
        _, _, active_cols = infer_input_computation_and_output_ports([0, 0, 0, 0, 0], target_dim=4)
        # computation_index=0, even -> active_cols=[0, 2]
        assert all(c % 2 == 0 for c in active_cols)

    @staticmethod
    def test_active_cols_odd_for_odd_computation_index() -> None:
        """Test that an odd computation index yields only odd active columns."""
        _, _, active_cols = infer_input_computation_and_output_ports([0, 0, 1, 1, 0], target_dim=4)
        # computation_index=1, odd -> active_cols=[1, 3]
        assert all(c % 2 == 1 for c in active_cols)

    @staticmethod
    def test_raises_for_too_short_route() -> None:
        """Test that a route with fewer than 2 nodes raises ValueError."""
        with pytest.raises(ValueError, match="at least 2 nodes"):
            infer_input_computation_and_output_ports([0], target_dim=2)


class TestConvertInputPorts:
    """Tests for convert_input_ports."""

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


class TestConvertOutputPorts:
    """Tests for convert_output_ports."""

    @staticmethod
    def test_first_window() -> None:
        """Test that output ports [0, 1] on a 4-mode chip gives [1, 1, 0, 0]."""
        result = convert_output_ports([0, 1], chip_dim=4)
        assert result == [1, 1, 0, 0]

    @staticmethod
    def test_second_window() -> None:
        """Test that output ports [2, 3] on a 4-mode chip gives [0, 0, 1, 1]."""
        result = convert_output_ports([2, 3], chip_dim=4)
        assert result == [0, 0, 1, 1]

    @staticmethod
    def test_empty_output_ports() -> None:
        """Test that no output ports yields an all-zero vector."""
        result = convert_output_ports([], chip_dim=4)
        assert result == [0, 0, 0, 0]

    @staticmethod
    def test_length_matches_chip_dim() -> None:
        """Test that the result length equals chip_dim."""
        result = convert_output_ports([0, 1], chip_dim=6)
        assert len(result) == 6


class TestGetInputPortsForComputationZone:
    """Tests for get_input_ports_for_computation_zone."""

    @staticmethod
    def test_first_active_col() -> None:
        """Test that active col 0 with target_dim=2 gives [1, 0]."""
        result = get_input_ports_for_computation_zone([0], target_dim=2)
        assert result == [1, 0]

    @staticmethod
    def test_second_active_col() -> None:
        """Test that active col 1 with target_dim=2 gives [0, 1]."""
        result = get_input_ports_for_computation_zone([1], target_dim=2)
        assert result == [0, 1]

    @staticmethod
    def test_multiple_active_cols() -> None:
        """Test that active cols [0, 2] with target_dim=4 gives [1, 0, 1, 0]."""
        result = get_input_ports_for_computation_zone([0, 2], target_dim=4)
        assert result == [1, 0, 1, 0]


class TestRouteToMovementMask:
    """Tests for route_to_movement_mask."""

    @staticmethod
    def test_straight_route_chip4_target2() -> None:
        """Test that the straight route on a 4-mode chip produces the expected movement mask."""
        # Straight route: all-BAR routing, compute zone at modes 0-1, layers 2-3
        mask = route_to_movement_mask([0, 0, 0, 0, 0], chip_dim=4, target_dim=2)

        expected = torch.tensor(
            [
                [MaskState.BAR, MaskState.BAR, MaskState.MZI, MaskState.MZI],
                [MaskState.BAR, MaskState.BAR, MaskState.MZI, MaskState.TOP_ONLY],
                [MaskState.BAR, MaskState.BAR, MaskState.BAR, MaskState.TOP_ONLY],
                [MaskState.BAR, MaskState.BAR, MaskState.BAR, MaskState.BAR],
            ],
            dtype=torch.int,
        )

        assert torch.equal(mask, expected)

    @staticmethod
    def test_mask_shape() -> None:
        """Test that the movement mask has shape (chip_dim, chip_dim)."""
        mask = route_to_movement_mask([0, 0, 0, 0, 0], chip_dim=4, target_dim=2)
        assert mask.shape == (4, 4)

    @staticmethod
    def test_empty_route_returns_all_bar() -> None:
        """Test that an empty route produces an all-BAR mask."""
        mask = route_to_movement_mask([], chip_dim=4, target_dim=2)
        assert torch.all(mask == MaskState.BAR)

    @staticmethod
    def test_compute_zone_contains_only_mzi_or_virtual_states() -> None:
        """Test that the compute zone contains only MZI, TOP_ONLY, or BOT_ONLY states."""
        mask = route_to_movement_mask([0, 0, 0, 0, 0], chip_dim=4, target_dim=2)
        compute_zone = mask[0:2, 2:4]
        valid_compute_states = {MaskState.MZI, MaskState.TOP_ONLY, MaskState.BOT_ONLY}
        assert all(v.item() in valid_compute_states for v in compute_zone.flatten())

    @staticmethod
    def test_routing_zone_contains_only_bar_or_cross() -> None:
        """Test that the routing zone contains only BAR or CROSS states."""
        mask = route_to_movement_mask([0, 0, 0, 0, 0], chip_dim=4, target_dim=2)
        routing_zone = mask[:, 0:2]
        valid_routing_states = {MaskState.BAR, MaskState.CROSS}
        assert all(v.item() in valid_routing_states for v in routing_zone.flatten())

    @staticmethod
    def test_invalid_later_layer_transition_odd_branch_raises() -> None:
        """Test that a non-adjacent transition in an odd later layer raises.

        Route index i=3 (i % 2 == 1) steps from position 0 to 2 (difference 2),
        which is not a valid routing-graph edge and must be rejected.
        """
        with pytest.raises(ValueError, match="Invalid edge from node_"):
            route_to_movement_mask([0, 0, 0, 2, 0], chip_dim=4, target_dim=2)

    @staticmethod
    def test_invalid_later_layer_transition_even_branch_raises() -> None:
        """Test that a non-adjacent transition in an even later layer raises.

        Route index i=4 (i % 2 == 0) steps from position 0 to 2 (difference 2),
        exercising the even-branch guard on a chip8/target4 layout.
        """
        with pytest.raises(ValueError, match="Invalid edge from node_"):
            route_to_movement_mask([0, 0, 0, 0, 2, 0, 0], chip_dim=8, target_dim=4)

    @staticmethod
    def test_later_layer_cross_slice_is_target_dim_wide() -> None:
        """Test that a valid later-layer cross marks a target_dim-wide CROSS slice.

        With target_dim=4 the cross at chip layer 2 (route index i=4, an even
        later layer) must span exactly four modes, not two.
        """
        mask = route_to_movement_mask([0, 0, 0, 0, 1, 1, 0], chip_dim=8, target_dim=4)
        # Cross starts at mode 0 and is target_dim (=4) rows wide on chip layer 2.
        assert torch.all(mask[0:4, 2] == MaskState.CROSS)
        # Modes outside that target_dim-wide window are untouched on that layer.
        assert torch.all(mask[4:8, 2] == MaskState.BAR)


class TestGetBestRoute:
    """Tests for get_best_route."""

    @staticmethod
    def test_ideal_bs_returns_deterministic_route(ideal_bs_chip4, ones_transmissions_chip4) -> None:
        """Test that ideal beam splitters yield a valid route with zero cost."""
        routing_graph = construct_graph(
            chip_dim=4,
            target_dim=2,
            input_transmission=ones_transmissions_chip4,
            output_transmission=ones_transmissions_chip4,
            beam_splitter_reflectivities=ideal_bs_chip4,
        )
        route, cost = get_best_route(routing_graph.graph, routing_graph.layers)

        # With all-ideal components: all paths have equal cost (0)
        assert isinstance(route, list)
        assert len(route) == 5  # number_of_layers for chip4/target2
        assert cost == pytest.approx(0.0)

    @staticmethod
    def test_route_starts_and_ends_at_zero(ideal_bs_chip4, ones_transmissions_chip4) -> None:
        """Test that the route begins and ends at node index 0 (source/sink)."""
        routing_graph = construct_graph(
            chip_dim=4,
            target_dim=2,
            input_transmission=ones_transmissions_chip4,
            output_transmission=ones_transmissions_chip4,
            beam_splitter_reflectivities=ideal_bs_chip4,
        )
        route, _ = get_best_route(routing_graph.graph, routing_graph.layers)

        assert route[0] == 0  # source
        assert route[-1] == 0  # sink


class TestRoutingLayerMappingRegression:
    """Regression test that the router reads the correct chip layer for each graph layer.

    Uses a chip with extreme, distinct beam-splitter reflectivities in the routing
    region (each MZI near-perfect at exactly one of bar/cross) and an ideal
    computation zone. The strong contrast forces a unique optimal route whose every
    step matches a physically-perfect MZI operation, so the total cost is zero and
    the computation window lands on modes [2, 3, 4, 5].

    If the graph-layer -> chip-layer mapping in ``graph.py`` is off (for example the
    sign of the offset in ``get_edge_fidelity_odd_graph_layer``), the router reads a
    neighbouring chip layer's fidelities and returns a different route
    (``[0, 1, 2, 2, 2, 1, 0]``, window [0, 1, 2, 3]). The near-0.5 beam splitters
    used by the other routing tests cannot catch this because there every MZI is
    near-perfect at both bar and cross, so the mapping barely affects the cost.
    """

    @staticmethod
    def test_get_best_route_follows_extreme_routing(extreme_routing_chip) -> None:
        """Test that the optimal route is the zero-cost path forced by the extreme routing values."""
        chip = extreme_routing_chip
        routing_graph = construct_graph(
            chip_dim=chip.chip_dim,
            target_dim=chip.target_dim,
            input_transmission=[1.0] * chip.chip_dim,
            output_transmission=[1.0] * chip.chip_dim,
            beam_splitter_reflectivities=chip.bs,
        )
        route, cost = get_best_route(routing_graph.graph, routing_graph.layers)

        # Bars through layers 0-1 and crosses through layers 2-3, all perfect -> cost 0.
        assert route == [0, 0, 0, 0, 1, 2, 0]
        assert cost == pytest.approx(0.0)

        # The route places the computation window on modes [2, 3, 4, 5].
        input_ports, output_ports, _ = infer_input_computation_and_output_ports(route, chip.target_dim)
        assert input_ports == [0, 2]
        assert output_ports == [2, 3, 4, 5]
