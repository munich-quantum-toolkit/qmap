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
  swapDistances = qc::SymmetricMatrix<SwapDistance>(arch->getNqubits());
  for (uint32_t i = 0; i < arch->getNqubits(); ++i) {
    for (uint32_t j = 0; j < i; ++j) {
      swapDistances(i, j) =
          arch->getSwapDistance(hwToCoordIdx.at(i), hwToCoordIdx.at(j));
    }
  }
}

void HardwareQubits::initNearbyQubits() {
  for (uint32_t i = 0; i < arch->getNqubits(); ++i) {
    computeNearbyQubits(i);
  }
}

void HardwareQubits::computeSwapDistance(HwQubit q1, HwQubit q2) {
  std::queue<HwQubit> q;
  std::vector<bool> visited(swapDistances.size(), false);
  std::vector<HwQubit> parent(swapDistances.size(), q2);

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

void HardwareQubits::resetSwapDistances() {
  // TODO Improve to only reset the swap distances necessary (use a breadth
  // first search)
  swapDistances = qc::SymmetricMatrix(arch->getNqubits(), -1);
}

void HardwareQubits::move(HwQubit hwQubit, CoordIndex newCoord) {
  if (newCoord >= arch->getNpositions()) {
    throw std::runtime_error("Invalid coordinate");
  }
  // check if new coordinate is already occupied
  for (const auto& [qubit, coord] : hwToCoordIdx) {
    if (coord == newCoord) {
      throw std::runtime_error("Coordinate already occupied");
    }
  }

  // remove qubit from old nearby qubits
  auto prevNearbyQubits = nearbyQubits.at(hwQubit);
  for (const auto& qubit : prevNearbyQubits) {
    nearbyQubits.at(qubit).erase(std::find(
        nearbyQubits.at(qubit).begin(), nearbyQubits.at(qubit).end(), hwQubit));
  }
  // move qubit and compute new nearby qubits
  hwToCoordIdx.at(hwQubit) = newCoord;
  computeNearbyQubits(hwQubit);

  // add qubit to new nearby qubits
  auto newNearbyQubits = nearbyQubits.at(hwQubit);
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

void HardwareQubits::computeNearbyQubits(HwQubit q) {
  std::set<HwQubit> newNearbyQubits;
  auto coordQ = hwToCoordIdx.at(q);
  for (const auto& coord : hwToCoordIdx) {
    if (coord.first == q) {
      continue;
    }
    if (arch->getEuclideanDistance(coordQ, coord.second) <=
        arch->getInteractionRadius()) {
      newNearbyQubits.emplace(coord.first);
    }
  }
  nearbyQubits.insert_or_assign(q, newNearbyQubits);
}

qc::fp HardwareQubits::getAllToAllSwapDistance(std::set<HwQubit>& qubits) {
  // two qubit gates
  if (qubits.size() == 2) {
    auto it = qubits.begin();
    auto q1 = *it;
    auto q2 = *(++it);
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
HardwareQubits::getBlockedQubits(const std::set<HwQubit>& qubits) {
  std::set<HwQubit> blockedQubits;
  for (const auto& qubit : qubits) {
    for (uint32_t i = 0; i < arch->getNqubits(); ++i) {
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
HardwareQubits::getNearbyFreeCoordinatesByCoord(CoordIndex idx) {
  std::set<CoordIndex> nearbyFreeCoordinates;
  for (auto const& coordIndex : this->arch->getNearbyCoordinates(idx)) {
    if (!this->isMapped(coordIndex)) {
      nearbyFreeCoordinates.emplace(coordIndex);
    }
  }
  return nearbyFreeCoordinates;
}

std::set<CoordIndex>
HardwareQubits::getNearbyOccupiedCoordinatesByCoord(CoordIndex idx) const {
  auto nearbyHwQubits = this->getNearbyQubits(this->getHwQubit(idx));
  return this->getCoordIndices(nearbyHwQubits);
}

std::vector<CoordIndex>
HardwareQubits::findClosestFreeCoord(CoordIndex coord, Direction direction,
                                     const CoordIndices& excludeCoord) {
  // return the closest free coord in general
  // and the closest free coord in the given direction
  std::vector<CoordIndex> closestFreeCoords;
  std::queue<CoordIndex> queue;
  queue.push(coord);
  std::set<CoordIndex> visited;
  visited.emplace(coord);
  bool foundClosest = false;
  while (!queue.empty()) {
    auto currentCoord = queue.front();
    queue.pop();
    auto nearbyCoords = this->arch->getNN(currentCoord);
    for (const auto& nearbyCoord : nearbyCoords) {
      if (std::ranges::find(std::ranges::reverse_view(visited), nearbyCoord) ==
          visited.rend()) {
        visited.emplace(nearbyCoord);
        if (!this->isMapped(nearbyCoord) &&
            std::ranges::find(excludeCoord, nearbyCoord) ==
                excludeCoord.end()) {
          if (!foundClosest) {
            closestFreeCoords.emplace_back(nearbyCoord);
          }
          foundClosest = true;
          if (direction == arch->getVector(coord, nearbyCoord).direction) {
            closestFreeCoords.emplace_back(nearbyCoord);
            return closestFreeCoords;
          }
        } else {
          queue.push(nearbyCoord);
        }
      }
    }
  }
  return closestFreeCoords;
}

} // namespace na
