# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""Routing graph construction and fidelity scoring for the photonic compiler."""

from __future__ import annotations

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


def generate_beam_splitter_matrix(
    chip_size: int,
    ideal_bs: bool = False,
    rng: np.random.Generator | None = None,
) -> np.ndarray:
    """Generate beam splitter reflectivity values as a 1D array.

    Values are generated with controlled global statistics and controlled
    intra-MZI differences.  The array is ordered MZI-by-MZI, strictly
    aligned with the spatial mapping of the unitary builder:
    ``[MZI_0_in, MZI_0_out, MZI_1_in, MZI_1_out, …]``.

    When ``ideal_bs`` is ``False`` the values are drawn so that
    (approximately, in finite samples):

    * global mean ≈ 0.552
    * global std ≈ 0.038
    * average absolute difference within each MZI ≈ 0.019 (exponential distribution)

    Args:
        chip_size: Number of spatial modes on the chip.
        ideal_bs: If ``True``, return an array filled with the ideal
            reflectivity of 0.5.
        rng: NumPy random generator for reproducibility.  Accepts a
            :class:`numpy.random.Generator`, an integer seed, or ``None``
            (creates a new generator with an unpredictable seed).

    Returns:
        1D NumPy array of beam splitter reflectivities with length
        ``2 * total_mzis``, where ``total_mzis`` is the total number of
        MZIs across all layers.
    """
    target_mean = 0.552
    target_std = 0.038
    target_avg_abs_diff = 0.019

    num_mzi_layers = chip_size  # 2 * chip_size physical layers → chip_size MZI layers

    group_sizes = []
    for layer_idx in range(num_mzi_layers):
        if layer_idx % 2 == 0:
            group_sizes.append(chip_size // 2)
        else:
            group_sizes.append(chip_size // 2 - 1)

    total_mzis = int(np.sum(group_sizes))

    if ideal_bs:
        return np.full(2 * total_mzis, 0.5)

    rng = np.random.default_rng(rng)

    # Intra-MZI differences follow an exponential distribution.
    deltas = rng.exponential(scale=target_avg_abs_diff, size=total_mzis)
    mean_delta = np.mean(deltas)
    if mean_delta > 0:
        deltas *= target_avg_abs_diff / mean_delta
    else:
        deltas[:] = target_avg_abs_diff

    diff_variance_component = np.mean((deltas / 2.0) ** 2)
    required_center_variance = max(target_std**2 - diff_variance_component, 0.0)

    centers_raw = rng.normal(loc=0.0, scale=1.0, size=total_mzis)
    centers_raw_var = np.var(centers_raw)
    if centers_raw_var > 0:
        centers = centers_raw * np.sqrt(required_center_variance / centers_raw_var)
    else:
        centers = np.zeros_like(centers_raw)
    centers += target_mean - np.mean(centers)

    signs = rng.choice([-1.0, 1.0], size=total_mzis)

    bs_values = np.zeros(2 * total_mzis)
    bs_values[0::2] = centers - signs * (deltas / 2.0)
    bs_values[1::2] = centers + signs * (deltas / 2.0)

    # Affine correction for exact global statistics.
    current_mean = np.mean(bs_values)
    current_std = np.std(bs_values)
    if current_std > 0:
        bs_values = (bs_values - current_mean) * (target_std / current_std) + target_mean
    else:
        bs_values[:] = target_mean

    # Re-adjust intra-pair differences.
    pair_abs_diffs = np.abs(bs_values[0::2] - bs_values[1::2])
    current_avg_abs_diff = np.mean(pair_abs_diffs)
    if current_avg_abs_diff > 0:
        ratio = target_avg_abs_diff / current_avg_abs_diff
        pair_means = 0.5 * (bs_values[0::2] + bs_values[1::2])
        pair_deltas = 0.5 * (bs_values[1::2] - bs_values[0::2]) * ratio
        bs_values[0::2] = pair_means - pair_deltas
        bs_values[1::2] = pair_means + pair_deltas

    # Final exact mean/std normalisation.
    current_mean = np.mean(bs_values)
    current_std = np.std(bs_values)
    if current_std > 0:
        bs_values = (bs_values - current_mean) * (target_std / current_std) + target_mean
    else:
        bs_values[:] = target_mean

    return bs_values


def determine_routing_fidelitites(
    beam_splitter_reflectivities: np.ndarray,
    chip_dim: int,
) -> tuple[list[float], list[float]]:
    """Compute bar and cross fidelities for all MZIs in the chip.

    Results are ordered sequentially by layer: entries for layer 0 come
    first, followed by layer 1, and so on.  Within each layer entries are
    ordered by mode index.

    Args:
        beam_splitter_reflectivities: 1D array of reflectivity values ordered
            sequentially by layer, as produced by
            :func:`generate_beam_splitter_matrix`.
        chip_dim: Number of spatial modes on the chip.

    Returns:
        A tuple ``(bar_fidelities, cross_fidelities)`` where each element is
        a flat list of per-MZI fidelity values indexed as
        ``[layer0_mzi0, layer0_mzi1, …, layer1_mzi0, …]``.
    """
    used_beam_splitters = 0
    bar_fidelities: list[float] = []
    cross_fidelities: list[float] = []

    for layer in range(chip_dim):
        if layer % 2 == 0:
            for i in range(chip_dim // 2):
                current_bs = [
                    beam_splitter_reflectivities[i + used_beam_splitters],
                    beam_splitter_reflectivities[i + used_beam_splitters + chip_dim // 2],
                ]
                bar_fidelities.append(bar_fidelity(current_bs))
                cross_fidelities.append(cross_fidelity(current_bs))
            used_beam_splitters += chip_dim
        if layer % 2 == 1:
            for i in range(chip_dim // 2 - 1):
                current_bs = [
                    beam_splitter_reflectivities[i + used_beam_splitters],
                    beam_splitter_reflectivities[i + used_beam_splitters + chip_dim // 2 - 2],
                ]
                bar_fidelities.append(bar_fidelity(current_bs))
                cross_fidelities.append(cross_fidelity(current_bs))
            used_beam_splitters += chip_dim - 2

    return bar_fidelities, cross_fidelities


def _edge_cost_from_mzi_block(
    fidelity_list: list[float],
    fidelity_offset: int,
    source_node_idx: int,
    mzi_count_in_layer: int,
    num_parallel_photons: int,
) -> float:
    """Return ``-log(product of fidelities)`` over the MZI block for one edge.

    For node ``i``, the contiguous MZI block starts at ``floor((i-1) / 2)``.
    For ``target_dim=4`` (two photons) this reproduces the mapping
    ``i=1,2 → [0, 1]``, ``i=3,4 → [1, 2]``, with a boundary fallback of 1.0.

    Args:
        fidelity_list: Flat list of fidelities for all MZIs.
        fidelity_offset: Index into ``fidelity_list`` where the relevant
            chip layer starts.
        source_node_idx: Index of the source node within its graph layer.
        mzi_count_in_layer: Total number of MZIs in the relevant chip layer.
        num_parallel_photons: Number of photons routed simultaneously
            (equals ``target_dim // 2``).

    Returns:
        Non-negative edge cost ``-log(combined_fidelity)``.
    """
    start_mzi_idx = (source_node_idx - 1) // 2

    combined_fidelity = 1.0
    for mzi_idx in range(start_mzi_idx, start_mzi_idx + num_parallel_photons):
        if 0 <= mzi_idx < mzi_count_in_layer:
            combined_fidelity *= fidelity_list[fidelity_offset + mzi_idx]

    return -np.log(combined_fidelity)


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

    Even graph layers (0, 2, 4, …) correspond to odd chip layers (1, 3, 5, …).
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

    chip_layer = graph_layer + 1  # even graph layer → next odd chip layer
    mzis_per_even_chip_layer = chip_dim // 2
    mzis_per_odd_chip_layer = chip_dim // 2 - 1

    fidelity_offset = 0
    for chip_layer_idx in range(chip_layer):
        if chip_layer_idx % 2 == 0:
            fidelity_offset += mzis_per_even_chip_layer
        else:
            fidelity_offset += mzis_per_odd_chip_layer

    fidelity_list = bar_fidelities if edge_type == "bar" else cross_fidelities

    return _edge_cost_from_mzi_block(
        fidelity_list=fidelity_list,
        fidelity_offset=fidelity_offset,
        source_node_idx=source_node_idx,
        mzi_count_in_layer=mzis_per_odd_chip_layer,
        num_parallel_photons=target_dim // 2,
    )


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

    Odd graph layers (1, 3, 5, …) correspond to even chip layers (0, 2, 4, …).
    Even chip layers have MZIs on all mode pairs: (0-1), (2-3), (4-5), …

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

    chip_layer = graph_layer - 1  # odd graph layer → preceding even chip layer
    mzis_per_even_chip_layer = chip_dim // 2
    mzis_per_odd_chip_layer = chip_dim // 2 - 1

    fidelity_offset = 0
    for chip_layer_idx in range(chip_layer):
        if chip_layer_idx % 2 == 0:
            fidelity_offset += mzis_per_even_chip_layer
        else:
            fidelity_offset += mzis_per_odd_chip_layer

    fidelity_list = bar_fidelities if edge_type == "bar" else cross_fidelities

    return _edge_cost_from_mzi_block(
        fidelity_list=fidelity_list,
        fidelity_offset=fidelity_offset,
        source_node_idx=source_node_idx,
        mzi_count_in_layer=mzis_per_even_chip_layer,
        num_parallel_photons=target_dim // 2,
    )


def construct_graph(
    chip_dim: int,
    target_dim: int,
    input_transmission: np.ndarray,
    output_transmission: np.ndarray,
    beam_splitter_reflectivities: np.ndarray,
) -> tuple[rx.PyDiGraph, dict[int, tuple[float, float]], list]:
    """Construct the routing DAG for photon placement optimisation.

    The graph encodes routing decisions as a shortest-path problem:

    * Layer 0 → 1 edges encode input placement costs (source to candidate
      input positions).
    * Intermediate edges encode routing costs through the chip's MZI layers.
    * Final edges to the sink encode output transmission costs and implicitly
      select the computation window.

    For ``chip_dim=8``, ``target_dim=4`` there are three candidate input
    positions, intermediate routing layers, and three output windows.

    Args:
        chip_dim: Total number of spatial modes on the chip.
        target_dim: Dimension of the target unitary.
        input_transmission: Per-mode input transmission coefficients, shape
            ``(chip_dim,)``.
        output_transmission: Per-mode output transmission coefficients, shape
            ``(chip_dim,)``.
        beam_splitter_reflectivities: 1D array of beam splitter reflectivities
            as produced by :func:`generate_beam_splitter_matrix`.

    Returns:
        A tuple ``(graph, pos, layers)`` where *graph* is the weighted directed
        acyclic graph, *pos* maps each node index to an ``(x, y)``
        visualisation coordinate, and *layers* is the list of per-layer node
        index arrays returned by rustworkx.
    """
    graph = rx.PyDiGraph()

    number_of_layers = int(chip_dim - target_dim + 3)
    number_nodes_first_layer = int((chip_dim - target_dim) / 2 + 1)
    number_nodes_intermediate_layers = int(chip_dim - target_dim + 2)

    bar_fidelities, cross_fidelities = determine_routing_fidelitites(beam_splitter_reflectivities, chip_dim)

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
                -np.log(np.prod(bar_fidelities[i : i + target_dim // 2 - 1])) for i in range(number_nodes_first_layer)
            ]
            cross_costs = [
                -np.log(np.prod(cross_fidelities[i : i + target_dim // 2 - 1])) for i in range(number_nodes_first_layer)
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

    return graph, pos, layers
