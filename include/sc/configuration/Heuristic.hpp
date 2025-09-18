/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

enum class Heuristic : std::uint8_t {
  /** maximum over all distances between any virtual qubit pair in the current
     layer; optimizing gate-count; admissible; tight */
  GateCountMaxDistance,
  /** sum over all distances between any virtual qubit pair in the current
     layer; optimizing gate-count; not admissible; tight */
  GateCountSumDistance,
  /** sum over all distances between any virtual qubit pair in the current layer
     minus the upper limit of viable shared swaps; optimizing gate-count;
     admissible; tight */
  GateCountSumDistanceMinusSharedSwaps,
  /** maximum of `Heuristic::GateCountMaxDistance` and
     `Heuristic::GateCountSumDistanceMinusSharedSwaps`; optimizing gate-count;
     admissible; tight */
  GateCountMaxDistanceOrSumDistanceMinusSharedSwaps,
  /** minimum cost if each virtual qubit pair/qubit is mapped to its
     individually best physical edge/qubit; optimizing fidelity; admissible;
     tight */
  FidelityBestLocation
};

/**
 * A heuristic is admissible if it never overestimates the cost of the best
 * reachable goal node, i.e. c(n*) <= c(n) + h(n) for cost function c,
 * heuristic h, any node n in the search graph, and n* the best reachable goal
 * node from n.
 */
[[maybe_unused]] static inline bool isAdmissible(const Heuristic heuristic) {
  switch (heuristic) {
  case Heuristic::GateCountMaxDistance:
  case Heuristic::FidelityBestLocation:
    return true;
  case Heuristic::GateCountSumDistanceMinusSharedSwaps:
  case Heuristic::GateCountMaxDistanceOrSumDistanceMinusSharedSwaps:
  case Heuristic::GateCountSumDistance:
    return false;
  }
  return false;
}

/**
 * A heuristic is non-decreasing if the estimated cost (i.e. c(n) + h(n)) is
 * non-decreasing along any path.
 */
[[maybe_unused]] static inline bool isNonDecreasing(const Heuristic heuristic) {
  switch (heuristic) {
  case Heuristic::GateCountMaxDistance:
  case Heuristic::FidelityBestLocation:
  case Heuristic::GateCountSumDistanceMinusSharedSwaps:
  case Heuristic::GateCountMaxDistanceOrSumDistanceMinusSharedSwaps:
    return true;
  case Heuristic::GateCountSumDistance:
    return false;
  }
  return false;
}

/**
 * A heuristic is principally admissible if it never overestimates the cost of
 * the globally optimal solution along the solution path, i.e. c(n*) <= c(n) +
 * h(n) for cost function c, heuristic h, any node n along the optimal solution
 * path, and n* the globally optimal solution node.
 */
[[maybe_unused]] static inline bool
isPrincipallyAdmissible(const Heuristic heuristic) {
  switch (heuristic) {
  case Heuristic::GateCountMaxDistance:
  case Heuristic::GateCountSumDistanceMinusSharedSwaps:
  case Heuristic::GateCountMaxDistanceOrSumDistanceMinusSharedSwaps:
  case Heuristic::FidelityBestLocation:
    return true;
  case Heuristic::GateCountSumDistance:
    return false;
  }
  return false;
}

/**
 * A heuristic is tight if it is 0 in all goal nodes, i.e. h(n*) = 0 for any
 * goal node n*.
 */
[[maybe_unused]] static inline bool isTight(const Heuristic heuristic) {
  switch (heuristic) {
  case Heuristic::GateCountMaxDistance:
  case Heuristic::GateCountSumDistanceMinusSharedSwaps:
  case Heuristic::GateCountMaxDistanceOrSumDistanceMinusSharedSwaps:
  case Heuristic::GateCountSumDistance:
    return true;
  case Heuristic::FidelityBestLocation:
    return false;
  }
  return false;
}

/**
 * A heuristic is fidelity aware if it takes into account the error rates of
 * physical qubits and minimizes the total error of the mapped circuit.
 */
[[maybe_unused]] static inline bool isFidelityAware(const Heuristic heuristic) {
  switch (heuristic) {
  case Heuristic::FidelityBestLocation:
    return true;
  case Heuristic::GateCountMaxDistance:
  case Heuristic::GateCountSumDistanceMinusSharedSwaps:
  case Heuristic::GateCountMaxDistanceOrSumDistanceMinusSharedSwaps:
  case Heuristic::GateCountSumDistance:
    return false;
  }
  return false;
}

[[maybe_unused]] static inline std::string toString(const Heuristic heuristic) {
  switch (heuristic) {
  case Heuristic::GateCountMaxDistance:
    return "gate_count_max_distance";
  case Heuristic::GateCountSumDistance:
    return "gate_count_sum_distance";
  case Heuristic::GateCountSumDistanceMinusSharedSwaps:
    return "gate_count_sum_distance_minus_shared_swaps";
  case Heuristic::GateCountMaxDistanceOrSumDistanceMinusSharedSwaps:
    return "gate_count_max_distance_or_sum_distance_minus_shared_swaps";
  case Heuristic::FidelityBestLocation:
    return "fidelity_best_location";
  }
  return " ";
}

[[maybe_unused]] static Heuristic
heuristicFromString(const std::string& heuristic) {
  if (heuristic == "gate_count_max_distance" || heuristic == "0") {
    return Heuristic::GateCountMaxDistance;
  }
  if (heuristic == "gate_count_sum_distance" || heuristic == "1") {
    return Heuristic::GateCountSumDistance;
  }
  if (heuristic == "gate_count_sum_distance_minus_shared_swaps" ||
      heuristic == "2") {
    return Heuristic::GateCountSumDistanceMinusSharedSwaps;
  }
  if (heuristic ==
          "gate_count_max_distance_or_sum_distance_minus_shared_swaps" ||
      heuristic == "3") {
    return Heuristic::GateCountMaxDistanceOrSumDistanceMinusSharedSwaps;
  }
  if (heuristic == "fidelity_best_location" || heuristic == "4") {
    return Heuristic::FidelityBestLocation;
  }
  throw std::invalid_argument("Invalid heuristic value: " + heuristic);
}
