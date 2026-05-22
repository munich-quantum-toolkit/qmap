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

#include "ir/operations/Operation.hpp"
#include "na/zoned/Architecture.hpp"

#include <array>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace na::zoned {
/// A non-owning reference layer of single-qubit gates (used for scheduling
/// output).
using SingleQubitGateRefLayer =
    std::vector<std::reference_wrapper<const qc::Operation>>;
/// An owning layer of single-qubit gates (used after decomposition).
using SingleQubitGateLayer = std::vector<std::unique_ptr<const qc::Operation>>;
/// A pair of qubits as an array that allows iterating over the qubits.
using QubitPair = std::array<qc::Qubit, 2>;
/// A list of two-qubit gates representing a two-qubit gate layer.
using TwoQubitGateLayer = std::vector<QubitPair>;
/// Placement of one layer as a mapping from qubits (indices) to sites
using Placement = std::vector<Site>;
/**
 * Routing from one layer to the next. The first dimension determines the
 * rearrangement group, the second all qubits that are moved in this group.
 */
using Routing = std::vector<std::vector<qc::Qubit>>;
/**
 * An unordered map from sites to values of type T
 * @tparam T the type of the value
 */
template <class T> using SiteMap = std::unordered_map<Site, T>;
/// An unordered set of sites
using SiteSet = std::unordered_set<Site>;

/**
 * Gate layers produced by the scheduler. Contains non-owning references to the
 * operations of the original quantum computation. There is always one
 * single-qubit gate layer more than two-qubit gate layers.
 */
struct SchedulerResult {
  /// Non-owning layers of single-qubit gates (refs into the original circuit).
  std::vector<SingleQubitGateRefLayer> singleQubitLayers;
  /// Layers of two-qubit gates.
  std::vector<TwoQubitGateLayer> twoQubitLayers;
};

/**
 * Gate layers produced by the decomposer. Contains owning copies of the
 * (potentially rewritten) operations. There is always one single-qubit gate
 * layer more than two-qubit gate layers.
 */
struct DecompositionResult {
  /// Owning layers of single-qubit gates (consumed by generate()).
  std::vector<SingleQubitGateLayer> singleQubitLayers;
  /// Layers of two-qubit gates (consumed by analyzeReuse() and synthesize()).
  std::vector<TwoQubitGateLayer> twoQubitLayers;
};
} // namespace na::zoned
