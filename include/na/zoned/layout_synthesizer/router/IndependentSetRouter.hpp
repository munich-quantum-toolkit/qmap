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

#include "ir/Definitions.hpp"
#include "na/zoned/Architecture.hpp"
#include "na/zoned/Types.hpp"
#include "na/zoned/layout_synthesizer/router/RouterBase.hpp"

#include <cstddef>
#include <functional>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace na::zoned {

/**
 * This class implements the default Router for the zoned neutral atom compiler
 * that forms groups of parallel movements by calculating a maximal independent
 * set.
 */
class IndependentSetRouter : public RouterBase {
  std::reference_wrapper<const Architecture> architecture_;

public:
  /**
   * The configuration of the IndependentSetRouter
   * @note IndependentSetRouter does not have any configuration parameters.
   */
  struct Config {
    template <typename BasicJsonType>
    friend void to_json(BasicJsonType& /* unused */,
                        const Config& /* unused */) {}
    template <typename BasicJsonType>
    friend void from_json(const BasicJsonType& /* unused */,
                          Config& /* unused */) {}
  };
  /// Create a IndependentSetRouter
  IndependentSetRouter(const Architecture& architecture,
                       const Config& /* unused */)
      : architecture_(architecture) {}
  /**
   * Given the computed placement, compute a possible routing.
   * @details For this task, all movements are put in a conflict graph where an
   * edge indicates that two atoms (nodes) cannot be moved together. The atoms
   * are sorted by their distance in decreasing order such that atoms with
   * larger distance are routed first and hopefully lead to more homogenous
   * routing groups with similar movement distances within one group.
   * @param placement is a vector of the atoms' placement at every layer
   * @return the routing, i.e., for every transition between two placements a
   * vector of groups containing atoms that can be moved simultaneously
   */
  [[nodiscard]] auto route(const std::vector<Placement>& placement) const
      -> std::vector<Routing>;

private:
  /**
   * Creates the conflict graph.
   * @details Atom/qubit indices are the nodes. Two nodes are connected if their
   * corresponding move with respect to the given @p start- and @p
   * targetPlacement stand in conflict with each other. The graph is
   * represented as adjacency lists.
   * @param atomsToMove are all atoms corresponding to nodes in the graph
   * @param startPlacement is the start placement of all atoms as a mapping from
   * atoms to their sites
   * @param targetPlacement is the target placement of the atoms
   * @return the conflict graph as an unordered_map, where the keys are the
   * nodes and the values are vectors of their neighbors
   */
  [[nodiscard]] auto
  createConflictGraph(const std::vector<qc::Qubit>& atomsToMove,
                      const Placement& startPlacement,
                      const Placement& targetPlacement) const
      -> std::unordered_map<qc::Qubit, std::vector<qc::Qubit>>;

  /**
   * Takes two sites, the start and target site and returns a 4D-vector of the
   * form (x-start, y-start, x-end, y-end) where the corresponding x- and
   * y-coordinates are the coordinates of the exact location of the given sites.
   * @param start is the start site
   * @param target is the target site
   * @return is the 4D-vector containing the exact site locations
   */
  [[nodiscard]] auto
  getMovementVector(const std::tuple<const SLM&, size_t, size_t>& start,
                    const std::tuple<const SLM&, size_t, size_t>& target) const
      -> std::tuple<size_t, size_t, size_t, size_t>;

  /**
   * Check whether two movements are compatible, i.e., the topological order
   * of the moved atoms remain the same.
   * @param v is a 4D-vector of the form (x-start, y-start, x-end, y-end)
   * @param w is the other 4D-vector of the form (x-start, y-start, x-end,
   * y-end)
   * @return true, if the given movement vectors are compatible, otherwise false
   */
  [[nodiscard]] static auto
  isCompatibleMovement(std::tuple<size_t, size_t, size_t, size_t> v,
                       std::tuple<size_t, size_t, size_t, size_t> w) -> bool;
};
} // namespace na::zoned
