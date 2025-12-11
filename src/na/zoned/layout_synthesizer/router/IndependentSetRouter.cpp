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
#include <list>
#include <set>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace na::zoned {
auto IndependentSetRouter::createStrictConflictGraph(
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
auto IndependentSetRouter::createRelaxedAndStrictConflictGraph(
    const std::vector<qc::Qubit>& atomsToMove, const Placement& startPlacement,
    const Placement& targetPlacement) const
    -> std::pair<
        std::unordered_map<qc::Qubit, std::vector<qc::Qubit>>,
        std::unordered_map<qc::Qubit, std::vector<std::pair<
                                          qc::Qubit, std::optional<double>>>>> {
  std::unordered_map<qc::Qubit, std::vector<qc::Qubit>> strictConflictGraph;
  std::unordered_map<qc::Qubit,
                     std::vector<std::pair<qc::Qubit, std::optional<double>>>>
      relaxedConflictGraph;
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
      const auto& comp = isRelaxedCompatibleMovement(atomMovementVector,
                                                     neighborMovementVector);
      if (comp.status != MovementCompatibility::Status::StrictlyCompatible) {
        strictConflictGraph.try_emplace(atom).first->second.emplace_back(
            neighbor);
        strictConflictGraph.try_emplace(neighbor).first->second.emplace_back(
            atom);
        relaxedConflictGraph.try_emplace(atom).first->second.emplace_back(
            neighbor, comp.mergeCost);
        relaxedConflictGraph.try_emplace(neighbor).first->second.emplace_back(
            atom, comp.mergeCost);
      }
    }
  }
  return {strictConflictGraph, relaxedConflictGraph};
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
/// @returns `(a^(1/3) + b^(1/3))^3`
auto sumCubeRootsCubed(const double a, const double b) -> double {
  double x = std::cbrt(a);
  double y = std::cbrt(b);
  // (x+y)^3 = a + b + 3*x*y*(x+y)
  return a + b + 3.0 * x * y * (x + y);
}
/// @returns `(a^(1/3) - b^(1/3))^3`
auto subCubeRootsCubed(const double a, const double b) -> double {
  double x = std::cbrt(a);
  double y = std::cbrt(b);
  // (x-y)^3 = a - b + 3*x*y*(y-x)
  return a - b + 3.0 * x * y * (y - x);
}
} // namespace
auto IndependentSetRouter::isRelaxedCompatibleMovement(
    const std::tuple<size_t, size_t, size_t, size_t>& v,
    const std::tuple<size_t, size_t, size_t, size_t>& w)
    -> MovementCompatibility {
  const auto& [v0, v1, v2, v3] = v;
  const auto& [w0, w1, w2, w3] = w;
  if (((v0 == w0) != (v2 == w2)) || ((v1 == w1) != (v3 == w3))) {
    return MovementCompatibility::incompatible();
  }
  // Helper to safely compute absolute difference
  auto distDouble = [](const auto a, const auto b) -> double {
    return static_cast<double>(a > b ? a - b : b - a);
  };
  if ((v0 < w0) != (v2 < w2) && (v1 < w1) != (v3 < w3)) {
    return MovementCompatibility::relaxedCompatible(
        sumCubeRootsCubed(distDouble(v0, w0) + distDouble(v2, w2),
                          distDouble(v1, w1) + distDouble(v3, w3)));
  }
  if ((v0 < w0) != (v2 < w2)) {
    return MovementCompatibility::relaxedCompatible(distDouble(v0, w0) +
                                                    distDouble(v2, w2));
  }
  if ((v1 < w1) != (v3 < w3)) {
    return MovementCompatibility::relaxedCompatible(distDouble(v1, w1) +
                                                    distDouble(v3, w3));
  }
  return MovementCompatibility::strictlyCompatible();
}
auto IndependentSetRouter::route(const std::vector<Placement>& placement) const
    -> std::vector<Routing> {
  // early return if no placement is given
  if (placement.empty()) {
    return {};
  }
  if (config_.method == Config::Method::STRICT) {
    return routeStrict(placement);
  }
  return routeRelaxed(placement);
}
auto IndependentSetRouter::routeStrict(
    const std::vector<Placement>& placement) const -> std::vector<Routing> {
  std::vector<Routing> routing;
  for (size_t i = 0; i + 1 < placement.size(); ++i) {
    const auto& startPlacement = placement[i];
    const auto& targetPlacement = placement[i + 1];
    auto atomsToMove = getAtomsToMove(startPlacement, targetPlacement);
    const auto conflictGraph =
        createStrictConflictGraph(atomsToMove, startPlacement, targetPlacement);
    auto& currentRouting = routing.emplace_back();
    while (!atomsToMove.empty()) {
      auto& group = currentRouting.emplace_back();
      std::vector<qc::Qubit> remainingAtoms;
      std::unordered_set<qc::Qubit> conflictingAtoms;
      for (const auto& atom : atomsToMove) {
        if (!conflictingAtoms.contains(atom)) {
          // if the atom does not conflict with any atom that is already in
          // the independent set, add it and mark its neighbors as conflicting
          group.emplace_back(atom);
          if (const auto conflictingNeighbors = conflictGraph.find(atom);
              conflictingNeighbors != conflictGraph.end()) {
            for (const auto neighbor : conflictingNeighbors->second) {
              conflictingAtoms.emplace(neighbor);
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
auto IndependentSetRouter::mergeConflictCost(
    std::optional<double>& existing, const std::optional<double>& incoming)
    -> void {
  if (existing.has_value()) {
    if (incoming.has_value()) {
      existing = std::max(*existing, *incoming);
    } else {
      existing = std::nullopt;
    }
  }
}
auto IndependentSetRouter::makeStrictRoutingForRelaxedRouting(
    std::vector<qc::Qubit> atomsToMove,
    const std::unordered_map<qc::Qubit, double>& atomsToDist,
    const std::unordered_map<qc::Qubit, std::vector<qc::Qubit>>& conflictGraph,
    const std::unordered_map<
        qc::Qubit, std::vector<std::pair<qc::Qubit, std::optional<double>>>>&
        relaxedConflictGraph) -> std::list<GroupInfo> {
  std::list<GroupInfo> groups;
  while (!atomsToMove.empty()) {
    auto& group = groups.emplace_back();
    std::vector<qc::Qubit> remainingAtoms;
    std::unordered_set<qc::Qubit> conflictingAtoms;
    for (const auto& atom : atomsToMove) {
      if (!conflictingAtoms.contains(atom)) {
        // if the atom does not conflict with any atom that is already in
        // the independent set, add it and mark its neighbors as conflicting
        group.independentSet.emplace_back(atom);
        const auto dist = atomsToDist.at(atom);
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
            if (!success) {
              mergeConflictCost(conflictIt->second, neighbor.second);
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
  return groups;
}
auto IndependentSetRouter::mergeGroups(
    const std::unordered_map<qc::Qubit, double>& atomsToDist,
    const std::unordered_map<
        qc::Qubit, std::vector<std::pair<qc::Qubit, std::optional<double>>>>&
        relaxedConflictGraph,
    std::list<GroupInfo>& groups) const -> void {
  for (auto groupIt = groups.rbegin(); groupIt != groups.rend();) {
    const auto& independentSet = groupIt->independentSet;
    std::unordered_map<qc::Qubit, GroupInfo*> atomToNewGroup;
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
          if (const auto conflictIt = group.relaxedConflictingAtoms.find(atom);
              conflictIt == group.relaxedConflictingAtoms.end()) {
            const auto dist = atomsToDist.at(atom);
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
            // can be added with additional cost because there is only a
            // relaxed conflict
            const auto dist = atomsToDist.at(atom);
            if (group.maxDistance >= dist) {
              if (costCubed > *conflictIt->second) {
                foundNewGroup = true;
                atomToNewGroup.insert_or_assign(atom, &group);
                costCubed = *conflictIt->second;
                cost = std::cbrt(*conflictIt->second);
              }
            } else {
              const auto c =
                  sumCubeRootsCubed(subCubeRootsCubed(dist, group.maxDistance),
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
    // and the offset cost, i.e., the time for the extra offsets, is less
    // than the cost for the current group. The cost for the current group
    // is the cubic root of the distance; hence, we compare the cubes of the
    // costs, i.e., the distance and the cubed costs directly.
    if (foundNewGroupForAllAtoms &&
        groupIt->maxDistance > config_.preferSplit * totalCostCubed) {
      std::ranges::for_each(atomToNewGroup, [&relaxedConflictGraph,
                                             &atomsToDist](const auto& pair) {
        const auto& [atom, group] = pair;
        // add atom to a new group
        group->independentSet.emplace_back(atom);
        const auto dist = atomsToDist.at(atom);
        if (group->maxDistance < dist) {
          group->maxDistance = dist;
        }
        if (const auto relaxedConflictingNeighbors =
                relaxedConflictGraph.find(atom);
            relaxedConflictingNeighbors != relaxedConflictGraph.end()) {
          for (const auto& neighbor : relaxedConflictingNeighbors->second) {
            auto [conflictIt, success] =
                group->relaxedConflictingAtoms.try_emplace(neighbor.first,
                                                           neighbor.second);
            if (!success) {
              mergeConflictCost(conflictIt->second, neighbor.second);
            }
          }
        }
      });
      // erase the current group from the linked list of groups; note that
      // a reverse pointer always points to the element in front of the
      // current iterator position. Hence, to erase the current group, we
      // increment reverse_iterator to move past the current element, then
      // .base() gives forward_iterator to the element to erase. After
      // erasing, we create a new reverse iterator pointing to the position
      // right before the erased element in the remaining list.
      const auto& a = (++groupIt).base();
      const auto& b = groups.erase(a);
      groupIt = std::make_reverse_iterator(b);
    } else {
      ++groupIt;
    }
  }
}
auto IndependentSetRouter::routeRelaxed(
    const std::vector<Placement>& placement) const -> std::vector<Routing> {
  std::vector<Routing> routing;
  for (size_t i = 0; i + 1 < placement.size(); ++i) {
    const auto& startPlacement = placement[i];
    const auto& targetPlacement = placement[i + 1];
    auto [atomsToMove, atomsToDist] =
        getAtomsToMoveWithDistance(startPlacement, targetPlacement);
    const auto& [strictConflictGraph, relaxedConflictGraph] =
        createRelaxedAndStrictConflictGraph(atomsToMove, startPlacement,
                                            targetPlacement);
    auto groups = makeStrictRoutingForRelaxedRouting(
        atomsToMove, atomsToDist, strictConflictGraph, relaxedConflictGraph);
    mergeGroups(atomsToDist, relaxedConflictGraph, groups);
    auto& currentRouting = routing.emplace_back();
    currentRouting.reserve(groups.size());
    for (auto& group : groups) {
      currentRouting.emplace_back(std::move(group.independentSet));
    }
  }
  return routing;
}
auto IndependentSetRouter::getAtomsToMove(
    const Placement& startPlacement, const Placement& targetPlacement) const
    -> std::vector<qc::Qubit> {
  std::set<std::pair<double, qc::Qubit>, std::greater<>>
      atomsToMoveOrderedDescByDist;
  assert(startPlacement.size() == targetPlacement.size());
  for (qc::Qubit atom = 0; atom < startPlacement.size(); ++atom) {
    const auto& [startSLM, startRow, startColumn] = startPlacement[atom];
    const auto& [targetSLM, targetRow, targetColumn] = targetPlacement[atom];
    // if atom must be moved
    if (&startSLM.get() != &targetSLM.get() || startRow != targetRow ||
        startColumn != targetColumn) {
      const auto distance = architecture_.get().distance(
          startSLM, startRow, startColumn, targetSLM, targetRow, targetColumn);
      atomsToMoveOrderedDescByDist.emplace(distance, atom);
    }
  }
  std::vector<qc::Qubit> atomsToMove;
  atomsToMove.reserve(atomsToMoveOrderedDescByDist.size());
  // put the atoms into the vector such they are ordered decreasingly by their
  // movement distance
  std::ranges::copy(atomsToMoveOrderedDescByDist | std::views::values,
                    std::back_inserter(atomsToMove));
  return atomsToMove;
}
auto IndependentSetRouter::getAtomsToMoveWithDistance(
    const Placement& startPlacement, const Placement& targetPlacement) const
    -> std::pair<std::vector<qc::Qubit>,
                 std::unordered_map<qc::Qubit, double>> {
  std::set<std::pair<double, qc::Qubit>, std::greater<>>
      atomsToMoveOrderedAscByDist;
  std::unordered_map<qc::Qubit, double> atomsToDist;
  assert(startPlacement.size() == targetPlacement.size());
  for (qc::Qubit atom = 0; atom < startPlacement.size(); ++atom) {
    const auto& [startSLM, startRow, startColumn] = startPlacement[atom];
    const auto& [targetSLM, targetRow, targetColumn] = targetPlacement[atom];
    // if atom must be moved
    if (&startSLM.get() != &targetSLM.get() || startRow != targetRow ||
        startColumn != targetColumn) {
      const auto distance = architecture_.get().distance(
          startSLM, startRow, startColumn, targetSLM, targetRow, targetColumn);
      atomsToMoveOrderedAscByDist.emplace(distance, atom);
      atomsToDist.emplace(atom, distance);
    }
  }
  std::vector<qc::Qubit> atomsToMove;
  atomsToMove.reserve(atomsToMoveOrderedAscByDist.size());
  // put the atoms into the vector such they are ordered decreasingly by their
  // movement distance
  std::ranges::copy(atomsToMoveOrderedAscByDist | std::views::values,
                    std::back_inserter(atomsToMove));
  return {atomsToMove, atomsToDist};
}
} // namespace na::zoned
