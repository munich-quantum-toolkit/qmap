/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "hybridmap/HardwareQubits.hpp"

#include "datastructures/SymmetricMatrix.hpp"
#include "hybridmap/NeutralAtomDefinitions.hpp"
#include "hybridmap/NeutralAtomUtils.hpp"
#include "ir/Definitions.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <queue>
#include <ranges>
#include <set>
#include <stdexcept>
#include <vector>

namespace na {
void HardwareQubits::initTrivialSwapDistances() {
  swapDistances = qc::SymmetricMatrix<SwapDistance>(nQubits);
  for (uint32_t i = 0; i < nQubits; ++i) {
    for (uint32_t j = 0; j < i; ++j) {
      swapDistances(i, j) =
          arch->getSwapDistance(hwToCoordIdx.at(i), hwToCoordIdx.at(j));
    }
  }
}

void HardwareQubits::initNearbyQubits() {
  for (uint32_t i = 0; i < nQubits; ++i) {
    computeNearbyQubits(i);
  }
}

void HardwareQubits::computeSwapDistance(HwQubit q1, const HwQubit q2) {
  std::queue<HwQubit> q;
  std::vector visited(swapDistances.size(), false);
  std::vector parent(swapDistances.size(), q2);

  q.push(q1);
  visited[q1] = true;
  parent[q1] = q1;
  bool found = false;
  while (!q.empty() && !found) {
    auto current = q.front();
    q.pop();
    for (const auto& nearbyQubit : nearbyQubits.at(current)) {
      if (!visited[nearbyQubit]) {
        q.push(nearbyQubit);
        visited[nearbyQubit] = true;
        parent[nearbyQubit] = current;
        if (nearbyQubit == q2) {
          found = true;
          break;
        }
      }
    }
  }
  if (!found) {
    swapDistances(q1, q2) = std::numeric_limits<SwapDistance>::max();
    return;
  }
  // recreate path
  std::vector<HwQubit> path;
  auto current = q2;
  while (current != q1) {
    path.emplace_back(current);
    current = parent[current];
  }
  path.emplace_back(q1);
  // update swap distances along path
  for (uint32_t start = 0; start < path.size() - 1; ++start) {
    for (uint32_t end = start + 1; end < path.size(); ++end) {
      swapDistances(path[start], path[end]) =
          static_cast<int>(end) - static_cast<int>(start) - 1;
    }
  }
}

std::vector<HwQubitsVector>
HardwareQubits::computeAllShortestPaths(const HwQubit q1,
                                        const HwQubit q2) const {
  std::vector<HwQubitsVector> allPaths;
  std::queue<HwQubitsVector> pathsQueue;
  auto shortestPathLength = std::numeric_limits<std::size_t>::max();

  pathsQueue.push(HwQubitsVector{q1});

  while (!pathsQueue.empty()) {
    auto currentPath = pathsQueue.front();
    pathsQueue.pop();

    if (currentPath.size() > shortestPathLength) {
      continue;
    }

    HwQubit const currentQubit = currentPath.back();

    if (currentQubit == q2) {
      if (shortestPathLength == std::numeric_limits<std::size_t>::max()) {
        shortestPathLength = currentPath.size();
      }
      if (currentPath.size() == shortestPathLength) {
        allPaths.push_back(currentPath);
      }
      continue;
    }

    for (const auto& neighbor : getNearbyQubits(currentQubit)) {
      if (std::ranges::find(currentPath, neighbor) == currentPath.end()) {
        auto newPath = currentPath;
        newPath.push_back(neighbor);
        pathsQueue.push(newPath);
      }
    }
  }

  return allPaths;
}
void HardwareQubits::resetSwapDistances() {
  // TODO Improve to only reset the swap distances necessary (use a breadth
  // first search)
  swapDistances = qc::SymmetricMatrix(nQubits, -1);
}

void HardwareQubits::move(HwQubit hwQubit, const CoordIndex newCoord) {
  if (newCoord >= arch->getNpositions()) {
    throw std::runtime_error("Invalid coordinate");
  }
  // check if new coordinate is already occupied
  for (const auto& coord : hwToCoordIdx | std::views::values) {
    if (coord == newCoord) {
      throw std::runtime_error("Coordinate already occupied");
    }
  }

  const auto oldCoord = hwToCoordIdx.at(hwQubit);
  if (const auto it = std::ranges::find(occupiedCoordinates, oldCoord);
      it != occupiedCoordinates.end()) {
    occupiedCoordinates.erase(it);
  }
  occupiedCoordinates.emplace_back(newCoord);
  freeCoordinates.emplace_back(oldCoord);
  if (const auto it2 = std::ranges::find(freeCoordinates, newCoord);
      it2 != freeCoordinates.end()) {
    freeCoordinates.erase(it2);
  }

  // remove qubit from old nearby qubits
  const auto prevNearbyQubits = nearbyQubits.at(hwQubit);
  for (const auto& qubit : prevNearbyQubits) {
    auto& neigh = nearbyQubits.at(qubit);
    if (auto it3 = std::ranges::find(neigh, hwQubit); it3 != neigh.end()) {
      neigh.erase(it3);
    }
  }
  // move qubit and compute new nearby qubits
  hwToCoordIdx.at(hwQubit) = newCoord;
  computeNearbyQubits(hwQubit);

  // add qubit to new nearby qubits
  const auto newNearbyQubits = nearbyQubits.at(hwQubit);
  for (const auto& qubit : newNearbyQubits) {
    nearbyQubits.at(qubit).emplace(hwQubit);
  }

  // update/reset swap distances
  resetSwapDistances();
}

std::vector<Swap> HardwareQubits::getNearbySwaps(HwQubit q) const {
  std::vector<Swap> swaps;
  swaps.reserve(nearbyQubits.size());
  for (const auto& nearbyQubit : nearbyQubits.at(q)) {
    swaps.emplace_back(q, nearbyQubit);
  }
  return swaps;
}

void HardwareQubits::computeNearbyQubits(const HwQubit qubit) {
  std::set<HwQubit> newNearbyQubits;
  const auto coordQ = hwToCoordIdx.at(qubit);
  for (const auto& coord : hwToCoordIdx) {
    if (coord.first == qubit) {
      continue;
    }
    if (arch->getEuclideanDistance(coordQ, coord.second) <=
        arch->getInteractionRadius()) {
      newNearbyQubits.emplace(coord.first);
    }
  }
  nearbyQubits.insert_or_assign(qubit, newNearbyQubits);
}

qc::fp HardwareQubits::getAllToAllSwapDistance(std::set<HwQubit>& qubits) {
  // two qubit gates
  if (qubits.size() == 2) {
    auto it = qubits.begin();
    const auto q1 = *it;
    const auto q2 = *++it;
    return getSwapDistance(q1, q2);
  }
  // for n > 2 all qubits need to be within the interaction radius of each other
  qc::fp totalDistance = 0;
  for (auto it1 = qubits.begin(); it1 != qubits.end(); ++it1) {
    for (auto it2 = std::next(it1); it2 != qubits.end(); ++it2) {
      totalDistance += getSwapDistance(*it1, *it2);
    }
  }
  return totalDistance;
}

std::set<HwQubit>
HardwareQubits::getBlockedQubits(const std::set<HwQubit>& qubits) const {
  std::set<HwQubit> blockedQubits;
  for (const auto& qubit : qubits) {
    for (uint32_t i = 0; i < hwToCoordIdx.maxKey(); ++i) {
      if (i == qubit) {
        continue;
      }
      // TODO improve by using the nearby coords as a preselection
      auto const distance = arch->getEuclideanDistance(hwToCoordIdx.at(qubit),
                                                       hwToCoordIdx.at(i));
      if (distance <=
          arch->getBlockingFactor() * arch->getInteractionRadius()) {
        blockedQubits.emplace(i);
      }
    }
  }
  return blockedQubits;
}

std::set<CoordIndex>
HardwareQubits::getNearbyFreeCoordinatesByCoord(const CoordIndex idx) const {
  std::set<CoordIndex> nearbyFreeCoordinates;
  for (auto const& coordIndex : arch->getNearbyCoordinates(idx)) {
    if (!isMapped(coordIndex)) {
      nearbyFreeCoordinates.emplace(coordIndex);
    }
  }
  return nearbyFreeCoordinates;
}

std::set<CoordIndex> HardwareQubits::getNearbyOccupiedCoordinatesByCoord(
    const CoordIndex idx) const {
  const auto nearbyHwQubits = getNearbyQubits(getHwQubit(idx));
  return getCoordIndices(nearbyHwQubits);
}

std::vector<CoordIndex>
HardwareQubits::findClosestFreeCoord(const CoordIndex coord,
                                     const Direction direction,
                                     const CoordIndices& excludedCoords) const {
  std::vector<CoordIndex> freeCoordsInDirection;
  for (const auto& freeCoord : freeCoordinates) {
    if (std::ranges::find(excludedCoords, freeCoord) != excludedCoords.end()) {
      continue;
    }
    if (direction == arch->getVector(coord, freeCoord).direction) {
      freeCoordsInDirection.emplace_back(freeCoord);
    }
  }
  if (freeCoordsInDirection.empty()) {
    // return all free coords except excluded
    auto allFreeCoords = freeCoordinates;
    for (const auto& excludedCoord : excludedCoords) {
      if (const auto pos = std::ranges::find(allFreeCoords, excludedCoord);
          pos != allFreeCoords.end()) {
        allFreeCoords.erase(pos);
      }
    }
    return allFreeCoords;
  }
  auto minDistance = std::numeric_limits<qc::fp>::max();
  CoordIndex minCoord = freeCoordsInDirection.front();
  for (const auto& freeCoord : freeCoordsInDirection) {
    if (const auto distance = arch->getEuclideanDistance(coord, freeCoord);
        distance < minDistance) {
      minDistance = distance;
      minCoord = freeCoord;
    }
  }
  return {minCoord};
}

HwQubit HardwareQubits::getClosestQubit(const CoordIndex coord,
                                        const HwQubits& ignored) const {
  HwQubit closestQubit = 0;
  bool noneFound = true;
  auto minDistance = std::numeric_limits<qc::fp>::max();
  for (auto const& [qubit, idx] : hwToCoordIdx) {
    if (ignored.contains(qubit)) {
      continue;
    }
    if (const auto distance = arch->getEuclideanDistance(coord, idx);
        distance < minDistance) {
      minDistance = distance;
      closestQubit = qubit;
      noneFound = false;
    }
  }
  if (noneFound) {
    throw std::runtime_error(
        "No available qubit found when searching for closest qubit.");
  }
  return closestQubit;
}

} // namespace na
