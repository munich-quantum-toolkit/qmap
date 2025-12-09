/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "na/zoned/layout_synthesizer/placer/HeuristicPlacer.hpp"

#include "ir/Definitions.hpp"
#include "na/zoned/Architecture.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <numeric>
#include <optional>
#include <queue>
#include <set>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace na::zoned {
auto HeuristicPlacer::isGoal(const size_t nGates, const GateNode& node)
    -> bool {
  return node.level == nGates;
}
auto HeuristicPlacer::isGoal(const size_t nAtoms, const AtomNode& node)
    -> bool {
  return node.level == nAtoms;
}
auto HeuristicPlacer::discretizePlacementOfAtoms(
    const Placement& placement, const std::vector<qc::Qubit>& atoms) const
    -> std::pair<RowColumnMap<uint8_t>, RowColumnMap<uint8_t>> {
  std::map<size_t, RowColumnSet> rows;
  std::map<size_t, RowColumnSet> columns;
  for (const auto atom : atoms) {
    const auto& [slm, r, c] = placement[atom];
    const auto& [x, y] = architecture_.get().exactSLMLocation(slm, r, c);
    rows.try_emplace(y).first->second.emplace(std::cref(slm), r);
    columns.try_emplace(x).first->second.emplace(std::cref(slm), c);
  }
  RowColumnMap<uint8_t> rowIndices;
  uint8_t rowIndex = 0;
  for (const auto& sites : rows | std::views::values) {
    for (const auto& site : sites) {
      rowIndices.emplace(site, rowIndex);
    }
    ++rowIndex;
  }
  RowColumnMap<uint8_t> columnIndices;
  uint8_t columnIndex = 0;
  for (const auto& sites : columns | std::views::values) {
    for (const auto& site : sites) {
      columnIndices.emplace(site, columnIndex);
    }
    ++columnIndex;
  }
  return std::pair{rowIndices, columnIndices};
}

auto HeuristicPlacer::discretizeNonOccupiedStorageSites(
    const SiteSet& occupiedSites) const
    -> std::pair<RowColumnMap<uint8_t>, RowColumnMap<uint8_t>> {
  std::map<size_t, std::pair<std::reference_wrapper<const SLM>, size_t>> rows;
  std::map<size_t, std::pair<std::reference_wrapper<const SLM>, size_t>>
      columns;
  for (const auto& slm : architecture_.get().storageZones) {
    // find rows with free sites
    for (size_t r = 0; r < slm->nRows; ++r) {
      for (size_t c = 0; c < slm->nCols; ++c) {
        if (!occupiedSites.contains(std::tie(*slm, r, c))) {
          // free site in row r found at column c
          rows.emplace(slm->location.second + (slm->siteSeparation.second * r),
                       std::pair{std::cref(*slm), r});
          break;
        }
      }
    }
    // find columns with free sites
    for (size_t c = 0; c < slm->nCols; ++c) {
      for (size_t r = 0; r < slm->nRows; ++r) {
        if (!occupiedSites.contains(std::tie(*slm, r, c))) {
          // free site in column c found at row r
          columns.emplace(slm->location.first + (slm->siteSeparation.first * c),
                          std::pair{std::cref(*slm), c});
          break;
        }
      }
    }
  }
  RowColumnMap<uint8_t> rowIndices;
  uint8_t rowIndex = 0;
  for (const auto& site : rows | std::views::values) {
    rowIndices.emplace(site, rowIndex++);
  }
  RowColumnMap<uint8_t> columnIndices;
  uint8_t columnIndex = 0;
  for (const auto& site : columns | std::views::values) {
    columnIndices.emplace(site, columnIndex++);
  }
  return std::pair{rowIndices, columnIndices};
}

auto HeuristicPlacer::discretizeNonOccupiedEntanglementSites(
    const SiteSet& occupiedSites) const
    -> std::pair<RowColumnMap<uint8_t>, RowColumnMap<uint8_t>> {
  std::map<size_t, RowColumnSet> rows;
  std::map<size_t, RowColumnSet> columns;
  for (const auto& zone : architecture_.get().entanglementZones) {
    for (const auto& slm : *zone) {
      // find rows with free sites
      for (size_t r = 0; r < slm.nRows; ++r) {
        for (size_t c = 0; c < slm.nCols; ++c) {
          if (!occupiedSites.contains(std::tie(slm, r, c))) {
            // free site in row r found at column c
            rows.try_emplace(slm.location.second +
                             (slm.siteSeparation.second * r))
                .first->second.emplace(std::cref(slm), r);
            break;
          }
        }
      }
      // find columns with free sites
      for (size_t c = 0; c < slm.nCols; ++c) {
        for (size_t r = 0; r < slm.nRows; ++r) {
          if (!occupiedSites.contains(std::tie(slm, r, c))) {
            // free site in column c found at row r
            columns
                .try_emplace(slm.location.first +
                             (slm.siteSeparation.first * c))
                .first->second.emplace(std::cref(slm), c);
            break;
          }
        }
      }
    }
  }
  RowColumnMap<uint8_t> rowIndices;
  uint8_t rowIndex = 0;
  for (const auto& sites : rows | std::views::values) {
    for (const auto& site : sites) {
      rowIndices.emplace(site, rowIndex);
    }
    ++rowIndex;
  }
  RowColumnMap<uint8_t> columnIndices;
  uint8_t columnIndex = 0;
  for (const auto& sites : columns | std::views::values) {
    for (const auto& site : sites) {
      columnIndices.emplace(site, columnIndex);
    }
    ++columnIndex;
  }
  return std::pair{rowIndices, columnIndices};
}

auto HeuristicPlacer::makeInitialPlacement(const size_t nQubits) const
    -> Placement {
  auto slmIt = architecture_.get().storageZones.cbegin();
  std::size_t c = 0;
  std::int64_t r = reverseInitialPlacement_
                       ? static_cast<std::int64_t>((*slmIt)->nRows) - 1
                       : 0;
  const std::int64_t step = reverseInitialPlacement_ ? -1 : 1;
  Placement initialPlacement;
  initialPlacement.reserve(nQubits);
  for (qc::Qubit qubit = 0; qubit < nQubits; ++qubit) {
    initialPlacement.emplace_back(**slmIt, r, c++);
    if (c == (*slmIt)->nCols) {
      // the end of the row reached, go to the next row
      r += step;
      c = 0;
      if (step == 1 ? r == static_cast<std::int64_t>((*slmIt)->nRows)
                    : r == -1) {
        // the end of the slm reached, go to the next slm
        ++slmIt;
        r = step == 1 ? static_cast<std::int64_t>((*slmIt)->nRows) - 1 : 0;
      }
    }
  }
  return initialPlacement;
}

auto HeuristicPlacer::makeIntermediatePlacement(
    const Placement& previousPlacement,
    const std::unordered_set<qc::Qubit>& previousReuseQubits,
    const std::unordered_set<qc::Qubit>& reuseQubits,
    const TwoQubitGateLayer& twoQubitGates,
    const TwoQubitGateLayer& nextTwoQubitGates)
    -> std::pair<Placement, Placement> {
  const auto& gatePlacement = placeGatesInEntanglementZone(
      previousPlacement, previousReuseQubits, twoQubitGates, reuseQubits,
      nextTwoQubitGates);
  SPDLOG_TRACE("Placed gates");
  return {gatePlacement,
          placeAtomsInStorageZone(gatePlacement, reuseQubits, twoQubitGates,
                                  nextTwoQubitGates)};
}

auto HeuristicPlacer::addGateOption(
    const RowColumnMap<uint8_t>& discreteTargetRows,
    const RowColumnMap<uint8_t>& discreteTargetColumns, const SLM& leftSLM,
    const size_t leftRow, const size_t leftCol, const SLM& rightSLM,
    const size_t rightRow, const size_t rightCol, const SLM& nearestSLM,
    const size_t r, const size_t c, GateJob& job) const -> void {
  //                  other
  //         ┌─┐       ┌─┐ <-- Entanglement sites
  //         └┬┘       └┬┘
  //          │╲dis2   ╱│
  //     dis1 │  ╲   ╱  │
  //          │    ╳    │
  //          │  ╱   ╲  │ dis4
  //          │╱dis3   ╲│
  //         ┌┴┐       ┌┴┐ <-- Storage sites
  //         └─┘       └─┘
  //          ^         ^
  //        atom1     atom2
  const auto& [otherSLM, otherRow, otherCol] =
      architecture_.get().otherEntanglementSite(nearestSLM, r, c);
  const auto dis1 = static_cast<float>(architecture_.get().distance(
      leftSLM, leftRow, leftCol, nearestSLM, r, c));
  const auto dis2 = static_cast<float>(architecture_.get().distance(
      rightSLM, rightRow, rightCol, nearestSLM, r, c));
  const auto dis3 = static_cast<float>(architecture_.get().distance(
      leftSLM, leftRow, leftCol, otherSLM, otherRow, otherCol));
  const auto dis4 = static_cast<float>(architecture_.get().distance(
      rightSLM, rightRow, rightCol, otherSLM, otherRow, otherCol));
  if (dis1 + dis4 <= dis2 + dis3) {
    job.options.emplace_back(GateJob::Option{
        std::array{
            std::array{
                discreteTargetRows.at(std::pair{std::cref(nearestSLM), r}),
                discreteTargetColumns.at(std::pair{std::cref(nearestSLM), c})},
            std::array{
                discreteTargetRows.at(std::pair{std::cref(otherSLM), otherRow}),
                discreteTargetColumns.at(
                    std::pair{std::cref(otherSLM), otherCol})}},
        std::array{dis1, dis4}});
  } else {
    job.options.emplace_back(GateJob::Option{
        std::array{
            std::array{
                discreteTargetRows.at(std::pair{std::cref(otherSLM), otherRow}),
                discreteTargetColumns.at(
                    std::pair{std::cref(otherSLM), otherCol})},
            std::array{
                discreteTargetRows.at(std::pair{std::cref(nearestSLM), r}),
                discreteTargetColumns.at(std::pair{std::cref(nearestSLM), c})}},
        std::array{dis2, dis3}});
  }
}

auto HeuristicPlacer::placeGatesInEntanglementZone(
    const Placement& previousPlacement,
    const std::unordered_set<qc::Qubit>& reuseQubits,
    const TwoQubitGateLayer& twoQubitGates,
    const std::unordered_set<qc::Qubit>& nextReuseQubits,
    const TwoQubitGateLayer& nextTwoQubitGates) -> Placement {
  // Duplicate the previous placement as a starting point for the current
  auto currentPlacement = previousPlacement;
  //===------------------------------------------------------------------===//
  // Find gates and atoms that must be placed
  //===------------------------------------------------------------------===//
  std::set<std::pair<double, QubitPair>, std::greater<>> gatesToPlace;
  std::vector<qc::Qubit> atomsToPlace;
  for (const auto& gate : twoQubitGates) {
    const auto& [first, second] = gate;
    if (const auto firstQubitReuse =
            reuseQubits.contains(first) &&
            std::get<0>(previousPlacement[first]).get().isEntanglement();
        !firstQubitReuse &&
        (!reuseQubits.contains(second) ||
         std::get<0>(previousPlacement[second]).get().isStorage())) {
      const auto& [storageSLM1, storageRow1, storageCol1] =
          previousPlacement[first];
      const auto& [storageSLM2, storageRow2, storageCol2] =
          previousPlacement[second];
      const auto& [nearestSLM, nearestRow, nearestCol] =
          architecture_.get().nearestEntanglementSite(storageSLM1, storageRow1,
                                                      storageCol1, storageSLM2,
                                                      storageRow2, storageCol2);
      const auto& [otherSLM, otherRow, otherCol] =
          architecture_.get().otherEntanglementSite(nearestSLM, nearestRow,
                                                    nearestCol);
      // Calculate the various distances to the entanglement sites
      //
      // Example:
      //       nearest    other
      //         ┌─┐       ┌─┐ <-- Entanglement sites
      //         └┬┘       └┬┘
      //          │╲dis2   ╱│
      //     dis1 │  ╲   ╱  │
      //          │    ╳    │
      //          │  ╱   ╲  │ dis4
      //          │╱dis3   ╲│
      //         ┌┴┐       ┌┴┐ <-- Storage sites
      //         └─┘       └─┘
      //          ^         ^
      //   gate->first   gate->second
      const auto dis1 =
          architecture_.get().distance(storageSLM1, storageRow1, storageCol1,
                                       nearestSLM, nearestRow, nearestCol);
      const auto dis2 =
          architecture_.get().distance(storageSLM2, storageRow2, storageCol2,
                                       nearestSLM, nearestRow, nearestCol);
      const auto dis3 = architecture_.get().distance(
          storageSLM1, storageRow1, storageCol1, otherSLM, otherRow, otherCol);
      const auto dis4 = architecture_.get().distance(
          storageSLM2, storageRow2, storageCol2, otherSLM, otherRow, otherCol);
      if (dis1 + dis4 <= dis2 + dis3) {
        // if the situation is as depicted in the example above
        gatesToPlace.emplace(std::max(dis1, dis4), gate);
      } else {
        // otherwise, either the entanglement sites or storage sites are
        // flipped
        gatesToPlace.emplace(std::max(dis2, dis3), gate);
      }
      atomsToPlace.emplace_back(first);
      atomsToPlace.emplace_back(second);
    } else {
      // one of the qubits is reused, so no need to place a gate
      if (firstQubitReuse) {
        const auto& [slm, r, c] = previousPlacement[first];
        currentPlacement[second] =
            architecture_.get().otherEntanglementSite(slm, r, c);
      } else {
        // the second qubit is reused
        const auto& [slm, r, c] = previousPlacement[second];
        currentPlacement[first] =
            architecture_.get().otherEntanglementSite(slm, r, c);
      }
    }
  }
  if (gatesToPlace.empty()) {
    return currentPlacement;
  }
  //===------------------------------------------------------------------===//
  // Discretize the previous placement of the atoms to be placed
  //===------------------------------------------------------------------===//
  const auto& [discreteRows, discreteColumns] =
      discretizePlacementOfAtoms(previousPlacement, atomsToPlace);
  //===------------------------------------------------------------------===//
  // Extract occupied entanglement sites from the previous placement
  //===------------------------------------------------------------------===//
  // This set will only contain the first SLM in a pair of entanglement
  // SLMs and represents the occupied pair of sites
  SiteSet occupiedEntanglementSites{};
  for (const auto qubit : reuseQubits) {
    const auto& [slm, r, c] = previousPlacement[qubit];
    if (slm.get().isEntanglement()) {
      occupiedEntanglementSites.emplace(slm, r, c);
      // also add the interaction partner's site to the set
      const auto& [otherSLM, otherRow, otherCol] =
          architecture_.get().otherEntanglementSite(slm, r, c);
      occupiedEntanglementSites.emplace(otherSLM, otherRow, otherCol);
    }
  }
  //===------------------------------------------------------------------===//
  // Discretize the free sites for the atoms to be placed
  //===------------------------------------------------------------------===//
  const auto& [discreteTargetRows, discreteTargetColumns] =
      discretizeNonOccupiedEntanglementSites(occupiedEntanglementSites);
  std::unordered_map<uint8_t, std::unordered_map<uint8_t, Site>> targetSites;
  for (const auto& [row, r] : discreteTargetRows) {
    const SLM& slm = row.first.get();
    auto& targetSitesForThisRow = targetSites.try_emplace(r).first->second;
    for (const auto& [column, c] : discreteTargetColumns) {
      if (&slm == &column.first.get()) {
        targetSitesForThisRow.emplace(c,
                                      std::tie(slm, row.second, column.second));
      }
    }
  }
  assert(!targetSites.empty());
  //===------------------------------------------------------------------===//
  // Initialize the gate jobs
  //===------------------------------------------------------------------===//
  /// The number of either atom or gate jobs that must be performed
  const size_t nJobs = gatesToPlace.size();
  /**
   * a list of all gates that must be placed in the entanglement zone before a
   * rydberg layer
   */
  std::vector<GateJob> gateJobs;
  gateJobs.reserve(nJobs);
  for (const auto& gate : gatesToPlace | std::views::values) {
    const auto& [leftAtom, rightAtom] = gate;
    const auto& [leftSLM, leftRow, leftCol] = previousPlacement[leftAtom];
    const auto& [rightSLM, rightRow, rightCol] = previousPlacement[rightAtom];
    const auto& [nearestSLM, nearestRow, nearestCol] =
        architecture_.get().nearestEntanglementSite(
            leftSLM, leftRow, leftCol, rightSLM, rightRow, rightCol);
    auto& job = gateJobs.emplace_back();
    job.qubits = gate;
    job.currentSites = std::array{
        std::array{discreteRows.at(std::pair{std::cref(leftSLM), leftRow}),
                   discreteColumns.at(std::pair{std::cref(leftSLM), leftCol})},
        std::array{
            discreteRows.at(std::pair{std::cref(rightSLM), rightRow}),
            discreteColumns.at(std::pair{std::cref(rightSLM), rightCol})}};
    size_t rLow = 0;
    size_t rHigh = nearestSLM.get().nRows;
    size_t cLow = 0;
    size_t cHigh = nearestSLM.get().nCols;
    if (config_.useWindow) {
      rLow = nearestRow > windowMinHeight_ / 2
                 ? nearestRow - (windowMinHeight_ / 2)
                 : 0;
      rHigh = std::min(nearestRow + (windowMinHeight_ / 2) + 1,
                       nearestSLM.get().nRows);
      cLow = nearestCol > config_.windowMinWidth / 2
                 ? nearestCol - (config_.windowMinWidth / 2)
                 : 0;
      cHigh = std::min(nearestCol + (config_.windowMinWidth / 2) + 1,
                       nearestSLM.get().nCols);
    }
    for (size_t r = rLow; r < rHigh; ++r) {
      for (size_t c = cLow; c < cHigh; ++c) {
        if (!occupiedEntanglementSites.contains(std::tie(nearestSLM, r, c))) {
          addGateOption(discreteTargetRows, discreteTargetColumns, leftSLM,
                        leftRow, leftCol, rightSLM, rightRow, rightCol,
                        nearestSLM, r, c, job);
        }
      }
    }
    size_t expansion = 0;
    while (config_.useWindow &&
           static_cast<double>(job.options.size()) <
               config_.windowShare * static_cast<double>(nJobs)) {
      // the window does not contain enough options, so expand it
      ++expansion;
      size_t windowWidth = 0;
      size_t windowHeight = 0;
      if (config_.windowRatio < 1.0) {
        // landscapes ==> expand width and adjust height
        windowWidth = config_.windowMinWidth + expansion;
        windowHeight = static_cast<size_t>(
            std::round(config_.windowRatio * static_cast<double>(windowWidth)));
      } else {
        // portraits ==> expand height and adjust width
        windowHeight = windowMinHeight_ + expansion;
        windowWidth = static_cast<size_t>(std::round(
            static_cast<double>(windowHeight) / config_.windowRatio));
      }
      auto rLowNew =
          nearestRow > windowHeight / 2 ? nearestRow - (windowHeight / 2) : 0;
      auto rHighNew =
          std::min(nearestRow + (windowHeight / 2) + 1, nearestSLM.get().nRows);
      auto cLowNew =
          nearestCol > windowWidth / 2 ? nearestCol - (windowWidth / 2) : 0;
      auto cHighNew =
          std::min(nearestCol + (windowWidth / 2) + 1, nearestSLM.get().nCols);
      if (rLowNew < rLow) {
        assert(rLow - rLowNew == 1);
        for (size_t c = cLowNew; c < cHighNew; ++c) {
          if (!occupiedEntanglementSites.contains(
                  std::tie(nearestSLM, rLowNew, c))) {
            addGateOption(discreteTargetRows, discreteTargetColumns, leftSLM,
                          leftRow, leftCol, rightSLM, rightRow, rightCol,
                          nearestSLM, rLowNew, c, job);
          }
        }
      }
      if (rHighNew > rHigh) {
        assert(rHighNew - rHigh == 1);
        for (size_t c = cLowNew; c < cHighNew; ++c) {
          // NOTE: we have to use rHighNew - 1 here, which is equal to rHigh
          if (!occupiedEntanglementSites.contains(
                  std::tie(nearestSLM, rHigh, c))) {
            addGateOption(discreteTargetRows, discreteTargetColumns, leftSLM,
                          leftRow, leftCol, rightSLM, rightRow, rightCol,
                          nearestSLM, rHigh, c, job);
          }
        }
      }
      if (cLowNew < cLow) {
        assert(cLow - cLowNew == 1);
        for (size_t r = rLow; r < rHigh; ++r) {
          if (!occupiedEntanglementSites.contains(
                  std::tie(nearestSLM, r, cLowNew))) {
            addGateOption(discreteTargetRows, discreteTargetColumns, leftSLM,
                          leftRow, leftCol, rightSLM, rightRow, rightCol,
                          nearestSLM, r, cLowNew, job);
          }
        }
      }
      if (cHighNew > cHigh) {
        assert(cHighNew - cHigh == 1);
        for (size_t r = rLow; r < rHigh; ++r) {
          // NOTE: we have to use cHighNew - 1 here, which is equal to cHigh
          if (!occupiedEntanglementSites.contains(
                  std::tie(nearestSLM, r, cHigh))) {
            addGateOption(discreteTargetRows, discreteTargetColumns, leftSLM,
                          leftRow, leftCol, rightSLM, rightRow, rightCol,
                          nearestSLM, r, cHigh, job);
          }
        }
      }
      rLow = rLowNew;
      rHigh = rHighNew;
      cLow = cLowNew;
      cHigh = cHighNew;
    }
    std::ranges::sort(
        job.options,
        [](const GateJob::Option& lhs, const GateJob::Option& rhs) -> bool {
          return lhs.distance < rhs.distance;
        });
    // Determine whether a lookahead for the gate should be considered.
    // That is the case if the gate to be placed contains a reuse qubit
    // because then we do not only decide the position of the gate in this layer
    // but also of the gate in the next layer.
    bool leftReuse = nextReuseQubits.contains(leftAtom);
    bool rightReuse = nextReuseQubits.contains(rightAtom);
    if (leftReuse || rightReuse) {
      qc::Qubit nextInteractionPartner = 0;
      for (const auto& nextGate : nextTwoQubitGates) {
        const auto& [nextLeftAtom, nextRightAtom] = nextGate;
        if (leftReuse) {
          if (nextLeftAtom == leftAtom) {
            nextInteractionPartner = nextRightAtom;
            break;
          }
          if (nextRightAtom == leftAtom) {
            nextInteractionPartner = nextLeftAtom;
            break;
          }
        }
        if (rightReuse) {
          if (nextLeftAtom == rightAtom) {
            nextInteractionPartner = nextRightAtom;
            break;
          }
          if (nextRightAtom == rightAtom) {
            nextInteractionPartner = nextLeftAtom;
            break;
          }
        }
      }
      job.meanLookaheadCost = 0.0F;
      const auto& [nextSLM, nextRow, nextCol] =
          previousPlacement[nextInteractionPartner];
      for (auto& option : job.options) {
        const auto& [row, col] = option.sites[leftReuse ? 0 : 1];
        const auto& [targetSLM, targetRow, targetCol] =
            targetSites.at(row).at(col);
        const auto distance = static_cast<float>(architecture_.get().distance(
            nextSLM, nextRow, nextCol, targetSLM, targetRow, targetCol));
        option.lookaheadCost = config_.lookaheadFactor * std::sqrt(distance);
        job.meanLookaheadCost += option.lookaheadCost;
      }
      job.meanLookaheadCost /= static_cast<float>(job.options.size());
    }
  }
  //===------------------------------------------------------------------===//
  // Get the extent of discrete source and target
  //===------------------------------------------------------------------===//
  assert(!discreteRows.empty()); // ==> the following std::max_element does not
                                 // return a nullptr
  const uint8_t maxDiscreteSourceRow =
      std::ranges::max_element(discreteRows, [](const auto& lhs,
                                                const auto& rhs) {
        return lhs.second < rhs.second;
      })->second;
  assert(
      !discreteColumns.empty()); // ==> the following std::max_element does not
  // return a nullptr
  const uint8_t maxDiscreteSourceColumn =
      std::ranges::max_element(discreteColumns, [](const auto& lhs,
                                                   const auto& rhs) {
        return lhs.second < rhs.second;
      })->second;
  assert(!discreteTargetRows
              .empty()); // ==> the following std::max_element does not
  // return a nullptr
  const uint8_t maxDiscreteTargetRow =
      std::ranges::max_element(discreteTargetRows, [](const auto& lhs,
                                                      const auto& rhs) {
        return lhs.second < rhs.second;
      })->second;
  assert(!discreteTargetColumns
              .empty()); // ==> the following std::max_element does not
  // return a nullptr
  const uint8_t maxDiscreteTargetColumn =
      std::ranges::max_element(discreteTargetColumns, [](const auto& lhs,
                                                         const auto& rhs) {
        return lhs.second < rhs.second;
      })->second;
  // return a nullptr
  const std::array<float, 2> scaleFactors{
      std::min(1.F, static_cast<float>(1 + maxDiscreteTargetRow) /
                        static_cast<float>(1 + maxDiscreteSourceRow)),
      std::min(1.F, static_cast<float>(1 + maxDiscreteTargetColumn) /
                        static_cast<float>(1 + maxDiscreteSourceColumn))};
  //===------------------------------------------------------------------===//
  // Run the DFS algorithm
  //===------------------------------------------------------------------===//
  const auto deepeningFactor = config_.deepeningFactor;
  const auto deepeningValue = config_.deepeningValue;
  std::shared_ptr<const GateNode> node;
  switch (config_.method) {
  case Config::Method::IDS:
    node = iterativeDivingSearch<GateNode>(
        std::make_shared<const GateNode>(),
        [&gateJobs](const auto& node) { return getNeighbors(gateJobs, node); },
        [nJobs](const auto& node) { return isGoal(nJobs, node); },
        [](const auto& node) { return getCost(node); },
        [&gateJobs, deepeningFactor, deepeningValue,
         &scaleFactors](const auto& node) {
          return getHeuristic(gateJobs, deepeningFactor, deepeningValue,
                              scaleFactors, node);
        },
        config_.trials, config_.queueCapacity);
    break;
  case Config::Method::ASTAR:
    node = aStarTreeSearch<GateNode>(
        std::make_shared<const GateNode>(),
        [&gateJobs](const auto& node) { return getNeighbors(gateJobs, node); },
        [nJobs](const auto& node) { return isGoal(nJobs, node); },
        [](const auto& node) { return getCost(node); },
        [&gateJobs, deepeningFactor, deepeningValue,
         &scaleFactors](const auto& node) {
          return getHeuristic(gateJobs, deepeningFactor, deepeningValue,
                              scaleFactors, node);
        },
        config_.maxNodes);
    break;
  default:
    qc::unreachable();
  }
  //===------------------------------------------------------------------===//
  // Extract the final mapping
  //===------------------------------------------------------------------===//
  while (node->parent != nullptr) {
    assert(0 < node->level);
    assert(node->level <= gateJobs.size());
    const auto& job = gateJobs[node->level - 1];
    const auto& option = job.options[node->option];
    for (size_t j = 0; j < 2; ++j) {
      const auto atom = job.qubits[j];
      const auto& [row, col] = option.sites[j];
      currentPlacement[atom] = targetSites.at(row).at(col);
    }
    node = node->parent;
  }
  return currentPlacement;
}

auto HeuristicPlacer::placeAtomsInStorageZone(
    const Placement& previousPlacement,
    const std::unordered_set<qc::Qubit>& reuseQubits,
    const TwoQubitGateLayer& twoQubitGates,
    const TwoQubitGateLayer& nextTwoQubitGates) -> Placement {
  // Duplicate the previous placement as a starting point for the current
  auto currentPlacement = previousPlacement;
  if (twoQubitGates.empty()) {
    return currentPlacement;
  }
  //===------------------------------------------------------------------===//
  // Find atoms that must be placed
  //===------------------------------------------------------------------===//
  std::vector<qc::Qubit> atomsToPlace;
  double maxDistance = 0.0;
  size_t atomIndexWithMaxDistance = 0;
  for (const auto& gate : twoQubitGates) {
    for (const auto qubit : gate) {
      const auto& [slm, r, c] = previousPlacement[qubit];
      const auto& [nearestSLM, nearestRow, nearestCol] =
          architecture_.get().nearestStorageSite(slm, r, c);
      const auto distance = architecture_.get().distance(
          slm, r, c, nearestSLM, nearestRow, nearestCol);
      if (distance > maxDistance) {
        maxDistance = distance;
        atomIndexWithMaxDistance = atomsToPlace.size();
      }
      atomsToPlace.emplace_back(qubit);
    }
  }
  //===------------------------------------------------------------------===//
  // Discretize the previous placement of the atoms to be placed
  //===------------------------------------------------------------------===//
  // Place the atoms with the longest distance first, but the place atoms in
  // increasing order of distance to the first atom
  // swap atoms at index 0 and atomIndexWithMaxDistance
  std::swap(atomsToPlace.front(), atomsToPlace.at(atomIndexWithMaxDistance));
  const auto& frontPlacement = previousPlacement[atomsToPlace.front()];
  std::set<std::pair<double, qc::Qubit>> atomsWithoutFirstAtom;
  for (auto atomIt = atomsToPlace.cbegin() + 1; atomIt != atomsToPlace.cend();
       ++atomIt) {
    const auto& [frontSLM, frontRow, frontCol] = frontPlacement;
    const auto& [slm, r, c] = previousPlacement[*atomIt];
    const auto distance =
        architecture_.get().distance(slm, r, c, frontSLM, frontRow, frontCol);
    atomsWithoutFirstAtom.emplace(distance, *atomIt);
  }
  std::ranges::transform(std::as_const(atomsWithoutFirstAtom),
                         atomsToPlace.begin() + 1,
                         [](const auto& pair) { return pair.second; });
  // Discretize the previous placement of the atoms to be placed that are
  // ordered now
  const auto& [discreteRows, discreteColumns] =
      discretizePlacementOfAtoms(previousPlacement, atomsToPlace);
  //===------------------------------------------------------------------===//
  // Extract occupied storage sites from the previous placement
  //===------------------------------------------------------------------===//
  SiteSet occupiedStorageSites{};
  for (const auto& [slm, r, c] : previousPlacement) {
    if (slm.get().isStorage()) {
      occupiedStorageSites.emplace(slm, r, c);
    }
  }
  //===------------------------------------------------------------------===//
  // Discretize the free sites for the atoms to be placed
  //===------------------------------------------------------------------===//
  const auto& [discreteTargetRows, discreteTargetColumns] =
      discretizeNonOccupiedStorageSites(occupiedStorageSites);
  std::unordered_map<uint8_t, std::unordered_map<uint8_t, Site>> targetSites;
  for (const auto& [row, r] : discreteTargetRows) {
    const SLM& slm = row.first.get();
    auto& targetSitesForThisRow = targetSites.try_emplace(r).first->second;
    for (const auto& [column, c] : discreteTargetColumns) {
      if (&slm == &column.first.get()) {
        targetSitesForThisRow.emplace(c,
                                      std::tie(slm, row.second, column.second));
      }
    }
  }
  assert(!targetSites.empty());
  //===------------------------------------------------------------------===//
  // Initialize atom jobs
  //===------------------------------------------------------------------===//
  /// The number of either atom or gate jobs that must be performed
  size_t nJobs = atomsToPlace.size();
  /**
   * a list of all atoms that must be placed in the storage zone after a
   * rydberg layer
   */
  std::vector<AtomJob> atomJobs;
  atomJobs.reserve(nJobs);
  uint8_t minDiscreteColumnOfNearestSite = std::numeric_limits<uint8_t>::max();
  uint8_t maxDiscreteColumnOfNearestSite = 0;
  for (const auto atom : atomsToPlace) {
    const auto& [previousSLM, previousRow, previousCol] =
        previousPlacement[atom];
    const auto& [nearestSLM, nearestRow, nearestCol] =
        architecture_.get().nearestStorageSite(previousSLM, previousRow,
                                               previousCol);
    uint8_t discreteColumnOfNearestSite = 0;
    const auto& it = discreteTargetColumns.find(
        std::pair{std::cref(nearestSLM), nearestCol});
    if (it != discreteTargetColumns.end()) {
      discreteColumnOfNearestSite = it->second;
    } else {
      // distance of the actual (occupied) nearest column and a column with free
      // sites
      int64_t minDistance = std::numeric_limits<int64_t>::max();
      for (const auto& [SlmColumn, discreteColumn] : discreteTargetColumns) {
        const auto& [slm, column] = SlmColumn;
        if (slm.get() == nearestSLM.get()) {
          if (const auto currentDistance =
                  std::abs(static_cast<int64_t>(column) -
                           static_cast<int64_t>(nearestCol)) < minDistance) {
            discreteColumnOfNearestSite = discreteColumn;
            minDistance = currentDistance;
          }
        }
      }
    }
    minDiscreteColumnOfNearestSite =
        std::min(minDiscreteColumnOfNearestSite, discreteColumnOfNearestSite);
    maxDiscreteColumnOfNearestSite =
        std::max(maxDiscreteColumnOfNearestSite, discreteColumnOfNearestSite);
    auto& job = atomJobs.emplace_back();
    job.atom = atom;
    job.currentSite = std::array{
        discreteRows.at(std::pair{std::cref(previousSLM), previousRow}),
        discreteColumns.at(std::pair{std::cref(previousSLM), previousCol})};
    if (reuseQubits.contains(atom)) {
      // atom can be reused, so we add an option for the atom to stay at the
      // current site
      job.options.emplace_back(AtomJob::Option{{0, 0}, true, 0.0F});
    }
    size_t rLow = 0;
    size_t rHigh = nearestSLM.get().nRows;
    size_t cLow = 0;
    size_t cHigh = nearestSLM.get().nCols;
    if (config_.useWindow) {
      rLow = nearestRow > windowMinHeight_ / 2
                 ? nearestRow - (windowMinHeight_ / 2)
                 : 0;
      rHigh = std::min(nearestRow + (windowMinHeight_ / 2) + 1,
                       nearestSLM.get().nRows);
      cLow = nearestCol > config_.windowMinWidth / 2
                 ? nearestCol - (config_.windowMinWidth / 2)
                 : 0;
      cHigh = std::min(nearestCol + (config_.windowMinWidth / 2) + 1,
                       nearestSLM.get().nCols);
    }
    for (size_t r = rLow; r < rHigh; ++r) {
      for (size_t c = cLow; c < cHigh; ++c) {
        if (!occupiedStorageSites.contains(std::tie(nearestSLM, r, c))) {
          const auto distance = static_cast<float>(architecture_.get().distance(
              previousSLM, previousRow, previousCol, nearestSLM, r, c));
          job.options.emplace_back(AtomJob::Option{
              {discreteTargetRows.at(std::pair{std::cref(nearestSLM), r}),
               discreteTargetColumns.at(std::pair{std::cref(nearestSLM), c})},
              false,
              distance});
        }
      }
    }
    size_t expansion = 0;
    while (config_.useWindow &&
           static_cast<double>(job.options.size()) <
               config_.windowShare * static_cast<double>(nJobs)) {
      // the window does not contain enough options, so expand it
      ++expansion;
      size_t windowWidth = 0;
      size_t windowHeight = 0;
      if (config_.windowRatio < 1.0) {
        // landscape ==> expand width and adjust height
        // the overall width and height is divided by 2 later, hence an
        // expansion of 2 is needed to actually increase the window size
        windowWidth = config_.windowMinWidth + 2 * expansion;
        windowHeight = static_cast<size_t>(
            std::round(config_.windowRatio * static_cast<double>(windowWidth)));
      } else {
        // portraits ==> expand height and adjust width
        // the overall width and height is divided by 2 later, hence an
        // expansion of 2 is needed to actually increase the window size
        windowHeight = windowMinHeight_ + 2 * expansion;
        windowWidth = static_cast<size_t>(std::round(
            static_cast<double>(windowHeight) / config_.windowRatio));
      }
      auto rLowNew =
          nearestRow > windowHeight / 2 ? nearestRow - (windowHeight / 2) : 0;
      auto rHighNew =
          std::min(nearestRow + (windowHeight / 2) + 1, nearestSLM.get().nRows);
      auto cLowNew =
          nearestCol > windowWidth / 2 ? nearestCol - (windowWidth / 2) : 0;
      auto cHighNew =
          std::min(nearestCol + (windowWidth / 2) + 1, nearestSLM.get().nCols);
      if (rLowNew < rLow) {
        assert(rLow - rLowNew == 1);
        for (size_t c = cLowNew; c < cHighNew; ++c) {
          if (!occupiedStorageSites.contains(
                  std::tie(nearestSLM, rLowNew, c))) {
            const auto distance =
                static_cast<float>(architecture_.get().distance(
                    previousSLM, previousRow, previousCol, nearestSLM, rLowNew,
                    c));
            job.options.emplace_back(AtomJob::Option{
                {discreteTargetRows.at(
                     std::pair{std::cref(nearestSLM), rLowNew}),
                 discreteTargetColumns.at(std::pair{std::cref(nearestSLM), c})},
                false,
                distance});
          }
        }
      }
      if (rHighNew > rHigh) {
        assert(rHighNew - rHigh == 1);
        for (size_t c = cLowNew; c < cHighNew; ++c) {
          // NOTE: we have to use rHighNew - 1 here, which is equal to rHigh
          if (!occupiedStorageSites.contains(std::tie(nearestSLM, rHigh, c))) {
            const auto distance =
                static_cast<float>(architecture_.get().distance(
                    previousSLM, previousRow, previousCol, nearestSLM, rHigh,
                    c));
            job.options.emplace_back(AtomJob::Option{
                {discreteTargetRows.at(std::pair{std::cref(nearestSLM), rHigh}),
                 discreteTargetColumns.at(std::pair{std::cref(nearestSLM), c})},
                false,
                distance});
          }
        }
      }
      if (cLowNew < cLow) {
        assert(cLow - cLowNew == 1);
        for (size_t r = rLow; r < rHigh; ++r) {
          if (!occupiedStorageSites.contains(
                  std::tie(nearestSLM, r, cLowNew))) {
            const auto distance =
                static_cast<float>(architecture_.get().distance(
                    previousSLM, previousRow, previousCol, nearestSLM, r,
                    cLowNew));
            job.options.emplace_back(AtomJob::Option{
                {discreteTargetRows.at(std::pair{std::cref(nearestSLM), r}),
                 discreteTargetColumns.at(
                     std::pair{std::cref(nearestSLM), cLowNew})},
                false,
                distance});
          }
        }
      }
      if (cHighNew > cHigh) {
        assert(cHighNew - cHigh == 1);
        for (size_t r = rLow; r < rHigh; ++r) {
          // NOTE: we have to use cHighNew - 1 here, which is equal to cHigh
          if (!occupiedStorageSites.contains(std::tie(nearestSLM, r, cHigh))) {
            const auto distance =
                static_cast<float>(architecture_.get().distance(
                    previousSLM, previousRow, previousCol, nearestSLM, r,
                    cHigh));
            job.options.emplace_back(AtomJob::Option{
                {discreteTargetRows.at(std::pair{std::cref(nearestSLM), r}),
                 discreteTargetColumns.at(
                     std::pair{std::cref(nearestSLM), cHigh})},
                false,
                distance});
          }
        }
      }
      rLow = rLowNew;
      rHigh = rHighNew;
      cLow = cLowNew;
      cHigh = cHighNew;
    }
    std::ranges::sort(
        job.options,
        [](const AtomJob::Option& lhs, const AtomJob::Option& rhs) -> bool {
          return lhs.distance < rhs.distance;
        });
    // Determine lookahead for the atom to be placed is a reuse qubit
    for (const auto& nextGate : nextTwoQubitGates) {
      const auto& [nextLeftAtom, nextRightAtom] = nextGate;
      if (nextLeftAtom == atom || nextRightAtom == atom) {
        const qc::Qubit nextInteractionPartner =
            nextLeftAtom == atom ? nextRightAtom : nextLeftAtom;
        const auto& [nextSLM, nextRow, nextCol] =
            previousPlacement[nextInteractionPartner];
        // use this attribute to sum up the costs and calculate the mean by
        // dividing by the number of options
        job.meanLookaheadCost = 0.0F;
        for (auto& option : job.options) {
          if (option.reuse) {
            const auto distance =
                static_cast<float>(architecture_.get().distance(
                    nextSLM, nextRow, nextCol, previousSLM, previousRow,
                    previousCol));
            // NOTE: the multiplication with the lookahead factor is missing
            // here on purpose as this is the distance the "reuse" costs by
            // taking the distance of the next interaction partner as cost here
            option.lookaheadCost =
                std::max(0.0F, std::sqrt(distance) - config_.reuseLevel);
          } else {
            const auto& [row, col] = option.site;
            const auto& [targetSLM, targetRow, targetCol] =
                targetSites.at(row).at(col);
            const auto distance = static_cast<float>(
                architecture_.get().distance(nextSLM, nextRow, nextCol,
                                             targetSLM, targetRow, targetCol));
            option.lookaheadCost =
                config_.lookaheadFactor * std::sqrt(distance);
          }
          job.meanLookaheadCost += option.lookaheadCost;
        }
        job.meanLookaheadCost /= static_cast<float>(job.options.size());
        break;
      }
    }
  }
  //===------------------------------------------------------------------===//
  // Get the extent of discrete source and target
  //===------------------------------------------------------------------===//
  assert(!discreteRows.empty()); // ==> the following std::max_element does not
                                 // return a nullptr
  const uint8_t maxDiscreteSourceRow =
      std::ranges::max_element(discreteRows, [](const auto& lhs,
                                                const auto& rhs) {
        return lhs.second < rhs.second;
      })->second;
  assert(
      !discreteColumns.empty()); // ==> the following std::max_element does not
  // return a nullptr
  const uint8_t maxDiscreteSourceColumn =
      std::ranges::max_element(discreteColumns, [](const auto& lhs,
                                                   const auto& rhs) {
        return lhs.second < rhs.second;
      })->second;
  assert(!discreteTargetRows
              .empty()); // ==> the following std::max_element does not
  // return a nullptr
  const uint8_t maxDiscreteTargetRow =
      std::ranges::max_element(discreteTargetRows, [](const auto& lhs,
                                                      const auto& rhs) {
        return lhs.second < rhs.second;
      })->second;
  assert(!discreteTargetColumns
              .empty()); // ==> the following std::max_element does not
  // return a nullptr
  const uint8_t maxDiscreteTargetColumn =
      std::ranges::max_element(discreteTargetColumns, [](const auto& lhs,
                                                         const auto& rhs) {
        return lhs.second < rhs.second;
      })->second;
  const std::array<float, 2> scaleFactors{
      std::min(1.F, static_cast<float>(1 + maxDiscreteTargetRow) /
                        static_cast<float>(1 + maxDiscreteSourceRow)),
      std::min(
          std::max(1.F, static_cast<float>(maxDiscreteColumnOfNearestSite -
                                           minDiscreteColumnOfNearestSite) /
                            static_cast<float>(maxDiscreteSourceColumn)),
          static_cast<float>(1 + maxDiscreteTargetColumn) /
              static_cast<float>(1 + maxDiscreteSourceColumn))};
  //===------------------------------------------------------------------===//
  // Run the DFS algorithm
  //===------------------------------------------------------------------===//
  /**
   * A list of all nodes that have been created so far.
   * This happens when a node is expanded by calling getNeighbors.
   */
  const auto deepeningFactor = config_.deepeningFactor;
  const auto deepeningValue = config_.deepeningValue;

  std::shared_ptr<const AtomNode> node;
  switch (config_.method) {
  case Config::Method::IDS:
    node = iterativeDivingSearch<AtomNode>(
        std::make_shared<const AtomNode>(),
        [&atomJobs](const auto& node) { return getNeighbors(atomJobs, node); },
        [nJobs](const auto& node) { return isGoal(nJobs, node); },
        [](const auto& node) { return getCost(node); },
        [&atomJobs, deepeningFactor, deepeningValue,
         &scaleFactors](const auto& node) {
          return getHeuristic(atomJobs, deepeningFactor, deepeningValue,
                              scaleFactors, node);
        },
        config_.trials, config_.queueCapacity);
    break;
  case Config::Method::ASTAR:
    node = aStarTreeSearch<AtomNode>(
        std::make_shared<const AtomNode>(),
        [&atomJobs](const auto& node) { return getNeighbors(atomJobs, node); },
        [nJobs](const auto& node) { return isGoal(nJobs, node); },
        [](const auto& node) { return getCost(node); },
        [&atomJobs, deepeningFactor, deepeningValue,
         &scaleFactors](const auto& node) {
          return getHeuristic(atomJobs, deepeningFactor, deepeningValue,
                              scaleFactors, node);
        },
        config_.maxNodes);
    break;
  default:
    qc::unreachable();
  }
  //===------------------------------------------------------------------===//
  // Extract the final mapping
  //===------------------------------------------------------------------===//
  while (node->parent != nullptr) {
    assert(0 < node->level);
    assert(node->level <= atomJobs.size());
    const auto& job = atomJobs[node->level - 1];
    assert(node->option < job.options.size());
    const auto& option = job.options[node->option];
    if (!option.reuse) {
      const auto atom = job.atom;
      const auto& [row, col] = option.site;
      currentPlacement[atom] = targetSites.at(row).at(col);
    }
    node = node->parent;
  }
  return currentPlacement;
}

auto HeuristicPlacer::getCost(const GateNode& node) -> float {
  float cost = node.lookaheadCost;
  for (const auto d : node.maxDistancesOfPlacedAtomsPerGroup) {
    cost += std::sqrt(d);
  }
  return cost;
}

auto HeuristicPlacer::getCost(const AtomNode& node) -> float {
  float cost = node.lookaheadCost;
  for (const auto d : node.maxDistancesOfPlacedAtomsPerGroup) {
    cost += std::sqrt(d);
  }
  return cost;
}

auto HeuristicPlacer::sumStdDeviationForGroups(
    const std::array<float, 2>& scaleFactors,
    const std::vector<CompatibilityGroup>& groups) -> float {
  float sumStdDev = 0.F;
  for (const auto& groupPair : groups) {
    for (size_t i = 0; i < 2; ++i) {
      const auto& group = groupPair[i];
      std::vector<float> diffs;
      diffs.reserve(groups.size());
      for (const auto& [key, value] : group) {
        diffs.emplace_back(static_cast<float>(value) -
                           (scaleFactors[i] * static_cast<float>(key)));
      }
      const auto n = static_cast<float>(group.size());
      const auto mean = std::accumulate(diffs.cbegin(), diffs.cend(), 0.F) / n;
      const auto variance =
          std::accumulate(diffs.cbegin(), diffs.cend(), 0.F,
                          [mean](const float acc, const float diff) -> float {
                            return acc + ((diff - mean) * (diff - mean));
                          }) /
          n;
      sumStdDev += std::sqrt(variance);
    }
  }
  return sumStdDev;
}

auto HeuristicPlacer::getHeuristic(const std::vector<AtomJob>& atomJobs,
                                   const float deepeningFactor,
                                   const float deepeningValue,
                                   const std::array<float, 2>& scaleFactors,
                                   const AtomNode& node) -> float {
  const auto nAtomJobs = atomJobs.size();
  const auto nUnplacedAtoms = static_cast<float>(nAtomJobs - node.level);
  float maxDistanceOfUnplacedAtom = 0.0F;
  float accMinLookaheadCost = 0.0F;
  for (size_t i = node.level; i < nAtomJobs; ++i) {
    const auto& job = atomJobs[i];
    accMinLookaheadCost += job.meanLookaheadCost;
    for (const auto& option : job.options) {
      if (option.reuse) {
        // since them jobs are sorted by distance and this has distance 0 as the
        // atom does not need to be moved at all, this job will always be the
        // first one for atoms that may be reused
        break;
      }
      if (!node.consumedFreeSites.contains(option.site)) {
        // this assumes that the first found free site is the nearest free site
        // for that atom. This requires that the job options are sorted by
        // distance.
        maxDistanceOfUnplacedAtom =
            std::max(maxDistanceOfUnplacedAtom, option.distance);
        break; // exit when the first free site is found
      }
    }
  }
  float maxDistanceOfPlacedAtom = 0.0F;
  for (const auto d : node.maxDistancesOfPlacedAtomsPerGroup) {
    maxDistanceOfPlacedAtom = std::max(maxDistanceOfPlacedAtom, d);
  }
  float heuristic = maxDistanceOfUnplacedAtom <= maxDistanceOfPlacedAtom
                        ? 0.F
                        : std::sqrt(maxDistanceOfUnplacedAtom) -
                              std::sqrt(maxDistanceOfPlacedAtom);
  heuristic += accMinLookaheadCost;
  heuristic +=
      deepeningFactor *
      (sumStdDeviationForGroups(scaleFactors, node.groups) + deepeningValue) *
      nUnplacedAtoms;
  return heuristic;
}

auto HeuristicPlacer::getHeuristic(const std::vector<GateJob>& gateJobs,
                                   const float deepeningFactor,
                                   const float deepeningValue,
                                   const std::array<float, 2>& scaleFactors,
                                   const GateNode& node) -> float {
  const auto nGateJobs = gateJobs.size();
  const auto nUnplacedGates = static_cast<float>(nGateJobs - node.level);
  float maxDistanceOfUnplacedAtom = 0.0F;
  float accMeanLookaheadCost = 0.0F;
  for (size_t i = node.level; i < nGateJobs; ++i) {
    const auto& job = gateJobs[i];
    accMeanLookaheadCost += job.meanLookaheadCost;
    for (const auto& option : job.options) {
      // this assumes that the first found pair of free sites is the nearest
      // pair of free sites for that gate. This requires that the job options
      // are sorted by distance.
      if (std::ranges::all_of(option.sites,
                              [&node](const DiscreteSite& site) -> bool {
                                return !node.consumedFreeSites.contains(site);
                              })) {
        maxDistanceOfUnplacedAtom =
            std::max(maxDistanceOfUnplacedAtom,
                     *std::ranges::max_element(option.distance));
        break; // exit when the first free site pair is found
      }
    }
  }
  float maxDistanceOfPlacedAtom = 0.0F;
  for (const auto d : node.maxDistancesOfPlacedAtomsPerGroup) {
    maxDistanceOfPlacedAtom = std::max(maxDistanceOfPlacedAtom, d);
  }
  float heuristic = maxDistanceOfUnplacedAtom <= maxDistanceOfPlacedAtom
                        ? 0.F
                        : std::sqrt(maxDistanceOfUnplacedAtom) -
                              std::sqrt(maxDistanceOfPlacedAtom);
  heuristic += accMeanLookaheadCost;
  heuristic +=
      deepeningFactor *
      (sumStdDeviationForGroups(scaleFactors, node.groups) + deepeningValue) *
      nUnplacedGates;
  return heuristic;
}

auto HeuristicPlacer::getNeighbors(const std::vector<AtomJob>& atomJobs,
                                   const std::shared_ptr<const AtomNode>& node)
    -> std::vector<std::shared_ptr<const AtomNode>> {
  const size_t atomToBePlacedNext = node->level;
  assert(atomToBePlacedNext < atomJobs.size());
  const auto& atomJob = atomJobs[atomToBePlacedNext];
  std::vector<std::shared_ptr<const AtomNode>> neighbors;
  assert(atomJob.options.size() <= std::numeric_limits<uint16_t>::max());
  for (uint16_t i = 0; i < static_cast<uint16_t>(atomJob.options.size()); ++i) {
    const auto& option = atomJob.options[i];
    const auto& [site, reuse, distance, lookaheadCost] = option;
    // skip the sites that are already consumed
    if (!reuse && node->consumedFreeSites.contains(site)) {
      continue;
    }
    // make a copy of the node, the parent of the child
    AtomNode child = *node; // make a copy
    child.parent = node;
    if (!reuse) {
      child.consumedFreeSites.emplace(site);
      // check whether the current placement is compatible with any existing
      // group
      checkCompatibilityAndAddPlacement(
          atomJob.currentSite.front(), site.front(), atomJob.currentSite.back(),
          site.back(), distance, child.groups,
          child.maxDistancesOfPlacedAtomsPerGroup);
    }
    child.option = i;
    ++child.level;
    child.lookaheadCost += lookaheadCost;
    // add the child to the list of children to be returned
    neighbors.emplace_back(std::make_shared<const AtomNode>(std::move(child)));
  }
  return neighbors;
}

auto HeuristicPlacer::getNeighbors(const std::vector<GateJob>& gateJobs,
                                   const std::shared_ptr<const GateNode>& node)
    -> std::vector<std::shared_ptr<const GateNode>> {
  const size_t gateToBePlacedNext = node->level;
  assert(gateToBePlacedNext < gateJobs.size());
  const auto& gateJob = gateJobs[gateToBePlacedNext];
  std::vector<std::shared_ptr<const GateNode>> neighbors;
  // Get the current placement of the atoms that must be placed next
  const auto& [currentSiteOfLeftAtom, currentSiteOfRightAtom] =
      gateJob.currentSites;
  assert(gateJob.options.size() <= std::numeric_limits<uint16_t>::max());
  for (uint16_t i = 0; i < static_cast<uint16_t>(gateJob.options.size()); ++i) {
    const auto& option = gateJob.options[i];
    const auto& [sites, distances, lookaheadCost] = option;
    const auto& [leftSite, rightSite] = sites;
    // skip if one of the sites is already consumed
    if (node->consumedFreeSites.contains(leftSite) ||
        node->consumedFreeSites.contains(rightSite)) {
      continue;
    }
    // make a copy of the node, the parent of the child as use this as a
    // starting point for the new node
    GateNode child = *node; // make a copy
    child.parent = node;
    ++child.level;
    child.option = i;
    child.consumedFreeSites.emplace(leftSite);
    child.consumedFreeSites.emplace(rightSite);
    // check whether the current placement is compatible with any existing
    // horizontal group
    checkCompatibilityAndAddPlacement(
        currentSiteOfLeftAtom.front(), leftSite.front(),
        currentSiteOfLeftAtom.back(), leftSite.back(), distances.front(),
        child.groups, child.maxDistancesOfPlacedAtomsPerGroup);
    checkCompatibilityAndAddPlacement(
        currentSiteOfRightAtom.front(), rightSite.front(),
        currentSiteOfRightAtom.back(), rightSite.back(), distances.back(),
        child.groups, child.maxDistancesOfPlacedAtomsPerGroup);
    child.lookaheadCost += lookaheadCost;
    // add the final child to the list of children to be returned
    neighbors.emplace_back(std::make_shared<const GateNode>(std::move(child)));
  }
  return neighbors;
}

auto HeuristicPlacer::checkCompatibilityWithGroup(
    const uint8_t key, const uint8_t value,
    const std::map<uint8_t, uint8_t>& group)
    -> std::optional<
        std::pair<std::map<uint8_t, uint8_t>::const_iterator, bool>> {
  if (auto it = group.lower_bound(key); it != group.end()) {
    // an assignment for this key already exists in this group
    if (const auto& [upperKey, upperValue] = *it; upperKey == key) {
      if (upperValue == value) {
        // the new placement is compatible with this group and key already
        // exists
        return std::pair{it, true};
      }
    } else {
      // if (upperKey > key)
      if (it != group.begin()) {
        // it can be safely decremented
        if (const auto lowerValue = std::prev(it)->second;
            lowerValue < value && value < upperValue) {
          // the new placement is compatible with this group
          return std::pair{it, false};
        }
      } else {
        // if (it == hGroup.begin())
        if (value < upperValue) {
          // the new placement is compatible with this group
          return std::pair{it, false};
        }
      }
    }
  } else {
    // if (it == hGroup.end())
    // it can be safely decremented because the group must contain
    // at least one element
    if (const auto lowerValue = std::prev(it)->second; lowerValue < value) {
      // the new placement is compatible with this group
      return std::pair{it, false};
    }
  }
  return std::nullopt;
}

auto HeuristicPlacer::checkCompatibilityAndAddPlacement(
    const uint8_t hKey, const uint8_t hValue, const uint8_t vKey,
    const uint8_t vValue, const float distance,
    std::vector<CompatibilityGroup>& groups, std::vector<float>& maxDistances)
    -> bool {
  size_t i = 0;
  for (auto& group : groups) {
    auto& [hGroup, vGroup] = group;
    if (const auto& hCompatible =
            checkCompatibilityWithGroup(hKey, hValue, hGroup);
        hCompatible) {
      if (const auto& vCompatible =
              checkCompatibilityWithGroup(vKey, vValue, vGroup)) {
        const auto& [hIt, hExists] = *hCompatible;
        const auto& [vIt, vExists] = *vCompatible;
        // the new placement is compatible with this group
        if (!hExists) {
          hGroup.emplace_hint(hIt, hKey, hValue);
        }
        if (!vExists) {
          vGroup.emplace_hint(vIt, vKey, vValue);
        }
        maxDistances[i] = std::max(maxDistances[i], distance);
        return true;
      }
    }
    ++i;
  }
  // no compatible group could be found, and a new group is created
  auto& [hGroup, vGroup] = groups.emplace_back();
  hGroup.emplace(hKey, hValue);
  vGroup.emplace(vKey, vValue);
  maxDistances.emplace_back(distance);
  return false;
}

auto HeuristicPlacer::Config::createWithAStarDefaults() -> Config {
  Config config;
  config.method = Method::ASTAR;
  config.deepeningFactor = 0.8F;
  config.deepeningValue = 0.2F;
  config.lookaheadFactor = 0.2F;
  return config;
}
auto HeuristicPlacer::Config::createWithIDSDefaults() -> Config {
  Config config;
  config.method = Method::IDS;
  config.deepeningFactor = 0.01F;
  config.deepeningValue = 0.0F;
  config.lookaheadFactor = 0.4F;
  return config;
}
auto HeuristicPlacer::Config::createForMethod(const Method method) -> Config {
  return method == Method::ASTAR ? createWithAStarDefaults()
                                 : createWithIDSDefaults();
}
HeuristicPlacer::HeuristicPlacer(const Architecture& architecture,
                                 const Config& config)
    : architecture_(architecture), config_(config) {
  // get first storage SLM and first entanglement SLM
  const auto& firstStorageSLM = *architecture_.get().storageZones.front();
  const auto& firstEntanglementSLM =
      architecture_.get().entanglementZones.front()->front();
  // check which side of the first storage SLM is closer to the entanglement
  // SLM
  if (firstStorageSLM.location.second < firstEntanglementSLM.location.second) {
    // if the entanglement SLM is closer to the last row of the storage, SLM
    // start initial placement of the atoms in the last row instead of the
    // first and hence revert initial placement
    reverseInitialPlacement_ = true;
  }
  windowMinHeight_ = static_cast<size_t>(std::round(
      config_.windowRatio * static_cast<double>(config_.windowMinWidth)));
}

auto HeuristicPlacer::place(
    const size_t nQubits,
    const std::vector<TwoQubitGateLayer>& twoQubitGateLayers,
    const std::vector<std::unordered_set<qc::Qubit>>& reuseQubits)
    -> std::vector<Placement> {
  std::vector<Placement> placement;
  placement.reserve((2 * twoQubitGateLayers.size()) + 1);
  placement.emplace_back(makeInitialPlacement(nQubits));
  for (size_t layer = 0; layer < twoQubitGateLayers.size(); ++layer) {
    const auto& [gatePlacement, qubitPlacement] = makeIntermediatePlacement(
        placement.back(),
        layer == 0 ? std::unordered_set<qc::Qubit>{} : reuseQubits[layer - 1],
        layer == reuseQubits.size() ? std::unordered_set<qc::Qubit>{}
                                    : reuseQubits[layer],
        twoQubitGateLayers[layer],
        layer == twoQubitGateLayers.size() - 1 ? TwoQubitGateLayer{}
                                               : twoQubitGateLayers[layer + 1]);
    placement.emplace_back(gatePlacement);
    placement.emplace_back(qubitPlacement);
    SPDLOG_DEBUG("Placed layer: {}", layer);
  }
  return placement;
}
} // namespace na::zoned
