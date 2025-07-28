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
#include "na/zoned/layout_synthesizer/placer/PlacerBase.hpp"

#include <cstddef>
#include <functional>
#include <optional>
#include <unordered_set>
#include <utility>
#include <vector>

namespace na::zoned {
/**
 * Class to find a qubit layout based on vertex matching of a weighted
 * bipartite graph
 */
class VertexMatchingPlacer : public PlacerBase {
  friend class
      VertexMatchingPlacerTest_MinimumWeightFullBipartiteMatching1_Test;
  friend class
      VertexMatchingPlacerTest_MinimumWeightFullBipartiteMatching2_Test;
  friend class
      VertexMatchingPlacerTest_MinimumWeightFullBipartiteMatchingExceptions_Test;
  friend class
      VertexMatchingPlacerTest_MinimumWeightFullBipartiteMatchingEmpty_Test;

  std::reference_wrapper<const Architecture> architecture_;
  /**
   * If true, during the initial placement the atoms are placed starting in the
   * last row instead of the first row in the first SLM
   */
  bool reverseInitialPlacement_ = false;

public:
  struct Config {

    /**
     * This flag indicates whether the placement should use a window when
     * selecting potential free sites, i.e., not all sites in a zone are
     * considered only those that are within a window centered around the
     * best candidate site for an atom.
     */
    bool useWindow = true;
    size_t windowSize = 10;

    /**
     * This flag indicates whether the placement between gates is dynamic.
     * @details If this flag is true, then the next placement in the storage
     * zone is computed via the minimal vertex matching algorithm. If this
     * flag is false, then the initial placement for the placement in the
     * storage through all layers and only the placement in the entanglement
     * zone is computed via the minimal vertex matching algorithm.
     */
    bool dynamicPlacement = true;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Config, useWindow, windowSize,
                                                dynamicPlacement);
  };

private:
  /// The configuration of the VertexMatchingPlacer
  Config config_;

  /**
   * This is multiplied with the cost of the movement without reuse to resemble
   * the additional cost of transferring the atoms from and to the AOD trap
   */
  constexpr static double costAtomTransfer_ = 0.9999;

public:
  /**
   * Create a VertexMatchingPlacer based on the given architecture and
   * configuration
   */
  VertexMatchingPlacer(const Architecture& architecture, const Config& config);
  /**
   * This function defines the interface of the placer. It places the qubits for
   * all layers using a minimal weight matching algorithm.
   *
   * @param nQubits The number of qubits to be placed
   * @param twoQubitGateLayers The qubit pairs that must be placed for each
   * layer
   * @param reuseQubits A set of qubits that can be reused for each layer
   * @return a placement of the qubits for all layers
   */
  [[nodiscard]] auto
  place(size_t nQubits,
        const std::vector<TwoQubitGateLayer>& twoQubitGateLayers,
        const std::vector<std::unordered_set<qc::Qubit>>& reuseQubits)
      -> std::vector<Placement>;

private:
  /// Generate qubit initial layout
  auto makeInitialPlacement(size_t nQubits) const -> Placement;

  /**
   * @note implemented following pseudocode in
   * https://www2.eecs.berkeley.edu/Pubs/TechRpts/1978/ERL-m-78-67.pdf
   */
  [[nodiscard]] static auto minimumWeightFullBipartiteMatching(
      const std::vector<std::vector<std::optional<double>>>& costMatrix)
      -> std::vector<size_t>;
  /**
   * Calculates the cost of rearranging atoms from one placement to the next.
   * @details This function is used to evaluate whether the option with or
   * without reuse is the more cost effective one.
   *
   * @param placementBefore The placement before the movement
   * @param placementAfter The placement after the movement
   * @return The cost of the movement
   */
  [[nodiscard]] auto
  computeMovementCostBetweenPlacements(const Placement& placementBefore,
                                       const Placement& placementAfter) const
      -> double;

  /**
   * This combines the cost of moving the atoms to the entanglement zone and
   * back.
   *
   * @param placementBefore The placement before the movement
   * @param placementBetween The placement between the movement
   * @param placementAfter The placement after the movement
   * @return The cost of the movement
   */
  [[nodiscard]] auto
  computeLayersMovementCost(const Placement& placementBefore,
                            const Placement& placementBetween,
                            const Placement& placementAfter) const -> double;

  /**
   * Decides which placement to use for the next layer, i.e., with our without
   * reuse.
   *
   * @param previousGatePlacement The placement before the movement
   * @param placementsWithoutReuse The placement without reuse
   * @param placementsWithReuse The placement with reuse
   * @return The placement to use for the next layer
   */
  [[nodiscard]] auto filterMapping(
      const Placement& previousGatePlacement,
      const std::pair<Placement, Placement>& placementsWithoutReuse,
      const std::pair<Placement, Placement>& placementsWithReuse) const
      -> std::pair<Placement, Placement>;
  /**
   * Generate gate mapping based on minimum weight matching for the first
   * layer of gates
   */
  [[nodiscard]] auto
  placeGatesInEntanglementZone(const Placement& previousQubitPlacement,
                               const std::unordered_set<qc::Qubit>& reuseQubits,
                               const TwoQubitGateLayer& twoQubitGates,
                               const TwoQubitGateLayer& nextTwoQubitGates,
                               bool reuse) const -> Placement;

  /// Generate qubit mapping based on minimum weight matching.
  auto placeAtomsInStorageZone(const Placement& initialPlacement,
                               const Placement& previousGatePlacement,
                               const std::unordered_set<qc::Qubit>& reuseQubits,
                               const TwoQubitGateLayer& nextTwoQubitGates,
                               bool reuse) const -> Placement;
};
} // namespace na::zoned
