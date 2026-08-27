/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "hybridmap/MoveToAodConverter.hpp"

#include "hybridmap/NeutralAtomArchitecture.hpp"
#include "hybridmap/NeutralAtomDefinitions.hpp"
#include "hybridmap/NeutralAtomUtils.hpp"
#include "ir/Definitions.hpp"
#include "ir/QuantumComputation.hpp"
#include "na/ir/entities/Location.hpp"
#include "na/ir/operations/AodOperation.hpp"
#include "na/ir/operations/NeutralAtomOpType.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <ranges>
#include <set>
#include <utility>
#include <vector>

namespace na {

qc::QuantumComputation
MoveToAodConverter::schedule(qc::QuantumComputation& circuit) {
  initFlyingAncillas();
  initMoveGroups(circuit);
  if (moveGroups.empty()) {
    return circuit;
  }
  processMoveGroups();

  // create new quantum circuit and insert AOD operations at the correct
  // indices
  auto groupIt = moveGroups.begin();
  uint32_t circuitIndex = 0;
  for (const auto& operation : circuit) {
    if (groupIt != moveGroups.end() &&
        circuitIndex == groupIt->getFirstCircuitIndex()) {
      // add move group
      for (auto& aodOperation : groupIt->activationOperations) {
        scheduledCircuit.emplace_back(
            std::make_unique<AodOperation>(aodOperation));
      }
      scheduledCircuit.emplace_back(
          std::make_unique<AodOperation>(groupIt->shuttlingOperation));
      for (auto& aodOperation : groupIt->deactivationOperations) {
        scheduledCircuit.emplace_back(
            std::make_unique<AodOperation>(aodOperation));
      }
      ++groupIt;
    } else if (!hasNeutralAtomOpType(*operation, NeutralAtomOpType::Move)) {
      scheduledCircuit.emplace_back(operation->clone());
    }
    ++circuitIndex;
  }

  return scheduledCircuit;
}

AtomMove MoveToAodConverter::convertOperationToMove(
    const qc::Operation& operation) const {
  auto origin = operation.getTargets().front();
  auto target = operation.getTargets().back();
  const auto requiresLoad = origin < arch.getNpositions();
  const auto requiresStore = target < arch.getNpositions();
  while (origin >= arch.getNpositions()) {
    origin -= arch.getNpositions();
  }
  while (target >= arch.getNpositions()) {
    target -= arch.getNpositions();
  }
  return {.origin = origin,
          .target = target,
          .requiresLoad = requiresLoad,
          .requiresStore = requiresStore};
}
void MoveToAodConverter::initFlyingAncillas() {
  if (ancillas.empty()) {
    return;
  }
  std::vector<CoordIndex> coords;
  std::vector<AodOperation::Dimension> dimensions;
  std::vector<qc::fp> starts;
  std::vector<qc::fp> ends;
  std::set<std::uint32_t> rowsActivated;
  std::set<std::uint32_t> columnsActivated;
  for (const auto& ancilla : ancillas) {
    auto coord = ancilla.coord.x + ancilla.coord.y * arch.getNcolumns();
    const auto offsets = ancilla.offset;
    coords.emplace_back(coord);
    coord -= 2 * arch.getNpositions();
    const auto column = (coord % arch.getNcolumns());
    const auto row = (coord / arch.getNcolumns());

    const auto offset =
        arch.getInterQubitDistance() / arch.getNAodIntermediateLevels();
    columnsActivated.insert(column);
    const auto x = column * arch.getInterQubitDistance() + (offsets.x * offset);
    dimensions.emplace_back(AodOperation::Dimension::X);
    starts.emplace_back(x);
    ends.emplace_back(x);
    rowsActivated.insert(row);
    const auto y = row * arch.getInterQubitDistance() + (offsets.y * offset);
    dimensions.emplace_back(AodOperation::Dimension::Y);
    starts.emplace_back(y);
    ends.emplace_back(y);
  }
  const AodOperation initialAodOperation(NeutralAtomOpType::AodActivate, coords,
                                         dimensions, starts, ends);
  scheduledCircuit.emplace_back(
      std::make_unique<AodOperation>(initialAodOperation));
}

void MoveToAodConverter::initMoveGroups(qc::QuantumComputation& circuit) {
  MoveGroup currentMoveGroup;
  uint32_t circuitIndex = 0;
  for (const auto& operation : circuit) {
    if (hasNeutralAtomOpType(*operation, NeutralAtomOpType::Move)) {
      const auto move = convertOperationToMove(*operation);
      if (currentMoveGroup.canAddMove(move, arch)) {
        currentMoveGroup.addMove(move, circuitIndex);
      } else {
        moveGroups.emplace_back(currentMoveGroup);
        currentMoveGroup = MoveGroup();
        currentMoveGroup.addMove(move, circuitIndex);
      }
    } else if (!currentMoveGroup.moves.empty() ||
               !currentMoveGroup.flyingAncillaMoves.empty()) {
      for (const auto& qubit : operation->getUsedQubits()) {
        if (std::ranges::find(currentMoveGroup.qubitsUsedByGates, qubit) ==
            currentMoveGroup.qubitsUsedByGates.end()) {
          currentMoveGroup.qubitsUsedByGates.emplace_back(qubit);
        }
      }
    }
    ++circuitIndex;
  }
  if (!currentMoveGroup.moves.empty() ||
      !currentMoveGroup.flyingAncillaMoves.empty()) {
    moveGroups.emplace_back(std::move(currentMoveGroup));
  }
}

bool MoveToAodConverter::MoveGroup::canAddMove(
    const AtomMove& move, const NeutralAtomArchitecture& archArg) {
  // if move would move a qubit that is used by a gate in this move group
  // return false
  if (std::ranges::find(qubitsUsedByGates, move.origin) !=
      qubitsUsedByGates.end()) {
    return false;
  }
  // checks if the operation can be executed in parallel
  const auto& movesToCheck =
      (move.requiresLoad || move.requiresStore) ? moves : flyingAncillaMoves;
  return std::ranges::all_of(
      movesToCheck,
      [&move, &archArg](const std::pair<AtomMove, uint32_t>& indexedMove) {
        const auto& groupedMove = indexedMove.first;
        // check that passby and move are not in same group
        if (move.requiresLoad != groupedMove.requiresLoad ||
            move.requiresStore != groupedMove.requiresStore) {
          return false;
        }
        // if start or end is same -> false
        if (move.origin == groupedMove.origin ||
            move.target == groupedMove.target) {
          return false;
        }
        // check if parallel executable
        const auto moveVector = archArg.getVector(move.origin, move.target);
        const auto groupedMoveVector =
            archArg.getVector(groupedMove.origin, groupedMove.target);
        return parallelCheck(moveVector, groupedMoveVector);
      });
}

bool MoveToAodConverter::MoveGroup::parallelCheck(const MoveVector& v1,
                                                  const MoveVector& v2) {
  if (!v1.overlap(v2)) {
    return true;
  }
  // overlap -> check if same direction
  if (v1.direction != v2.direction) {
    return false;
  }
  // same direction -> check if include
  if (v1.include(v2) || v2.include(v1)) {
    return false;
  }
  return true;
}

void MoveToAodConverter::MoveGroup::addMove(const AtomMove& move,
                                            const uint32_t circuitIndex) {
  if (move.requiresLoad || move.requiresStore) {
    moves.emplace_back(move, circuitIndex);
  } else {
    flyingAncillaMoves.emplace_back(move, circuitIndex);
  }
  qubitsUsedByGates.emplace_back(move.target);
}

void MoveToAodConverter::AodTransitionBuilder::addTransition(
    const DimensionMergeTypes& mergeTypes, const Location& origin,
    const AtomMove& move, const MoveVector& moveVector,
    const bool requiresAtomTransfer) {
  const auto x = static_cast<std::uint32_t>(origin.x);
  const auto y = static_cast<std::uint32_t>(origin.y);
  const auto signX = moveVector.direction.getSignX();
  const auto signY = moveVector.direction.getSignY();
  const auto deltaX = moveVector.xEnd - moveVector.xStart;
  const auto deltaY = moveVector.yEnd - moveVector.yStart;
  auto xMoves =
      getDimensionMovesFromInitialPosition(AodOperation::Dimension::X, x);
  auto yMoves =
      getDimensionMovesFromInitialPosition(AodOperation::Dimension::Y, y);

  switch (mergeTypes.x) {
  case TransitionMergeType::Trivial:
    switch (mergeTypes.y) {
    case TransitionMergeType::Trivial:
      transitions.emplace_back(
          Transition{{x, deltaX, signX, requiresAtomTransfer},
                     {y, deltaY, signY, requiresAtomTransfer},
                     move});
      break;
    case TransitionMergeType::Merge:
      mergeTransitionDimension(
          AodOperation::Dimension::Y,
          Transition{AodOperation::Dimension::Y,
                     {y, deltaY, signY, requiresAtomTransfer},
                     move},
          Transition{AodOperation::Dimension::X,
                     {x, deltaX, signX, requiresAtomTransfer},
                     move});
      yMoves =
          getDimensionMovesFromInitialPosition(AodOperation::Dimension::Y, y);
      reassignOffsets(yMoves, signY);
      break;
    case TransitionMergeType::Append:
      transitions.emplace_back(
          Transition{{x, deltaX, signX, requiresAtomTransfer},
                     {y, deltaY, signY, requiresAtomTransfer},
                     move});
      yMoves =
          getDimensionMovesFromInitialPosition(AodOperation::Dimension::Y, y);
      reassignOffsets(yMoves, signY);
      break;
    default:
      break;
    }
    break;
  case TransitionMergeType::Merge:
    switch (mergeTypes.y) {
    case TransitionMergeType::Trivial:
      mergeTransitionDimension(
          AodOperation::Dimension::X,
          Transition{AodOperation::Dimension::X,
                     {x, deltaX, signX, requiresAtomTransfer},
                     move},
          Transition{AodOperation::Dimension::Y,
                     {y, deltaY, signY, requiresAtomTransfer},
                     move});
      xMoves =
          getDimensionMovesFromInitialPosition(AodOperation::Dimension::X, x);
      reassignOffsets(xMoves, signX);
      break;
    case TransitionMergeType::Merge:
      mergeTransitionDimension(
          AodOperation::Dimension::X,
          Transition{AodOperation::Dimension::X,
                     {x, deltaX, signX, requiresAtomTransfer},
                     move},
          Transition{AodOperation::Dimension::Y,
                     {y, deltaY, signY, requiresAtomTransfer},
                     move});
      break;
    case TransitionMergeType::Append:
      mergeTransitionDimension(
          AodOperation::Dimension::X,
          Transition{AodOperation::Dimension::X,
                     {x, deltaX, signX, requiresAtomTransfer},
                     move},
          Transition{AodOperation::Dimension::Y,
                     {y, deltaY, signY, requiresAtomTransfer},
                     move});
      yMoves =
          getDimensionMovesFromInitialPosition(AodOperation::Dimension::Y, y);
      reassignOffsets(yMoves, signY);
      break;
    default:
      break;
    }
    break;
  case TransitionMergeType::Append:
    switch (mergeTypes.y) {
    case TransitionMergeType::Trivial:
      transitions.emplace_back(
          Transition{{x, deltaX, signX, requiresAtomTransfer},
                     {y, deltaY, signY, requiresAtomTransfer},
                     move});
      xMoves =
          getDimensionMovesFromInitialPosition(AodOperation::Dimension::X, x);
      reassignOffsets(xMoves, signX);
      break;
    case TransitionMergeType::Merge:
      mergeTransitionDimension(
          AodOperation::Dimension::Y,
          Transition{AodOperation::Dimension::Y,
                     {y, deltaY, signY, requiresAtomTransfer},
                     move},
          Transition{AodOperation::Dimension::X,
                     {x, deltaX, signX, requiresAtomTransfer},
                     move});
      xMoves =
          getDimensionMovesFromInitialPosition(AodOperation::Dimension::X, x);
      reassignOffsets(xMoves, signX);
      break;
    case TransitionMergeType::Append:
      transitions.emplace_back(
          Transition{{x, deltaX, signX, requiresAtomTransfer},
                     {y, deltaY, signY, requiresAtomTransfer},
                     move});
      xMoves =
          getDimensionMovesFromInitialPosition(AodOperation::Dimension::X, x);
      reassignOffsets(xMoves, signX);
      yMoves =
          getDimensionMovesFromInitialPosition(AodOperation::Dimension::Y, y);
      reassignOffsets(yMoves, signY);
      break;
    default:
      break;
    }
    break;
  default:
    break;
  }
}
void MoveToAodConverter::AodTransitionBuilder::addFlyingAncillaTransition(
    const Location& origin, const AtomMove& move, const MoveVector& moveVector,
    const bool requiresAtomTransfer) {
  const auto x = static_cast<std::uint32_t>(origin.x);
  const auto y = static_cast<std::uint32_t>(origin.y);
  const auto signX = moveVector.direction.getSignX();
  const auto signY = moveVector.direction.getSignY();
  const auto deltaX = moveVector.xEnd - moveVector.xStart;
  const auto deltaY = moveVector.yEnd - moveVector.yStart;

  transitions.emplace_back(Transition{{x, deltaX, signX, requiresAtomTransfer},
                                      {y, deltaY, signY, requiresAtomTransfer},
                                      move});
}

[[nodiscard]] MoveToAodConverter::PhaseMergeTypes
MoveToAodConverter::canAddTransition(
    const AodTransitionBuilder& activationBuilder,
    const AodTransitionBuilder& deactivationBuilder, const Location& origin,
    const MoveVector& moveVector, const Location& final,
    const MoveVector& reverseMoveVector,
    const AodOperation::Dimension dimension) {
  const auto start = static_cast<std::uint32_t>(
      dimension == AodOperation::Dimension::X ? origin.x : origin.y);
  const auto end = static_cast<std::uint32_t>(
      dimension == AodOperation::Dimension::X ? final.x : final.y);
  const auto delta = static_cast<qc::fp>(end - start);

  // Get Moves that start/end at the same position as the current move
  const auto activationDimensionMoves =
      activationBuilder.getDimensionMovesFromInitialPosition(dimension, start);
  const auto deactivationDimensionMoves =
      deactivationBuilder.getDimensionMovesFromInitialPosition(dimension, end);

  // both empty
  if (activationDimensionMoves.empty() && deactivationDimensionMoves.empty()) {
    return {.activation = TransitionMergeType::Trivial,
            .deactivation = TransitionMergeType::Trivial};
  }
  // one empty
  if (activationDimensionMoves.empty()) {
    if (deactivationBuilder.hasIntermediateSpaceAtInitialPosition(
            dimension, end, reverseMoveVector.direction.getSign(dimension))) {
      return {.activation = TransitionMergeType::Trivial,
              .deactivation = TransitionMergeType::Append};
    }
    return {.activation = TransitionMergeType::Trivial,
            .deactivation = TransitionMergeType::Impossible};
  }
  if (deactivationDimensionMoves.empty()) {
    if (activationBuilder.hasIntermediateSpaceAtInitialPosition(
            dimension, start, moveVector.direction.getSign(dimension))) {
      return {.activation = TransitionMergeType::Append,
              .deactivation = TransitionMergeType::Trivial};
    }
    return {.activation = TransitionMergeType::Impossible,
            .deactivation = TransitionMergeType::Trivial};
  }
  // both not empty
  // if same moves exist -> merge, else append
  for (const auto& activationDimensionMove : activationDimensionMoves) {
    for (const auto& deactivationDimensionMove : deactivationDimensionMoves) {
      if (activationDimensionMove->initialPosition == start &&
          deactivationDimensionMove->initialPosition == end &&
          std::abs(activationDimensionMove->delta) == std::abs(delta) &&
          std::abs(deactivationDimensionMove->delta) == std::abs(delta)) {
        return {.activation = TransitionMergeType::Merge,
                .deactivation = TransitionMergeType::Merge};
      }
    }
  }
  if (activationBuilder.hasIntermediateSpaceAtInitialPosition(
          dimension, start, moveVector.direction.getSign(dimension)) &&
      deactivationBuilder.hasIntermediateSpaceAtInitialPosition(
          dimension, end, reverseMoveVector.direction.getSign(dimension))) {
    return {.activation = TransitionMergeType::Append,
            .deactivation = TransitionMergeType::Append};
  }
  return {.activation = TransitionMergeType::Impossible,
          .deactivation = TransitionMergeType::Impossible};
}

void MoveToAodConverter::AodTransitionBuilder::reassignOffsets(
    std::vector<std::shared_ptr<DimensionMove>>& dimensionMoves,
    const int32_t sign) {
  std::ranges::sort(dimensionMoves,
                    [](const std::shared_ptr<DimensionMove>& a,
                       const std::shared_ptr<DimensionMove>& b) {
                      return std::abs(a->delta) < std::abs(b->delta);
                    });
  int32_t offset = sign;
  for (const auto& dimensionMove : dimensionMoves) {
    // same sign
    if (dimensionMove->delta * sign >= 0) {
      dimensionMove->offset = offset;
      offset += sign;
    }
  }
}

void MoveToAodConverter::processMoveGroups() {
  // convert the moves from MoveGroup to AodOperations
  for (auto groupIt = moveGroups.begin(); groupIt != moveGroups.end();
       ++groupIt) {
    AodTransitionBuilder activationBuilder{arch,
                                           NeutralAtomOpType::AodActivate};
    AodTransitionBuilder deactivationBuilder{arch,
                                             NeutralAtomOpType::AodDeactivate};

    const auto resultMoves =
        processMoves(groupIt->moves, activationBuilder, deactivationBuilder);
    auto movesToRemove = resultMoves.first;
    auto possibleNewMoveGroup = resultMoves.second;

    processFlyingAncillaMoves(groupIt->flyingAncillaMoves, activationBuilder,
                              deactivationBuilder);

    // remove from current move group
    for (const auto& moveToRemove : movesToRemove) {
      groupIt->moves.erase(
          std::ranges::remove_if(groupIt->moves,
                                 [&moveToRemove](const auto& movePair) {
                                   return movePair.first == moveToRemove;
                                 })
              .begin(),
          groupIt->moves.end());
    }
    if (!possibleNewMoveGroup.moves.empty()) {
      groupIt =
          moveGroups.emplace(groupIt + 1, std::move(possibleNewMoveGroup));
      possibleNewMoveGroup = MoveGroup();
      --groupIt;
    }
    groupIt->activationOperations = activationBuilder.buildPhaseOperations();
    groupIt->deactivationOperations =
        deactivationBuilder.buildPhaseOperations();
    groupIt->shuttlingOperation = MoveGroup::buildShuttlingOperation(
        activationBuilder, deactivationBuilder);
  }
}

std::pair<std::vector<AtomMove>, MoveToAodConverter::MoveGroup>
MoveToAodConverter::processMoves(
    const std::vector<std::pair<AtomMove, uint32_t>>& moves,
    AodTransitionBuilder& activationBuilder,
    AodTransitionBuilder& deactivationBuilder) const {

  MoveGroup possibleNewMoveGroup;
  std::vector<AtomMove> movesToRemove;
  for (const auto& movePair : moves) {
    const auto& move = movePair.first;
    const auto circuitIndex = movePair.second;
    auto origin = arch.getCoordinate(move.origin);
    auto target = arch.getCoordinate(move.target);
    const auto moveVector = arch.getVector(move.origin, move.target);
    const auto reverseMoveVector = arch.getVector(move.target, move.origin);
    const auto xMergeTypes = canAddTransition(
        activationBuilder, deactivationBuilder, origin, moveVector, target,
        reverseMoveVector, AodOperation::Dimension::X);
    const auto yMergeTypes = canAddTransition(
        activationBuilder, deactivationBuilder, origin, moveVector, target,
        reverseMoveVector, AodOperation::Dimension::Y);
    const DimensionMergeTypes activationMergeTypes{.x = xMergeTypes.activation,
                                                   .y = yMergeTypes.activation};
    const DimensionMergeTypes deactivationMergeTypes{
        .x = xMergeTypes.deactivation, .y = yMergeTypes.deactivation};
    if (activationMergeTypes.x == TransitionMergeType::Impossible ||
        activationMergeTypes.y == TransitionMergeType::Impossible ||
        deactivationMergeTypes.x == TransitionMergeType::Impossible ||
        deactivationMergeTypes.y == TransitionMergeType::Impossible) {
      // move could not be added as not sufficient intermediate levels
      // add new move group and add move to it
      possibleNewMoveGroup.addMove(move, circuitIndex);
      movesToRemove.emplace_back(move);
    } else {
      activationBuilder.addTransition(activationMergeTypes, origin, move,
                                      moveVector, move.requiresLoad);
      deactivationBuilder.addTransition(deactivationMergeTypes, target, move,
                                        reverseMoveVector, move.requiresStore);
    }
  }

  return {movesToRemove, possibleNewMoveGroup};
}
void MoveToAodConverter::processFlyingAncillaMoves(
    const std::vector<std::pair<AtomMove, uint32_t>>& flyingAncillaMoves,
    AodTransitionBuilder& activationBuilder,
    AodTransitionBuilder& deactivationBuilder) const {
  for (const auto& key : flyingAncillaMoves | std::views::keys) {
    const auto& flyingAncillaMove = key;
    auto origin = arch.getCoordinate(flyingAncillaMove.origin);
    auto target = arch.getCoordinate(flyingAncillaMove.target);
    const auto moveVector =
        arch.getVector(flyingAncillaMove.origin, flyingAncillaMove.target);
    const auto reverseMoveVector =
        arch.getVector(flyingAncillaMove.target, flyingAncillaMove.origin);

    activationBuilder.addFlyingAncillaTransition(
        origin, flyingAncillaMove, moveVector, flyingAncillaMove.requiresLoad);
    deactivationBuilder.addFlyingAncillaTransition(
        target, flyingAncillaMove, reverseMoveVector,
        flyingAncillaMove.requiresStore);
  }
}

AodOperation MoveToAodConverter::MoveGroup::buildShuttlingOperation(
    const AodTransitionBuilder& activationBuilder,
    const AodTransitionBuilder& deactivationBuilder) {
  // For each initial operation, find the corresponding final operation
  // and connect with an aod move operations
  // all can be done in parallel in a single move
  std::vector<AodOperation::Segment> segments;
  std::vector<CoordIndex> targets;

  const auto d = activationBuilder.arch->getInterQubitDistance();
  const auto interD = activationBuilder.arch->getInterQubitDistance() /
                      activationBuilder.arch->getNAodIntermediateLevels();

  constexpr std::array dimensions{AodOperation::Dimension::X,
                                  AodOperation::Dimension::Y};

  // connect move operations
  for (const auto& activation : activationBuilder.transitions) {
    for (const auto& deactivation : deactivationBuilder.transitions) {
      if (activation.moves == deactivation.moves) {
        // get target qubits
        qc::Targets starts;
        qc::Targets ends;
        const auto nPos = activationBuilder.arch->getNpositions();
        for (const auto& move : activation.moves) {
          if (move.requiresLoad) {
            starts.emplace_back(move.origin);
          } else if (move.requiresStore) {
            starts.emplace_back(move.origin + nPos);
          } else {
            starts.emplace_back(move.origin + (2 * nPos));
          }
          if (move.requiresStore) {
            ends.emplace_back(move.target);
          } else if (move.requiresLoad) {
            ends.emplace_back(move.target + nPos);
          } else {
            ends.emplace_back(move.target + (2 * nPos));
          }
        }

        // Ensure that the ordering of the target qubits such that atoms are
        // moved away before used as a target
        for (size_t i = 0; i < starts.size(); i++) {
          const auto pos = std::ranges::find(targets, starts[i]);
          if (pos == targets.end()) {
            // if the start qubit is not already in the target qubits
            targets.emplace_back(starts[i]);
            targets.emplace_back(ends[i]);
          } else {
            // insert the (end, start) pair immediately before the existing
            // start
            const auto newPos = targets.insert(pos, ends[i]);
            targets.insert(newPos, starts[i]);
          }
        }

        for (const auto& dimension : dimensions) {
          const auto& activationMoves = activation.getDimensionMoves(dimension);
          const auto& deactivationMoves =
              deactivation.getDimensionMoves(dimension);
          for (size_t i = 0; i < activationMoves.size(); i++) {
            const auto& start = activationMoves[i]->initialPosition * d +
                                activationMoves[i]->offset * interD;
            const auto& end = deactivationMoves[i]->initialPosition * d +
                              deactivationMoves[i]->offset * interD;
            if (std::abs(start - end) > 0.0001) {
              segments.emplace_back(dimension, start, end);
            }
          }
        }
      }
    }
  }

  return {NeutralAtomOpType::AodMove, targets, segments};
}

std::vector<
    std::shared_ptr<MoveToAodConverter::AodTransitionBuilder::DimensionMove>>
MoveToAodConverter::AodTransitionBuilder::getDimensionMovesFromInitialPosition(
    const AodOperation::Dimension dimension,
    const uint32_t initialPosition) const {
  std::vector<std::shared_ptr<DimensionMove>> dimensionMoves;
  for (const auto& transition : transitions) {
    for (auto& dimensionMove : transition.getDimensionMoves(dimension)) {
      if (dimensionMove->initialPosition == initialPosition) {
        dimensionMoves.emplace_back(dimensionMove);
      }
    }
  }
  return dimensionMoves;
}

uint32_t
MoveToAodConverter::AodTransitionBuilder::getMaxOffsetAtInitialPosition(
    const AodOperation::Dimension dimension, const uint32_t initialPosition,
    const int32_t sign) const {
  const auto dimensionMoves =
      getDimensionMovesFromInitialPosition(dimension, initialPosition);
  uint32_t maxOffset = 0;
  for (const auto& dimensionMove : dimensionMoves) {
    const auto offset = dimensionMove->offset;
    if (offset * sign >= 0) {
      maxOffset = std::max(maxOffset, static_cast<uint32_t>(std::abs(offset)));
    }
  }
  return maxOffset;
}

bool MoveToAodConverter::AodTransitionBuilder::
    hasIntermediateSpaceAtInitialPosition(
        const AodOperation::Dimension dimension, const uint32_t initialPosition,
        const int32_t sign) const {
  uint32_t neighboringPosition = initialPosition;
  if (sign > 0) {
    neighboringPosition += 1;
  } else {
    neighboringPosition -= 1;
  }
  const auto neighboringMoves =
      getDimensionMovesFromInitialPosition(dimension, neighboringPosition);
  if (neighboringMoves.empty()) {
    return getMaxOffsetAtInitialPosition(dimension, initialPosition, sign) <
           arch->getNAodIntermediateLevels();
  }
  return getMaxOffsetAtInitialPosition(dimension, initialPosition, sign) +
             getMaxOffsetAtInitialPosition(dimension, neighboringPosition,
                                           sign) <
         arch->getNAodIntermediateLevels();
}
void MoveToAodConverter::AodTransitionBuilder::computeInitialAndOffsetSegments(
    const AodOperation::Dimension dimension,
    const std::shared_ptr<DimensionMove>& dimensionMove,
    std::vector<AodOperation::Segment>& initialSegments,
    std::vector<AodOperation::Segment>& offsetSegments) const {

  const auto d = arch->getInterQubitDistance();
  const auto interD =
      arch->getInterQubitDistance() / arch->getNAodIntermediateLevels();

  initialSegments.emplace_back(
      dimension, static_cast<qc::fp>(dimensionMove->initialPosition) * d,
      static_cast<qc::fp>(dimensionMove->initialPosition) * d);
  if (phaseOperationType == NeutralAtomOpType::AodActivate) {
    offsetSegments.emplace_back(
        dimension, static_cast<qc::fp>(dimensionMove->initialPosition) * d,
        static_cast<qc::fp>(dimensionMove->initialPosition) * d +
            static_cast<qc::fp>(dimensionMove->offset) * interD);
  } else {
    offsetSegments.emplace_back(
        dimension,
        static_cast<qc::fp>(dimensionMove->initialPosition) * d +
            static_cast<qc::fp>(dimensionMove->offset) * interD,
        static_cast<qc::fp>(dimensionMove->initialPosition) * d);
  }
}

void MoveToAodConverter::AodTransitionBuilder::mergeTransitionDimension(
    const AodOperation::Dimension dimension,
    const Transition& dimensionTransition,
    const Transition& complementaryDimensionTransition) {
  for (auto& currentTransition : transitions) {
    const auto& dimensionMoves = currentTransition.getDimensionMoves(dimension);
    for (const auto& dimensionMove : dimensionMoves) {
      const auto& matchingMove =
          dimensionTransition.getDimensionMoves(dimension)[0];
      if (dimensionMove->initialPosition == matchingMove->initialPosition &&
          dimensionMove->delta == matchingMove->delta) {
        // append move
        currentTransition.moves.emplace_back(dimensionTransition.moves[0]);
        // add the move in the complementary dimension
        if (dimension == AodOperation::Dimension::X) {
          currentTransition.yMoves.emplace_back(
              complementaryDimensionTransition.yMoves[0]);
        } else {
          currentTransition.xMoves.emplace_back(
              complementaryDimensionTransition.xMoves[0]);
        }
        return;
      }
    }
  }
}

std::vector<AodOperation>
MoveToAodConverter::AodTransitionBuilder::buildPhaseOperations(
    const Transition& transition) const {
  CoordIndices transitionTargets;
  transitionTargets.reserve(transition.moves.size());
  for (const auto& move : transition.moves) {
    if (phaseOperationType == NeutralAtomOpType::AodActivate) {
      if (move.requiresLoad) {
        transitionTargets.emplace_back(move.origin);
      }
    } else {
      if (move.requiresStore) {
        transitionTargets.emplace_back(move.target);
      }
    }
  }
  CoordIndices offsetTargets;
  offsetTargets.reserve(transition.moves.size() * 2);
  for (const auto& target : transitionTargets) {
    offsetTargets.emplace_back(target);
    offsetTargets.emplace_back(target);
  }

  std::vector<AodOperation::Segment> initialSegments;
  std::vector<AodOperation::Segment> offsetSegments;

  for (const auto& dimensionMove : transition.xMoves) {
    if (dimensionMove->requiresAtomTransfer) {
      computeInitialAndOffsetSegments(AodOperation::Dimension::X, dimensionMove,
                                      initialSegments, offsetSegments);
    }
  }
  for (const auto& dimensionMove : transition.yMoves) {
    if (dimensionMove->requiresAtomTransfer) {
      computeInitialAndOffsetSegments(AodOperation::Dimension::Y, dimensionMove,
                                      initialSegments, offsetSegments);
    }
  }
  if (initialSegments.empty() && offsetSegments.empty()) {
    return {};
  }

  return {
      AodOperation(phaseOperationType, transitionTargets, initialSegments),
      AodOperation(NeutralAtomOpType::AodMove, offsetTargets, offsetSegments)};
}

std::vector<AodOperation>
MoveToAodConverter::AodTransitionBuilder::buildPhaseOperations() const {
  std::vector<AodOperation> aodOperations;
  for (const auto& transition : transitions) {
    auto operations = buildPhaseOperations(transition);
    // insert ancilla dodging operations
    aodOperations.insert(aodOperations.end(), operations.begin(),
                         operations.end());
  }
  if (phaseOperationType == NeutralAtomOpType::AodActivate) {
    return aodOperations;
  }
  std::ranges::reverse(aodOperations);
  return aodOperations;
}
} // namespace na
