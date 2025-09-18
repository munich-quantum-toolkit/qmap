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
auto CodeGenerator::appendRearrangement(
    const Placement& startPlacement, const Routing& routing,
    const Placement& targetPlacement,
    const std::vector<std::reference_wrapper<const Atom>>& atoms,
    NAComputation& code) const -> void {
  for (const auto& qubits : routing) {
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
