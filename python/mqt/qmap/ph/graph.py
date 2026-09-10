# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""Routing graph construction and fidelity scoring for the photonic compiler."""

from __future__ import annotations

import math
from dataclasses import dataclass
from typing import TYPE_CHECKING

import rustworkx as rx

from .mesh import mzi_top_modes

if TYPE_CHECKING:
    from collections.abc import Iterable, Sequence


def bar_fidelity(r: Sequence[float]) -> float:
    """Compute the straight-through probability of an MZI with bar phases.

    Args:
        r: Input and output beam-splitter reflectivities.

    Returns:
        Probability of remaining in the input mode.
    """
    return (math.sqrt(r[0] * r[1]) + math.sqrt((1 - r[0]) * (1 - r[1]))) ** 2


def cross_fidelity(r: Sequence[float]) -> float:
    """Compute the swap probability of an MZI with cross phases.

    Args:
        r: Input and output beam-splitter reflectivities.

    Returns:
        Probability of moving to the paired mode.
    """
    return (math.sqrt((1 - r[0]) * r[1]) + math.sqrt(r[0] * (1 - r[1]))) ** 2


def determine_routing_fidelities(
    beam_splitter_reflectivities: list[float],
    chip_dim: int,
) -> tuple[list[float], list[float]]:
    """Compute bar and cross fidelities in layer and MZI order.

    Args:
        beam_splitter_reflectivities: Flat in/out reflectivity pairs.
        chip_dim: Number of spatial modes and layers.

    Returns:
        Bar and cross fidelities for every MZI.
    """
    count = sum(len(mzi_top_modes(chip_dim, layer)) for layer in range(chip_dim))
    pairs = [beam_splitter_reflectivities[2 * i : 2 * i + 2] for i in range(count)]
    return [bar_fidelity(pair) for pair in pairs], [cross_fidelity(pair) for pair in pairs]


def _loss_cost(fidelities: Iterable[float]) -> float:
    """Sum negative log probabilities, with infinite cost for a blocked path.

    Summing logs avoids underflow in products of small probabilities. Clamping
    to one removes roundoff above one from the MZI fidelity calculation.
    """
    return sum(-math.log(min(value, 1.0)) if value > 0 else math.inf for value in fidelities)


def get_edge_cost_for_graph_layer(
    graph_layer: int,
    source_node_idx: int,
    target_node_idx: int,
    bar_fidelities: list[float],
    cross_fidelities: list[float],
    chip_dim: int,
    target_dim: int = 4,
) -> float:
    """Compute the cost of routing a dual-rail photon window through one layer.

    Args:
        graph_layer: Graph layer being left; physical layer is one less.
        source_node_idx: Physical mode occupied by the first photon.
        target_node_idx: Physical mode of that photon after the layer.
        bar_fidelities: Bar probabilities in layer and MZI order.
        cross_fidelities: Cross probabilities in layer and MZI order.
        chip_dim: Number of spatial modes.
        target_dim: Even width of the computation zone.

    Returns:
        Sum of negative log probabilities for the traversed MZIs. Uncoupled
        edge modes contribute no loss.

    Raises:
        ValueError: If the transition does not follow the mesh pairing or the
            target dimension is odd.
    """
    chip_layer = graph_layer - 1
    tops = mzi_top_modes(chip_dim, chip_layer)
    source_top = source_node_idx if source_node_idx in tops else source_node_idx - 1
    if source_node_idx != target_node_idx and not (
        source_top in tops and {source_node_idx, target_node_idx} == {source_top, source_top + 1}
    ):
        msg = f"Invalid edge: nodes {source_node_idx} and {target_node_idx} do not form an MZI pair."
        raise ValueError(msg)
    if target_dim % 2:
        msg = f"target_dim must be even, got {target_dim}."
        raise ValueError(msg)

    offset = sum(len(mzi_top_modes(chip_dim, layer)) for layer in range(chip_layer))
    fidelities = bar_fidelities if source_node_idx == target_node_idx else cross_fidelities
    traversed = []
    for mode in range(source_node_idx, source_node_idx + target_dim, 2):
        top = mode if mode in tops else mode - 1
        if top in tops:
            traversed.append(fidelities[offset + tops.index(top)])
    return _loss_cost(traversed)


@dataclass
class RoutingGraph:
    """Routing DAG and its per-layer node indices."""

    graph: rx.PyDiGraph
    layers: list[rx.NodeIndices]


def construct_graph(
    chip_dim: int,
    target_dim: int,
    input_transmission: list[float],
    output_transmission: list[float],
    beam_splitter_reflectivities: list[float],
) -> RoutingGraph:
    """Construct the routing DAG for photon placement optimization.

    Source edges select dual-rail input modes. Each intermediate edge routes
    those photons through one physical layer. Sink edges select the output
    window. Weights are negative log transmission probabilities.

    Args:
        chip_dim: Total number of spatial modes on the chip.
        target_dim: Dimension of the target unitary.
        input_transmission: Per-mode input transmission probabilities.
        output_transmission: Per-mode output transmission probabilities.
        beam_splitter_reflectivities: Flat in/out reflectivity pairs in layer
            and MZI order.

    Returns:
        Weighted directed acyclic graph and its layer indices.

    Raises:
        ValueError: If the target is not positive and even, the chip is no
            larger than the target, or the chip dimension is odd.
    """
    if target_dim <= 0:
        msg = f"target_dim must be positive, got {target_dim}."
        raise ValueError(msg)
    if chip_dim <= target_dim:
        msg = f"chip_dim ({chip_dim}) must be greater than target_dim ({target_dim})."
        raise ValueError(msg)
    if target_dim % 2:
        msg = f"target_dim must be even, got {target_dim}."
        raise ValueError(msg)
    if (chip_dim - target_dim) % 2:
        msg = f"chip_dim - target_dim must be even, got chip_dim={chip_dim}, target_dim={target_dim}."
        raise ValueError(msg)

    graph = rx.PyDiGraph()
    routing_layers = chip_dim - target_dim
    num_nodes = routing_layers + 2
    bar, cross = determine_routing_fidelities(beam_splitter_reflectivities, chip_dim)
    layers = [graph.add_nodes_from(["source"]), graph.add_nodes_from(range(num_nodes // 2))]
    layers.extend(graph.add_nodes_from(range(num_nodes)) for _ in range(routing_layers))
    layers.append(graph.add_nodes_from(["sink"]))

    for i, node in enumerate(layers[1]):
        graph.add_edge(layers[0][0], node, _loss_cost(input_transmission[2 * i : 2 * i + target_dim : 2]))

    for chip_layer in range(routing_layers):
        graph_layer = chip_layer + 1
        tops = mzi_top_modes(num_nodes, chip_layer)
        for i, node in enumerate(layers[graph_layer]):
            mode = 2 * i if chip_layer == 0 else i
            top = mode if mode in tops else mode - 1
            destinations = [mode]
            if top in tops:
                destinations.append(top + 1 if mode == top else top)
            for destination in destinations:
                cost = get_edge_cost_for_graph_layer(graph_layer, mode, destination, bar, cross, chip_dim, target_dim)
                graph.add_edge(node, layers[graph_layer + 1][destination], cost)

    for i, node in enumerate(layers[-2]):
        start = i // 2 * 2
        graph.add_edge(node, layers[-1][0], _loss_cost(output_transmission[start : start + target_dim]))

    return RoutingGraph(graph=graph, layers=layers)
