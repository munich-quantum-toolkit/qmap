/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "na/zoned/layout_synthesizer/router/IndependentSetRouter.hpp"

#include "ir/Definitions.hpp"
#include "na/zoned/Architecture.hpp"

#include <cassert>
#include <cstddef>
#include <functional>
#include <iostream>
#include <set>
#include <sstream>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace na::zoned {
auto IndependentSetRouter::createConflictGraph(
    const std::vector<qc::Qubit>& atomsToMove, const Placement& startPlacement,
    const Placement& targetPlacement) const
    -> std::unordered_map<qc::Qubit, std::vector<qc::Qubit>> {
  std::unordered_map<qc::Qubit, std::vector<qc::Qubit>> conflictGraph;
  for (auto atomIt = atomsToMove.cbegin(); atomIt != atomsToMove.cend();
       ++atomIt) {
    const auto& atom = *atomIt;
    const auto& atomMovementVector =
        getMovementVector(startPlacement[atom], targetPlacement[atom]);
    for (auto neighborIt = atomIt + 1; neighborIt != atomsToMove.cend();
         ++neighborIt) {
      const auto& neighbor = *neighborIt;
      const auto& neighborMovementVector = getMovementVector(
          startPlacement[neighbor], targetPlacement[neighbor]);
      if (!isCompatibleMovement(atomMovementVector, neighborMovementVector)) {
        conflictGraph.try_emplace(atom).first->second.emplace_back(neighbor);
        conflictGraph.try_emplace(neighbor).first->second.emplace_back(atom);
      }
    }
  }
  return conflictGraph;
}
auto IndependentSetRouter::getMovementVector(
    const std::tuple<const SLM&, size_t, size_t>& start,
    const std::tuple<const SLM&, size_t, size_t>& target) const
    -> std::tuple<size_t, size_t, size_t, size_t> {
  const auto& [startSLM, startRow, startColumn] = start;
  const auto& [startX, startY] =
      architecture_.get().exactSLMLocation(startSLM, startRow, startColumn);
  const auto& [targetSLM, targetRow, targetColumn] = target;
  const auto& [targetX, targetY] =
      architecture_.get().exactSLMLocation(targetSLM, targetRow, targetColumn);
  return std::make_tuple(startX, startY, targetX, targetY);
}
auto IndependentSetRouter::isCompatibleMovement(
    std::tuple<size_t, size_t, size_t, size_t> v,
    std::tuple<size_t, size_t, size_t, size_t> w) -> bool {
  const auto& [v0, v1, v2, v3] = v;
  const auto& [w0, w1, w2, w3] = w;
  if ((v0 == w0) != (v2 == w2)) {
    return false;
  }
  if ((v0 < w0) != (v2 < w2)) {
    return false;
  }
  if ((v1 == w1) != (v3 == w3)) {
    return false;
  }
  if ((v1 < w1) != (v3 < w3)) {
    return false;
  }
  return true;
}
auto IndependentSetRouter::route(const std::vector<Placement>& placement) const
    -> std::vector<Routing> {
  std::vector<Routing> routing;
  // early return if no placement is given
  if (placement.empty()) {
    return routing;
  }
  for (auto it = placement.cbegin(); true;) {
    const auto& startPlacement = *it;
    if (++it == placement.cend()) {
      break;
    }
    const auto& targetPlacement = *it;
    std::set<std::pair<double, qc::Qubit>, std::greater<>>
        atomsToMoveOrderedAscByDist;
    assert(startPlacement.size() == targetPlacement.size());
    for (qc::Qubit atom = 0; atom < startPlacement.size(); ++atom) {
      const auto& [startSLM, startRow, startColumn] = startPlacement[atom];
      const auto& [targetSLM, targetRow, targetColumn] = targetPlacement[atom];
      // if atom must be moved
      if (&startSLM.get() != &targetSLM.get() || startRow != targetRow ||
          startColumn != targetColumn) {
        const auto distance =
            architecture_.get().distance(startSLM, startRow, startColumn,
                                         targetSLM, targetRow, targetColumn);
        atomsToMoveOrderedAscByDist.emplace(distance, atom);
      }
    }
    std::vector<qc::Qubit> atomsToMove;
    atomsToMove.reserve(atomsToMoveOrderedAscByDist.size());
    // put the atoms into the vector such they are ordered decreasingly by their
    // movement distance
    for (const auto& atomIt : atomsToMoveOrderedAscByDist) {
      atomsToMove.emplace_back(atomIt.second);
    }
    auto conflictGraph =
        createConflictGraph(atomsToMove, startPlacement, targetPlacement);
    auto& currentRouting = routing.emplace_back();
    while (!atomsToMove.empty()) {
      auto& independentSet = currentRouting.emplace_back();
      std::vector<qc::Qubit> remainingAtoms;
      std::unordered_set<qc::Qubit> conflictingAtomsMoves;
      for (const auto& atom : atomsToMove) {
        if (conflictingAtomsMoves.find(atom) == conflictingAtomsMoves.end()) {
          // if the atom does not conflict with any atom that is already in the
          // independent set, add it and mark its neighbors as conflicting
          independentSet.emplace_back(atom);
          if (const auto conflictingNeighbors = conflictGraph.find(atom);
              conflictingNeighbors != conflictGraph.end()) {
            for (const auto neighbor : conflictingNeighbors->second) {
              conflictingAtomsMoves.emplace(neighbor);
            }
          }
        } else {
          // if an atom could not be put into the current independent set, add
          // it to the remaining atoms
          remainingAtoms.emplace_back(atom);
        }
      }
      atomsToMove = remainingAtoms;
    }
  }
  return routing;
}
} // namespace na::zoned
