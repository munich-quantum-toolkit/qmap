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
// Unique-y comparison (ignores multiplicity)
template <typename Set1, typename Set2, typename Map>
  requires std::is_same_v<std::decay_t<Set1>, std::decay_t<Set2>>
auto isSameMappedSet(Set1&& a, Set2&& b, Map m) -> bool {
  using Value = typename std::decay_t<Set1>::value_type;
  using MappedT = std::decay_t<std::invoke_result_t<Map&, Value>>;
  std::unordered_set<MappedT> mappedA, mappedB;
  std::ranges::for_each(
      a, [&m, &mappedA](const auto& i) { mappedA.emplace(m(i)); });
  std::ranges::for_each(
      b, [&m, &mappedB](const auto& i) { mappedB.emplace(m(i)); });
  return mappedA == mappedB;
}
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
        if (const auto opType = op.get().getType(); opType == qc::RY) {
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
      if (op.get().getType() == qc::RZ || op.get().getType() == qc::P) {
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
        } else if (op.get().getType() == qc::Vdg) {
          code.emplaceBack<LocalUOp>(atoms[qubit], -qc::PI_2, qc::PI_2,
                                     -qc::PI_2);
        } else if (op.get().getType() == qc::SX) {
          code.emplaceBack<LocalUOp>(atoms[qubit], qc::PI_2, -qc::PI_2,
                                     qc::PI_2);
        } else if (op.get().getType() == qc::SXdg ||
                   op.get().getType() == qc::V) {
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
[[nodiscard]] auto enumerate(auto&& data) {
  return std::views::all(std::forward<decltype(data)>(data)) |
         std::views::transform([i = 0UL](const auto& value) mutable {
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
auto CodeGenerator::RearrangementGenerator::addSourceMove(
    const std::unordered_map<int64_t, size_t>& sourceXToAodCol,
    const std::unordered_map<int64_t, size_t>& sourceYToAodRow,
    const std::vector<std::reference_wrapper<const Atom>>& atoms,
    NAComputation& code) -> void {
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
          Location{.x = static_cast<double>(newLocation.first),
                   .y = static_cast<double>(newLocation.second)});
      location = newLocation;
    }
  }
  if (!atomsToOffset.empty()) {
    code.emplaceBack<MoveOp>(atomsToOffset, offsetTargetLocations);
  }
}
auto CodeGenerator::RearrangementGenerator::addTargetMove(
    const std::unordered_map<int64_t, size_t>& targetXToAodCol,
    const std::unordered_map<int64_t, size_t>& targetYToAodRow,
    const std::vector<std::reference_wrapper<const Atom>>& atoms,
    NAComputation& code) -> void {
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
          Location{.x = static_cast<double>(newLocation.first),
                   .y = static_cast<double>(newLocation.second)});
      location = newLocation;
    }
  }
  if (!atomsToOffset.empty()) {
    code.emplaceBack<MoveOp>(atomsToOffset, offsetTargetLocations);
  }
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
  const int64_t sign =
      (rearrangementDirection_ == RearrangementDirection::UP ? 1 : -1);
  for (const auto& [sourceY, qubitsToLoad] : yToQubitsToBeLoaded) {
    // Get the AOD row to load the atoms in this row.
    assert(sourceYToAodRow.contains(sourceY));
    const auto newAodRow = sourceYToAodRow[sourceY];
    if (aodRowsToY_.contains(newAodRow)) {
      // Atoms in this column already loaded, skip
      continue;
    }
    auto allQubitsToLoad = qubitsToLoad;
    // already include a virtual offset move by `startD / 2`
    const auto y = sourceY + (sign * sourceDy_ / 2);
    const auto it = aodRowsToY_.emplace(newAodRow, y).first;
    // Push already activated rows away if necessary.
    auto nextY = y - sourceDy_;
    for (auto lowerIt = std::make_reverse_iterator(it);
         lowerIt != aodRowsToY_.rend() && lowerIt->second > nextY; ++lowerIt) {
      lowerIt->second = nextY;
      nextY -= nextY > sourceMinY_ ? sourceDy_ : sourceDy_ / 2;
    }
    nextY = y + sourceDy_;
    for (auto upperIt = std::next(it); true; ++upperIt) {
      // check whether another aod row could be loaded
      for (auto anotherSourceYIt =
               yToQubitsToBeLoaded.lower_bound(nextY - (sign * sourceDy_ / 2));
           anotherSourceYIt != yToQubitsToBeLoaded.end(); ++anotherSourceYIt) {
        if (const auto anotherAodRow = sourceYToAodRow[anotherSourceYIt->first];
            std::prev(upperIt)->first < anotherAodRow) {
          if (upperIt == aodRowsToY_.end() || anotherAodRow < upperIt->first) {
            if (isSameMappedSet(qubitsToLoad, anotherSourceYIt->second,
                                [this](const auto q) {
                                  return movements_.at(q).sourceX;
                                })) {
              aodRowsToY_.emplace(sourceYToAodRow[anotherSourceYIt->first],
                                  anotherSourceYIt->first +
                                      (sign * sourceDy_ / 2));
              std::ranges::for_each(anotherSourceYIt->second,
                                    [&allQubitsToLoad](const auto q) {
                                      allQubitsToLoad.emplace(q);
                                    });
              nextY =
                  anotherSourceYIt->first + sourceDy_ + (sign * sourceDy_ / 2);
            }
          }
        }
      }
      if (upperIt == aodRowsToY_.end()) {
        break;
      }
      if (upperIt->second < nextY) {
        upperIt->second = nextY;
        nextY += nextY < sourceMaxY_ ? sourceDy_ : sourceDy_ / 2;
      }
    }
    addSourceMove(sourceXToAodCol, sourceYToAodRow, atoms, code);
    // Align aod columns
    for (const auto qubit : qubitsToLoad) {
      const auto& qubitMovement = movements_.at(qubit);
      const auto aodCol = sourceXToAodCol.at(qubitMovement.sourceX);
      aodColsToX_[aodCol] = qubitMovement.sourceX;
    }
    addSourceMove(sourceXToAodCol, sourceYToAodRow, atoms, code);
    // Load new atoms
    std::vector<const Atom*> atomsToLoad;
    for (const auto& qubit : allQubitsToLoad) {
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
    addSourceMove(sourceXToAodCol, sourceYToAodRow, atoms, code);
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

  // A map from the source x-coordinate of the column to the AOD column that
  // will load the atoms in this column. Since loadColumnByColumn is always
  // followed by storeColumnByColumn, see `generate(...)`, we do not attempt
  // reordering columns while loading. Hence, we enumerate and sort the columns
  // by their initial x-coordinate
  std::unordered_map<int64_t, size_t> sourceXToAodCol;
  for (const auto& [aodCol, revMove] : enumerate(horizontalMoves_)) {
    sourceXToAodCol.emplace(revMove.first, aodCol);
  }
  // A map from the source y-coordinate of the row to the AOD row that
  // will load the atoms in this column.
  std::unordered_map<int64_t, size_t> sourceYToAodRow;
  for (const auto& [aodRow, y] : enumerate(verticalMoves_ | std::views::keys)) {
    sourceYToAodRow.emplace(y, aodRow);
  }

  const int64_t sign =
      (rearrangementDirection_ == RearrangementDirection::UP ? 1 : -1);
  // Load the atoms column-wise
  for (const auto& [sourceX, qubitsToLoad] : xToQubitsToBeLoaded) {
    // Get the AOD column to load the atoms in this column.
    assert(sourceXToAodCol.contains(sourceX));
    const auto newAodCol = sourceXToAodCol[sourceX];
    if (aodColsToX_.contains(newAodCol)) {
      // Atoms in this column already loaded, skip
      continue;
    }
    auto allQubitsToLoad = qubitsToLoad;
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
        nextX -= nextX > sourceMinX_ ? sourceDx_ : sourceDx_ / 2;
      }
      nextX = sourceX + sourceDx_ + (sourceDx_ / 2);
      for (auto upperIt = std::next(it); true; ++upperIt) {
        // check whether another aod column could be loaded
        for (auto anotherSourceXIt =
                 xToQubitsToBeLoaded.lower_bound(nextX - sourceDx_ / 2);
             anotherSourceXIt != xToQubitsToBeLoaded.end();
             ++anotherSourceXIt) {
          if (const auto anotherAodCol =
                  sourceXToAodCol[anotherSourceXIt->first];
              std::prev(upperIt)->first < anotherAodCol) {
            if (upperIt == aodColsToX_.end() ||
                anotherAodCol < upperIt->first) {
              if (isSameMappedSet(qubitsToLoad, anotherSourceXIt->second,
                                  [this](const auto q) {
                                    return movements_.at(q).sourceY;
                                  })) {
                aodColsToX_.emplace(anotherAodCol,
                                    anotherSourceXIt->first + sourceDx_ / 2);
                std::ranges::for_each(anotherSourceXIt->second,
                                      [&allQubitsToLoad](const auto q) {
                                        allQubitsToLoad.emplace(q);
                                      });
                nextX = anotherSourceXIt->first + sourceDx_ + (sourceDx_ / 2);
              }
            }
          }
        }
        if (upperIt == aodColsToX_.end()) {
          break;
        }
        if (upperIt->second < nextX) {
          upperIt->second = nextX;
          nextX += nextX < sourceMaxX_ ? sourceDx_ : sourceDx_ / 2;
        }
      }
    } else {
      bool left = columnKind == QubitMovement::SiteKind::ENTANGLEMENT_LEFT;
      const auto offsetX = (sourceDx_ - pairSep_) / 3;
      // already include a virtual offset move
      const auto it =
          aodColsToX_.emplace(newAodCol, sourceX + (left ? -offsetX : offsetX))
              .first;
      // Push already activated columns away if necessary.
      auto nextX = sourceX -
                   (left ? sourceDx_ - pairSep_ - offsetX : pairSep_ + offsetX);
      for (auto lowerIt = std::make_reverse_iterator(it);
           lowerIt != aodColsToX_.rend() && lowerIt->second > nextX;
           ++lowerIt) {
        lowerIt->second = nextX;
        nextX -=
            ((nextX > sourceMinX_ && left) ? sourceDx_ - offsetX : offsetX);
        left = !left;
      }
      left = columnKind == QubitMovement::SiteKind::ENTANGLEMENT_LEFT;
      nextX = sourceX +
              (left ? pairSep_ + offsetX : sourceDx_ - pairSep_ - offsetX);
      for (auto upperIt = std::next(it); true; ++upperIt) {
        // check whether another aod column could be loaded
        for (auto anotherSourceXIt = xToQubitsToBeLoaded.lower_bound(
                 nextX -
                 (left ? pairSep_ + offsetX : sourceDx_ - pairSep_ - offsetX));
             anotherSourceXIt != xToQubitsToBeLoaded.end();
             ++anotherSourceXIt) {
          if (const auto anotherAodCol =
                  sourceXToAodCol[anotherSourceXIt->first];
              std::prev(upperIt)->first < anotherAodCol) {
            if (upperIt == aodColsToX_.end() ||
                anotherAodCol < upperIt->first) {
              if (isSameMappedSet(qubitsToLoad, anotherSourceXIt->second,
                                  [this](const auto q) {
                                    return movements_.at(q).sourceY;
                                  })) {
                left =
                    movements_[*anotherSourceXIt->second.cbegin()].sourceSite ==
                    QubitMovement::SiteKind::ENTANGLEMENT_LEFT;
                aodColsToX_.emplace(anotherAodCol,
                                    anotherSourceXIt->first +
                                        (left ? -offsetX : offsetX));
                std::ranges::for_each(anotherSourceXIt->second,
                                      [&allQubitsToLoad](const auto q) {
                                        allQubitsToLoad.emplace(q);
                                      });
                nextX = anotherSourceXIt->first +
                        (left ? pairSep_ + offsetX
                              : sourceDx_ - pairSep_ - offsetX);
              }
            }
          }
        }
        if (upperIt == aodColsToX_.end()) {
          break;
        }
        if (upperIt->second < nextX) {
          upperIt->second = nextX;
          nextX += nextX >= sourceMaxX_ || left ? offsetX : sourceDx_ - offsetX;
          left = !left;
        }
      }
    }
    addSourceMove(sourceXToAodCol, sourceYToAodRow, atoms, code);
    // Align aod rows
    for (const auto qubit : qubitsToLoad) {
      const auto& qubitMovement = movements_.at(qubit);
      const auto aodRow = sourceYToAodRow.at(qubitMovement.sourceY);
      aodRowsToY_[aodRow] = qubitMovement.sourceY;
    }
    // Write out offset move before loading new atoms
    addSourceMove(sourceXToAodCol, sourceYToAodRow, atoms, code);
    // Load new atoms
    std::vector<const Atom*> atomsToLoad;
    for (const auto& qubit : allQubitsToLoad) {
      atomsToLoad.emplace_back(&atoms[qubit].get());
      const auto& qubitMovement = movements_.at(qubit);
      shuttlingQubitToCurrentLocation_.emplace(
          qubit, std::pair{qubitMovement.sourceX, qubitMovement.sourceY});
      // Make a virtual offset of rows with new atoms
      const auto aodRow = sourceYToAodRow.at(qubitMovement.sourceY);
      aodRowsToY_[aodRow] = qubitMovement.sourceY + (sign * sourceDy_ / 2);
    }
    code.emplaceBack<LoadOp>(atomsToLoad);
    addSourceMove(sourceXToAodCol, sourceYToAodRow, atoms, code);
  }
}
auto CodeGenerator::RearrangementGenerator::storeRowByRow(
    const std::vector<std::reference_wrapper<const Atom>>& atoms,
    NAComputation& code) -> void {
  const int64_t sign =
      (rearrangementDirection_ == RearrangementDirection::DOWN ? 1 : -1);
  // Since storeRowByRow is only called after loadRowByRow, the rows are already
  // in order. Hence, the AOD rows must be enumerated and sorted by their final
  // y-coordinate. See also `generate(...)`.
  std::map<int64_t, int64_t> revVerticalMoves;
  for (const auto& [k, v] : verticalMoves_) {
    revVerticalMoves.emplace(v, k);
  }
  // A map from the target y-coordinate of the row to the AOD row that
  // will store the atoms in this row.
  std::unordered_map<int64_t, size_t> targetYToAodRow;
  std::unordered_map<size_t, int64_t> aodRowToTargetY;
  for (const auto& [aodRow, move] : enumerate(revVerticalMoves)) {
    targetYToAodRow.emplace(move.first, aodRow);
    aodRowToTargetY.emplace(aodRow, move.first);
    aodRowsToY_[aodRow] = move.first + (sign * targetDy_ / 2);
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

  {
    const auto firstTargetY = yToQubitsToBeStored.cbegin()->first;
    assert(targetYToAodRow.contains(firstTargetY));
    const auto oldAodRow = targetYToAodRow[firstTargetY];
    const auto it = aodRowsToY_.find(oldAodRow);
    const auto y = it->second;
    auto nextY = y;
    for (auto lowerIt = std::make_reverse_iterator(it);
         lowerIt != aodRowsToY_.rend(); ++lowerIt) {
      nextY -= nextY > targetMinY_ ? targetDy_ : targetDy_ / 2;
      if (lowerIt->second > nextY) {
        lowerIt->second = nextY;
      } else {
        nextY = lowerIt->second;
      }
    }
    nextY = y;
    for (auto upperIt = std::next(it); upperIt != aodRowsToY_.end();
         ++upperIt) {
      nextY += nextY < targetMaxY_ ? targetDy_ : targetDy_ / 2;
      if (upperIt->second < nextY) {
        upperIt->second = nextY;
      } else {
        nextY = upperIt->second;
      }
    }
  }

  for (const auto& [targetY, qubitsToStore] : yToQubitsToBeStored) {
    // Get the AOD column to store the atom from
    assert(targetYToAodRow.contains(targetY));
    const auto oldAodRow = targetYToAodRow[targetY];
    const auto it = aodRowsToY_.find(oldAodRow);
    if (it == aodRowsToY_.end()) {
      // Atoms in this row already stored, skip
      continue;
    }
    auto allQubitsToStore = qubitsToStore;
    std::vector rowsToStore{std::pair{oldAodRow, targetY}};
    const auto y = targetY + (sign * targetDy_ / 2);
    it->second = y;
    // Push still activated columns away if necessary
    auto nextY = y - targetDy_;
    for (auto lowerIt = std::make_reverse_iterator(it);
         lowerIt != aodRowsToY_.rend() && lowerIt->second > nextY; ++lowerIt) {
      lowerIt->second = nextY;
      nextY -= nextY > targetMinY_ ? targetDy_ : targetDy_ / 2;
    }
    nextY = y + targetDy_;
    // counts the activated columns after the last column that was aligned
    int64_t freeCols = 0;
    for (auto upperIt = std::next(it); upperIt != aodRowsToY_.end();
         ++upperIt) {
      // check whether another aod row could also be stored
      const auto aodRowTargetY = aodRowToTargetY[upperIt->first];
      const auto& aodColQubits = yToQubitsToBeStored[aodRowTargetY];
      if (aodRowTargetY >=
              nextY - (sign * targetDy_ / 2) + (freeCols * targetDy_) &&
          isSameMappedSet(qubitsToStore, aodColQubits, [this](const auto q) {
            return movements_.at(q).targetX;
          })) {
        rowsToStore.emplace_back(upperIt->first, aodRowTargetY);
        aodRowsToY_[upperIt->first] = aodRowTargetY + (sign * targetDy_ / 2);
        std::ranges::for_each(aodColQubits, [&allQubitsToStore](const auto q) {
          allQubitsToStore.emplace(q);
        });
        // Push free columns away if necessary
        nextY = aodRowTargetY - (sign * targetDy_ / 2);
        for (auto lowerIt = std::make_reverse_iterator(upperIt);
             lowerIt != aodRowsToY_.rend() && lowerIt->second > nextY;
             ++lowerIt) {
          lowerIt->second = nextY;
          nextY -= nextY > targetMinY_ ? targetDy_ : targetDy_ / 2;
        }
        nextY = aodRowTargetY + targetDy_ + (sign * targetDy_ / 2);
        freeCols = 0;
      } else if (upperIt->second < nextY) {
        upperIt->second = nextY;
        nextY += nextY < targetMaxY_ ? targetDy_ : targetDy_ / 2;
      } else {
        ++freeCols;
      }
    }
    addTargetMove(targetXToAodCol, targetYToAodRow, atoms, code);
    // Align aod columns
    for (const auto qubit : qubitsToStore) {
      const auto& qubitMovement = movements_.at(qubit);
      const auto aodCol = targetXToAodCol.at(qubitMovement.targetX);
      aodColsToX_[aodCol] = qubitMovement.targetX;
    }
    for (const auto [row, rowTargetY] : rowsToStore) {
      aodRowsToY_[row] = rowTargetY;
    }
    addTargetMove(targetXToAodCol, targetYToAodRow, atoms, code);
    // Store old atoms
    std::vector<const Atom*> atomsToStore;
    for (const auto& qubit : allQubitsToStore) {
      atomsToStore.emplace_back(&atoms[qubit].get());
      shuttlingQubitToCurrentLocation_.erase(qubit);
      const auto& qubitMovement = movements_.at(qubit);
      const auto aodCol = targetXToAodCol.at(qubitMovement.targetX);
      if (qubitMovement.targetSite == QubitMovement::SiteKind::STORAGE) {
        aodColsToX_[aodCol] = qubitMovement.targetX + (targetDx_ / 2);
      } else if (qubitMovement.targetSite ==
                 QubitMovement::SiteKind::ENTANGLEMENT_LEFT) {
        aodColsToX_[aodCol] = qubitMovement.targetX - (targetDx_ / 4);
      } else {
        aodColsToX_[aodCol] = qubitMovement.targetX + (targetDx_ / 4);
      }
    }
    code.emplaceBack<StoreOp>(atomsToStore);
    for (const auto row : rowsToStore | std::views::keys) {
      aodRowsToY_.erase(row);
    }
    addTargetMove(targetXToAodCol, targetYToAodRow, atoms, code);
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
  std::unordered_map<size_t, int64_t> aodColToTargetX;
  for (const auto& [aodCol, move] : enumerate(horizontalMoves_)) {
    targetXToAodCol.emplace(move.second, aodCol);
    aodColToTargetX.emplace(aodCol, move.second);
  }
  // A set of y-coordinates of the rows to the AOD rows that
  // will store the atoms in this column
  std::set<int64_t> targetYs;
  for (const auto v : verticalMoves_ | std::views::values) {
    targetYs.emplace(v);
  }
  std::unordered_map<int64_t, size_t> targetYToAodRow;
  const int64_t sign =
      (rearrangementDirection_ == RearrangementDirection::DOWN ? 1 : -1);
  for (const auto& [aodRow, y] : enumerate(targetYs)) {
    targetYToAodRow.emplace(y, aodRow);
    // Make a virtual move of all rows to their target y-coordinates
    aodRowsToY_[aodRow] = y + (sign * targetDy_ / 2);
  }

  for (const auto& [targetX, qubitsToStore] : xToQubitsToBeStored) {
    // Make a virtual move of all columns to their target x-coordinates
    assert(targetXToAodCol.contains(targetX));
    const auto aodCol = targetXToAodCol[targetX];
    if (const auto columnKind =
            movements_.at(*qubitsToStore.begin()).targetSite;
        columnKind == QubitMovement::SiteKind::STORAGE) {
      aodColsToX_[aodCol] = targetX + targetDx_ / 2;
    } else {
      const auto offsetX = (targetDx_ - pairSep_) / 3;
      if (columnKind == QubitMovement::SiteKind::ENTANGLEMENT_LEFT) {
        aodColsToX_[aodCol] = targetX - offsetX;
      } else {
        aodColsToX_[aodCol] = targetX + offsetX;
      }
    }
  }

  // align the columns to the target region, i.e., align the first column to be
  // stored and spread the others around
  {
    const auto& [firstTargetX, firstQubitsToStore] =
        *xToQubitsToBeStored.cbegin();
    assert(targetXToAodCol.contains(firstTargetX));
    const auto oldAodCol = targetXToAodCol[firstTargetX];
    const auto it = aodColsToX_.find(oldAodCol);
    const auto x = it->second;
    auto columnKind = movements_.at(*firstQubitsToStore.begin()).targetSite;
    auto nextX = x;
    for (auto lowerIt = std::make_reverse_iterator(it);
         lowerIt != aodColsToX_.rend(); ++lowerIt) {
      if (columnKind == QubitMovement::SiteKind::STORAGE) {
        nextX -= nextX > targetMinX_ ? targetDx_ : targetDx_ / 2;
      } else {
        const auto offsetX = (targetDx_ - pairSep_) / 3;
        nextX -=
            nextX > targetMinX_ &&
                    columnKind == QubitMovement::SiteKind::ENTANGLEMENT_RIGHT
                ? targetDx_ - offsetX
                : offsetX;
        columnKind = columnKind == QubitMovement::SiteKind::ENTANGLEMENT_LEFT
                         ? QubitMovement::SiteKind::ENTANGLEMENT_RIGHT
                         : QubitMovement::SiteKind::ENTANGLEMENT_LEFT;
      }
      if (lowerIt->second > nextX) {
        lowerIt->second = nextX;
      } else {
        if (columnKind != QubitMovement::SiteKind::STORAGE &&
            (nextX - lowerIt->second) % targetDx_ != 0) {
          columnKind = columnKind == QubitMovement::SiteKind::ENTANGLEMENT_LEFT
                           ? QubitMovement::SiteKind::ENTANGLEMENT_RIGHT
                           : QubitMovement::SiteKind::ENTANGLEMENT_LEFT;
        }
        nextX = lowerIt->second;
      }
    }
    columnKind = movements_.at(*firstQubitsToStore.begin()).targetSite;
    nextX = x;
    for (auto upperIt = std::next(it); upperIt != aodColsToX_.end();
         ++upperIt) {
      if (columnKind == QubitMovement::SiteKind::STORAGE) {
        nextX += nextX > targetMinX_ ? targetDx_ : targetDx_ / 2;
      } else {
        const auto offsetX = (targetDx_ - pairSep_) / 3;
        nextX +=
            nextX > targetMinX_ &&
                    columnKind == QubitMovement::SiteKind::ENTANGLEMENT_LEFT
                ? targetDx_ - offsetX
                : offsetX;
        columnKind = columnKind == QubitMovement::SiteKind::ENTANGLEMENT_LEFT
                         ? QubitMovement::SiteKind::ENTANGLEMENT_RIGHT
                         : QubitMovement::SiteKind::ENTANGLEMENT_LEFT;
      }
      if (upperIt->second < nextX) {
        upperIt->second = nextX;
      } else {
        if (columnKind != QubitMovement::SiteKind::STORAGE &&
            (upperIt->second - nextX) % targetDx_ != 0) {
          columnKind = columnKind == QubitMovement::SiteKind::ENTANGLEMENT_LEFT
                           ? QubitMovement::SiteKind::ENTANGLEMENT_RIGHT
                           : QubitMovement::SiteKind::ENTANGLEMENT_LEFT;
        }
        nextX = upperIt->second;
      }
    }
  }

  for (const auto& [targetX, qubitsToStore] : xToQubitsToBeStored) {
    // Get the AOD column to store the atom from
    assert(targetXToAodCol.contains(targetX));
    const auto oldAodCol = targetXToAodCol[targetX];
    const auto it = aodColsToX_.find(oldAodCol);
    if (it == aodColsToX_.end()) {
      // Atoms in this column already stored, skip
      continue;
    }
    auto allQubitsToStore = qubitsToStore;
    std::vector colsToStore{std::pair{oldAodCol, targetX}};
    if (const auto columnKind =
            movements_.at(*qubitsToStore.begin()).targetSite;
        columnKind == QubitMovement::SiteKind::STORAGE) {
      it->second = targetX + (targetDx_ / 2);
      // Push still activated columns away if necessary
      auto nextX = targetX - (targetDx_ / 2);
      for (auto lowerIt = std::make_reverse_iterator(it);
           lowerIt != aodColsToX_.rend() && lowerIt->second > nextX;
           ++lowerIt) {
        lowerIt->second = nextX;
        nextX -= nextX > targetMinX_ ? targetDx_ : targetDx_ / 2;
      }
      nextX = targetX + targetDx_ + (targetDx_ / 2);
      // counts the activated columns after the last column that was aligned
      int64_t freeCols = 0;
      for (auto upperIt = std::next(it); upperIt != aodColsToX_.end();
           ++upperIt) {
        // check whether this aod column could also be stored
        const auto aodColTargetX = aodColToTargetX[upperIt->first];
        const auto& aodColQubits = xToQubitsToBeStored[aodColTargetX];
        if (aodColTargetX >= nextX + (freeCols * targetDx_) - (targetDx_ / 2) &&
            isSameMappedSet(qubitsToStore, aodColQubits, [this](const auto q) {
              return movements_.at(q).targetY;
            })) {
          colsToStore.emplace_back(upperIt->first, aodColTargetX);
          aodColsToX_[upperIt->first] = aodColTargetX + (targetDx_ / 2);
          std::ranges::for_each(aodColQubits,
                                [&allQubitsToStore](const auto q) {
                                  allQubitsToStore.emplace(q);
                                });
          // Push free columns away if necessary
          nextX = aodColTargetX - (targetDx_ / 2);
          for (auto lowerIt = std::make_reverse_iterator(upperIt);
               lowerIt != aodColsToX_.rend() && lowerIt->second > nextX;
               ++lowerIt) {
            lowerIt->second = nextX;
            nextX -= nextX > targetMinX_ ? targetDx_ : targetDx_ / 2;
          }
          nextX = aodColTargetX + targetDx_ + (targetDx_ / 2);
          freeCols = 0;
        } else if (upperIt->second < nextX) {
          upperIt->second = nextX;
          nextX += nextX < targetMaxX_ ? targetDx_ : targetDx_ / 2;
          freeCols = 0;
        } else {
          ++freeCols;
        }
      }
    } else {
      bool left = columnKind == QubitMovement::SiteKind::ENTANGLEMENT_LEFT;
      const auto offsetX = (targetDx_ - pairSep_) / 3;
      it->second = targetX + (left ? -offsetX : offsetX);
      // Push already activated columns away if necessary.
      auto nextX = targetX -
                   (left ? targetDx_ - pairSep_ - offsetX : pairSep_ + offsetX);
      for (auto lowerIt = std::make_reverse_iterator(it);
           lowerIt != aodColsToX_.rend() && lowerIt->second > nextX;
           ++lowerIt) {
        lowerIt->second = nextX;
        nextX -=
            ((nextX > targetMinX_ && left) ? targetDx_ - offsetX : offsetX);
        left = !left;
      }
      left = columnKind == QubitMovement::SiteKind::ENTANGLEMENT_LEFT;
      nextX = targetX +
              (left ? pairSep_ + offsetX : targetDx_ - pairSep_ - offsetX);
      // counts the activated columns after the last column that was aligned
      int64_t freeCols = 0;
      for (auto upperIt = std::next(it); upperIt != aodColsToX_.end();
           ++upperIt) {
        // check whether this aod column could also be stored
        const auto aodColTargetX = aodColToTargetX[upperIt->first];
        const auto& aodColQubits = xToQubitsToBeStored[aodColTargetX];
        if (aodColTargetX >= nextX - (left ? offsetX : -offsetX) +
                                 ((freeCols / 2) * targetDx_) +
                                 (freeCols % 2 == 1
                                      ? (left ? offsetX : targetDx_ - offsetX)
                                      : 0) &&
            isSameMappedSet(qubitsToStore, aodColQubits, [this](const auto q) {
              return movements_.at(q).targetY;
            })) {
          left = movements_[*aodColQubits.cbegin()].targetSite ==
                 QubitMovement::SiteKind::ENTANGLEMENT_LEFT;
          colsToStore.emplace_back(upperIt->first, aodColTargetX);
          aodColsToX_[upperIt->first] =
              aodColTargetX + (left ? -offsetX : offsetX);
          std::ranges::for_each(aodColQubits,
                                [&allQubitsToStore](const auto q) {
                                  allQubitsToStore.emplace(q);
                                });
          // Push free columns away if necessary
          nextX = aodColTargetX -
                  (left ? targetDx_ - pairSep_ - offsetX : pairSep_ + offsetX);
          for (auto lowerIt = std::make_reverse_iterator(upperIt);
               lowerIt != aodColsToX_.rend() && lowerIt->second > nextX;
               ++lowerIt) {
            lowerIt->second = nextX;
            nextX -=
                nextX > targetMinX_ && left ? targetDx_ - offsetX : offsetX;
            left = !left;
          }
          left = movements_[*aodColQubits.cbegin()].targetSite ==
                 QubitMovement::SiteKind::ENTANGLEMENT_LEFT;
          nextX = aodColTargetX +
                  (left ? pairSep_ + offsetX : targetDx_ - pairSep_ - offsetX);
          freeCols = 0;
        } else if (upperIt->second < nextX) {
          upperIt->second = nextX;
          nextX += nextX >= targetMaxX_ || left ? offsetX : targetDx_ - offsetX;
          left = !left;
          freeCols = 0;
        } else {
          ++freeCols;
        }
      }
    }
    addTargetMove(targetXToAodCol, targetYToAodRow, atoms, code);
    // Align aod rows
    for (const auto qubit : qubitsToStore) {
      const auto& qubitMovement = movements_.at(qubit);
      const auto aodRow = targetYToAodRow.at(qubitMovement.targetY);
      aodRowsToY_[aodRow] = qubitMovement.targetY;
    }
    for (const auto [col, colTargetX] : colsToStore) {
      aodColsToX_[col] = colTargetX;
    }
    addTargetMove(targetXToAodCol, targetYToAodRow, atoms, code);
    // Store old atoms
    std::vector<const Atom*> atomsToStore;
    for (const auto& qubit : allQubitsToStore) {
      atomsToStore.emplace_back(&atoms[qubit].get());
      shuttlingQubitToCurrentLocation_.erase(qubit);
      // Make a virtual offset of rows with old atoms
      const auto& qubitMovement = movements_.at(qubit);
      const auto aodRow = targetYToAodRow.at(qubitMovement.targetY);
      aodRowsToY_[aodRow] = qubitMovement.targetY + (sign * targetDy_ / 2);
    }
    code.emplaceBack<StoreOp>(atomsToStore);
    for (const auto col : colsToStore | std::views::keys) {
      aodColsToX_.erase(col);
    }
    addTargetMove(targetXToAodCol, targetYToAodRow, atoms, code);
  }
}
CodeGenerator::RearrangementGenerator::RearrangementGenerator(
    const Architecture& arch, const Placement& sourcePlacement,
    const Placement& targetPlacement, const std::vector<qc::Qubit>& qubits)
    : architecture_(arch) {
  if (qubits.empty()) {
    return;
  }
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
  sourceMinX_ = static_cast<int64_t>(sourceSlm.location.first);
  sourceMaxX_ = sourceMinX_ + sourceDx_ * static_cast<int64_t>(sourceSlm.nCols);
  sourceMinY_ = static_cast<int64_t>(sourceSlm.location.second);
  sourceMaxY_ = sourceMinY_ + sourceDy_ * static_cast<int64_t>(sourceSlm.nRows);
  // We do the same for the target zone
  const auto& targetSlm = std::get<0>(targetPlacement.at(qubits.front())).get();
  targetDx_ = static_cast<int64_t>(targetSlm.siteSeparation.first);
  targetDy_ = static_cast<int64_t>(targetSlm.siteSeparation.second);
  targetMinX_ = static_cast<int64_t>(targetSlm.location.first);
  targetMaxX_ = targetMinX_ + targetDx_ * static_cast<int64_t>(targetSlm.nCols);
  targetMinY_ = static_cast<int64_t>(targetSlm.location.second);
  targetMaxY_ = targetMinY_ + targetDy_ * static_cast<int64_t>(targetSlm.nRows);

  if (sourceSlm.isEntanglement()) {
    pairSep_ =
        std::abs(static_cast<int64_t>(
                     sourceSlm.entanglementZone_->back().location.first) -
                 static_cast<int64_t>(
                     sourceSlm.entanglementZone_->front().location.first));
  } else {
    assert(targetSlm.isEntanglement());
    pairSep_ =
        std::abs(static_cast<int64_t>(
                     targetSlm.entanglementZone_->back().location.first) -
                 static_cast<int64_t>(
                     targetSlm.entanglementZone_->front().location.first));
  }

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

  const auto anyVerticalMove = verticalMoves_.begin();
  assert(anyVerticalMove != verticalMoves_.end());
  rearrangementDirection_ = anyVerticalMove->first < anyVerticalMove->second
                                ? RearrangementDirection::UP
                                : RearrangementDirection::DOWN;
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
  rydbergZones.reserve(architecture_.get().rydbergRangeMinX.size());
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
