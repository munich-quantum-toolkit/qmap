import numpy as np
import pytest

torch = pytest.importorskip("torch")

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
    def test_straight_route_first_position(self):
        # Source → input 0 → ... → compute 0 → sink
        input_ports, output_ports, active_cols = infer_input_computation_and_output_ports(
            [0, 0, 0, 0, 0], target_dim=2
        )
        assert input_ports == [0]
        assert output_ports == [0, 1]
        assert active_cols == [0]

    def test_route_at_second_input_position(self):
        # Source → input 1 → intermediate nodes → compute at odd index → sink
        input_ports, output_ports, active_cols = infer_input_computation_and_output_ports(
            [0, 1, 1, 1, 0], target_dim=2
        )
        assert input_ports == [2]
        assert output_ports == [0, 1]
        assert active_cols == [1]

    def test_active_cols_even_for_even_computation_index(self):
        _, _, active_cols = infer_input_computation_and_output_ports(
            [0, 0, 0, 0, 0], target_dim=4
        )
        # computation_index=0, even → active_cols=[0, 2]
        assert all(c % 2 == 0 for c in active_cols)

    def test_active_cols_odd_for_odd_computation_index(self):
        _, _, active_cols = infer_input_computation_and_output_ports(
            [0, 0, 1, 1, 0], target_dim=4
        )
        # computation_index=1, odd → active_cols=[1, 3]
        assert all(c % 2 == 1 for c in active_cols)

    def test_raises_for_too_short_route(self):
        with pytest.raises(ValueError):
            infer_input_computation_and_output_ports([0], target_dim=2)


class TestConvertInputPorts:
    def test_first_mode_active(self):
        # input_ports=[0] on a 4-mode chip: mode 0 gets photon, mode 1 skipped
        result = convert_input_ports([0], chip_dim=4)
        assert result == [1, 0, 0, 0]

    def test_third_mode_active(self):
        # input_ports=[2]: mode 2 gets photon, mode 3 skipped
        result = convert_input_ports([2], chip_dim=4)
        assert result == [0, 0, 1, 0]

    def test_no_active_modes(self):
        result = convert_input_ports([], chip_dim=4)
        assert result == [0, 0, 0, 0]

    def test_total_length_matches_chip_dim(self):
        result = convert_input_ports([0], chip_dim=6)
        assert len(result) == 6


class TestConvertOutputPorts:
    def test_first_window(self):
        result = convert_output_ports([0, 1], chip_dim=4)
        assert result == [1, 1, 0, 0]

    def test_second_window(self):
        result = convert_output_ports([2, 3], chip_dim=4)
        assert result == [0, 0, 1, 1]

    def test_empty_output_ports(self):
        result = convert_output_ports([], chip_dim=4)
        assert result == [0, 0, 0, 0]

    def test_length_matches_chip_dim(self):
        result = convert_output_ports([0, 1], chip_dim=6)
        assert len(result) == 6


class TestGetInputPortsForComputationZone:
    def test_first_active_col(self):
        result = get_input_ports_for_computation_zone([0], target_dim=2)
        assert result == [1, 0]

    def test_second_active_col(self):
        result = get_input_ports_for_computation_zone([1], target_dim=2)
        assert result == [0, 1]

    def test_multiple_active_cols(self):
        result = get_input_ports_for_computation_zone([0, 2], target_dim=4)
        assert result == [1, 0, 1, 0]


class TestRouteToMovementMask:
    def test_straight_route_chip4_target2(self):
        # Straight route: all-BAR routing, compute zone at modes 0–1, layers 2–3
        mask = route_to_movement_mask([0, 0, 0, 0, 0], chip_dim=4, target_dim=2)

        expected = torch.tensor([
            [MaskState.BAR, MaskState.BAR, MaskState.MZI,      MaskState.MZI     ],
            [MaskState.BAR, MaskState.BAR, MaskState.MZI,      MaskState.TOP_ONLY],
            [MaskState.BAR, MaskState.BAR, MaskState.BAR,      MaskState.TOP_ONLY],
            [MaskState.BAR, MaskState.BAR, MaskState.BAR,      MaskState.BAR     ],
        ], dtype=torch.int)

        assert torch.equal(mask, expected)

    def test_mask_shape(self):
        mask = route_to_movement_mask([0, 0, 0, 0, 0], chip_dim=4, target_dim=2)
        assert mask.shape == (4, 4)

    def test_empty_route_returns_all_bar(self):
        mask = route_to_movement_mask([], chip_dim=4, target_dim=2)
        assert torch.all(mask == MaskState.BAR)

    def test_compute_zone_contains_only_mzi_or_virtual_states(self):
        mask = route_to_movement_mask([0, 0, 0, 0, 0], chip_dim=4, target_dim=2)
        compute_zone = mask[0:2, 2:4]
        valid_compute_states = {MaskState.MZI, MaskState.TOP_ONLY, MaskState.BOT_ONLY}
        assert all(v.item() in valid_compute_states for v in compute_zone.flatten())

    def test_routing_zone_contains_only_bar_or_cross(self):
        mask = route_to_movement_mask([0, 0, 0, 0, 0], chip_dim=4, target_dim=2)
        routing_zone = mask[:, 0:2]
        valid_routing_states = {MaskState.BAR, MaskState.CROSS}
        assert all(v.item() in valid_routing_states for v in routing_zone.flatten())


class TestGetBestRoute:
    def test_ideal_bs_returns_deterministic_route(self, ideal_bs_chip4, ones_transmissions_chip4):
        from mqt.qmap.ph.graph import construct_graph

        graph, _, layers = construct_graph(
            chip_dim=4,
            target_dim=2,
            input_transmission=ones_transmissions_chip4,
            output_transmission=ones_transmissions_chip4,
            beam_splitter_reflectivities=ideal_bs_chip4,
        )
        route, cost = get_best_route(graph, layers)

        # With all-ideal components: all paths have equal cost (0)
        assert isinstance(route, list)
        assert len(route) == 5  # number_of_layers for chip4/target2
        assert cost == pytest.approx(0.0)

    def test_route_starts_and_ends_at_zero(self, ideal_bs_chip4, ones_transmissions_chip4):
        from mqt.qmap.ph.graph import construct_graph

        graph, _, layers = construct_graph(
            chip_dim=4,
            target_dim=2,
            input_transmission=ones_transmissions_chip4,
            output_transmission=ones_transmissions_chip4,
            beam_splitter_reflectivities=ideal_bs_chip4,
        )
        route, _ = get_best_route(graph, layers)

        assert route[0] == 0   # source
        assert route[-1] == 0  # sink
