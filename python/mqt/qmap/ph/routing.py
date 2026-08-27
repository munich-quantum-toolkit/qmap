# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""Shortest-path routing and port inference for the photonic compiler."""

from __future__ import annotations

from enum import IntEnum
from typing import TYPE_CHECKING

import torch

if TYPE_CHECKING:
    import rustworkx as rx


class MaskState(IntEnum):
    """State codes for every (mode, layer) cell of the movement mask.

    The movement mask guides the phase optimizer: cells in routing layers
    are forced to implement bar or cross operations, while cells in the
    computation zone are free optimization parameters.
    """

    MZI = 0  # Compute: both phases are learnable
    BAR = 1  # Routing: passes light straight through (0, pi)
    CROSS = 2  # Routing: swaps ports (0, 0)
    TOP_ONLY = 3  # Virtual PS: top is param, bottom is top + pi
    BOT_ONLY = 4  # Virtual PS: bottom is param, top is bottom + pi


def find_optimal_routing_dag(
    graph: rx.PyDiGraph,
    layers: list,
    source_node: int,
) -> tuple[dict[int, float], dict[int, int | None]]:
    """Compute the shortest path through a layered photonic DAG.

    Performs a forward sweep over each layer in topological order, reading
    edge weights directly from the graph.

    Args:
        graph: Weighted directed acyclic graph as produced by
            :func:`graph.construct_graph`.
        layers: Per-layer node index arrays as returned by
            :func:`graph.construct_graph`.
        source_node: Node index of the DAG source.

    Returns:
        A tuple ``(distances, predecessors)`` where *distances* maps each
        node index to its minimum accumulated cost from the source, and
        *predecessors* maps each node index to the preceding node index on
        the optimal path (``None`` for the source).
    """
    distances: dict[int, float] = {node: float("inf") for layer in layers for node in layer}
    predecessors: dict[int, int | None] = {node: None for layer in layers for node in layer}
    distances[source_node] = 0.0

    for layer in layers:
        for u in layer:
            if distances[u] == float("inf"):
                continue
            for _, v, weight in graph.out_edges(u):
                accumulated_cost = distances[u] + weight
                if accumulated_cost < distances[v]:
                    distances[v] = accumulated_cost
                    predecessors[v] = u

    return distances, predecessors


def reconstruct_path(
    predecessors: dict[int, int | None],
    target_node: int,
) -> list[int]:
    """Walk backward from the sink to reconstruct the optimal node sequence.

    Args:
        predecessors: Mapping from node index to the preceding node index on
            the optimal path, as returned by
            :func:`find_optimal_routing_dag`.
        target_node: Node index of the DAG sink.

    Returns:
        Ordered list of node indices from source to sink.  Returns an empty
        list if ``target_node`` was unreachable.
    """
    path: list[int] = []
    current: int | None = target_node

    while current is not None:
        path.append(current)
        current = predecessors[current]

    path.reverse()

    if len(path) == 1 and predecessors[target_node] is None:
        return []

    return path


def get_best_route(
    graph: rx.PyDiGraph,
    layers: list,
) -> tuple[list[int], float]:
    """Find the optimal routing path and return relative node indices per layer.

    Args:
        graph: Weighted directed acyclic graph as produced by
            :func:`graph.construct_graph`.
        layers: Per-layer node index arrays as returned by
            :func:`graph.construct_graph`.

    Returns:
        A tuple ``(relative_path_indices, final_cost)`` where
        *relative_path_indices* is the list of within-layer positions of each
        chosen node and *final_cost* is the total accumulated path cost.
    """
    source_node = layers[0][0]
    sink_node = layers[-1][0]

    distances, predecessors = find_optimal_routing_dag(graph, layers, source_node)
    absolute_path_nodes = reconstruct_path(predecessors, sink_node)

    relative_path_indices: list[int] = []
    if absolute_path_nodes:
        for layer_idx, node_id in enumerate(absolute_path_nodes):
            relative_index = list(layers[layer_idx]).index(node_id)
            relative_path_indices.append(relative_index)

    return relative_path_indices, distances[sink_node]


def infer_input_computation_and_output_ports(
    route: list[int],
    target_dim: int,
) -> tuple[list[int], list[int], list[int]]:
    """Infer input ports, output ports, and computation-zone active columns from a route.

    The first node after the source defines the input window and the last
    node before the sink defines the computation/output window.

    Args:
        route: Relative-index path as returned by :func:`get_best_route`.
        target_dim: Dimension of the target unitary.

    Returns:
        A tuple ``(input_ports, output_ports, active_cols)`` where
        *input_ports* are the physical mode indices used for photon injection,
        *output_ports* are the physical mode indices of the computation zone,
        and *active_cols* are the column indices active within the computation
        zone (even or odd, depending on the routing outcome).

    Raises:
        ValueError: If ``route`` contains fewer than two nodes.
    """
    if len(route) < 2:
        msg = "Route must have at least 2 nodes (source and sink)"
        raise ValueError(msg)

    input_index = route[1]
    computation_index = route[-2]

    input_ports_cache = [(input_index * 2) + i for i in range(target_dim)]
    input_ports = input_ports_cache[::2]

    active_cols = list(range(0, target_dim, 2)) if computation_index % 2 == 0 else list(range(1, target_dim, 2))

    output_index_cache = computation_index - 1 if computation_index % 2 == 1 else computation_index

    output_ports = [output_index_cache + i for i in range(target_dim)]

    return input_ports, output_ports, active_cols


def route_to_movement_mask(
    route: list[int],
    chip_dim: int,
    target_dim: int,
) -> torch.Tensor:
    """Convert a routing path to a movement mask for the phase optimizer.

    The mask encodes the state of each (mode, layer) cell on the chip using
    :class:`MaskState` values.

    Args:
        route: Relative-index path as returned by :func:`get_best_route`.
        chip_dim: Total number of spatial modes on the chip.
        target_dim: Dimension of the target unitary.

    Returns:
        Integer tensor of shape ``(chip_dim, chip_dim)`` containing
        :class:`MaskState` codes for every (mode, layer) position.

    Raises:
        ValueError: If the route contains a non-adjacent transition between
            consecutive layers (a step that is neither straight-through nor a
            move to an immediate neighbor), which cannot correspond to a valid
            routing-graph edge.
    """
    movement_mask = torch.ones((chip_dim, chip_dim), dtype=torch.int)

    if len(route) < 2:
        return movement_mask

    # Each transition is realized in one chip layer (chip_layer = i - 2). A single rule covers
    # every layer: a "bar" step (mode unchanged) leaves the column as BAR, while a "cross" step
    # (a move to an immediate neighbor) marks a target_dim-wide CROSS run.
    for i in range(2, len(route) - 1):
        chip_layer = i - 2
        # The node being left. The input-layer node (i == 2) is half-indexed - node n sits on
        # physical mode 2n - whereas every later node is already a physical mode index.
        prev_mode = int(route[i - 1]) * 2 if i == 2 else int(route[i - 1])
        node = int(route[i])

        if prev_mode == node:
            continue  # bar: photons pass straight through; the column stays BAR

        if abs(prev_mode - node) != 1:
            msg = (
                f"Invalid edge from node_{route[i - 1]} to node_{node} at chip layer {chip_layer}: a routing "
                f"transition must be straight-through (bar) or a move to an immediate neighbor (cross)."
            )
            raise ValueError(msg)

        # cross: the run starts at the top mode of the MZI pair (in this chip layer) that contains
        # prev_mode. Even chip layers pair modes (0,1),(2,3),...; odd chip layers pair (1,2),(3,4),...
        # - so rounding prev_mode down to the layer's parity gives that pair's top mode.
        row_start = prev_mode - ((prev_mode - chip_layer % 2) % 2)
        movement_mask[row_start : row_start + target_dim, chip_layer] = MaskState.CROSS

    output_index = route[-2]
    mode_start = int((output_index // 2) * 2)
    mode_end = min(mode_start + target_dim, chip_dim)
    compute_layer_start = max(0, chip_dim - target_dim)

    movement_mask[mode_start:mode_end, compute_layer_start:chip_dim] = MaskState.MZI

    # Convert mixed compute-boundary pairs to virtual phase-shifter states.
    for chip_layer in range(compute_layer_start, chip_dim):
        if chip_layer % 2 == 0:
            mzi_pairs = [(i, i + 1) for i in range(0, chip_dim - 1, 2)]
        else:
            mzi_pairs = [(i, i + 1) for i in range(1, chip_dim - 1, 2)]

        for top, bot in mzi_pairs:
            top_is_compute = movement_mask[top, chip_layer].item() == MaskState.MZI
            bot_is_compute = movement_mask[bot, chip_layer].item() == MaskState.MZI

            if top_is_compute and not bot_is_compute:
                movement_mask[top, chip_layer] = MaskState.TOP_ONLY
                movement_mask[bot, chip_layer] = MaskState.TOP_ONLY
            elif bot_is_compute and not top_is_compute:
                movement_mask[top, chip_layer] = MaskState.BOT_ONLY
                movement_mask[bot, chip_layer] = MaskState.BOT_ONLY

    return movement_mask
