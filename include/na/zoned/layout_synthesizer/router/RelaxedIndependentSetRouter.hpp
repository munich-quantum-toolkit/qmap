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
class RelaxedIndependentSetRouter : public RouterBase {
  std::reference_wrapper<const Architecture> architecture_;

public:
  /**
   * The configuration of the RelaxedIndependentSetRouter
   * @note RelaxedIndependentSetRouter does not have any configuration
   * parameters.
   */
  struct Config {
    /**
     * @brief Threshold factor for group merging decisions during routing.
     * @details First, a strict routing is computed resulting in a set of
     * rearrangement groups. Afterward, some of those are merged with existing
     * groups based on the relaxed constraints. Higher values of this
     * parameter favor keeping groups separate; lower values favor merging.
     * In particular, a value of 0.0 merges all possible groups. (Default: 1.0)
     */
    double preferSplit = 1.0;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Config, preferSplit);
  };

private:
  /// The configuration of the relaxed independent set router
  Config config_;

public:
  /// Create a RelaxedIndependentSetRouter
  RelaxedIndependentSetRouter(const Architecture& architecture,
                              const Config& config)
      : architecture_(architecture), config_(config) {}
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
  [[nodiscard]] auto
  createRelaxedConflictGraph(const std::vector<qc::Qubit>& atomsToMove,
                             const Placement& startPlacement,
                             const Placement& targetPlacement) const
      -> std::unordered_map<
          qc::Qubit, std::vector<std::pair<qc::Qubit, std::optional<double>>>>;

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
  isCompatibleMovement(const std::tuple<size_t, size_t, size_t, size_t>& v,
                       const std::tuple<size_t, size_t, size_t, size_t>& w)
      -> bool;
  /**
   * Check whether two movements are incompatible with respect to the relaxed
   * routing constraints, i.e., moved atoms remain not on the same row (column).
   * This is, however, independent of their topological order (i.e., relaxed).
   * @param v is a 4D-vector of the form (x-start, y-start, x-end, y-end)
   * @param w is the other 4D-vector of the form (x-start, y-start, x-end,
   * y-end)
   * @return true, if the given movement vectors are incompatible, otherwise
   * false
   */
  [[nodiscard]] static auto isRelaxedIncompatibleMovement(
      const std::tuple<size_t, size_t, size_t, size_t>& v,
      const std::tuple<size_t, size_t, size_t, size_t>& w)
      -> std::optional<std::optional<double>>;
};
} // namespace na::zoned
