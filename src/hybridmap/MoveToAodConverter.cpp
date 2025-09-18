/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
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
#include "ir/operations/AodOperation.hpp"
#include "ir/operations/OpType.hpp"
#include "na/entities/Location.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

namespace na {

qc::QuantumComputation
MoveToAodConverter::schedule(qc::QuantumComputation& qc) {
  initFlyingAncillas();
  initMoveGroups(qc);
  if (moveGroups.empty()) {
    return qc;
  }
  processMoveGroups();
  // postProcessMoveGroups();

  // create new quantum circuit and insert AOD operations at the correct
  // indices
  auto groupIt = moveGroups.begin();
  uint32_t idx = 0;
  for (const auto& op : qc) {
    if (groupIt != moveGroups.end() && idx == groupIt->getFirstIdx()) {
      // add move group
      for (auto& aodOp : groupIt->processedOpsInit) {
        qcScheduled.emplace_back(std::make_unique<AodOperation>(aodOp));
      }
      qcScheduled.emplace_back(
          std::make_unique<AodOperation>(groupIt->processedOpShuttle));
      for (auto& aodOp : groupIt->processedOpsFinal) {
        qcScheduled.emplace_back(std::make_unique<AodOperation>(aodOp));
      }
      groupIt++;
    } else if (op->getType() != qc::OpType::Move) {
      qcScheduled.emplace_back(op->clone());
    }
    idx++;
  }

  return qcScheduled;
}

AtomMove MoveToAodConverter::convertOpToMove(qc::Operation* get) {
  auto q1 = get->getTargets().front();
  auto q2 = get->getTargets().back();
  const auto load1 = q1 < arch.getNpositions();
  const auto load2 = q2 < arch.getNpositions();
  while (q1 >= arch.getNpositions()) {
    q1 -= arch.getNpositions();
  }
  while (q2 >= arch.getNpositions()) {
    q2 -= arch.getNpositions();
  }
  return {q1, q2, load1, load2};
}
void MoveToAodConverter::initFlyingAncillas() {
  std::vector<CoordIndex> coords;
  std::vector<Dimension> dirs;
  std::vector<qc::fp> starts;
  std::vector<qc::fp> ends;
  std::set<std::uint32_t> rowsActivated;
  std::set<std::uint32_t> columnsActivated;
  for (const auto& ancilla : ancillas) {
    auto coord = ancilla.coord.x + (ancilla.coord.y * arch.getNcolumns());
    const auto offsets = ancilla.offset;
    coords.emplace_back(coord);
    coord -= 2 * arch.getNpositions();
    const auto column = (coord % arch.getNcolumns());
    const auto row = (coord / arch.getNcolumns());

    const auto offset =
        arch.getInterQubitDistance() / arch.getNAodIntermediateLevels();
    columnsActivated.insert(column);
    const auto x = column * arch.getInterQubitDistance() + (offsets.x * offset);
    dirs.emplace_back(Dimension::X);
    starts.emplace_back(x);
    ends.emplace_back(x);
    rowsActivated.insert(row);
    const auto y = row * arch.getInterQubitDistance() + (offsets.y * offset);
    dirs.emplace_back(Dimension::Y);
    starts.emplace_back(y);
    ends.emplace_back(y);
  }
  const AodOperation aodInit(qc::OpType::AodActivate, coords, dirs, starts,
                             ends);
  qcScheduled.emplace_back(std::make_unique<AodOperation>(aodInit));
}

void MoveToAodConverter::initMoveGroups(qc::QuantumComputation& qc) {
  MoveGroup currentMoveGroup;
  MoveGroup const lastMoveGroup;
  uint32_t idx = 0;
  for (auto& op : qc) {
    if (op->getType() == qc::OpType::Move) {
      const auto move = convertOpToMove(op.get());
      if (currentMoveGroup.canAddMove(move, arch)) {
        currentMoveGroup.addMove(move, idx);
      } else {
        moveGroups.emplace_back(currentMoveGroup);
        currentMoveGroup = MoveGroup();
        currentMoveGroup.addMove(move, idx);
      }
    } else if (!currentMoveGroup.moves.empty() ||
               !currentMoveGroup.movesFa.empty()) {
      for (const auto& qubit : op->getUsedQubits()) {
        if (std::ranges::find(currentMoveGroup.qubitsUsedByGates, qubit) ==
            currentMoveGroup.qubitsUsedByGates.end()) {
          currentMoveGroup.qubitsUsedByGates.emplace_back(qubit);
        }
      }
    }
    idx++;
  }
  if (!currentMoveGroup.moves.empty() || !currentMoveGroup.movesFa.empty()) {
    moveGroups.emplace_back(std::move(currentMoveGroup));
  }
}

bool MoveToAodConverter::MoveGroup::canAddMove(
    const AtomMove& move, const NeutralAtomArchitecture& archArg) {
  // if move would move a qubit that is used by a gate in this move group
  // return false
  if (std::ranges::find(qubitsUsedByGates, move.c1) !=
      qubitsUsedByGates.end()) {
    return false;
  }
  // checks if the op can be executed in parallel
  std::vector<std::pair<AtomMove, uint32_t>>* movesToCheck;
  if (move.load1 || move.load2) {
    movesToCheck = &moves;
  } else {
    movesToCheck = &movesFa;
  }
  return std::ranges::all_of(
      *movesToCheck,
      [&move, &archArg](const std::pair<AtomMove, uint32_t> opPair) {
        auto moveGroup = opPair.first;
        // check that passby and move are not in same group
        if (move.load1 != moveGroup.load1 || move.load2 != moveGroup.load2) {
          return false;
        }
        // if start or end is same -> false
        if (move.c1 == moveGroup.c1 || move.c2 == moveGroup.c2) {
          return false;
        }
        // check if parallel executable
        auto moveVector = archArg.getVector(move.c1, move.c2);
        auto opVector = archArg.getVector(moveGroup.c1, moveGroup.c2);
        return parallelCheck(moveVector, opVector);
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
                                            const uint32_t idx) {
  if (move.load1 || move.load2) {
    moves.emplace_back(move, idx);
  } else {
    movesFa.emplace_back(move, idx);
  }
  qubitsUsedByGates.emplace_back(move.c2);
}

void MoveToAodConverter::AodActivationHelper::addActivation(
    std::pair<ActivationMergeType, ActivationMergeType> merge,
    const Location& origin, const AtomMove& move, MoveVector v, bool needLoad) {
  const auto x = static_cast<std::uint32_t>(origin.x);
  const auto y = static_cast<std::uint32_t>(origin.y);
  const auto signX = v.direction.getSignX();
  const auto signY = v.direction.getSignY();
  const auto deltaX = v.xEnd - v.xStart;
  const auto deltaY = v.yEnd - v.yStart;
  auto aodMovesX = getAodMovesFromInit(Dimension::X, x);
  auto aodMovesY = getAodMovesFromInit(Dimension::Y, y);

  switch (merge.first) {
  case ActivationMergeType::Trivial:
    switch (merge.second) {
    case ActivationMergeType::Trivial:
      allActivations.emplace_back(AodActivation{
          {x, deltaX, signX, needLoad}, {y, deltaY, signY, needLoad}, move});
      break;
    case ActivationMergeType::Merge:
      mergeActivationDim(
          Dimension::Y,
          AodActivation{Dimension::Y, {y, deltaY, signY, needLoad}, move},
          AodActivation{Dimension::X, {x, deltaX, signX, needLoad}, move});
      aodMovesY = getAodMovesFromInit(Dimension::Y, y);
      reAssignOffsets(aodMovesY, signY);
      break;
    case ActivationMergeType::Append:
      allActivations.emplace_back(AodActivation{
          {x, deltaX, signX, needLoad}, {y, deltaY, signY, needLoad}, move});
      aodMovesY = getAodMovesFromInit(Dimension::Y, y);
      reAssignOffsets(aodMovesY, signY);
      break;
    default:
      break;
    }
    break;
  case ActivationMergeType::Merge:
    switch (merge.second) {
    case ActivationMergeType::Trivial:
      mergeActivationDim(
          Dimension::X,
          AodActivation{Dimension::X, {x, deltaX, signX, needLoad}, move},
          AodActivation{Dimension::Y, {y, deltaY, signY, needLoad}, move});
      aodMovesX = getAodMovesFromInit(Dimension::X, x);
      reAssignOffsets(aodMovesX, signX);
      break;
    case ActivationMergeType::Merge:
      throw std::runtime_error("Merge in both dimensions should never happen.");
    case ActivationMergeType::Append:
      mergeActivationDim(
          Dimension::X,
          AodActivation{Dimension::X, {x, deltaX, signX, needLoad}, move},
          AodActivation{Dimension::Y, {y, deltaY, signY, needLoad}, move});
      aodMovesY = getAodMovesFromInit(Dimension::Y, y);
      reAssignOffsets(aodMovesY, signY);
      break;
    default:
      break;
    }
    break;
  case ActivationMergeType::Append:
    switch (merge.second) {
    case ActivationMergeType::Trivial:
      allActivations.emplace_back(AodActivation{
          {x, deltaX, signX, needLoad}, {y, deltaY, signY, needLoad}, move});
      aodMovesX = getAodMovesFromInit(Dimension::X, x);
      reAssignOffsets(aodMovesX, signX);
      break;
    case ActivationMergeType::Merge:
      mergeActivationDim(
          Dimension::Y,
          AodActivation{Dimension::Y, {y, deltaY, signY, needLoad}, move},
          AodActivation{Dimension::X, {x, deltaX, signX, needLoad}, move});
      aodMovesX = getAodMovesFromInit(Dimension::X, x);
      reAssignOffsets(aodMovesX, signX);
      break;
    case ActivationMergeType::Append:
      allActivations.emplace_back(AodActivation{
          {x, deltaX, signX, needLoad}, {y, deltaY, signY, needLoad}, move});
      aodMovesX = getAodMovesFromInit(Dimension::X, x);
      reAssignOffsets(aodMovesX, signX);
      aodMovesY = getAodMovesFromInit(Dimension::Y, y);
      reAssignOffsets(aodMovesY, signY);
      break;
    default:
      break;
    }
    break;
  default:
    break;
  }
}
void MoveToAodConverter::AodActivationHelper::addActivationFa(
    const Location& origin, const AtomMove& move, MoveVector v, bool needLoad) {
  const auto x = static_cast<std::uint32_t>(origin.x);
  const auto y = static_cast<std::uint32_t>(origin.y);
  const auto signX = v.direction.getSignX();
  const auto signY = v.direction.getSignY();
  const auto deltaX = v.xEnd - v.xStart;
  const auto deltaY = v.yEnd - v.yStart;

  allActivations.emplace_back(AodActivation{
      {x, deltaX, signX, needLoad}, {y, deltaY, signY, needLoad}, move});
}

[[nodiscard]] std::pair<ActivationMergeType, ActivationMergeType>
MoveToAodConverter::canAddActivation(
    const AodActivationHelper& activationHelper,
    const AodActivationHelper& deactivationHelper, const Location& origin,
    const MoveVector& v, const Location& final, const MoveVector& vReverse,
    Dimension dim) {
  auto start =
      static_cast<std::uint32_t>(dim == Dimension::X ? origin.x : origin.y);
  auto end =
      static_cast<std::uint32_t>(dim == Dimension::X ? final.x : final.y);
  const qc::fp delta = static_cast<qc::fp>(end - start);

  // Get Moves that start/end at the same position as the current move
  auto aodMovesActivation = activationHelper.getAodMovesFromInit(dim, start);
  auto aodMovesDeactivation = deactivationHelper.getAodMovesFromInit(dim, end);

  // both empty
  if (aodMovesActivation.empty() && aodMovesDeactivation.empty()) {
    return std::make_pair(ActivationMergeType::Trivial,
                          ActivationMergeType::Trivial);
  }
  // one empty
  if (aodMovesActivation.empty()) {
    if (deactivationHelper.checkIntermediateSpaceAtInit(
            dim, end, vReverse.direction.getSign(dim))) {
      return std::make_pair(ActivationMergeType::Trivial,
                            ActivationMergeType::Append);
    }
    return std::make_pair(ActivationMergeType::Trivial,
                          ActivationMergeType::Impossible);
  }
  if (aodMovesDeactivation.empty()) {
    if (activationHelper.checkIntermediateSpaceAtInit(
            dim, start, v.direction.getSign(dim))) {
      return std::make_pair(ActivationMergeType::Append,
                            ActivationMergeType::Trivial);
    }
    return std::make_pair(ActivationMergeType::Impossible,
                          ActivationMergeType::Trivial);
  }
  // both not empty
  // if same moves exist -> merge, else append
  for (const auto& aodMoveActivation : aodMovesActivation) {
    for (const auto& aodMoveDeactivation : aodMovesDeactivation) {
      if (aodMoveActivation->init == start &&
          aodMoveDeactivation->init == end &&
          std::abs(aodMoveActivation->delta) == std::abs(delta) &&
          std::abs(aodMoveDeactivation->delta) == std::abs(delta)) {
        return std::make_pair(ActivationMergeType::Merge,
                              ActivationMergeType::Merge);
      }
    }
  }
  if (activationHelper.checkIntermediateSpaceAtInit(dim, start,
                                                    v.direction.getSign(dim)) &&
      deactivationHelper.checkIntermediateSpaceAtInit(
          dim, end, vReverse.direction.getSign(dim))) {
    return std::make_pair(ActivationMergeType::Append,
                          ActivationMergeType::Append);
  }
  return std::make_pair(ActivationMergeType::Impossible,
                        ActivationMergeType::Impossible);
}

void MoveToAodConverter::AodActivationHelper::reAssignOffsets(
    std::vector<std::shared_ptr<AodMove>>& aodMoves, int32_t sign) {
  std::ranges::sort(aodMoves, [](const std::shared_ptr<AodMove>& a,
                                 const std::shared_ptr<AodMove>& b) {
    return std::abs(a->delta) < std::abs(b->delta);
  });
  int32_t offset = sign;
  for (auto& aodMove : aodMoves) {
    // same sign
    if (aodMove->delta * sign >= 0) {
      aodMove->offset = offset;
      offset += sign;
    }
  }
}

void MoveToAodConverter::processMoveGroups() {
  // convert the moves from MoveGroup to AodOperations
  for (auto groupIt = moveGroups.begin(); groupIt != moveGroups.end();
       ++groupIt) {
    AodActivationHelper aodActivationHelper{arch, qc::OpType::AodActivate,
                                            (&ancillas)};
    AodActivationHelper aodDeactivationHelper{arch, qc::OpType::AodDeactivate,
                                              (&ancillas)};

    const auto resultMoves = processMoves(groupIt->moves, aodActivationHelper,
                                          aodDeactivationHelper);
    auto movesToRemove = resultMoves.first;
    auto possibleNewMoveGroup = resultMoves.second;

    processMovesFa(groupIt->movesFa, aodActivationHelper,
                   aodDeactivationHelper);

    // remove from current move group
    for (const auto& moveToRemove : movesToRemove) {
      groupIt->moves.erase(
          std::remove_if(groupIt->moves.begin(), groupIt->moves.end(),
                         [&moveToRemove](const auto& movePair) {
                           return movePair.first == moveToRemove;
                         }),
          groupIt->moves.end());
    }
    if (!possibleNewMoveGroup.moves.empty()) {
      groupIt =
          moveGroups.emplace(groupIt + 1, std::move(possibleNewMoveGroup));
      possibleNewMoveGroup = MoveGroup();
      groupIt--;
    }
    groupIt->processedOpsInit = aodActivationHelper.getAodOperations();
    groupIt->processedOpsFinal = aodDeactivationHelper.getAodOperations();
    groupIt->processedOpShuttle = MoveGroup::connectAodOperations(
        aodActivationHelper, aodDeactivationHelper);
  }
}
void MoveToAodConverter::postProcessMoveGroups() {
  for (auto groupIt = moveGroups.begin(); groupIt != moveGroups.end() - 1;
       ++groupIt) {
    auto nextGroupIt = groupIt + 1;
    for (const auto& move : groupIt->moves) {
      for (const auto& moveNext : nextGroupIt->moves) {
        if (move.first.c2 == moveNext.first.c1 && move.first.load2 &&
            moveNext.first.load1) {
          // moveNext is dependent on move
          // moveNext can only be executed
          if (groupIt->processedOpsFinal.size() == 2 &&
              groupIt->processedOpsInit.size() == 2) {
            groupIt->processedOpsFinal.pop_back();
            // next remove first move from next group
            nextGroupIt->processedOpsInit.erase(
                nextGroupIt->processedOpsInit.begin());
          }
        }
      }
    }
  }
}

std::pair<std::vector<AtomMove>, MoveToAodConverter::MoveGroup>
MoveToAodConverter::processMoves(
    const std::vector<std::pair<AtomMove, uint32_t>>& moves,
    AodActivationHelper& aodActivationHelper,
    AodActivationHelper& aodDeactivationHelper) {

  MoveGroup possibleNewMoveGroup;
  std::vector<AtomMove> movesToRemove;
  for (auto& movePair : moves) {
    auto& move = movePair.first;
    const auto idx = movePair.second;
    auto origin = arch.getCoordinate(move.c1);
    auto target = arch.getCoordinate(move.c2);
    auto v = arch.getVector(move.c1, move.c2);
    auto vReverse = arch.getVector(move.c2, move.c1);
    auto canAddX = canAddActivation(aodActivationHelper, aodDeactivationHelper,
                                    origin, v, target, vReverse, Dimension::X);
    auto canAddY = canAddActivation(aodActivationHelper, aodDeactivationHelper,
                                    origin, v, target, vReverse, Dimension::Y);
    auto activationCanAddXY = std::make_pair(canAddX.first, canAddY.first);
    auto deactivationCanAddXY = std::make_pair(canAddX.second, canAddY.second);
    if (activationCanAddXY.first == ActivationMergeType::Impossible ||
        activationCanAddXY.second == ActivationMergeType::Impossible ||
        deactivationCanAddXY.first == ActivationMergeType::Impossible ||
        deactivationCanAddXY.second == ActivationMergeType::Impossible) {
      // move could not be added as not sufficient intermediate levels
      // add new move group and add move to it
      possibleNewMoveGroup.addMove(move, idx);
      movesToRemove.emplace_back(move);
    } else {
      aodActivationHelper.addActivation(activationCanAddXY, origin, move, v,
                                        move.load1);
      aodDeactivationHelper.addActivation(deactivationCanAddXY, target, move,
                                          vReverse, move.load2);
    }
  }

  return {movesToRemove, possibleNewMoveGroup};
}
void MoveToAodConverter::processMovesFa(
    const std::vector<std::pair<AtomMove, uint32_t>>& movesFa,
    AodActivationHelper& aodActivationHelper,
    AodActivationHelper& aodDeactivationHelper) const {
  for (const auto& moveFaPair : movesFa) {
    const auto& moveFa = moveFaPair.first;
    const auto idx = moveFaPair.second;
    auto origin = arch.getCoordinate(moveFa.c1);
    auto target = arch.getCoordinate(moveFa.c2);
    const auto v = arch.getVector(moveFa.c1, moveFa.c2);
    const auto vReverse = arch.getVector(moveFa.c2, moveFa.c1);

    aodActivationHelper.addActivationFa(origin, moveFa, v, moveFa.load1);
    aodDeactivationHelper.addActivationFa(target, moveFa, vReverse,
                                          moveFa.load2);
  }
}

AodOperation MoveToAodConverter::MoveGroup::connectAodOperations(
    const AodActivationHelper& aodActivationHelper,
    const AodActivationHelper& aodDeactivationHelper) {
  // for each init operation find the corresponding final operation
  // and connect with an aod move operations
  // all can be done in parallel in a single move
  std::vector<SingleOperation> aodOperations;
  std::vector<CoordIndex> targetQubits;

  auto d = aodActivationHelper.arch->getInterQubitDistance();
  auto interD = aodActivationHelper.arch->getInterQubitDistance() /
                aodActivationHelper.arch->getNAodIntermediateLevels();

  std::vector<na::Dimension> dimensions = {na::Dimension::X, na::Dimension::Y};

  // connect move operations
  for (const auto& activation : aodActivationHelper.allActivations) {
    for (const auto& deactivation : aodDeactivationHelper.allActivations) {
      if (activation.moves == deactivation.moves) {
        // get target qubits
        qc::Targets starts;
        qc::Targets ends;
        const auto nPos = aodActivationHelper.arch->getNpositions();
        for (const auto& move : activation.moves) {
          if (move.load1) {
            starts.emplace_back(move.c1);
          } else if (move.load2) {
            starts.emplace_back(move.c1 + nPos);
          } else {
            starts.emplace_back(move.c1 + (2 * nPos));
          }
          if (move.load2) {
            ends.emplace_back(move.c2);
          } else if (move.load1) {
            ends.emplace_back(move.c2 + nPos);
          } else {
            ends.emplace_back(move.c2 + (2 * nPos));
          }
        }

        // Ensure that the ordering of the target qubits such that atoms are
        // moved away before used as a target
        for (size_t i = 0; i < starts.size(); i++) {
          const auto pos =
              std::find(targetQubits.begin(), targetQubits.end(), starts[i]);
          if (pos == targetQubits.end()) {
            // if the start qubit is not already in the target qubits
            targetQubits.emplace_back(starts[i]);
            targetQubits.emplace_back(ends[i]);
          } else {
            // insert before one before the found position
            const auto newPos = targetQubits.insert(pos - 1, ends[i]);
            targetQubits.insert(newPos, starts[i]);
          }
        }

        for (const auto& dim : dimensions) {
          const auto& activationDim = activation.getActivates(dim);
          const auto& deactivationDim = deactivation.getActivates(dim);
          for (size_t i = 0; i < activationDim.size(); i++) {
            const auto& start =
                activationDim[i]->init * d + activationDim[i]->offset * interD;
            const auto& end = deactivationDim[i]->init * d +
                              deactivationDim[i]->offset * interD;
            if (std::abs(start - end) > 0.0001) {
              if (start > 10000) {
                int i = 0;
              }
              aodOperations.emplace_back(dim, start, end);
            }
          }
        }
      }
    }
  }

  return {qc::OpType::AodMove, targetQubits, aodOperations};
}

std::vector<std::shared_ptr<MoveToAodConverter::AodActivationHelper::AodMove>>
MoveToAodConverter::AodActivationHelper::getAodMovesFromInit(
    Dimension dim, uint32_t init) const {
  std::vector<std::shared_ptr<AodMove>> aodMoves;
  for (const auto& activation : allActivations) {
    for (auto& aodMove : activation.getActivates(dim)) {
      if (aodMove->init == init) {
        aodMoves.emplace_back(aodMove);
      }
    }
  }
  return aodMoves;
}

uint32_t MoveToAodConverter::AodActivationHelper::getMaxOffsetAtInit(
    Dimension dim, uint32_t init, int32_t sign) const {
  auto aodMoves = getAodMovesFromInit(dim, init);
  if (aodMoves.empty()) {
    return 0;
  }
  uint32_t maxOffset = 0;
  for (const auto& aodMove : aodMoves) {
    auto offset = aodMove->offset;
    if (offset * sign >= 0) {
      maxOffset = std::max(maxOffset, static_cast<uint32_t>(std::abs(offset)));
    }
  }
  return maxOffset;
}

bool MoveToAodConverter::AodActivationHelper::checkIntermediateSpaceAtInit(
    Dimension dim, uint32_t init, int32_t sign) const {
  uint32_t neighborX = init;
  if (sign > 0) {
    neighborX += 1;
  } else {
    neighborX -= 1;
  }
  auto aodMoves = getAodMovesFromInit(dim, init);
  auto aodMovesNeighbor = getAodMovesFromInit(dim, neighborX);
  if (aodMoves.empty() && aodMovesNeighbor.empty()) {
    return true;
  }
  if (aodMoves.empty()) {
    return getMaxOffsetAtInit(dim, neighborX, sign) <
           arch->getNAodIntermediateLevels();
  }
  if (aodMovesNeighbor.empty()) {
    return getMaxOffsetAtInit(dim, init, sign) <
           arch->getNAodIntermediateLevels();
  }
  return getMaxOffsetAtInit(dim, init, sign) +
             getMaxOffsetAtInit(dim, neighborX, sign) <
         arch->getNAodIntermediateLevels();
}
void MoveToAodConverter::AodActivationHelper::computeInitAndOffsetOperations(
    Dimension dimension, const std::shared_ptr<AodMove>& aodMove,
    std::vector<SingleOperation>& initOperations,
    std::vector<SingleOperation>& offsetOperations) const {

  auto d = this->arch->getInterQubitDistance();
  auto interD = this->arch->getInterQubitDistance() /
                this->arch->getNAodIntermediateLevels();

  initOperations.emplace_back(dimension, static_cast<qc::fp>(aodMove->init) * d,
                              static_cast<qc::fp>(aodMove->init) * d);
  if (type == qc::OpType::AodActivate) {
    offsetOperations.emplace_back(
        dimension, static_cast<qc::fp>(aodMove->init) * d,
        static_cast<qc::fp>(aodMove->init) * d +
            static_cast<qc::fp>(aodMove->offset) * interD);
  } else {
    offsetOperations.emplace_back(dimension,
                                  static_cast<qc::fp>(aodMove->init) * d +
                                      static_cast<qc::fp>(aodMove->offset) *
                                          interD,
                                  static_cast<qc::fp>(aodMove->init) * d);
  }
}

void MoveToAodConverter::AodActivationHelper::mergeActivationDim(
    Dimension dim, const AodActivation& activationDim,
    const AodActivation& activationOtherDim) {
  // merge activations
  for (auto& activationCurrent : allActivations) {
    auto activates = activationCurrent.getActivates(dim);
    for (auto& aodMove : activates) {
      if (aodMove->init == activationDim.getActivates(dim)[0]->init &&
          aodMove->delta == activationDim.getActivates(dim)[0]->delta) {
        // append move
        activationCurrent.moves.emplace_back(activationDim.moves[0]);
        // add activation in the other dimension
        if (dim == Dimension::X) {
          activationCurrent.activateYs.emplace_back(
              activationOtherDim.activateYs[0]);
        } else {
          activationCurrent.activateXs.emplace_back(
              activationOtherDim.activateXs[0]);
        }
        return;
      }
    }
  }
}

std::vector<AodOperation>
MoveToAodConverter::AodActivationHelper::getAodOperation(
    const AodActivationHelper::AodActivation& activation) const {
  CoordIndices qubitsActivation;
  qubitsActivation.reserve(activation.moves.size());
  for (const auto& move : activation.moves) {
    if (type == qc::OpType::AodActivate) {
      if (move.load1) {
        qubitsActivation.emplace_back(move.c1);
      }
    } else {
      if (move.load2) {
        qubitsActivation.emplace_back(move.c2);
      }
    }
  }
  CoordIndices qubitsOffset;
  qubitsOffset.reserve(activation.moves.size() * 2);
  for (const auto& qubit : qubitsActivation) {
    qubitsOffset.emplace_back(qubit);
    qubitsOffset.emplace_back(qubit);
  }

  std::vector<SingleOperation> initOperations;
  std::vector<SingleOperation> offsetOperations;

  for (const auto& aodMove : activation.activateXs) {
    if (aodMove->load) {
      computeInitAndOffsetOperations(Dimension::X, aodMove, initOperations,
                                     offsetOperations);
    }
  }
  for (const auto& aodMove : activation.activateYs) {
    if (aodMove->load) {
      computeInitAndOffsetOperations(Dimension::Y, aodMove, initOperations,
                                     offsetOperations);
    }
  }
  if (initOperations.empty() && offsetOperations.empty()) {
    return {};
  }
  std::vector<AodOperation> aodOperations;

  if (initOperations.empty()) {
    return {AodOperation(qc::OpType::AodMove, qubitsOffset, offsetOperations)};
  }
  if (offsetOperations.empty()) {
    return {AodOperation(type, qubitsActivation, initOperations)};
  }
  return {AodOperation(type, qubitsActivation, initOperations),
          AodOperation(qc::OpType::AodMove, qubitsOffset, offsetOperations)};
}

std::vector<AodOperation>
MoveToAodConverter::AodActivationHelper::getAodOperations() const {
  std::vector<AodOperation> aodOperations;
  for (const auto& activation : allActivations) {
    auto operations = getAodOperation(activation);
    // insert ancilla dodging operations
    aodOperations.insert(aodOperations.end(), operations.begin(),
                         operations.end());
  }
  if (type == qc::OpType::AodActivate) {
    return aodOperations;
  }
  std::reverse(aodOperations.begin(), aodOperations.end());
  return aodOperations;
}
} // namespace na
