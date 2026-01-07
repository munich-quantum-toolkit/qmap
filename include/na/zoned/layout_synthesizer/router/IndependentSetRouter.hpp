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

#include "ir/Definitions.hpp"
#include "na/zoned/Architecture.hpp"
#include "na/zoned/Types.hpp"
#include "na/zoned/layout_synthesizer/router/RouterBase.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <list>
#include <nlohmann/json.hpp>
#include <optional>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace na::zoned {

/**
 * This class implements a Router for the zoned neutral atom compiler that forms
 * groups of parallel movements by calculating a maximal independent set.
 */
class IndependentSetRouter : public RouterBase {
  std::reference_wrapper<const Architecture> architecture_;

public:
  /// The configuration of the IndependentSetRouter
  struct Config {
    /// The routing method.
    enum class Method : uint8_t {
      /**
       * Use strict routing, i.e., the relative order of atoms must be
       * maintained throughout a movement.
       */
      STRICT,
      /**
       * Use relaxed routing, i.e., the relative order of atoms may change
       * throughout a movement by applying offsets during pick-up and drop-off.
       */
      RELAXED
    };
    Method method = Method::RELAXED;
    /**
     * @brief Threshold factor for group merging decisions during routing.
     * @details First, a strict routing is computed resulting in a set of
     * rearrangement groups. Afterward, some of those are merged with existing
     * groups based on the relaxed constraints. Higher values of this
     * parameter favor keeping groups separate; lower values favor merging.
     * In particular, a value of 0.0 merges all possible groups. (Default: 1.0)
     * @note This value is only relevant if the routing method RELAXED is used
     * and ignored otherwise.
     */
    double preferSplit = 1.0;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Config, method, preferSplit)
  };

private:
  /// The configuration of the independent set router
  const Config config_;

public:
  /// Create an IndependentSetRouter with the given configuration
  IndependentSetRouter(const Architecture& architecture, const Config& config)
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
      -> std::vector<Routing> override;

private:
  /**
   * Computes a strict routing for the given placement.
   * @param placement is the placement for all layers
   * @returns a strict routing
   */
  [[nodiscard]] auto routeStrict(const std::vector<Placement>& placement) const
      -> std::vector<Routing>;
  /**
   * Updates the existing cost if the incoming cost is greater or indicating
   * incompatibility (`std::nullopt`), which dominates any finite cost.
   * @param existing is the existing cost
   * @param incoming is the new cost
   */
  static auto mergeConflictCost(std::optional<double>& existing,
                                const std::optional<double>& incoming) -> void;
  /// Information about a group during routing
  struct GroupInfo {
    /// The set of independent atoms in the group
    std::vector<qc::Qubit> independentSet;
    /// Maximum movement distance of any atom in the group
    double maxDistance = 0.0;
    /// All atoms that are in conflict with atoms in the independent set
    std::unordered_map<qc::Qubit, std::optional<double>>
        relaxedConflictingAtoms;
  };
  /**
   * Computes the strict routing before movement groups are merged according to
   * the relaxed routing constraints.
   * @param atomsToMove is the vector or atoms to move
   * @param atomsToDist is a map from atoms to their movement distance
   * @param conflictGraph is the conflict graph based on the strict routing
   * constraints.
   * @param relaxedConflictGraph is the conflict graph based on the relaxed
   * routing constraints with weighted edges for strict conflicts.
   * @returns a list of strict routing groups.
   */
  [[nodiscard]] static auto makeStrictRoutingForRelaxedRouting(
      std::vector<qc::Qubit> atomsToMove,
      const std::unordered_map<qc::Qubit, double>& atomsToDist,
      const std::unordered_map<qc::Qubit, std::vector<qc::Qubit>>&
          conflictGraph,
      const std::unordered_map<
          qc::Qubit, std::vector<std::pair<qc::Qubit, std::optional<double>>>>&
          relaxedConflictGraph) -> std::list<GroupInfo>;
  /**
   * Merges movement groups if all movement of one group can be combined with
   * other movement groups based on the relaxed routing constraints.
   * The merging decisions are driven by the configured `preferSplit` threshold.
   * @param atomsToDist is a map from atoms to their movement distance.
   * @param relaxedConflictGraph is the conflict graph based on the relaxed
   * routing constraints with weighted edges for strict conflicts.
   * @param groups is a list of movement groups that is modified by this
   * function.
   */
  auto mergeGroups(
      const std::unordered_map<qc::Qubit, double>& atomsToDist,
      const std::unordered_map<
          qc::Qubit, std::vector<std::pair<qc::Qubit, std::optional<double>>>>&
          relaxedConflictGraph,
      std::list<GroupInfo>& groups) const -> void;
  /**
   * Computes a relaxed routing for the given placement.
   * @param placement is the placement for all layers
   * @returns a relaxed routing
   */
  [[nodiscard]] auto routeRelaxed(const std::vector<Placement>& placement) const
      -> std::vector<Routing>;
  /**
   * @param startPlacement is the start placement.
   * @param targetPlacement is the target placement.
   * @returns the atoms to move
   */
  [[nodiscard]] auto getAtomsToMove(const Placement& startPlacement,
                                    const Placement& targetPlacement) const
      -> std::vector<qc::Qubit>;
  /**
   * @param startPlacement is the start placement.
   * @param targetPlacement is the target placement.
   * @returns a pair consisting of a vector containing the atoms to move and a
   * map from atoms to their movement distance.
   */
  [[nodiscard]] auto
  getAtomsToMoveWithDistance(const Placement& startPlacement,
                             const Placement& targetPlacement) const
      -> std::pair<std::vector<qc::Qubit>,
                   std::unordered_map<qc::Qubit, double>>;
  /**
   * Creates the conflict graph with respect to the strict routing constraints.
   * @details Atom/qubit indices are the nodes. Two nodes are connected if their
   * corresponding move with respect to the given @p start- and @p
   * targetPlacement stands in conflict with each other. The graph is
   * represented as adjacency lists.
   * @param atomsToMove are all atoms corresponding to nodes in the graph
   * @param startPlacement is the start placement of all atoms as a mapping from
   * atoms to their sites
   * @param targetPlacement is the target placement of the atoms
   * @return the conflict graph as an unordered_map, where the keys are the
   * nodes and the values are vectors of their neighbors
   */
  [[nodiscard]] auto
  createStrictConflictGraph(const std::vector<qc::Qubit>& atomsToMove,
                            const Placement& startPlacement,
                            const Placement& targetPlacement) const
      -> std::unordered_map<qc::Qubit, std::vector<qc::Qubit>>;
  /**
   * Creates both the strict and relaxed conflict graphs.
   * @details Atom/qubit indices are the nodes. Two nodes are connected if their
   * corresponding move with respect to the given @p start- and @p
   * targetPlacement stands in conflict with each other based on the strict
   * routing constraints. The graph is represented as adjacency lists.
   * @par
   * * In contrast to the strict conflict graph, all edges that do not represent
   * a conflict with respect to the relaxed routing constraints carry a weight
   * representing the cost for merging the two adjacent movements. These edges
   * correspond to the value `RelaxedCompatible` returned by @ref
   * isRelaxedCompatibleMovement. The weight of other edges, i.e.,
   * `Incompatible`, is `std::nullopt`.
   * @param atomsToMove are all atoms corresponding to nodes in the graph.
   * @param startPlacement is the start placement of all atoms as a mapping from
   * atoms to their sites.
   * @param targetPlacement is the target placement of the atoms.
   * @return a pair consisting of:
   *   - the strict conflict graph as an unordered_map where the keys are the
   *     nodes and the values are vectors of their neighbors, and
   *   - the relaxed conflict graph as an unordered_map where the keys are the
   *     nodes and the values are vectors of (neighbor, optional merge cost)
   *     pairs.
   */
  [[nodiscard]] auto
  createRelaxedAndStrictConflictGraph(const std::vector<qc::Qubit>& atomsToMove,
                                      const Placement& startPlacement,
                                      const Placement& targetPlacement) const
      -> std::pair<
          std::unordered_map<qc::Qubit, std::vector<qc::Qubit>>,
          std::unordered_map<
              qc::Qubit,
              std::vector<std::pair<qc::Qubit, std::optional<double>>>>>;

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
   * Check whether two movements are strictly compatible, i.e., atoms remain on
   * the same row (column) and maintain their relative/topological order.
   * @param v is a 4D-vector of the form (x-start, y-start, x-end, y-end)
   * @param w is the other 4D-vector of the form (x-start, y-start, x-end,
   * y-end)
   * @returns `true` if the movements are (strictly) compatible, `false`
   * otherwise.
   */
  [[nodiscard]] static auto
  isCompatibleMovement(const std::tuple<size_t, size_t, size_t, size_t>& v,
                       const std::tuple<size_t, size_t, size_t, size_t>& w)
      -> bool;
  /**
   * @brief This struct indicates whether two movements are strictly compatible,
   * relaxed compatible together with the corresponding merging cost, or
   * (completely) incompatible.
   */
  struct MovementCompatibility {
    enum class Status : uint8_t {
      /**
       * @brief The movements are strictly compatible.
       * @details The atoms remain on the same row (column) and maintain their
       * relative/topological order during the movement.
       */
      StrictlyCompatible,
      /**
       * @brief The movements are compatible with respect to the relaxed routing
       * constraints.
       * @details The moved atoms must still remain on the same row (column) but
       * may change their relative/topological order during the movement
       * achieved by offsets during pick-up and drop-off.
       */
      RelaxedCompatible,
      /**
       * @brief The movements are incompatible.
       * @details The atoms starting in one row (column) do not end up in the
       * same row (column).
       */
      Incompatible
    };

    /// Indicates the type of compatibility
    Status status = Status::Incompatible;
    /**
     * @brief In the case of `RelaxedCompatible`, the cost to merge the two
     * movements.
     * @details The cost is calculated such that it represents the extra time
     * the offset takes to shift the loaded atoms to deal with the relaxed
     * routing constraints. More precisely, the cost is proportional to the
     * cubed time for the sake of easier computation because then it is just
     * proportional to the distance of the offset. If only one offset is
     * required, i.e., either horizontal or vertical, the cost is the raw
     * distance of the offset. If both offsets are required, then the cost is
     * calculated as the sum of the third roots of the individual distances and
     * then cubed again.
     * Hence, the cost must always be a non-negative number.
     */
    std::optional<double> mergeCost = std::nullopt;

    /// Factory methods for strict compatibility
    [[nodiscard]] static auto strictlyCompatible() -> MovementCompatibility {
      return {.status = Status::StrictlyCompatible, .mergeCost = std::nullopt};
    }

    /// Factory method for incompatibility
    [[nodiscard]] static auto incompatible() -> MovementCompatibility {
      return {.status = Status::Incompatible, .mergeCost = std::nullopt};
    }

    [[nodiscard]] static auto relaxedCompatible(const double cost)
        -> MovementCompatibility {
      return {.status = Status::RelaxedCompatible, .mergeCost = cost};
    }
  };
  /**
   * Check whether two movements are incompatible with respect to the relaxed
   * routing constraints, i.e., moved atoms remain not on the same row (column).
   * This is, however, independent of their topological order (i.e., relaxed).
   * @param v is a 4D-vector of the form (x-start, y-start, x-end, y-end)
   * @param w is the other 4D-vector of the form (x-start, y-start, x-end,
   * y-end)
   * @returns a @ref MovementCompatibility object indicating whether they are
   * strictly compatible, relaxed compatible (with merge cost), or incompatible.
   */
  [[nodiscard]] static auto isRelaxedCompatibleMovement(
      const std::tuple<size_t, size_t, size_t, size_t>& v,
      const std::tuple<size_t, size_t, size_t, size_t>& w)
      -> MovementCompatibility;
};
NLOHMANN_JSON_SERIALIZE_ENUM(
    IndependentSetRouter::Config::Method,
    {
        {IndependentSetRouter::Config::Method::STRICT, "strict"},
        {IndependentSetRouter::Config::Method::RELAXED, "relaxed"},
    })
} // namespace na::zoned
