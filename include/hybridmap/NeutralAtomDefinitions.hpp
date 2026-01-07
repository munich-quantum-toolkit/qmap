/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#pragma once

#include "datastructures/SymmetricMatrix.hpp"
#include "ir/Definitions.hpp"
#include "ir/operations/Operation.hpp"

#include <cstdint>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

namespace qc {
// forward declaration
class Operation;
} // namespace qc

namespace na {
/**
 * @brief Index of a site in the SLM grid where an atom may reside.
 * @details Refers to a physical coordinate slot; occupancy varies during
 * mapping/shuttling.
 */
using CoordIndex = std::uint32_t;
using CoordIndices = std::vector<CoordIndex>;
using AdjacencyMatrix = qc::SymmetricMatrix<std::uint8_t>;

/**
 * @brief Identifier of an atom (hardware qubit) in the architecture.
 * @details May or may not currently host a logical qubit; linked to a
 * coordinate index.
 */
using HwQubit = uint32_t;
using HwQubits = std::set<HwQubit>;
using HwQubitsVector = std::vector<HwQubit>;
/**
 * @brief Bridge operation and the sequence of hardware qubits it spans.
 * @details The vector lists qubits involved in mediating an interaction (e.g.,
 * for a CZ bridge chain).
 */
using Bridge = std::pair<const qc::Operation*, std::vector<HwQubit>>;
using Bridges = std::vector<Bridge>;
using HwPositions [[maybe_unused]] =
    std::vector<HwQubits>; // Deprecated alias for historical layout snapshots.
/**
 * @brief Set of logical circuit qubits (indices in the quantum computation).
 */
using Qubits = std::set<qc::Qubit>;

/**
 * @brief Hardware-level SWAP operand pair (one endpoint can be unmapped).
 */
using Swap = std::pair<HwQubit, HwQubit>;
using Swaps = std::vector<Swap>;
/**
 * @brief SWAP annotated with a weight (e.g., cost or heuristic score).
 */
using WeightedSwap = std::pair<Swap, qc::fp>;
using WeightedSwaps = std::vector<WeightedSwap>;
/**
 * @brief Distance (# of SWAPs) between hardware qubits under current mapping.
 */
using SwapDistance = int32_t;
/**
 * @brief Atom shuttle between two coordinate indices.
 * @details c1: source (expected occupied), c2: destination (expected free).
 * load1/load2 specify load/unload actions (e.g., addressing focus).
 */
struct AtomMove {
  CoordIndex c1 = 0;
  CoordIndex c2 = 0;
  bool load1 = true;
  bool load2 = true;

  /**
   * @brief Equality comparison.
   * @param other Move to compare.
   * @return True if all fields match.
   */
  bool operator==(const AtomMove& other) const {
    return c1 == other.c1 && c2 == other.c2 && load1 == other.load1 &&
           load2 == other.load2;
  }
  /**
   * @brief Inequality comparison.
   * @param other Move to compare.
   * @return True if any field differs.
   */
  bool operator!=(const AtomMove& other) const { return !(*this == other); }

  /**
   * @brief Less-than comparison for ordering.
   * @param other Move to compare.
   * @return True if this move is less than the other in lexicographical order.
   */
  bool operator<(const AtomMove& other) const {
    return std::tie(c1, c2, load1, load2) <
           std::tie(other.c1, other.c2, other.load1, other.load2);
  }
};

/**
 * @brief List of quantum operations (gate pointers), e.g., a layer.
 */
using GateList = std::vector<const qc::Operation*>;
/**
 * @brief Collection of gate lists (e.g., per-qubit candidate sets).
 */
using GateLists = std::vector<GateList>;

} // namespace na
