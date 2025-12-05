/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "na/zoned/layout_synthesizer/router/RelaxedIndependentSetRouter.hpp"

#include "ir/Definitions.hpp"
#include "na/zoned/Architecture.hpp"

#include <cassert>
#include <cstddef>
#include <functional>
#include <list>
#include <set>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace na::zoned {
auto RelaxedIndependentSetRouter::createConflictGraph(
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
auto RelaxedIndependentSetRouter::createRelaxedConflictGraph(
    const std::vector<qc::Qubit>& atomsToMove, const Placement& startPlacement,
    const Placement& targetPlacement) const
    -> std::unordered_map<
        qc::Qubit, std::vector<std::pair<qc::Qubit, std::optional<double>>>> {
  std::unordered_map<qc::Qubit,
                     std::vector<std::pair<qc::Qubit, std::optional<double>>>>
      conflictGraph;
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
      if (const auto& comp = isRelaxedCompatibleMovement(
              atomMovementVector, neighborMovementVector);
          comp.status != MovementCompatibility::Status::StrictlyCompatible) {
        conflictGraph.try_emplace(atom).first->second.emplace_back(
            neighbor, comp.mergeCost);
        conflictGraph.try_emplace(neighbor).first->second.emplace_back(
            atom, comp.mergeCost);
      }
    }
  }
  return conflictGraph;
}
auto RelaxedIndependentSetRouter::getMovementVector(
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
auto RelaxedIndependentSetRouter::isCompatibleMovement(
    const std::tuple<size_t, size_t, size_t, size_t>& v,
    const std::tuple<size_t, size_t, size_t, size_t>& w) -> bool {
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
namespace {
auto sumCubeRootsCubed(const double a, const double b) -> double {
  double x = std::cbrt(a);
  double y = std::cbrt(b);
  // (x+y)^3 = a + b + 3*x*y*(x+y)
  return a + b + 3.0 * x * y * (x + y);
}
auto subCubeRootsCubed(const double a, const double b) -> double {
  double x = std::cbrt(a);
  double y = std::cbrt(b);
  // (x-y)^3 = a - b + 3*x*y*(y-x)
  return a - b + 3.0 * x * y * (y - x);
}
} // namespace
auto RelaxedIndependentSetRouter::isRelaxedCompatibleMovement(
    const std::tuple<size_t, size_t, size_t, size_t>& v,
    const std::tuple<size_t, size_t, size_t, size_t>& w)
    -> MovementCompatibility {
  const auto& [v0, v1, v2, v3] = v;
  const auto& [w0, w1, w2, w3] = w;
  if (((v0 == w0) != (v2 == w2)) || ((v1 == w1) != (v3 == w3))) {
    return MovementCompatibility::incompatible();
  }
  if ((v0 < w0) != (v2 < w2) && (v1 < w1) != (v3 < w3)) {
    return MovementCompatibility::relaxedCompatible(sumCubeRootsCubed(
        static_cast<double>(
            std::abs(static_cast<int64_t>(v0) - static_cast<int64_t>(w0)) +
            std::abs(static_cast<int64_t>(v2) - static_cast<int64_t>(w2))),
        static_cast<double>(
            std::abs(static_cast<int64_t>(v1) - static_cast<int64_t>(w1)) +
            std::abs(static_cast<int64_t>(v3) - static_cast<int64_t>(w3)))));
  }
  if ((v0 < w0) != (v2 < w2)) {
    return MovementCompatibility::relaxedCompatible(static_cast<double>(
        std::abs(static_cast<int64_t>(v0) - static_cast<int64_t>(w0)) +
        std::abs(static_cast<int64_t>(v2) - static_cast<int64_t>(w2))));
  }
  if ((v1 < w1) != (v3 < w3)) {
    return MovementCompatibility::relaxedCompatible(static_cast<double>(
        std::abs(static_cast<int64_t>(v1) - static_cast<int64_t>(w1)) +
        std::abs(static_cast<int64_t>(v3) - static_cast<int64_t>(w3))));
  }
  return MovementCompatibility::strictlyCompatible();
}
auto RelaxedIndependentSetRouter::route(
    const std::vector<Placement>& placement) const -> std::vector<Routing> {
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
    std::unordered_map<qc::Qubit, double> atomToDist;
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
        atomToDist.emplace(atom, distance);
      }
    }
    std::vector<qc::Qubit> atomsToMove;
    atomsToMove.reserve(atomsToMoveOrderedAscByDist.size());
    // put the atoms into the vector such they are ordered decreasingly by their
    // movement distance
    for (const auto& val : atomsToMoveOrderedAscByDist | std::views::values) {
      atomsToMove.emplace_back(val);
    }
    const auto conflictGraph =
        createConflictGraph(atomsToMove, startPlacement, targetPlacement);
    const auto relaxedConflictGraph = createRelaxedConflictGraph(
        atomsToMove, startPlacement, targetPlacement);
    struct GroupInfo {
      std::vector<qc::Qubit> independentSet;
      double maxDistance = 0.0;
      std::unordered_map<qc::Qubit, std::optional<double>>
          relaxedConflictingAtoms;
    };
    std::list<GroupInfo> groups;
    while (!atomsToMove.empty()) {
      auto& group = groups.emplace_back();
      std::vector<qc::Qubit> remainingAtoms;
      std::unordered_set<qc::Qubit> conflictingAtoms;
      for (const auto& atom : atomsToMove) {
        if (!conflictingAtoms.contains(atom)) {
          // if the atom does not conflict with any atom that is already in the
          // independent set, add it and mark its neighbors as conflicting
          group.independentSet.emplace_back(atom);
          const auto dist = atomToDist.at(atom);
          if (group.maxDistance < dist) {
            group.maxDistance = dist;
          }
          if (const auto conflictingNeighbors = conflictGraph.find(atom);
              conflictingNeighbors != conflictGraph.end()) {
            for (const auto neighbor : conflictingNeighbors->second) {
              conflictingAtoms.emplace(neighbor);
            }
            assert(relaxedConflictGraph.contains(atom));
            for (const auto neighbor : relaxedConflictGraph.at(atom)) {
              auto [conflictIt, success] =
                  group.relaxedConflictingAtoms.try_emplace(neighbor.first,
                                                            neighbor.second);
              if (!success && conflictIt->second.has_value()) {
                if (neighbor.second.has_value()) {
                  conflictIt->second =
                      std::max(*conflictIt->second, *neighbor.second);
                } else {
                  conflictIt->second = std::nullopt;
                }
              }
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
    // try to merge rearrangement steps
    for (auto groupIt = groups.rbegin(); groupIt != groups.rend();) {
      const auto& independentSet = groupIt->independentSet;
      std::unordered_map<qc::Qubit, decltype(groups)::value_type*>
          atomToNewGroup;
      // find the best new group for each qubit in independent set and record
      // costs
      auto totalCost = 0.0;
      auto totalCostCubed = 0.0;
      bool foundNewGroupForAllAtoms = true;
      for (const auto& atom : independentSet) {
        bool foundNewGroup = false;
        auto cost = std::numeric_limits<double>::max();
        auto costCubed = std::numeric_limits<double>::max();
        for (auto& group : groups | std::views::reverse) {
          // filter current group
          if (&group != &*groupIt) {
            if (const auto conflictIt =
                    group.relaxedConflictingAtoms.find(atom);
                conflictIt == group.relaxedConflictingAtoms.end()) {
              const auto dist = atomToDist.at(atom);
              if (group.maxDistance >= dist) {
                foundNewGroup = true;
                atomToNewGroup.insert_or_assign(atom, &group);
                cost = 0;
                costCubed = 0;
                break;
              }
              const auto diff = subCubeRootsCubed(dist, group.maxDistance);
              if (costCubed > diff) {
                foundNewGroup = true;
                atomToNewGroup.insert_or_assign(atom, &group);
                costCubed = diff;
                cost = std::cbrt(diff);
              }
            } else if (conflictIt->second.has_value()) {
              // can be added with additional cost because there is a strict
              // conflict
              const auto dist = atomToDist.at(atom);
              if (group.maxDistance > dist) {
                if (costCubed > *conflictIt->second) {
                  foundNewGroup = true;
                  atomToNewGroup.insert_or_assign(atom, &group);
                  costCubed = *conflictIt->second;
                  cost = std::cbrt(*conflictIt->second);
                }
              } else {
                const auto c = sumCubeRootsCubed(
                    subCubeRootsCubed(dist, group.maxDistance),
                    *conflictIt->second);
                if (costCubed > c) {
                  foundNewGroup = true;
                  atomToNewGroup.insert_or_assign(atom, &group);
                  costCubed = c;
                  cost = std::cbrt(c);
                }
              }
            }
          }
        }
        if (!foundNewGroup) {
          foundNewGroupForAllAtoms = false;
          break;
        }
        // Note the following identity to calculate the new cubed offset as
        // offsetCostCubed' = (offsetCost + bestCost)^3
        //
        // Identity: (x + y)^3 = x^3 + y^3 + 3xy(x+y)
        totalCostCubed = costCubed + totalCostCubed +
                         3 * cost * totalCost * (cost + totalCost);
        totalCost += cost;
      }
      // if all atoms in the independent set could be assigned to a new group
      // and the offset cost, i.e., the time for the extra offsets, is less than
      // the cost for the current group. The cost for the current group is the
      // cubic root of the distance; hence, we compare the cubes of the costs,
      // i.e., the distance and the cubed costs directly.
      if (foundNewGroupForAllAtoms &&
          groupIt->maxDistance > config_.preferSplit * totalCostCubed) {
        std::ranges::for_each(atomToNewGroup, [&relaxedConflictGraph,
                                               &atomToDist](const auto& pair) {
          const auto& [atom, group] = pair;
          // add atom to a new group
          group->independentSet.emplace_back(atom);
          const auto dist = atomToDist.at(atom);
          if (group->maxDistance < dist) {
            group->maxDistance = dist;
          }
          if (const auto relaxedConflictingNeighbors =
                  relaxedConflictGraph.find(atom);
              relaxedConflictingNeighbors != relaxedConflictGraph.end()) {
            for (const auto neighbor : relaxedConflictingNeighbors->second) {
              auto [conflictIt, success] =
                  group->relaxedConflictingAtoms.try_emplace(neighbor.first,
                                                             neighbor.second);
              if (!success && conflictIt->second.has_value()) {
                if (neighbor.second.has_value()) {
                  conflictIt->second =
                      std::max(*conflictIt->second, *neighbor.second);
                } else {
                  conflictIt->second = std::nullopt;
                }
              }
            }
          }
        });
        // erase the current group from the linked list of groups; note that
        // a reverse pointer always points to the element in front of the
        // current iterator position.
        // After erasing, we create a new reverse iterator pointing to the
        // same logical position in the remaining list.
        const auto& a = (++groupIt).base();
        const auto& b = groups.erase(a);
        groupIt = std::make_reverse_iterator(b);
      } else {
        ++groupIt;
      }
    }
    auto& currentRouting = routing.emplace_back();
    currentRouting.reserve(groups.size());
    for (auto& group : groups) {
      currentRouting.emplace_back(std::move(group.independentSet));
    }
  }
  return routing;
}
} // namespace na::zoned
