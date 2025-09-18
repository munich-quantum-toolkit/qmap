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

enum class LookaheadHeuristic : std::uint8_t {
  /** no lookahead */
  None,
  /** maximum over all distances between any virtual qubit pair in the given
     layer; optimizing gate-count */
  GateCountMaxDistance,
  /** sum over all distances between any virtual qubit pair in the given layer;
     optimizing gate-count */
  GateCountSumDistance
};

/**
 * A heuristic is fidelity aware if it takes into account the error rates of
 * physical qubits and minimizes the total error of the mapped circuit.
 */
[[maybe_unused]] static inline bool
isFidelityAware(const LookaheadHeuristic heuristic) {
  switch (heuristic) {
  case LookaheadHeuristic::None:
  case LookaheadHeuristic::GateCountMaxDistance:
  case LookaheadHeuristic::GateCountSumDistance:
    return false;
  }
  return false;
}

[[maybe_unused]] static inline std::string
toString(const LookaheadHeuristic heuristic) {
  switch (heuristic) {
  case LookaheadHeuristic::None:
    return "none";
  case LookaheadHeuristic::GateCountMaxDistance:
    return "gate_count_max_distance";
  case LookaheadHeuristic::GateCountSumDistance:
    return "gate_count_sum_distance";
  }
  return " ";
}

[[maybe_unused]] static LookaheadHeuristic
lookaheadHeuristicFromString(const std::string& heuristic) {
  if (heuristic == "none" || heuristic == "0") {
    return LookaheadHeuristic::None;
  }
  if (heuristic == "gate_count_max_distance" || heuristic == "1") {
    return LookaheadHeuristic::GateCountMaxDistance;
  }
  if (heuristic == "gate_count_sum_distance" || heuristic == "2") {
    return LookaheadHeuristic::GateCountSumDistance;
  }
  throw std::invalid_argument("Invalid lookahead heuristic value: " +
                              heuristic);
}
