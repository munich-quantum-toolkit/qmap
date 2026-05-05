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
    def test_ideal_bs_gives_one(self):
        assert bar_fidelity([0.5, 0.5]) == pytest.approx(1.0)

    def test_fully_reflective_gives_one(self):
        # Both BSs reflect fully → bar state transmits all light
        assert bar_fidelity([1.0, 1.0]) == pytest.approx(1.0)

    def test_fully_transmissive_gives_one(self):
        # Both BSs transmit fully → bar state also reaches fidelity 1
        assert bar_fidelity([0.0, 0.0]) == pytest.approx(1.0)

    def test_cross_reflectivities_gives_zero(self):
        # r0=1, r1=0 → cross config → bar fidelity should be 0
        assert bar_fidelity([1.0, 0.0]) == pytest.approx(0.0)

    def test_symmetric_value(self):
        # bar_fidelity is symmetric: swapping r0 and r1 gives the same result
        assert bar_fidelity([0.3, 0.7]) == pytest.approx(bar_fidelity([0.7, 0.3]))

    def test_returns_float_in_unit_interval(self):
        result = bar_fidelity([0.45, 0.55])
        assert 0.0 <= result <= 1.0


class TestCrossFidelity:
    def test_ideal_bs_gives_one(self):
        assert cross_fidelity([0.5, 0.5]) == pytest.approx(1.0)

    def test_fully_reflective_gives_zero(self):
        assert cross_fidelity([1.0, 1.0]) == pytest.approx(0.0)

    def test_fully_transmissive_gives_zero(self):
        assert cross_fidelity([0.0, 0.0]) == pytest.approx(0.0)

    def test_bar_reflectivities_gives_one(self):
        # r0=1, r1=0 → cross state transmits all light
        assert cross_fidelity([1.0, 0.0]) == pytest.approx(1.0)

    def test_symmetric_value(self):
        assert cross_fidelity([0.3, 0.7]) == pytest.approx(cross_fidelity([0.7, 0.3]))

    def test_complementary_with_bar_at_ideal(self):
        # At ideal BS, both bar and cross fidelity are 1.0
        r = [0.5, 0.5]
        assert bar_fidelity(r) == pytest.approx(cross_fidelity(r))

    def test_returns_float_in_unit_interval(self):
        result = cross_fidelity([0.45, 0.55])
        assert 0.0 <= result <= 1.0


class TestGenerateBeamSplitterMatrix:
    def test_ideal_returns_all_half(self):
        bs = generate_beam_splitter_matrix(chip_size=4, ideal_bs=True)
        assert np.all(bs == 0.5)

    def test_ideal_correct_size_chip4(self):
        # chip_size=4: MZIs per layer [2, 1, 2, 1] → 6 total → 12 BS values
        bs = generate_beam_splitter_matrix(chip_size=4, ideal_bs=True)
        assert len(bs) == 12

    def test_ideal_correct_size_chip6(self):
        # chip_size=6: MZIs per layer [3, 2, 3, 2, 3, 2] → 15 total → 30 BS values
        bs = generate_beam_splitter_matrix(chip_size=6, ideal_bs=True)
        assert len(bs) == 30

    def test_random_has_correct_size_chip4(self):
        bs = generate_beam_splitter_matrix(chip_size=4, ideal_bs=False, rng=np.random.default_rng(0))
        assert len(bs) == 12

    def test_random_values_in_unit_interval(self):
        bs = generate_beam_splitter_matrix(chip_size=4, ideal_bs=False, rng=np.random.default_rng(0))
        assert np.all(bs >= 0.0) and np.all(bs <= 1.0)


class TestDetermineRoutingFidelities:
    def test_ideal_bs_all_bar_fidelities_are_one(self, ideal_bs_chip4):
        bar_fids, _ = determine_routing_fidelitites(ideal_bs_chip4, chip_dim=4)
        assert all(pytest.approx(1.0) == f for f in bar_fids)

    def test_ideal_bs_all_cross_fidelities_are_one(self, ideal_bs_chip4):
        _, cross_fids = determine_routing_fidelitites(ideal_bs_chip4, chip_dim=4)
        assert all(pytest.approx(1.0) == f for f in cross_fids)

    def test_correct_number_of_fidelities_chip4(self, ideal_bs_chip4):
        # chip_size=4: 4 layers → MZIs [2, 1, 2, 1] → 6 fidelity values each
        bar_fids, cross_fids = determine_routing_fidelitites(ideal_bs_chip4, chip_dim=4)
        assert len(bar_fids) == 6
        assert len(cross_fids) == 6


class TestConstructGraph:
    def test_graph_has_nodes_and_edges(self, ideal_bs_chip4, ones_transmissions_chip4):
        graph, pos, layers = construct_graph(
            chip_dim=4,
            target_dim=2,
            input_transmission=ones_transmissions_chip4,
            output_transmission=ones_transmissions_chip4,
            beam_splitter_reflectivities=ideal_bs_chip4,
        )
        assert graph.num_nodes() > 0
        assert graph.num_edges() > 0

    def test_number_of_layers_chip4_target2(self, ideal_bs_chip4, ones_transmissions_chip4):
        # number_of_layers = chip_dim - target_dim + 3 = 5
        _, _, layers = construct_graph(
            chip_dim=4,
            target_dim=2,
            input_transmission=ones_transmissions_chip4,
            output_transmission=ones_transmissions_chip4,
            beam_splitter_reflectivities=ideal_bs_chip4,
        )
        assert len(layers) == 5
