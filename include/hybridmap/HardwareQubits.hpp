/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#ifndef HYBRIDMAP_HARDWARE_QUBITS_HPP
#define HYBRIDMAP_HARDWARE_QUBITS_HPP

#include "datastructures/SymmetricMatrix.hpp"
#include "hybridmap/NeutralAtomArchitecture.hpp"
#include "hybridmap/NeutralAtomDefinitions.hpp"
#include "hybridmap/NeutralAtomUtils.hpp"
#include "ir/Definitions.hpp"
#include "ir/Permutation.hpp"
#include "ir/operations/Operation.hpp"

#include <algorithm>
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
 * @brief Class that represents the hardware qubits of a neutral atom quantum
 * computer.
 * @details Stores the mapping from circuit qubits to hardware qubits and from
 * hardware qubits to atom coordinates. Maintains cached swap distances and
 * nearby-qubit relations derived from the architecture's interaction radius.
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
   * @brief Initializes the swap distances between the hardware qubits for the
   * trivial initial layout.
   * @details Initializes the swap distances between the hardware qubits. This
   * is only valid for the trivial initial layout.
   */
  void initTrivialSwapDistances();
  /**
   * @brief Initializes the nearby qubits for each hardware qubit.
   * @details Nearby qubits are the qubits that are closer than the interaction
   * radius. Therefore, they can be swapped with a single swap operation.
   */
  void initNearbyQubits();
  /**
   * @brief Computes the nearby qubits for a single hardware qubit.
   * @details Determines nearby qubits by comparing Euclidean distance to the
   * architecture's interaction radius. Called by initNearbyQubits().
   * @param qubit The hardware qubit for which the nearby qubits are computed.
   */
  void computeNearbyQubits(HwQubit qubit);

  /**
   * @brief Computes the swap distance between two hardware qubits.
   * @details Computes the swap distance between two hardware qubits. This
   * function is called by getSwapDistance(). It uses a breadth-first search
   * to find the shortest path between the two qubits.
   * @param q1 The first hardware qubit.
   * @param q2 The second hardware qubit.
   */
  void computeSwapDistance(HwQubit q1, HwQubit q2);

  /**
   * @brief Resets the swap distances between the hardware qubits.
   * @details Used after each shuttling operation to invalidate all cached swap
   * distances (set to -1), forcing recomputation on demand.
   */
  void resetSwapDistances();

public:
  // Constructors
  HardwareQubits() = default;
  /**
   * @brief Construct hardware qubit layout and caches.
   * @param architecture Reference to the neutral atom architecture.
   * @param nQubits Number of hardware qubits managed in the mapping.
   * @param initialCoordinateMapping Strategy for initial coordinate assignment:
   * Trivial assigns coordinates 0..nQubits-1 in order; Random shuffles over
   * all positions.
   * @param seed Random seed used for Random initial mapping. If 0, a
   * std::random_device() seeds the RNG.
   * @details Initializes nearby-qubit relations and occupied/free coordinate
   * lists. For Trivial mapping, swap distances are precomputed; for Random,
   * swap distances are left invalid (-1) to be computed lazily.
   */
  explicit HardwareQubits(
      const NeutralAtomArchitecture& architecture, const CoordIndex nQubits = 0,
      const InitialCoordinateMapping initialCoordinateMapping = Trivial,
      uint32_t seed = 0)
      : arch(&architecture), nQubits(nQubits) {

    swapDistances = qc::SymmetricMatrix<SwapDistance>(this->nQubits);

    switch (initialCoordinateMapping) {
    case Trivial:
      for (uint32_t i = 0; i < this->nQubits; ++i) {
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
      for (uint32_t i = 0; i < this->nQubits; ++i) {
        hwToCoordIdx.emplace(i, indices[i]);
        occupiedCoordinates.emplace_back(indices[i]);
      }

      swapDistances = qc::SymmetricMatrix(this->nQubits, -1);
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
   * @brief Compute all shortest paths between two hardware qubits.
   * @details Performs a breadth-first exploration over the nearby-qubit graph
   * (edges exist between qubits within the interaction radius) and returns all
   * minimal-length paths from q1 to q2 as sequences of hardware qubits.
   */
  [[nodiscard]] std::vector<HwQubitsVector>
  computeAllShortestPaths(HwQubit q1, HwQubit q2) const;

  /** Get number of hardware qubits tracked by this instance. */
  [[nodiscard]] CoordIndex getNumQubits() const { return nQubits; }

  /**
   * @brief Checks if a hardware qubit is mapped to a coordinate.
   * @param idx The coordinate index.
   * @return Boolean indicating if the hardware qubit is mapped to a coordinate.
   */
  [[nodiscard]] bool isMapped(const CoordIndex idx) const {
    return std::ranges::find(occupiedCoordinates, idx) !=
           occupiedCoordinates.end();
  }
  /**
   * @brief Updates mapping after moving a hardware qubit to a coordinate.
   * @details Verifies that the coordinate exists and is unoccupied, updates the
   * mapping, refreshes nearby-qubit relations for the moved qubit and its
   * neighbors, and invalidates cached swap distances.
   * @param hwQubit The hardware qubit to be moved.
   * @param newCoord The new coordinate of the hardware qubit.
   */
  void move(HwQubit hwQubit, CoordIndex newCoord);

  /**
   * @brief Remove a hardware qubit from the mapping and caches.
   * @details Erases the qubit from the coordinate mapping and nearby lists and
   * invalidates swap distances involving that qubit.
   */
  void removeHwQubit(const HwQubit hwQubit) {
    hwToCoordIdx.erase(hwQubit);
    freeCoordinates.emplace_back(initialHwPos.at(hwQubit));
    occupiedCoordinates.emplace_back(initialHwPos.at(hwQubit));
    initialHwPos.erase(hwQubit);
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
   * @brief Convert operation's qubits from hardware indices to coordinates.
   * @param op The operation to be updated in-place.
   */
  void mapToCoordIdx(qc::Operation* op) const {
    op->setTargets(hwToCoordIdx.apply(op->getTargets()));
    if (op->isControlled()) {
      op->setControls(hwToCoordIdx.apply(op->getControls()));
    }
  }

  /**
   * @brief Returns the coordinate index of a hardware qubit.
   * @param qubit The hardware qubit.
   * @return The coordinate index of the hardware qubit.
   */
  [[nodiscard]] CoordIndex getCoordIndex(const HwQubit qubit) const {
    return hwToCoordIdx.at(qubit);
  }
  /**
   * @brief Returns the coordinate indices of a set of hardware qubits.
   * @param hwQubits The set of hardware qubits.
   * @return The coordinate indices of the hardware qubits.
   */
  [[nodiscard]] std::set<CoordIndex>
  getCoordIndices(const std::set<HwQubit>& hwQubits) const {
    std::set<CoordIndex> coordIndices;
    for (auto const& hwQubit : hwQubits) {
      coordIndices.emplace(this->getCoordIndex(hwQubit));
    }
    return coordIndices;
  }

  [[nodiscard]] std::vector<CoordIndex>
  getCoordIndices(const std::vector<HwQubit>& hwQubits) const {
    std::vector<CoordIndex> coordIndices;
    coordIndices.reserve(hwQubits.size());
    for (auto const& hwQubit : hwQubits) {
      coordIndices.emplace_back(this->getCoordIndex(hwQubit));
    }
    return coordIndices;
  }

  /**
   * @brief Return the hardware qubit at a given coordinate.
   * @details Throws std::runtime_error if no hardware qubit is mapped there.
   * @param coordIndex The coordinate index.
   * @return The hardware qubit at the coordinate.
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
   * @brief Returns the swap distance between two hardware qubits.
   * @details If not computed yet, uses a breadth-first search over nearby
   * qubits to compute the minimal number of intermediate swaps. When closeBy
   * is false, one additional step is allowed to stop in the vicinity of q2.
   * @param q1 The first hardware qubit.
   * @param q2 The second hardware qubit.
   * @param closeBy If the swap should be performed to the exact position of q2
   * or just to its vicinity.
   * @return The swap distance between the two hardware qubits (0 if equal). If
   * closeBy==false, returns the distance plus one.
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
   * @brief Returns the nearby hardware qubits of a hardware qubit.
   * @param q The hardware qubit.
   * @return The nearby hardware qubits of the hardware qubit.
   */
  [[nodiscard]] HwQubits getNearbyQubits(const HwQubit q) const {
    return nearbyQubits.at(q);
  }

  /**
   * @brief Returns vector of all possible swaps for a hardware qubit.
   * @param q The hardware qubit.
   * @return The vector of all possible swaps for the hardware qubit.
   */
  [[nodiscard]] std::vector<Swap> getNearbySwaps(HwQubit q) const;

  /**
   * @brief Returns the unoccupied coordinates in the vicinity of a coordinate.
   * @param idx The coordinate index.
   * @return The unoccupied coordinates in the vicinity of the coordinate.
   */
  [[nodiscard]] std::set<CoordIndex>
  getNearbyFreeCoordinatesByCoord(CoordIndex idx) const;

  /**
   * @brief Returns the occupied coordinates in the vicinity of a coordinate.
   * @param idx The coordinate index.
   * @return The occupied coordinates in the vicinity of the coordinate.
   */
  [[nodiscard]] std::set<CoordIndex>
  getNearbyOccupiedCoordinatesByCoord(CoordIndex idx) const;

  /**
   * @brief Computes the summed swap distance between all hardware qubits in a
   * set.
   * @param qubits The set of hardware qubits.
   * @return The summed pairwise swap distance among all qubits in the set.
   * For two qubits, this reduces to their swap distance.
   */
  qc::fp getAllToAllSwapDistance(std::set<HwQubit>& qubits);

  /**
   * @brief Find free coordinates in a given direction from a coordinate.
   * @details Returns the nearest free coordinate along the specified direction
   * (as a single-element vector). If no free coordinate exists in that
   * direction, returns all currently free coordinates excluding the provided
   * exclusions.
   * @param coord The starting coordinate index.
   * @param direction The direction in which the search is performed
   * (Left/Right, Down/Up).
   * @param excludedCoords Coordinates to be ignored in the search.
   * @return Either a singleton containing the closest free coordinate in the
   * given direction, or a list of all free coordinates if none exist in that
   * direction.
   */
  [[nodiscard]] std::vector<CoordIndex>
  findClosestFreeCoord(CoordIndex coord, Direction direction,
                       const CoordIndices& excludedCoords = {}) const;

  /**
   * @brief Find the hardware qubit closest (by Euclidean distance) to a
   * coordinate, ignoring a set of qubits.
   */
  [[nodiscard]] HwQubit getClosestQubit(CoordIndex coord,
                                        const HwQubits& ignored) const;

  // Blocking
  /**
   * @brief Computes all hardware qubits that are blocked by a set of hardware
   * qubits.
   * @param qubits The input hardware qubits.
   * @return The blocked hardware qubits.
   */
  [[nodiscard]] std::set<HwQubit>
  getBlockedQubits(const std::set<HwQubit>& qubits) const;

  /**
   * @brief Get the initial hardware-to-coordinate mapping (at construction).
   * @return A map from hardware qubit to its initial coordinate index.
   */
  [[nodiscard]] std::map<HwQubit, CoordIndex> getInitHwPos() const {
    std::map<HwQubit, HwQubit> initialHwPosMap;
    for (auto const& pair : initialHwPos) {
      initialHwPosMap[pair.first] = pair.second;
    }
    return initialHwPosMap;
  }
};
} // namespace na

#endif // HYBRIDMAP_HARDWARE_QUBITS_HPP
