# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""Routing graph construction and fidelity scoring for the photonic compiler."""

from __future__ import annotations

from dataclasses import dataclass
from typing import TYPE_CHECKING

import numpy as np
import rustworkx as rx

if TYPE_CHECKING:
    from collections.abc import Sequence


def bar_fidelity(r: Sequence[float]) -> float:
    """Compute the fidelity of an MZI to perform a bar operation.

    Args:
        r: Pair of beam splitter reflectivities ``[r_in, r_out]``.

    Returns:
        Fidelity value in ``[0, 1]`` indicating how well the MZI transmits
        light straight through.
    """
    cache = np.sqrt(r[0] * r[1]) + np.sqrt((1 - r[0]) * (1 - r[1]))
    return cache**2


def cross_fidelity(r: Sequence[float]) -> float:
    """Compute the fidelity of an MZI to perform a cross operation.

    Args:
        r: Pair of beam splitter reflectivities ``[r_in, r_out]``.

    Returns:
        Fidelity value in ``[0, 1]`` indicating how well the MZI swaps
        the two input modes.
    """
    cache = np.sqrt((1 - r[0]) * r[1]) + np.sqrt(r[0] * (1 - r[1]))
    return cache**2


def determine_routing_fidelities(
    beam_splitter_reflectivities: list[float],
    chip_dim: int,
) -> tuple[list[float], list[float]]:
    """Compute bar and cross fidelities for all MZIs in the chip.

    Results are ordered sequentially by layer: entries for layer 0 come
    first, followed by layer 1, and so on.  Within each layer entries are
    ordered by mode index.

    Args:
        beam_splitter_reflectivities: Flat list of measured reflectivity
            values ordered MZI-by-MZI as in/out pairs
            (``[MZI_0_in, MZI_0_out, MZI_1_in, MZI_1_out, ...]``), layer by layer.
        chip_dim: Number of spatial modes on the chip.

    Returns:
        A tuple ``(bar_fidelities, cross_fidelities)`` where each element is
        a flat list of per-MZI fidelity values indexed as
        ``[layer0_mzi0, layer0_mzi1, ..., layer1_mzi0, ...]``.
    """
    bs_idx = 0
    bar_fidelities: list[float] = []
    cross_fidelities: list[float] = []

    for layer in range(chip_dim):
        mzi_count = chip_dim // 2 if layer % 2 == 0 else chip_dim // 2 - 1
        for _ in range(mzi_count):
            current_bs = [
                beam_splitter_reflectivities[bs_idx],
                beam_splitter_reflectivities[bs_idx + 1],
            ]
            bar_fidelities.append(bar_fidelity(current_bs))
            cross_fidelities.append(cross_fidelity(current_bs))
            bs_idx += 2

    return bar_fidelities, cross_fidelities


def _combined_fidelity_from_mzi_block(
    fidelity_list: list[float],
    fidelity_offset: int,
    source_node_idx: int,
    mzi_count_in_layer: int,
    num_parallel_photons: int,
) -> float:
    """Return the product of fidelities over the MZI block for one edge.

    For node ``i``, the contiguous MZI block starts at ``floor((i-1) / 2)``.
    For ``target_dim=4`` (two photons) this reproduces the mapping
    ``i=1,2 -> [0, 1]``, ``i=3,4 -> [1, 2]``, with a boundary fallback of 1.0.

    Args:
        fidelity_list: Flat list of fidelities for all MZIs.
        fidelity_offset: Index into ``fidelity_list`` where the relevant
            chip layer starts.
        source_node_idx: Index of the source node within its graph layer.
        mzi_count_in_layer: Total number of MZIs in the relevant chip layer.
        num_parallel_photons: Number of photons routed simultaneously
            (equals ``target_dim // 2``).

    Returns:
        Combined fidelity in ``[0, 1]``: the product of the fidelities of
        the ``num_parallel_photons`` MZIs used by this edge.
    """
    start_mzi_idx = (source_node_idx - 1) // 2

    combined_fidelity = 1.0
    for mzi_idx in range(start_mzi_idx, start_mzi_idx + num_parallel_photons):
        if 0 <= mzi_idx < mzi_count_in_layer:
            combined_fidelity *= fidelity_list[fidelity_offset + mzi_idx]

    return combined_fidelity


def _edge_cost_from_fidelity(fidelity: float) -> float:
    """Convert a fidelity value into a non-negative routing-graph edge cost.

    Args:
        fidelity: Fidelity value in ``[0, 1]``.

    Returns:
        Non-negative edge cost ``-log(fidelity)``.
    """
    return -np.log(fidelity)


def get_edge_fidelity_even_graph_layer(
    graph_layer: int,
    source_node_idx: int,
    target_node_idx: int,
    bar_fidelities: list[float],
    cross_fidelities: list[float],
    chip_dim: int,
    target_dim: int = 4,
) -> float:
    """Compute the edge cost for an edge starting from an even graph layer.

    An edge leaving graph layer ``L`` traverses chip layer ``L - 1``, so the
    even graph layers (2, 4, 6, ...) map to the odd chip layers (1, 3, 5, ...).
    Odd chip layers have MZIs only on in-between mode pairs, excluding the
    first and last modes.

    Each graph edge routes ``target_dim // 2`` photons in parallel, using
    that many adjacent MZIs in the corresponding chip layer.

    Args:
        graph_layer: Even layer index in the routing graph.
        source_node_idx: Index of the source node within its graph layer.
        target_node_idx: Index of the target node within its graph layer.
        bar_fidelities: Bar fidelities ordered sequentially by chip layer
            then MZI index.
        cross_fidelities: Cross fidelities ordered sequentially by chip layer
            then MZI index.
        chip_dim: Number of spatial modes on the chip.
        target_dim: Dimension of the target unitary; must be even.

    Returns:
        Non-negative edge cost ``-log(combined_fidelity)``.

    Raises:
        ValueError: If ``source_node_idx`` and ``target_node_idx`` are neither
            equal nor adjacent, or if ``target_dim`` is odd.
    """
    if source_node_idx == target_node_idx:
        edge_type = "bar"
    elif abs(source_node_idx - target_node_idx) == 1:
        edge_type = "cross"
    else:
        msg = f"Invalid edge: nodes {source_node_idx} and {target_node_idx} must be identical or adjacent"
        raise ValueError(msg)

    if target_dim % 2 != 0:
        msg_0 = f"target_dim must be even, got {target_dim}"
        raise ValueError(msg_0)

    chip_layer = graph_layer - 1  # edge leaving graph layer L traverses chip layer L-1
    mzis_per_even_chip_layer = chip_dim // 2
    mzis_per_odd_chip_layer = chip_dim // 2 - 1

    fidelity_offset = 0
    for chip_layer_idx in range(chip_layer):
        if chip_layer_idx % 2 == 0:
            fidelity_offset += mzis_per_even_chip_layer
        else:
            fidelity_offset += mzis_per_odd_chip_layer

    fidelity_list = bar_fidelities if edge_type == "bar" else cross_fidelities

    combined_fidelity = _combined_fidelity_from_mzi_block(
        fidelity_list=fidelity_list,
        fidelity_offset=fidelity_offset,
        source_node_idx=source_node_idx,
        mzi_count_in_layer=mzis_per_odd_chip_layer,
        num_parallel_photons=target_dim // 2,
    )
    return _edge_cost_from_fidelity(combined_fidelity)


def get_edge_fidelity_odd_graph_layer(
    graph_layer: int,
    source_node_idx: int,
    target_node_idx: int,
    bar_fidelities: list[float],
    cross_fidelities: list[float],
    chip_dim: int,
    target_dim: int = 4,
) -> float:
    """Compute the edge cost for an edge starting from an odd graph layer.

    Odd graph layers (1, 3, 5, ...) correspond to even chip layers (0, 2, 4, ...).
    Even chip layers have MZIs on all mode pairs: (0-1), (2-3), (4-5), ...

    Each graph edge routes ``target_dim // 2`` photons in parallel, using
    that many adjacent MZIs in the corresponding chip layer.

    Args:
        graph_layer: Odd layer index in the routing graph.
        source_node_idx: Index of the source node within its graph layer.
        target_node_idx: Index of the target node within its graph layer.
        bar_fidelities: Bar fidelities ordered sequentially by chip layer
            then MZI index.
        cross_fidelities: Cross fidelities ordered sequentially by chip layer
            then MZI index.
        chip_dim: Number of spatial modes on the chip.
        target_dim: Dimension of the target unitary; must be even.

    Returns:
        Non-negative edge cost ``-log(combined_fidelity)``.

    Raises:
        ValueError: If ``source_node_idx`` and ``target_node_idx`` are neither
            equal nor adjacent, or if ``target_dim`` is odd.
    """
    if source_node_idx == target_node_idx:
        edge_type = "bar"
    elif abs(source_node_idx - target_node_idx) == 1:
        edge_type = "cross"
    else:
        msg = f"Invalid edge: nodes {source_node_idx} and {target_node_idx} must be identical or adjacent"
        raise ValueError(msg)

    if target_dim % 2 != 0:
        msg_0 = f"target_dim must be even, got {target_dim}"
        raise ValueError(msg_0)

    chip_layer = graph_layer - 1  # odd graph layer -> preceding even chip layer
    mzis_per_even_chip_layer = chip_dim // 2
    mzis_per_odd_chip_layer = chip_dim // 2 - 1

    fidelity_offset = 0
    for chip_layer_idx in range(chip_layer):
        if chip_layer_idx % 2 == 0:
            fidelity_offset += mzis_per_even_chip_layer
        else:
            fidelity_offset += mzis_per_odd_chip_layer

    fidelity_list = bar_fidelities if edge_type == "bar" else cross_fidelities

    combined_fidelity = _combined_fidelity_from_mzi_block(
        fidelity_list=fidelity_list,
        fidelity_offset=fidelity_offset,
        source_node_idx=source_node_idx,
        mzi_count_in_layer=mzis_per_even_chip_layer,
        num_parallel_photons=target_dim // 2,
    )
    return _edge_cost_from_fidelity(combined_fidelity)


@dataclass
class RoutingGraph:
    """Layered routing DAG produced by :func:`construct_graph`.

    Attributes:
        graph: Weighted directed acyclic graph encoding the routing decisions
            as a shortest-path problem.
        positions: Maps each node index to an ``(x, y)`` visualization
            coordinate.  Not used by the routing itself -- retained for
            plotting and debugging the graph.
        layers: Per-layer node index arrays as returned by rustworkx.
    """

    graph: rx.PyDiGraph
    positions: dict[int, tuple[float, float]]
    layers: list


def construct_graph(
    chip_dim: int,
    target_dim: int,
    input_transmission: list[float],
    output_transmission: list[float],
    beam_splitter_reflectivities: list[float],
) -> RoutingGraph:
    """Construct the routing DAG for photon placement optimization.

    The graph encodes routing decisions as a shortest-path problem:

    * Layer 0 -> 1 edges encode input placement costs (source to candidate
      input positions).
    * Intermediate edges encode routing costs through the chip's MZI layers.
    * Final edges to the sink encode output transmission costs and implicitly
      select the computation zone.

    For ``chip_dim=8``, ``target_dim=4`` there are three candidate input
    positions, intermediate routing layers, and three output windows.

    Args:
        chip_dim: Total number of spatial modes on the chip.
        target_dim: Dimension of the target unitary.
        input_transmission: Per-mode input transmission coefficients, a list
            of length ``chip_dim``.
        output_transmission: Per-mode output transmission coefficients, a list
            of length ``chip_dim``.
        beam_splitter_reflectivities: Flat list of measured beam-splitter
            reflectivities ordered MZI-by-MZI as in/out pairs, layer by layer.

    Returns:
        A :class:`RoutingGraph` bundling the weighted directed acyclic graph,
        the per-node ``(x, y)`` visualization coordinates, and the per-layer
        node index arrays.

    Raises:
        ValueError: If ``target_dim`` is not positive, ``chip_dim <= target_dim``,
            ``target_dim`` is odd, or ``chip_dim - target_dim`` is odd.
    """
    if target_dim <= 0:
        msg = f"target_dim must be positive, got {target_dim}."
        raise ValueError(msg)
    if chip_dim <= target_dim:
        msg = f"chip_dim ({chip_dim}) must be greater than target_dim ({target_dim})."
        raise ValueError(msg)
    if target_dim % 2 != 0:
        msg = f"target_dim must be even, got {target_dim}."
        raise ValueError(msg)
    if (chip_dim - target_dim) % 2 != 0:
        msg = f"chip_dim - target_dim must be even, got chip_dim={chip_dim}, target_dim={target_dim}."
        raise ValueError(msg)

    graph = rx.PyDiGraph()

    number_of_layers = int(chip_dim - target_dim + 3)
    number_nodes_first_layer = int((chip_dim - target_dim) / 2 + 1)
    number_nodes_intermediate_layers = int(chip_dim - target_dim + 2)

    bar_fidelities, cross_fidelities = determine_routing_fidelities(beam_splitter_reflectivities, chip_dim)

    # Photons enter on every other mode (dual-rail), so only even-indexed
    # transmissions are relevant for input cost.
    input_transmissions = input_transmission[::2]
    input_window = target_dim // 2
    input_transmissions_per_edge = [
        -np.log(np.prod(input_transmissions[i : i + input_window])) for i in range(number_nodes_first_layer)
    ]

    output_transmissions_per_edge = [
        -np.log(np.prod(output_transmission[i : i + target_dim])) for i in range(0, int(chip_dim - target_dim + 1), 2)
    ]

    layers: list = []
    edges: list = []

    for layer in range(number_of_layers):
        if layer == 0:
            current_layer_nodes = graph.add_nodes_from(["source"])
        elif layer == 1:
            current_layer_nodes = graph.add_nodes_from([f"input_node_{i}" for i in range(number_nodes_first_layer)])
        elif layer == number_of_layers - 1:
            current_layer_nodes = graph.add_nodes_from(["sink"])
        else:
            current_layer_nodes = graph.add_nodes_from([f"node_{i}" for i in range(number_nodes_intermediate_layers)])
        layers.append(current_layer_nodes)

    for layer in range(number_of_layers - 1):
        if layer == 0:
            edges.append([
                (layers[layer][0], layers[layer + 1][i], input_transmissions_per_edge[i])
                for i in range(number_nodes_first_layer)
            ])
        elif layer == 1:
            bar_costs = [
                -np.log(np.prod(bar_fidelities[i : i + target_dim // 2])) for i in range(number_nodes_first_layer)
            ]
            cross_costs = [
                -np.log(np.prod(cross_fidelities[i : i + target_dim // 2])) for i in range(number_nodes_first_layer)
            ]
            current_edges = [
                (layers[layer][i], layers[layer + 1][2 * i], bar_costs[i]) for i in range(number_nodes_first_layer)
            ]
            current_edges += [
                (layers[layer][i], layers[layer + 1][2 * i + 1], cross_costs[i])
                for i in range(number_nodes_first_layer)
            ]
            edges.append(current_edges)
        elif layer == number_of_layers - 2:
            edges.append([
                (layers[layer][i], layers[layer + 1][0], output_transmissions_per_edge[i // 2])
                for i in range(number_nodes_intermediate_layers)
            ])
        elif layer % 2 == 1:
            n = number_nodes_intermediate_layers
            current_edges = [
                (
                    layers[layer][i],
                    layers[layer + 1][i],
                    get_edge_fidelity_odd_graph_layer(
                        layer, i, i, bar_fidelities, cross_fidelities, chip_dim, target_dim
                    ),
                )
                for i in range(n)
            ]
            current_edges += [
                (
                    layers[layer][i],
                    layers[layer + 1][i + 1],
                    get_edge_fidelity_odd_graph_layer(
                        layer, i, i + 1, bar_fidelities, cross_fidelities, chip_dim, target_dim
                    ),
                )
                for i in range(0, n, 2)
            ]
            current_edges += [
                (
                    layers[layer][i],
                    layers[layer + 1][i - 1],
                    get_edge_fidelity_odd_graph_layer(
                        layer, i, i - 1, bar_fidelities, cross_fidelities, chip_dim, target_dim
                    ),
                )
                for i in range(1, n, 2)
            ]
            edges.append(current_edges)
        elif layer % 2 == 0 and layer != 0:
            n = number_nodes_intermediate_layers
            current_edges = [
                (
                    layers[layer][i],
                    layers[layer + 1][i],
                    get_edge_fidelity_even_graph_layer(
                        layer, i, i, bar_fidelities, cross_fidelities, chip_dim, target_dim
                    ),
                )
                for i in range(n)
            ]
            current_edges += [
                (
                    layers[layer][i],
                    layers[layer + 1][i + 1],
                    get_edge_fidelity_even_graph_layer(
                        layer, i, i + 1, bar_fidelities, cross_fidelities, chip_dim, target_dim
                    ),
                )
                for i in range(1, n - 1, 2)
            ]
            current_edges += [
                (
                    layers[layer][i],
                    layers[layer + 1][i - 1],
                    get_edge_fidelity_even_graph_layer(
                        layer, i, i - 1, bar_fidelities, cross_fidelities, chip_dim, target_dim
                    ),
                )
                for i in range(2, n, 2)
            ]
            edges.append(current_edges)

    for edges_of_a_layer in edges:
        graph.add_edges_from(edges_of_a_layer)

    pos: dict[int, tuple[float, float]] = {}
    for layer in range(number_of_layers):
        if layer == 0:
            pos[layers[layer][0]] = (layer, -number_nodes_first_layer / 2 - 1)
        elif layer == 1:
            for i, node in enumerate(layers[layer]):
                pos[node] = (layer, -(i * 2))
        elif layer == number_of_layers - 1:
            pos[layers[layer][0]] = (layer, -number_nodes_first_layer / 2 - 1)
        else:
            for i, node in enumerate(layers[layer]):
                pos[node] = (layer, -i)

    return RoutingGraph(graph=graph, positions=pos, layers=layers)
