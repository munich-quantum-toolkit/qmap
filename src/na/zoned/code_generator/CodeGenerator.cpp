/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "na/zoned/code_generator/CodeGenerator.hpp"

#include "ir/Definitions.hpp"
#include "ir/operations/CompoundOperation.hpp"
#include "ir/operations/OpType.hpp"
#include "ir/operations/Operation.hpp"
#include "na/NAComputation.hpp"
#include "na/entities/Atom.hpp"
#include "na/entities/Location.hpp"
#include "na/entities/Zone.hpp"
#include "na/operations/GlobalCZOp.hpp"
#include "na/operations/GlobalRYOp.hpp"
#include "na/operations/LoadOp.hpp"
#include "na/operations/LocalRZOp.hpp"
#include "na/operations/LocalUOp.hpp"
#include "na/operations/MoveOp.hpp"
#include "na/operations/StoreOp.hpp"
#include "na/zoned/Architecture.hpp"
#include "na/zoned/Types.hpp"

#include <cassert>
#include <cstddef>
#include <iterator>
#include <map>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace na::zoned {
auto CodeGenerator::appendSingleQubitGates(
    const size_t nQubits, const SingleQubitGateLayer& singleQubitGates,
    const std::vector<std::reference_wrapper<const Atom>>& atoms,
    const Zone& globalZone, NAComputation& code) const -> void {
  for (const auto& op : singleQubitGates) {
    // A flag to indicate if the gate is a gate on one qubit.
    // This flag is used for circuit consisting of only one qubit since in this
    // case, global and local gates are the same.
    bool singleQubitGate = false;
    if (op.get().isGlobal(nQubits)) {
      // a global operation can be wrapped in a compound operation or a standard
      // operation acting on all qubits
      if (op.get().isCompoundOperation()) {
        const auto& compOp =
            dynamic_cast<const qc::CompoundOperation&>(op.get());
        const auto opType = compOp.front()->getType();
        if (opType == qc::RY) {
          code.emplaceBack<GlobalRYOp>(globalZone,
                                       compOp.front()->getParameter().front());
        } else if (opType == qc::Y) {
          code.emplaceBack<GlobalRYOp>(globalZone, qc::PI);
        } else {
          // this case should never occur since the scheduler should filter out
          // other global gates that are not supported already.
          assert(false);
        }
      } else {
        const auto opType = op.get().getType();
        if (opType == qc::RY) {
          code.emplaceBack<GlobalRYOp>(globalZone,
                                       op.get().getParameter().front());
        } else if (opType == qc::Y) {
          code.emplaceBack<GlobalRYOp>(globalZone, qc::PI);
        } else if (nQubits == 1) {
          // special case for one qubit, fall through to local gates
          singleQubitGate = true;
        } else {
          // this case should never occur since the scheduler should filter out
          // other global gates that are not supported already.
          assert(false);
        }
      }
    } else {
      // if a gate is not global, it is assumed to be a local gate.
      singleQubitGate = true;
    }
    if (singleQubitGate) {
      // one qubit gates act exactly on one qubit and are converted to local
      // gates
      assert(op.get().getNqubits() == 1);
      const qc::Qubit qubit = op.get().getTargets().front();
      // By default, all variants of rotational z-gates are supported
      if (op.get().getType() == qc::RZ) {
        code.emplaceBack<LocalRZOp>(atoms[qubit],
                                    op.get().getParameter().front());
      } else if (op.get().getType() == qc::Z) {
        code.emplaceBack<LocalRZOp>(atoms[qubit], qc::PI);
      } else if (op.get().getType() == qc::S) {
        code.emplaceBack<LocalRZOp>(atoms[qubit], qc::PI_2);
      } else if (op.get().getType() == qc::Sdg) {
        code.emplaceBack<LocalRZOp>(atoms[qubit], -qc::PI_2);
      } else if (op.get().getType() == qc::T) {
        code.emplaceBack<LocalRZOp>(atoms[qubit], qc::PI_4);
      } else if (op.get().getType() == qc::Tdg) {
        code.emplaceBack<LocalRZOp>(atoms[qubit], -qc::PI_4);
      } else if (op.get().getType() == qc::P) {
        code.emplaceBack<LocalRZOp>(atoms[qubit],
                                    op.get().getParameter().front());
      } else {
        // in this case, the gate is not any variant of a rotational z-gate.
        // depending on the settings, a warning is printed.
        if (config_.warnUnsupportedGates) {
          SPDLOG_WARN(
              "Gate not part of basis gates will be inserted as U3 gate: {}",
              qc::toString(op.get().getType()));
        }
        if (op.get().getType() == qc::U) {
          code.emplaceBack<LocalUOp>(
              atoms[qubit], op.get().getParameter().front(),
              op.get().getParameter().at(1), op.get().getParameter().at(2));
        } else if (op.get().getType() == qc::U2) {
          code.emplaceBack<LocalUOp>(atoms[qubit], qc::PI_2,
                                     op.get().getParameter().front(),
                                     op.get().getParameter().at(1));
        } else if (op.get().getType() == qc::RX) {
          code.emplaceBack<LocalUOp>(atoms[qubit],
                                     op.get().getParameter().front(), -qc::PI_2,
                                     qc::PI_2);
        } else if (op.get().getType() == qc::RY) {
          code.emplaceBack<LocalUOp>(atoms[qubit],
                                     op.get().getParameter().front(), 0, 0);
        } else if (op.get().getType() == qc::H) {
          code.emplaceBack<LocalUOp>(atoms[qubit], qc::PI_2, 0, qc::PI);
        } else if (op.get().getType() == qc::X) {
          code.emplaceBack<LocalUOp>(atoms[qubit], qc::PI, 0, qc::PI);
        } else if (op.get().getType() == qc::Y) {
          code.emplaceBack<LocalUOp>(atoms[qubit], qc::PI, qc::PI_2, qc::PI_2);
        } else if (op.get().getType() == qc::V) {
          code.emplaceBack<LocalUOp>(atoms[qubit], -qc::PI_2, -qc::PI_2,
                                     qc::PI_2);
        } else if (op.get().getType() == qc::Vdg) {
          code.emplaceBack<LocalUOp>(atoms[qubit], -qc::PI_2, qc::PI_2,
                                     -qc::PI_2);
        } else if (op.get().getType() == qc::SX) {
          code.emplaceBack<LocalUOp>(atoms[qubit], qc::PI_2, -qc::PI_2,
                                     qc::PI_2);
        } else if (op.get().getType() == qc::SXdg) {
          code.emplaceBack<LocalUOp>(atoms[qubit], -qc::PI_2, -qc::PI_2,
                                     qc::PI_2);
        } else {
          // if the gate type is not recognized, an error is printed and the
          // gate is not included in the output.
          std::ostringstream oss;
          oss << "\033[1;31m[ERROR]\033[0m Unsupported single-qubit gate: "
              << op.get().getType() << "\n";
          throw std::invalid_argument(oss.str());
        }
      }
    }
  }
}
auto CodeGenerator::appendTwoQubitGates(
    const Placement& currentPlacement, const Routing& executionRouting,
    const Placement& executionPlacement, const Routing& targetRouting,
    const Placement& targetPlacement,
    const std::vector<std::reference_wrapper<const Atom>>& atoms,
    const std::vector<std::reference_wrapper<const Zone>>& zones,
    NAComputation& code) const -> void {
  appendRearrangement(currentPlacement, executionRouting, executionPlacement,
                      atoms, code);
  std::vector<const Zone*> zonePtrs;
  zonePtrs.reserve(zones.size());
  std::transform(zones.begin(), zones.end(), std::back_inserter(zonePtrs),
                 [](const auto& zone) { return &zone.get(); });
  code.emplaceBack<GlobalCZOp>(zonePtrs);
  appendRearrangement(executionPlacement, targetRouting, targetPlacement, atoms,
                      code);
}
namespace {
auto enumerate(const auto& data) {
  return data | std::views::transform([i = 0](const auto& value) mutable {
           return std::make_pair(i++, value);
         });
}
} // namespace
auto CodeGenerator::appendRearrangement(
    const Placement& startPlacement, const Routing& routing,
    const Placement& targetPlacement,
    const std::vector<std::reference_wrapper<const Atom>>& atoms,
    NAComputation& code) const -> void {
  for (const auto& qubits : routing) {
    // Map collecting all atoms that must be loaded within each source row (y-
    // coordinate). It is intentionally an 'ordered' map to save the sorting
    // afterward.
    std::map<size_t, std::vector<qc::Qubit>> yToQubitsToBeLoaded;
    // Map collecting all atoms that must be stored within each target column
    // (x-coordinate). It is intentionally an 'ordered' map to save the sorting
    // afterward.
    std::map<size_t, std::vector<qc::Qubit>> xToQubitsToBeStored;
    // Since rows cannot split, this map collects the start (key) and end
    // (value) y-position of each row that must be moved.
    std::unordered_map<size_t, size_t> revVerticalMoves;
    // Since columns cannot split, this map collects the start (key) and end
    // (value) x-position of each column that must be moved.
    std::unordered_map<size_t, size_t> horizontalMoves;

    // We assume that all qubits to be loaded are in the same zone. We extract
    // the vertical separation of the zone from the first qubit's zone.
    const auto startDy =
        std::get<0>(startPlacement.front()).get().siteSeparation.second;
    const auto startMaxY = startDy * (std::get<0>(startPlacement.front()).get().nRows);
    // We do the same for the target zone
    const auto targetDy =
        std::get<0>(targetPlacement[0]).get().siteSeparation.second;
    const auto targetMaxY = startDy * (std::get<0>(startPlacement.front()).get().nRows);

    for (const auto qubit : qubits) {
      // get the current location of the qubit
      const auto& [currentSlm, currentR, currentC] = startPlacement[qubit];
      const auto& [currentX, currentY] =
          architecture_.get().exactSLMLocation(currentSlm, currentR, currentC);
      yToQubitsToBeLoaded.try_emplace(currentY).first->second.emplace_back(
          qubit);
      // get the target location of the qubit
      const auto& [targetSLM, targetR, targetC] = targetPlacement[qubit];
      const auto& [targetX, targetY] =
          architecture_.get().exactSLMLocation(targetSLM, targetR, targetC);
      xToQubitsToBeStored.try_emplace(targetX).first->second.emplace_back(
          qubit);
      // record the moves
      const auto verticalIt =
          revVerticalMoves.try_emplace(targetY, currentY).first;
      // If this does not hold, the input was invalid for this generator.
      // More precisely, this conditional assert ensures that rows do not split.
      assert(verticalIt->second == currentY);
      const auto& horizontalIt =
          horizontalMoves.try_emplace(currentX, targetX).first;
      // If this does not hold, the input was invalid for this generator.
      // More precisely, this conditional assert ensures that columns do not
      // split.
      assert(horizontalIt->second == targetX);
    }

    // A map from the source y-coordinate of the row to the AOD row that will
    // load the atoms in this row.
    std::unordered_map<size_t, size_t> yToAodRow;
    for (const auto& [aodRow, revMove] : enumerate(revVerticalMoves)) {
      yToAodRow.emplace(revMove.second, aodRow);
    }
    // A map from the target x-coordinate of the column to the AOD column that
    // will store the atoms in this column.
    std::unordered_map<size_t, size_t> xToAodCol;
    for (const auto& [aodCol, move] : enumerate(horizontalMoves)) {
      xToAodCol.emplace(move.second, aodCol);
    }

    // A map from activated AOD columns to their current x-coordinate. This is
    // intentionally an 'ordered' map to ease the pushing of activated columns.
    std::map<size_t, size_t> aodCols;
    // A map from activated AOD rows to their current y-coordinate. This is
    // intentionally an 'ordered' map to ease the pushing of activated rows.
    std::map<size_t, size_t> aodRows;
    // A map from already loaded qubits to their current location.
    std::unordered_map<qc::Qubit, std::pair<size_t, size_t>>
        loadedQubitsToTheirLocation;

    // Load the atoms row-wise
    for (const auto& [currentY, qubitsToLoad] : yToQubitsToBeLoaded) {
      // Get the AOD row to load the atoms in this row.
      assert(yToAodRow.find(currentY) != yToAodRow.end());
      const auto newAodRow = yToAodRow[currentY];
      const auto it = aodRows.emplace(newAodRow, currentY).first;
      // Push already activated rows away if necessary.
      auto nextY = currentY - (startDy / 2);
      for (auto lowerIt = std::next(std::make_reverse_iterator(it));
           lowerIt != aodRows.crend() && lowerIt->second > nextY; ++lowerIt) {
        lowerIt->second = nextY;
        nextY -= nextY > 0 ? startDy : startDy / 2;
      }
      for (auto upperIt = std::next(it);
           upperIt != aodRows.cend() && upperIt->second < nextY; ++upperIt) {
        upperIt->second = nextY;
        nextY += nextY < startMaxY ? startDy : startDy / 2;
      }
    }

    std::map<size_t, std::map<size_t, qc::Qubit>> rowsWithQubits;
    std::vector<const Atom*> atomsToMove;
    std::vector<Location> targetLocations;
    for (const auto& qubit : qubits) {
      // get the current location of the qubit
      const auto& [slm, r, c] = startPlacement[qubit];
      const auto& [x, y] = architecture_.get().exactSLMLocation(slm, r, c);
      rowsWithQubits.try_emplace(y).first->second.emplace(x, qubit);
      atomsToMove.emplace_back(&atoms[qubit].get());
      // get the target location of the qubit
      const auto& [targetSLM, targetR, targetC] = targetPlacement[qubit];
      const auto& [targetX, targetY] =
          architecture_.get().exactSLMLocation(targetSLM, targetR, targetC);
      targetLocations.emplace_back(
          Location{static_cast<double>(targetX), static_cast<double>(targetY)});
    }
    std::vector<std::pair<qc::Qubit, std::pair<size_t, size_t>>>
        alreadyLoadedQubits;
    const auto& [minY, firstRow] = *rowsWithQubits.cbegin();
    std::vector<const Atom*> firstAtomsToLoad;
    firstAtomsToLoad.reserve(firstRow.size());
    for (const auto& [x, qubit] : firstRow) {
      alreadyLoadedQubits.emplace_back(qubit, std::pair{x, minY});
      firstAtomsToLoad.emplace_back(&atoms[qubit].get());
    }
    code.emplaceBack<LoadOp>(firstAtomsToLoad);
    // if there are more than one row with atoms to move, we pick them up
    // row-by-row as a simple strategy to avoid ghost-spots
    for (auto it = std::next(rowsWithQubits.cbegin());
         it != rowsWithQubits.cend(); ++it) {
      const auto& [yCoordinateOfRow, row] = *it;
      // perform an offset move to avoid ghost-spots
      std::vector<const Atom*> atomsToOffset;
      std::vector<Location> offsetTargetLocations;
      atomsToOffset.reserve(alreadyLoadedQubits.size());
      offsetTargetLocations.reserve(alreadyLoadedQubits.size());
      for (const auto& [qubit, location] : alreadyLoadedQubits) {
        atomsToOffset.emplace_back(&atoms[qubit].get());
        const auto& [x, y] = location;
        if (row.find(x) != row.end()) {
          // new atoms get picked up in the column at x, i.e., only do a
          // vertical offset
          offsetTargetLocations.emplace_back(
              Location{static_cast<double>(x),
                       static_cast<double>(y + config_.parkingOffset)});
        } else {
          // no new atoms get picked up in the column at x, i.e., do a
          // diagonal offset to avoid any ghost-spots
          offsetTargetLocations.emplace_back(
              Location{static_cast<double>(x + config_.parkingOffset),
                       static_cast<double>(y + config_.parkingOffset)});
        }
      }
      code.emplaceBack<MoveOp>(atomsToOffset, offsetTargetLocations);
      // load the new atoms
      std::vector<const Atom*> atomsToLoad;
      atomsToLoad.reserve(row.size());
      for (const auto& [x, qubit] : row) {
        alreadyLoadedQubits.emplace_back(qubit, std::pair{x, yCoordinateOfRow});
        atomsToLoad.emplace_back(&atoms[qubit].get());
      }
      code.emplaceBack<LoadOp>(atomsToLoad);
    }
    // all atoms are loaded, now move them to their target locations
    code.emplaceBack<MoveOp>(atomsToMove, targetLocations);
    code.emplaceBack<StoreOp>(atomsToMove);
  }
}
auto CodeGenerator::generate(
    const std::vector<SingleQubitGateLayer>& singleQubitGateLayers,
    const std::vector<Placement>& placement,
    const std::vector<Routing>& routing) const -> NAComputation {
  NAComputation code;
  std::vector<std::reference_wrapper<const Zone>> rydbergZones;
  for (size_t i = 0; i < architecture_.get().rydbergRangeMinX.size(); ++i) {
    rydbergZones.emplace_back(code.emplaceBackZone(
        "zone_cz" + std::to_string(i),
        Zone::Extent{
            static_cast<double>(architecture_.get().rydbergRangeMinX.at(i)),
            static_cast<double>(architecture_.get().rydbergRangeMinY.at(i)),
            static_cast<double>(architecture_.get().rydbergRangeMaxX.at(i)),
            static_cast<double>(architecture_.get().rydbergRangeMaxY.at(i))}));
  }
  size_t minX = std::numeric_limits<size_t>::max();
  size_t maxX = std::numeric_limits<size_t>::min();
  size_t minY = std::numeric_limits<size_t>::max();
  size_t maxY = std::numeric_limits<size_t>::min();
  for (const auto& zone : architecture_.get().storageZones) {
    minX = std::min(minX, zone->location.first);
    maxX = std::max(maxX, zone->location.first +
                              zone->siteSeparation.first * zone->nCols);
    minY = std::min(minY, zone->location.second);
    maxY = std::max(maxY, zone->location.second +
                              zone->siteSeparation.second * zone->nRows);
  }
  const auto& globalZone = code.emplaceBackZone(
      "global",
      Zone::Extent{static_cast<double>(minX), static_cast<double>(minY),
                   static_cast<double>(maxX), static_cast<double>(maxY)});
  const auto& initialPlacement = placement.front();
  std::vector<std::reference_wrapper<const Atom>> atoms;
  atoms.reserve(initialPlacement.size());
  for (const auto& [slm, r, c] : initialPlacement) {
    atoms.emplace_back(
        code.emplaceBackAtom("atom" + std::to_string(atoms.size())));
    const auto& [x, y] = architecture_.get().exactSLMLocation(slm, r, c);
    code.emplaceInitialLocation(atoms.back(), x, y);
  }
  // early return if no single-qubit gates are given
  if (singleQubitGateLayers.empty()) {
    return code;
  }
  assert(2 * singleQubitGateLayers.size() == placement.size() + 1);
  assert(placement.size() == routing.size() + 1);
  appendSingleQubitGates(atoms.size(), singleQubitGateLayers.front(), atoms,
                         globalZone, code);
  for (size_t layer = 0; layer + 1 < singleQubitGateLayers.size(); ++layer) {
    appendTwoQubitGates(placement[2 * layer], routing[2 * layer],
                        placement[(2 * layer) + 1], routing[(2 * layer) + 1],
                        placement[2 * (layer + 1)], atoms, rydbergZones, code);
    appendSingleQubitGates(atoms.size(), singleQubitGateLayers[layer + 1],
                           atoms, globalZone, code);
  }
  return code;
}
} // namespace na::zoned
