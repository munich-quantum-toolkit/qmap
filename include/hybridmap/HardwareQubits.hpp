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

#include "datastructures/SymmetricMatrix.hpp"
#include "hybridmap/NeutralAtomArchitecture.hpp"
#include "hybridmap/NeutralAtomDefinitions.hpp"
#include "hybridmap/NeutralAtomUtils.hpp"
#include "ir/Definitions.hpp"
#include "ir/Permutation.hpp"
#include "ir/operations/Operation.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <map>
#include <numeric>
#include <random>
#include <ranges>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace na {

/**
 * @brief Represents hardware qubits and their placement on a neutral atom
 * device.
 * @details Manages mapping (hardware qubit -> coordinate), maintains cached
 * swap distances and nearby-qubit sets derived from the architecture
 * interaction radius, and provides utilities for movement, neighborhood
 * queries, blocking analysis, and coordinate-based path heuristics.
 */
class HardwareQubits {
protected:
  const NeutralAtomArchitecture* arch = nullptr;
  CoordIndex nQubits = 0;
  qc::Permutation hwToCoordIdx;
  qc::SymmetricMatrix<SwapDistance> swapDistances;
  std::map<HwQubit, HwQubits> nearbyQubits;
  std::vector<CoordIndex> freeCoordinates;
  std::vector<CoordIndex> occupiedCoordinates;
  qc::Permutation initialHwPos;

  /**
   * @brief Precompute swap distances for trivial initial layout.
   * @details Fills symmetric matrix with shortest-path distances (in edges)
   * when coordinates align with hardware indices.
   */
  void initTrivialSwapDistances();
  /**
   * @brief Initialize per-qubit nearby sets (within interaction radius).
   * @details Nearby qubits are those reachable by a single
   * entangling/interaction edge and hence by one swap.
   */
  void initNearbyQubits();
  /**
   * @brief Populate nearby set for one qubit.
   * @param qubit Hardware qubit whose neighbors are computed.
   */
  void computeNearbyQubits(HwQubit qubit);

  /**
   * @brief Compute swap distance (BFS) between two hardware qubits and cache
   * it.
   * @param q1 First hardware qubit.
   * @param q2 Second hardware qubit.
   */
  void computeSwapDistance(HwQubit q1, HwQubit q2);

  /**
   * @brief Invalidate all cached swap distances (set entries to -1).
   * @note Call after shuttling alters physical adjacency relevance.
   */
  void resetSwapDistances();

public:
  // Constructors
  HardwareQubits() = default;
  /**
   * @brief Construct hardware qubit layout and caches.
   * @param architecture Device architecture reference.
   * @param nQubits Number of hardware qubits to track.
   * @param initialCoordinateMapping Initial placement strategy
   * (Trivial/Random).
   * @param seed RNG seed for Random; if 0 uses std::random_device().
   * @details Populates hw->coord mapping, nearby sets, occupied/free coordinate
   * lists; precomputes swap distances for trivial mapping, leaves them lazy
   * (-1) for random mapping.
   */
  explicit HardwareQubits(
      const NeutralAtomArchitecture& architecture, const CoordIndex nQubits = 0,
      const InitialCoordinateMapping initialCoordinateMapping = Trivial,
      uint32_t seed = 0)
      : arch(&architecture), nQubits(nQubits) {

    assert(nQubits <= architecture.getNpositions() &&
           "Number of hardware qubits exceeds available positions.");
    swapDistances = qc::SymmetricMatrix<SwapDistance>(nQubits);

    switch (initialCoordinateMapping) {
    case Trivial:
      for (uint32_t i = 0; i < nQubits; ++i) {
        hwToCoordIdx.emplace(i, i);
        occupiedCoordinates.emplace_back(i);
      }
      initTrivialSwapDistances();
      break;
    case Random:
      std::vector<CoordIndex> indices(architecture.getNpositions());
      std::iota(indices.begin(), indices.end(), 0);
      if (seed == 0) {
        seed = std::random_device()();
      }
      std::mt19937 g(seed);
      std::ranges::shuffle(indices, g);
      for (uint32_t i = 0; i < nQubits; ++i) {
        hwToCoordIdx.emplace(i, indices[i]);
        occupiedCoordinates.emplace_back(indices[i]);
      }

      swapDistances = qc::SymmetricMatrix(nQubits, -1);
    }
    initNearbyQubits();

    for (uint32_t i = 0; i < architecture.getNpositions(); ++i) {
      if (std::ranges::find(occupiedCoordinates, i) ==
          occupiedCoordinates.end()) {
        freeCoordinates.emplace_back(i);
      }
    }

    initialHwPos = hwToCoordIdx;
  }

  /**
   * @brief Enumerate all minimal-length paths between two qubits.
   * @details BFS builds predecessor layers; all shortest sequences q1->...->q2
   * are returned.
   * @param q1 Source hardware qubit.
   * @param q2 Target hardware qubit.
   * @return Vector of shortest paths, each path a vector of hardware qubits.
   */
  [[nodiscard]] std::vector<HwQubitsVector>
  computeAllShortestPaths(HwQubit q1, HwQubit q2) const;

  /**
   * @brief Number of hardware qubits tracked.
   * @return Hardware qubit count.
   */
  [[nodiscard]] CoordIndex getNumQubits() const { return nQubits; }

  /**
   * @brief Check whether a coordinate is currently occupied.
   * @param idx Coordinate index.
   * @return True if occupied by some hardware qubit; false otherwise.
   */
  [[nodiscard]] bool isMapped(const CoordIndex idx) const {
    return std::ranges::find(occupiedCoordinates, idx) !=
           occupiedCoordinates.end();
  }
  /**
   * @brief Move a hardware qubit to a new coordinate.
   * @details Validates destination free, updates mapping and occupancy sets,
   * recalculates affected nearby sets, invalidates swap distance cache.
   * @param hwQubit Hardware qubit identifier.
   * @param newCoord Destination coordinate index.
   * @throw std::runtime_error If newCoord invalid or already occupied.
   */
  void move(HwQubit hwQubit, CoordIndex newCoord);

  /**
   * @brief Remove a hardware qubit from mapping, occupancy and nearby caches.
   * @details Invalidates all swap distances involving the qubit and erases it
   * from neighbor sets.
   * @param hwQubit Hardware qubit to remove.
   */
  void removeHwQubit(const HwQubit hwQubit) {
    const auto currentCoord = hwToCoordIdx.at(hwQubit);
    hwToCoordIdx.erase(hwQubit);
    initialHwPos.erase(hwQubit);

    if (auto it = std::ranges::find(occupiedCoordinates, currentCoord);
        it != occupiedCoordinates.end()) {
      occupiedCoordinates.erase(it);
    }
    if (std::ranges::find(freeCoordinates, currentCoord) ==
        freeCoordinates.end()) {
      freeCoordinates.emplace_back(currentCoord);
    }
    // set swap distances to -1
    for (uint32_t i = 0; i < swapDistances.size(); ++i) {
      swapDistances(hwQubit, i) = -1;
      swapDistances(i, hwQubit) = -1;
    }
    nearbyQubits.erase(hwQubit);
    for (auto& nearby : nearbyQubits | std::views::values) {
      nearby.erase(hwQubit);
    }
  }

  /**
   * @brief Rewrite operation qubit indices from hardware IDs to coordinates.
   * @param op Operation pointer (modified in place).
   */
  void mapToCoordIdx(qc::Operation* op) const {
    op->setTargets(hwToCoordIdx.apply(op->getTargets()));
    if (op->isControlled()) {
      op->setControls(hwToCoordIdx.apply(op->getControls()));
    }
  }

  /**
   * @brief Coordinate index mapped to a hardware qubit.
   * @param qubit Hardware qubit.
   * @return Coordinate index.
   * @throw std::out_of_range If qubit not in mapping.
   */
  [[nodiscard]] CoordIndex getCoordIndex(const HwQubit qubit) const {
    return hwToCoordIdx.at(qubit);
  }
  /**
   * @brief Coordinate indices for a set of hardware qubits.
   * @param hwQubits Set of hardware qubits.
   * @return Set of coordinate indices.
   */
  [[nodiscard]] std::set<CoordIndex>
  getCoordIndices(const std::set<HwQubit>& hwQubits) const {
    std::set<CoordIndex> coordIndices;
    for (auto const& hwQubit : hwQubits) {
      coordIndices.emplace(getCoordIndex(hwQubit));
    }
    return coordIndices;
  }

  [[nodiscard]] std::vector<CoordIndex>
  getCoordIndices(const std::vector<HwQubit>& hwQubits) const {
    std::vector<CoordIndex> coordIndices;
    coordIndices.reserve(hwQubits.size());
    for (auto const& hwQubit : hwQubits) {
      coordIndices.emplace_back(getCoordIndex(hwQubit));
    }
    return coordIndices;
  }

  /**
   * @brief Hardware qubit occupying a coordinate.
   * @param coordIndex Coordinate index.
   * @return Hardware qubit ID.
   * @throw std::runtime_error If no qubit occupies the coordinate.
   */
  [[nodiscard]] HwQubit getHwQubit(const CoordIndex coordIndex) const {
    for (auto const& [hwQubit, index] : hwToCoordIdx) {
      if (index == coordIndex) {
        return hwQubit;
      }
    }
    throw std::runtime_error("There is no qubit at this coordinate " +
                             std::to_string(coordIndex));
  }

  // Swap Distances and Nearby qc::Qubits

  /**
   * @brief Swap distance between two hardware qubits (lazy cached).
   * @param q1 First qubit.
   * @param q2 Second qubit.
   * @param closeBy If false, allow stopping adjacent to q2 (adds 1 to
   * distance).
   * @return Distance in number of swaps (0 if identical).
   */
  [[nodiscard]] SwapDistance getSwapDistance(const HwQubit q1, const HwQubit q2,
                                             const bool closeBy = true) {
    if (q1 == q2) {
      return 0;
    }
    if (swapDistances(q1, q2) < 0) {
      computeSwapDistance(q1, q2);
    }
    if (closeBy) {
      return swapDistances(q1, q2);
    }
    return swapDistances(q1, q2) + 1;
  }

  /**
   * @brief Nearby hardware qubits (within interaction radius).
   * @param q Hardware qubit.
   * @return Set of nearby qubits.
   */
  [[nodiscard]] HwQubits getNearbyQubits(const HwQubit q) const {
    return nearbyQubits.at(q);
  }

  /**
   * @brief All possible immediate swaps (pairs with each nearby qubit).
   * @param q Hardware qubit.
   * @return Vector of swap pairs.
   */
  [[nodiscard]] std::vector<Swap> getNearbySwaps(HwQubit q) const;

  /**
   * @brief Unoccupied coordinates within interaction radius of a coordinate.
   * @param idx Coordinate index.
   * @return Set of free coordinate indices nearby.
   */
  [[nodiscard]] std::set<CoordIndex>
  getNearbyFreeCoordinatesByCoord(CoordIndex idx) const;

  /**
   * @brief Occupied coordinates within interaction radius of a coordinate.
   * @param idx Coordinate index.
   * @return Set of occupied coordinate indices nearby.
   */
  [[nodiscard]] std::set<CoordIndex>
  getNearbyOccupiedCoordinatesByCoord(CoordIndex idx) const;

  /**
   * @brief Sum of pairwise swap distances among a set of qubits.
   * @param qubits Set of hardware qubits (modified if needed for caching).
   * @return Summed distances; for two qubits equals their distance.
   */
  qc::fp getAllToAllSwapDistance(std::set<HwQubit>& qubits);

  /**
   * @brief Find closest free coordinate in a direction, else return all free.
   * @param coord Starting coordinate.
   * @param direction Direction sign (per dimension) encapsulated in Direction.
   * @param excludedCoords Coordinates to ignore.
   * @return Singleton vector with nearest directional free coordinate or all
   * free coordinates if none in direction.
   */
  [[nodiscard]] std::vector<CoordIndex>
  findClosestFreeCoord(CoordIndex coord, Direction direction,
                       const CoordIndices& excludedCoords = {}) const;

  /**
   * @brief Hardware qubit closest by Euclidean distance to a coordinate,
   * excluding some.
   * @param coord Target coordinate index.
   * @param ignored Set of qubits to ignore.
   * @return Closest hardware qubit ID.
   */
  [[nodiscard]] HwQubit getClosestQubit(CoordIndex coord,
                                        const HwQubits& ignored) const;

  // Blocking
  /**
   * @brief Hardware qubits blocked by interactions of given qubits.
   * @param qubits Active hardware qubits.
   * @return Set of hardware qubits considered blocked.
   */
  [[nodiscard]] std::set<HwQubit>
  getBlockedQubits(const std::set<HwQubit>& qubits) const;

  /**
   * @brief Initial hardware->coordinate mapping from construction time.
   * @return Map of hardware qubit to initial coordinate index.
   */
  [[nodiscard]] std::map<HwQubit, CoordIndex> getInitHwPos() const {
    std::map<HwQubit, CoordIndex> initialHwPosMap;
    for (auto const& pair : initialHwPos) {
      initialHwPosMap[pair.first] = pair.second;
    }
    return initialHwPosMap;
  }
};
} // namespace na
