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
#include <cstdint>
#include <map>
#include <numeric>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace na {

/**
 * @brief Class that represents the hardware qubits of a neutral atom quantum
 * @details Class that represents the hardware qubits of a neutral atom quantum
 * computer. It stores the mapping from the circuit qubits to the hardware
 * qubits and the mapping from the hardware qubits to the coordinates of the
 * neutral atoms. It also stores the swap distances between the hardware
 * qubits.
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
   * radius. Therefore they can be swapped with a single swap operation.
   */
  void initNearbyQubits();
  /**
   * @brief Computes the nearby qubits for a single hardware qubit.
   * @details Computes the nearby qubits for a single hardware qubit. This
   * function is called by initNearbyQubits(). It uses the nearby coordinates
   * of the neutral atom architecture to compute the nearby qubits.
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
   * @details Used after each shuttling operation to reset the swap distances.
   */
  void resetSwapDistances();

public:
  // Constructors
  HardwareQubits() = default;
  explicit HardwareQubits(
      const NeutralAtomArchitecture& architecture, const CoordIndex nQubits = 0,
      const InitialCoordinateMapping initialCoordinateMapping =
          InitialCoordinateMapping::Trivial,
      uint32_t seed = 0)
      : arch(&architecture) {
    this->nQubits = nQubits;
    swapDistances = SymmetricMatrix<SwapDistance>(this->nQubits);

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
      std::shuffle(indices.begin(), indices.end(), g);
      for (uint32_t i = 0; i < this->nQubits; ++i) {
        hwToCoordIdx.emplace(i, indices[i]);
        occupiedCoordinates.emplace_back(indices[i]);
      }

      swapDistances = qc::SymmetricMatrix(this->nQubits, -1);
    }
    initNearbyQubits();

    for (uint32_t i = 0; i < architecture.getNpositions(); ++i) {
      if (std::find(occupiedCoordinates.begin(), occupiedCoordinates.end(),
                    i) == occupiedCoordinates.end()) {
        freeCoordinates.emplace_back(i);
      }
    }

    initialHwPos = hwToCoordIdx;
  }

  [[nodiscard]] std::vector<HwQubitsVector>
  computeAllShortestPaths(HwQubit q1, HwQubit q2) const;

  // Mapping
  [[nodiscard]] const qc::Permutation& getHwToCoordIdx() const {
    return hwToCoordIdx;
  }

  [[nodiscard]] CoordIndex getNumQubits() const { return nQubits; }

  /**
   * @brief Checks if a hardware qubit is mapped to a coordinate.
   * @param idx The coordinate index.
   * @return Boolean indicating if the hardware qubit is mapped to a coordinate.
   */
  [[nodiscard]] bool isMapped(CoordIndex idx) const {
    if (std::find(occupiedCoordinates.begin(), occupiedCoordinates.end(),
                  idx) != occupiedCoordinates.end()) {
      return true;
    }
    return false;
  }
  /**
   * @brief Updates mapping after moving a hardware qubit to a coordinate.
   * @details Checks if the coordinate is valid and free. If yes, the mapping is
   * updated.
   * @param hwQubit The hardware qubit to be moved.
   * @param newCoord The new coordinate of the hardware qubit.
   */
  void move(HwQubit hwQubit, CoordIndex newCoord);

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
    for (auto& [qubit, nearby] : nearbyQubits) {
      nearby.erase(hwQubit);
    }
  }

  /**
   * @brief Converts gate qubits from hardware qubits to coordinate indices.
   * @param op The operation.
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
   * @brief Returns the hardware qubit at a coordinate.
   * @details Returns the hardware qubit at a coordinate. Throws an exception if
   * there is no hardware qubit at the coordinate.
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

  // Forwards from architecture class

  /**
   * @brief Returns the nearby coordinates of a hardware qubit.
   * @param q The hardware qubit.
   * @return The nearby coordinates of the hardware qubit.
   */
  [[nodiscard]] [[maybe_unused]] std::set<CoordIndex>
  getNearbyCoordinates(const HwQubit q) const {
    return this->arch->getNearbyCoordinates(this->getCoordIndex(q));
  }

  // Swap Distances and Nearby qc::Qubits

  /**
   * @brief Returns the swap distance between two hardware qubits.
   * @details Returns the swap distance between two hardware qubits. If the
   * swap distance is not yet computed, it is computed using a breadth-first
   * search.
   * @param q1 The first hardware qubit.
   * @param q2 The second hardware qubit.
   * @param closeBy If the swap should be performed to the exact position of q2
   * or just to its vicinity.
   * @return The swap distance between the two hardware qubits.
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
   * @return The summed swap distance between all hardware qubits in the set.
   */
  qc::fp getAllToAllSwapDistance(std::set<HwQubit>& qubits);

  /**
   * @brief Computes the closest free coordinate in a given direction.
   * @details Uses a breadth-first search to find the closest free coordinate in
   * a given direction.
   * @param coord The hardware qubit to start the search from.
   * @param direction The direction in which the search is performed
   * (Left/Right, Down/Up)
   * @param excludedCoords Coordinates to be ignored in the search.
   * @return The closest free coordinate in the given direction.
   */
  [[nodiscard]] std::vector<CoordIndex>
  findClosestFreeCoord(HwQubit coord, Direction direction,
                       const CoordIndices& excludedCoords = {}) const;

  [[nodiscard]] std::vector<CoordIndex>
  findClosestAncillaCoord(CoordIndex coord, Direction direction,
                          int circQubitSize,
                          const CoordIndices& excludedCoords = {}) const;

  [[nodiscard]] HwQubit getClosestQubit(CoordIndex coord,
                                        HwQubits ignored) const;

  // Blocking
  /**
   * @brief Computes all hardware qubits that are blocked by a set of hardware
   * qubits.
   * @param qubits The input hardware qubits.
   * @return The blocked hardware qubits.
   */
  [[nodiscard]] std::set<HwQubit>
  getBlockedQubits(const std::set<HwQubit>& qubits) const;

  [[nodiscard]] std::map<HwQubit, CoordIndex> getInitHwPos() const {
    std::map<HwQubit, HwQubit> initialHwPosMap;
    for (auto const& pair : initialHwPos) {
      initialHwPosMap[pair.first] = pair.second;
    }
    return initialHwPosMap;
  }
};
} // namespace na
