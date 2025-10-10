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
[[nodiscard]] auto enumerate(const auto& data) {
  return data | std::views::transform([i = 0UL](const auto& value) mutable {
           return std::pair{i++, value};
         });
}
} // namespace
auto CodeGenerator::appendRearrangement(
    const Placement& startPlacement, const Routing& routing,
    const Placement& targetPlacement,
    const std::vector<std::reference_wrapper<const Atom>>& atoms,
    NAComputation& code) const -> void {
  for (const auto& qubits : routing) {
    RearrangementGenerator rearrangementGenerator(
        architecture_.get(), startPlacement, targetPlacement, qubits);
    rearrangementGenerator.generate(atoms, code);
  }
}
auto CodeGenerator::RearrangementGenerator::getLocationFromSite(
    const Site& site) -> std::pair<int64_t, int64_t> {
  const auto& [slm, r, c] = site;
  const auto& [x, y] = architecture_.get().exactSLMLocation(slm, r, c);
  return {static_cast<int64_t>(x), static_cast<int64_t>(y)};
}
auto CodeGenerator::RearrangementGenerator::getSiteKindFromSite(
    const Site& site) -> QubitMovement::SiteKind {
  const auto& slm = std::get<0>(site).get();
  if (slm.isStorage()) {
    return QubitMovement::SiteKind::STORAGE;
  }
  if (slm.entanglementZone_->front().location.first <
      slm.entanglementZone_->back().location.first) {
    if (slm == slm.entanglementZone_->front()) {
      return QubitMovement::SiteKind::ENTANGLEMENT_LEFT;
    }
    return QubitMovement::SiteKind::ENTANGLEMENT_RIGHT;
  }
  if (slm == slm.entanglementZone_->back()) {
    return QubitMovement::SiteKind::ENTANGLEMENT_LEFT;
  }
  return QubitMovement::SiteKind::ENTANGLEMENT_RIGHT;
}
auto CodeGenerator::RearrangementGenerator::loadRowByRow(
    const std::vector<std::reference_wrapper<const Atom>>& atoms,
    NAComputation& code) -> void {
  // Map collecting all atoms that must be loaded within each source row
  // (y-coordinate). It is intentionally an 'ordered' map to save the sorting
  // afterward. This set is intentionally an 'ordered' map to ensure
  // deterministic (ordered atoms) code generation.
  std::map<int64_t, std::set<qc::Qubit>> yToQubitsToBeLoaded;

  for (const auto& [qubit, movement] : movements_) {
    // record the qubits in each row to be loaded
    yToQubitsToBeLoaded.try_emplace(movement.sourceY)
        .first->second.emplace(qubit);
  }

  // Since rows cannot split, this map collects the end (key) and start
  // (value) y-position of each row that must be moved. It is intentionally an
  // 'ordered' map to save the sorting afterward.
  std::map<int64_t, int64_t> revVerticalMoves;
  for (const auto& [k, v] : verticalMoves_) {
    revVerticalMoves.emplace(v, k);
  }

  // A map from the source y-coordinate of the row to the AOD row that will
  // load the atoms in this row. Here it is important that the moves are
  // sorted by their final y-coordinate.
  std::unordered_map<int64_t, size_t> sourceYToAodRow;
  for (const auto& [aodRow, revMove] : enumerate(revVerticalMoves)) {
    sourceYToAodRow.emplace(revMove.second, aodRow);
  }
  // A map from the source x-coordinate of the column to the AOD column that
  // will load the atoms in this column.
  std::unordered_map<int64_t, size_t> sourceXToAodCol;
  for (const auto& [aodCol, x] :
       enumerate(horizontalMoves_ | std::views::keys)) {
    sourceXToAodCol.emplace(x, aodCol);
  }

  // Load the atoms row-wise
  for (const auto& [sourceY, qubitsToLoad] : yToQubitsToBeLoaded) {
    // Get the AOD row to load the atoms in this row.
    assert(sourceYToAodRow.contains(sourceY));
    const auto newAodRow = sourceYToAodRow[sourceY];
    // already include a virtual offset move by `startD / 2`
    const auto it =
        aodRowsToY_.emplace(newAodRow, sourceY + sourceDy_ / 2).first;
    // Push already activated rows away if necessary.
    auto nextY = sourceY - (sourceDy_ / 2);
    for (auto lowerIt = std::make_reverse_iterator(it);
         lowerIt != aodRowsToY_.rend() && lowerIt->second > nextY; ++lowerIt) {
      lowerIt->second = nextY;
      nextY -= nextY > 0 ? sourceDy_ : sourceDy_ / 2;
    }
    nextY = sourceY + sourceDy_ + (sourceDy_ / 2);
    for (auto upperIt = std::next(it);
         upperIt != aodRowsToY_.end() && upperIt->second < nextY; ++upperIt) {
      upperIt->second = nextY;
      nextY += nextY < sourceMaxY_ ? sourceDy_ : sourceDy_ / 2;
    }
    // Align aod columns
    for (const auto qubit : qubitsToLoad) {
      const auto& qubitMovement = movements_.at(qubit);
      const auto aodCol = sourceXToAodCol.at(qubitMovement.sourceX);
      aodColsToX_[aodCol] = qubitMovement.sourceX;
    }
    // Write out offset move before loading new atoms
    std::vector<const Atom*> atomsToOffset;
    std::vector<Location> offsetTargetLocations;
    for (auto& [qubit, location] : shuttlingQubitToCurrentLocation_) {
      const auto& qubitMovement = movements_.at(qubit);
      const std::pair newLocation{
          aodColsToX_.at(sourceXToAodCol.at(qubitMovement.sourceX)),
          aodRowsToY_.at(sourceYToAodRow.at(qubitMovement.sourceY))};
      if (location != newLocation) {
        atomsToOffset.emplace_back(&atoms[qubit].get());
        offsetTargetLocations.emplace_back(
            static_cast<double>(newLocation.first),
            static_cast<double>(newLocation.second));
        location = newLocation;
      }
    }
    if (!atomsToOffset.empty()) {
      code.emplaceBack<MoveOp>(atomsToOffset, offsetTargetLocations);
    }
    // Load new atoms
    std::vector<const Atom*> atomsToLoad;
    for (const auto& qubit : qubitsToLoad) {
      atomsToLoad.emplace_back(&atoms[qubit].get());
      const auto& qubitMovement = movements_.at(qubit);
      shuttlingQubitToCurrentLocation_.emplace(
          qubit, std::pair{qubitMovement.sourceX, qubitMovement.sourceY});
      // Make a virtual offset of columns with new atoms
      const auto aodCol = sourceXToAodCol.at(qubitMovement.sourceX);
      if (qubitMovement.sourceSite == QubitMovement::SiteKind::STORAGE) {
        aodColsToX_[aodCol] = qubitMovement.sourceX + sourceDx_ / 2;
      } else {
        if (qubitMovement.sourceSite ==
            QubitMovement::SiteKind::ENTANGLEMENT_LEFT) {
          aodColsToX_[aodCol] = qubitMovement.sourceX - sourceDx_ / 4;
        } else {
          aodColsToX_[aodCol] = qubitMovement.sourceX + sourceDx_ / 4;
        }
      }
    }
    code.emplaceBack<LoadOp>(atomsToLoad);
  }
}
auto CodeGenerator::RearrangementGenerator::loadColumnByColumn(
    const std::vector<std::reference_wrapper<const Atom>>& atoms,
    NAComputation& code) -> void {
  // Map collecting all atoms that must be loaded within each source column
  // (x-coordinate). It is intentionally an 'ordered' map to save the sorting
  // afterward. This set is intentionally an 'ordered' map to ensure
  // deterministic (ordered atoms) code generation.
  std::map<int64_t, std::set<qc::Qubit>> xToQubitsToBeLoaded;

  for (const auto& [qubit, movement] : movements_) {
    // record the qubits in each column to be loaded
    xToQubitsToBeLoaded.try_emplace(movement.sourceX)
        .first->second.emplace(qubit);
  }

  // Since columns cannot split, this map collects the end (key) and start
  // (value) x-position of each column that must be moved. It is intentionally
  // an 'ordered' map to save the sorting afterward.
  std::map<int64_t, int64_t> revHorizontalMoves;
  for (const auto& [k, v] : horizontalMoves_) {
    revHorizontalMoves.emplace(v, k);
  }

  // A map from the source x-coordinate of the column to the AOD column that
  // will load the atoms in this column. Here it is important that the moves are
  // sorted by their final x-coordinate.
  std::unordered_map<int64_t, size_t> sourceXToAodCol;
  for (const auto& [aodCol, revMove] : enumerate(revHorizontalMoves)) {
    sourceXToAodCol.emplace(revMove.second, aodCol);
  }
  // A map from the source y-coordinate of the row to the AOD row that
  // will load the atoms in this column.
  std::unordered_map<int64_t, size_t> sourceYToAodRow;
  for (const auto& [aodRow, y] : enumerate(verticalMoves_ | std::views::keys)) {
    sourceYToAodRow.emplace(y, aodRow);
  }

  // Load the atoms column-wise
  for (const auto& [sourceX, qubitsToLoad] : xToQubitsToBeLoaded) {
    // Get the AOD column to load the atoms in this column.
    assert(sourceXToAodCol.contains(sourceX));
    const auto newAodCol = sourceXToAodCol[sourceX];
    if (const auto columnKind = movements_.at(*qubitsToLoad.begin()).sourceSite;
        columnKind == QubitMovement::SiteKind::STORAGE) {
      // already include a virtual offset move by `startDx / 2`
      const auto it =
          aodColsToX_.emplace(newAodCol, sourceX + sourceDx_ / 2).first;
      // Push already activated columns away if necessary.
      auto nextX = sourceX - (sourceDx_ / 2);
      for (auto lowerIt = std::make_reverse_iterator(it);
           lowerIt != aodColsToX_.rend() && lowerIt->second > nextX;
           ++lowerIt) {
        lowerIt->second = nextX;
        nextX -= nextX > 0 ? sourceDx_ : sourceDx_ / 2;
      }
      nextX = sourceX + sourceDx_ + (sourceDx_ / 2);
      for (auto upperIt = std::next(it);
           upperIt != aodColsToX_.end() && upperIt->second < nextX;
           ++upperIt) {
        upperIt->second = nextX;
        nextX += nextX < sourceMaxX_ ? sourceDx_ : sourceDx_ / 2;
      }
    } else if (columnKind == QubitMovement::SiteKind::ENTANGLEMENT_LEFT) {
      // already include a virtual offset move by `startDx / 2`
      const auto it =
          aodColsToX_.emplace(newAodCol, sourceX - sourceDx_ / 4).first;
      // Push already activated columns away if necessary.
      bool odd = true;
      auto nextX = sourceX - (sourceDx_ / 2);
      for (auto lowerIt = std::make_reverse_iterator(it);
           lowerIt != aodColsToX_.rend() && lowerIt->second > nextX;
           ++lowerIt) {
        lowerIt->second = nextX;
        nextX -= nextX > 0 || odd ? sourceDx_ - sourceDx_ / 4 : sourceDx_ / 4;
        odd = !odd;
      }
      odd = false;
      nextX = sourceX + sourceDx_ - (sourceDx_ / 4);
      for (auto upperIt = std::next(it);
           upperIt != aodColsToX_.end() && upperIt->second < nextX;
           ++upperIt) {
        upperIt->second = nextX;
        nextX += nextX < sourceMaxX_ || odd ? sourceDx_ - sourceDx_ / 4
                                            : sourceDx_ / 4;
        odd = !odd;
      }
    } else {
      // already include a virtual offset move by `startDx / 2`
      const auto it =
          aodColsToX_.emplace(newAodCol, sourceX + sourceDx_ / 4).first;
      // Push already activated columns away if necessary.
      bool odd = false;
      auto nextX = sourceX - sourceDx_ + (sourceDx_ / 4);
      for (auto lowerIt = std::make_reverse_iterator(it);
           lowerIt != aodColsToX_.rend() && lowerIt->second > nextX;
           ++lowerIt) {
        lowerIt->second = nextX;
        nextX -= nextX > 0 || odd ? sourceDx_ - sourceDx_ / 4 : sourceDx_ / 4;
        odd = !odd;
      }
      odd = true;
      nextX = sourceX + (sourceDx_ / 2);
      for (auto upperIt = std::next(it);
           upperIt != aodColsToX_.end() && upperIt->second < nextX;
           ++upperIt) {
        upperIt->second = nextX;
        nextX += nextX < sourceMaxX_ || odd ? sourceDx_ - sourceDx_ / 4
                                            : sourceDx_ / 4;
        odd = !odd;
      }
    }
    // Align aod rows
    for (const auto qubit : qubitsToLoad) {
      const auto& qubitMovement = movements_.at(qubit);
      const auto aodRow = sourceYToAodRow.at(qubitMovement.sourceY);
      aodRowsToY_[aodRow] = qubitMovement.sourceY;
    }
    // Write out offset move before loading new atoms
    std::vector<const Atom*> atomsToOffset;
    std::vector<Location> offsetTargetLocations;
    for (auto& [qubit, location] : shuttlingQubitToCurrentLocation_) {
      const auto& qubitMovement = movements_.at(qubit);
      const std::pair newLocation{
          aodColsToX_.at(sourceXToAodCol.at(qubitMovement.sourceX)),
          aodRowsToY_.at(sourceYToAodRow.at(qubitMovement.sourceY))};
      if (location != newLocation) {
        atomsToOffset.emplace_back(&atoms[qubit].get());
        offsetTargetLocations.emplace_back(
            static_cast<double>(newLocation.first),
            static_cast<double>(newLocation.second));
        location = newLocation;
      }
    }
    if (!atomsToOffset.empty()) {
      code.emplaceBack<MoveOp>(atomsToOffset, offsetTargetLocations);
    }
    // Load new atoms
    std::vector<const Atom*> atomsToLoad;
    for (const auto& qubit : qubitsToLoad) {
      atomsToLoad.emplace_back(&atoms[qubit].get());
      const auto& qubitMovement = movements_.at(qubit);
      shuttlingQubitToCurrentLocation_.emplace(
          qubit, std::pair{qubitMovement.sourceX, qubitMovement.sourceY});
      // Make a virtual offset of rows with new atoms
      const auto aodRow = sourceYToAodRow.at(qubitMovement.sourceY);
      aodRowsToY_[aodRow] = qubitMovement.sourceY + sourceDy_ / 2;
    }
    code.emplaceBack<LoadOp>(atomsToLoad);
  }
}
auto CodeGenerator::RearrangementGenerator::storeRowByRow(
    const std::vector<std::reference_wrapper<const Atom>>& atoms,
    NAComputation& code) -> void {
  // A map from the target y-coordinate of the row to the AOD row that
  // will store the atoms in this row. Here it is important that the moves
  // are sorted by their initial y-coordinate.
  std::unordered_map<int64_t, size_t> targetYToAodRow;
  for (const auto& [aodRow, move] : enumerate(verticalMoves_)) {
    targetYToAodRow.emplace(move.second, aodRow);
  }
  // A set of target x-coordinate of the columns to the AOD columns that
  // will store the atoms in this column
  std::set<int64_t> targetXs;
  for (const auto x : horizontalMoves_ | std::views::values) {
    targetXs.emplace(x);
  }
  std::unordered_map<int64_t, size_t> targetXToAodCol;
  for (const auto& [aodCol, x] : enumerate(targetXs)) {
    targetXToAodCol.emplace(x, aodCol);
  }

  // Map collecting all atoms that must be stored within each target row
  // (y-coordinate). It is intentionally an 'ordered' map to save the sorting
  // afterward. This set is intentionally an 'ordered' map to ensure
  // deterministic (ordered atoms) code generation.
  std::map<int64_t, std::set<qc::Qubit>> yToQubitsToBeStored;

  for (const auto& [qubit, movement] : movements_) {
    // record the qubits in each column to be loaded
    yToQubitsToBeStored.try_emplace(movement.targetY)
        .first->second.emplace(qubit);
    // Make a virtual move of all columns to their target x-coordinates
    const auto x = movement.targetX;
    const auto aodCol = targetXToAodCol.at(x);
    if (movement.targetSite == QubitMovement::SiteKind::STORAGE) {
      aodColsToX_[aodCol] = x + targetDx_ / 2;
    } else {
      if (movement.targetSite == QubitMovement::SiteKind::ENTANGLEMENT_LEFT) {
        aodColsToX_[aodCol] = x - targetDx_ / 4;
      } else {
        aodColsToX_[aodCol] = x + targetDx_ / 4;
      }
    }
  }

  for (const auto& [targetY, qubitsToStore] : yToQubitsToBeStored) {
    // Get the AOD column to store the atom from
    assert(targetYToAodRow.contains(targetY));
    const auto oldAodRow = targetYToAodRow[targetY];
    const auto it = aodRowsToY_.find(oldAodRow);
    assert(it != aodRowsToY_.end());
    it->second = targetY;
    // Push still activated columns away if necessary
    auto nextY = targetY - (targetDy_ / 2);
    for (auto lowerIt = std::make_reverse_iterator(it);
         lowerIt != aodRowsToY_.rend() && lowerIt->second > nextY; ++lowerIt) {
      lowerIt->second = nextY;
      nextY -= nextY > 0 ? targetDy_ : targetDy_ / 2;
    }
    nextY = targetY + targetDy_ + (targetDy_ / 2);
    for (auto upperIt = std::next(it);
         upperIt != aodRowsToY_.end() && upperIt->second < nextY; ++upperIt) {
      upperIt->second = nextY;
      nextY += nextY < targetMaxY_ ? targetDy_ : targetDy_ / 2;
    }
    // Align aod columns
    for (const auto qubit : qubitsToStore) {
      const auto& qubitMovement = movements_.at(qubit);
      const auto aodCol = targetXToAodCol.at(qubitMovement.targetX);
      aodColsToX_[aodCol] = qubitMovement.targetX;
    }
    std::vector<const Atom*> atomsToOffset;
    std::vector<Location> offsetTargetLocations;
    for (auto& [qubit, location] : shuttlingQubitToCurrentLocation_) {
      const auto& qubitMovement = movements_.at(qubit);
      const std::pair newLocation{
          aodColsToX_.at(targetXToAodCol.at(qubitMovement.targetX)),
          aodRowsToY_.at(targetYToAodRow.at(qubitMovement.targetY))};
      if (location != newLocation) {
        atomsToOffset.emplace_back(&atoms[qubit].get());
        offsetTargetLocations.emplace_back(
            static_cast<double>(newLocation.first),
            static_cast<double>(newLocation.second));
        location = newLocation;
      }
    }
    if (!atomsToOffset.empty()) {
      code.emplaceBack<MoveOp>(atomsToOffset, offsetTargetLocations);
    }
    // Store old atoms
    std::vector<const Atom*> atomsToStore;
    for (const auto& qubit : qubitsToStore) {
      atomsToStore.emplace_back(&atoms[qubit].get());
      shuttlingQubitToCurrentLocation_.erase(qubit);
      // Make a virtual offset of rows with old atoms
      const auto& qubitMovement = movements_.at(qubit);
      const auto aodCol = targetXToAodCol.at(qubitMovement.targetX);
      aodColsToX_[aodCol] = qubitMovement.targetX + targetDx_ / 2;
    }
    code.emplaceBack<StoreOp>(atomsToStore);
    aodRowsToY_.erase(it);
  }
}
auto CodeGenerator::RearrangementGenerator::storeColumnByColumn(
    const std::vector<std::reference_wrapper<const Atom>>& atoms,
    NAComputation& code) -> void {
  // Map collecting all atoms that must be stored within each target column
  // (x-coordinate). It is intentionally an 'ordered' map to save the sorting
  // afterward. This set is intentionally an 'ordered' map to ensure
  // deterministic (ordered atoms) code generation.
  std::map<int64_t, std::set<qc::Qubit>> xToQubitsToBeStored;

  for (const auto& [qubit, movement] : movements_) {
    // record the qubits in each column to be loaded
    xToQubitsToBeStored.try_emplace(movement.targetX)
        .first->second.emplace(qubit);
  }

  // A map from the target x-coordinate of the column to the AOD column that
  // will store the atoms in this column. Here it is important that the moves
  // are sorted by their initial x-coordinate.
  std::unordered_map<int64_t, size_t> targetXToAodCol;
  for (const auto& [aodCol, move] : enumerate(horizontalMoves_)) {
    targetXToAodCol.emplace(move.second, aodCol);
  }
  // A set of y-coordinates of the rows to the AOD rows that
  // will store the atoms in this column
  std::set<int64_t> targetYs;
  for (const auto v : verticalMoves_ | std::views::values) {
    targetYs.emplace(v);
  }
  std::unordered_map<int64_t, size_t> targetYToAodRow;
  for (const auto& [aodRow, y] : enumerate(targetYs)) {
    targetYToAodRow.emplace(y, aodRow);
    // Make a virtual move of all rows to their target y-coordinates
    aodRowsToY_[aodRow] = y + targetDy_ / 2;
  }

  for (const auto& [targetX, qubitsToStore] : xToQubitsToBeStored) {
    // Get the AOD column to store the atom from
    assert(targetXToAodCol.contains(targetX));
    const auto oldAodCol = targetXToAodCol[targetX];
    const auto it = aodColsToX_.find(oldAodCol);
    assert(it != aodColsToX_.end());
    it->second = targetX;
    if (const auto columnKind =
            movements_.at(*qubitsToStore.begin()).targetSite;
        columnKind == QubitMovement::SiteKind::STORAGE) {
      // Push still activated columns away if necessary
      auto nextX = targetX - (targetDx_ / 2);
      for (auto lowerIt = std::make_reverse_iterator(it);
           lowerIt != aodColsToX_.rend() && lowerIt->second > nextX;
           ++lowerIt) {
        lowerIt->second = nextX;
        nextX -= nextX > 0 ? targetDx_ : targetDx_ / 2;
      }
      nextX = targetX + targetDx_ + (targetDx_ / 2);
      for (auto upperIt = std::next(it);
           upperIt != aodColsToX_.end() && upperIt->second < nextX;
           ++upperIt) {
        upperIt->second = nextX;
        nextX += nextX < targetMaxX_ ? targetDx_ : targetDx_ / 2;
      }
    } else if (columnKind == QubitMovement::SiteKind::ENTANGLEMENT_LEFT) {
      // Push already activated columns away if necessary.
      bool odd = true;
      auto nextX = targetX - (targetDx_ / 2);
      for (auto lowerIt = std::make_reverse_iterator(it);
           lowerIt != aodColsToX_.rend() && lowerIt->second > nextX;
           ++lowerIt) {
        lowerIt->second = nextX;
        nextX -= nextX > 0 || odd ? targetDx_ - targetDx_ / 4 : targetDx_ / 4;
        odd = !odd;
      }
      odd = false;
      nextX = targetX + targetDx_ - (targetDx_ / 4);
      for (auto upperIt = std::next(it);
           upperIt != aodColsToX_.end() && upperIt->second < nextX;
           ++upperIt) {
        upperIt->second = nextX;
        nextX += nextX < targetMaxX_ || odd ? targetDx_ - targetDx_ / 4
                                            : targetDx_ / 4;
        odd = !odd;
      }
    } else {
      // Push already activated columns away if necessary.
      bool odd = false;
      auto nextX = targetX - targetDx_ + (targetDx_ / 4);
      for (auto lowerIt = std::make_reverse_iterator(it);
           lowerIt != aodColsToX_.rend() && lowerIt->second > nextX;
           ++lowerIt) {
        lowerIt->second = nextX;
        nextX -= nextX > 0 || odd ? targetDx_ - targetDx_ / 4 : targetDx_ / 4;
      }
      odd = true;
      nextX = targetX + (targetDx_ / 2);
      for (auto upperIt = std::next(it);
           upperIt != aodColsToX_.end() && upperIt->second < nextX;
           ++upperIt) {
        upperIt->second = nextX;
        nextX += nextX < targetMaxX_ || odd ? targetDx_ - targetDx_ / 4
                                            : targetDx_ / 4;
        odd = !odd;
      }
    }
    // Align aod rows
    for (const auto qubit : qubitsToStore) {
      const auto& qubitMovement = movements_.at(qubit);
      const auto aodRow = targetYToAodRow.at(qubitMovement.targetY);
      aodRowsToY_[aodRow] = qubitMovement.targetY;
    }
    std::vector<const Atom*> atomsToOffset;
    std::vector<Location> offsetTargetLocations;
    for (auto& [qubit, location] : shuttlingQubitToCurrentLocation_) {
      const auto& qubitMovement = movements_.at(qubit);
      const std::pair newLocation{
          aodColsToX_.at(targetXToAodCol.at(qubitMovement.targetX)),
          aodRowsToY_.at(targetYToAodRow.at(qubitMovement.targetY))};
      if (location != newLocation) {
        atomsToOffset.emplace_back(&atoms[qubit].get());
        offsetTargetLocations.emplace_back(
            static_cast<double>(newLocation.first),
            static_cast<double>(newLocation.second));
        location = newLocation;
      }
    }
    if (!atomsToOffset.empty()) {
      code.emplaceBack<MoveOp>(atomsToOffset, offsetTargetLocations);
    }
    // Store old atoms
    std::vector<const Atom*> atomsToStore;
    for (const auto& qubit : qubitsToStore) {
      atomsToStore.emplace_back(&atoms[qubit].get());
      shuttlingQubitToCurrentLocation_.erase(qubit);
      // Make a virtual offset of rows with old atoms
      const auto& qubitMovement = movements_.at(qubit);
      const auto aodRow = targetYToAodRow.at(qubitMovement.targetY);
      aodRowsToY_[aodRow] = qubitMovement.targetY + targetDy_ / 2;
    }
    code.emplaceBack<StoreOp>(atomsToStore);
    aodColsToX_.erase(it);
  }
}
CodeGenerator::RearrangementGenerator::RearrangementGenerator(
    const Architecture& arch, const Placement& sourcePlacement,
    const Placement& targetPlacement, const std::vector<qc::Qubit>& qubits)
    : architecture_(arch) {
  // extract the movement of every single qubit
  std::ranges::for_each(qubits, [&](const auto& qubit) {
    const auto [sourceX, sourceY] = getLocationFromSite(sourcePlacement[qubit]);
    const auto sourceSite = getSiteKindFromSite(sourcePlacement[qubit]);
    const auto [targetX, targetY] = getLocationFromSite(targetPlacement[qubit]);
    const auto targetSite = getSiteKindFromSite(targetPlacement[qubit]);
    movements_.emplace(qubit, QubitMovement{sourceSite, sourceX, sourceY,
                                            targetSite, targetX, targetY});
  });

  // We assume that all qubits to be loaded are in the same zone. We extract
  // the vertical separation of the zone from the first qubit's zone.
  const auto& sourceSlm = std::get<0>(sourcePlacement.at(qubits.front())).get();
  sourceDx_ = static_cast<int64_t>(sourceSlm.siteSeparation.first);
  sourceDy_ = static_cast<int64_t>(sourceSlm.siteSeparation.second);
  sourceMaxX_ = sourceDx_ * static_cast<int64_t>(sourceSlm.nCols);
  sourceMaxY_ = sourceDy_ * static_cast<int64_t>(sourceSlm.nRows);
  // We do the same for the target zone
  const auto& targetSlm = std::get<0>(targetPlacement.at(qubits.front())).get();
  targetDx_ = static_cast<int64_t>(targetSlm.siteSeparation.first);
  targetDy_ = static_cast<int64_t>(targetSlm.siteSeparation.second);
  targetMaxX_ = targetDx_ * static_cast<int64_t>(targetSlm.nCols);
  targetMaxY_ = targetDy_ * static_cast<int64_t>(targetSlm.nRows);

  for (const auto& [qubit, movement] : movements_) {
    // record the moves
    const auto verticalIt =
        verticalMoves_.try_emplace(movement.sourceY, movement.targetY).first;
    // If this does not hold, the input was invalid for this generator.
    // More precisely, this conditional `assert` ensures that rows do not
    // split.
    assert(verticalIt->second == movement.targetY);
    const auto& horizontalIt =
        horizontalMoves_.try_emplace(movement.sourceX, movement.targetX).first;
    // If this does not hold, the input was invalid for this generator.
    // More precisely, this conditional `assert` ensures that columns do not
    // split.
    assert(horizontalIt->second == movement.targetX);
  }

  // Check the vertical moves whether all rows remain in the same order
  identicalRowOrder_ =
      std::ranges::is_sorted(verticalMoves_ | std::views::values);
  // Check the horizontal moves whether all columns remain in the same order
  identicalColumnOrder_ =
      std::ranges::is_sorted(horizontalMoves_ | std::views::values);
}
auto CodeGenerator::RearrangementGenerator::generate(
    const std::vector<std::reference_wrapper<const Atom>>& atoms,
    NAComputation& code) -> void {
  if (identicalRowOrder_ && horizontalMoves_.size() < verticalMoves_.size()) {
    loadColumnByColumn(atoms, code);
    storeColumnByColumn(atoms, code);
  } else if (identicalColumnOrder_ &&
             verticalMoves_.size() < horizontalMoves_.size()) {
    loadRowByRow(atoms, code);
    storeRowByRow(atoms, code);
  } else {
    loadRowByRow(atoms, code);
    storeColumnByColumn(atoms, code);
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
