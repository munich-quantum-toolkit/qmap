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

#include "ir/operations/Operation.hpp"
#include "na/zoned/Architecture.hpp"

#include <array>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace na::zoned {
/// A list of single-qubit gates representing a single-qubit gate layer.
using SingleQubitGateLayer =
    std::vector<std::reference_wrapper<const qc::Operation>>;
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
} // namespace na::zoned
