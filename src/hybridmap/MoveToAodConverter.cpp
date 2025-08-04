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
  initMoveGroups(qc);
  if (moveGroups.empty()) {
    return qc;
  }
  processMoveGroups();

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

void MoveToAodConverter::initMoveGroups(qc::QuantumComputation& qc) {
  MoveGroup currentMoveGroup;
  MoveGroup const lastMoveGroup;
  uint32_t idx = 0;
  for (auto& op : qc) {
    if (op->getType() == qc::OpType::Move) {
      AtomMove const move{op->getTargets()[0], op->getTargets()[1]};
      if (currentMoveGroup.canAdd(move, arch)) {
        currentMoveGroup.add(move, idx);
      } else {
        moveGroups.emplace_back(currentMoveGroup);
        currentMoveGroup = MoveGroup();
        currentMoveGroup.add(move, idx);
      }
    } else if (op->getNqubits() > 1 && !currentMoveGroup.moves.empty()) {
      for (const auto& qubit : op->getUsedQubits()) {
        if (std::ranges::find(currentMoveGroup.qubitsUsedByGates, qubit) ==
            currentMoveGroup.qubitsUsedByGates.end()) {
          currentMoveGroup.qubitsUsedByGates.emplace_back(qubit);
        }
      }
    }
    idx++;
  }
  if (!currentMoveGroup.moves.empty()) {
    moveGroups.emplace_back(std::move(currentMoveGroup));
  }
}

bool MoveToAodConverter::MoveGroup::canAdd(
    const AtomMove& move, const NeutralAtomArchitecture& archArg) {
  // if move would move a qubit that is used by a gate in this move group
  // return false
  if (std::ranges::find(qubitsUsedByGates, move.first) !=
      qubitsUsedByGates.end()) {
    return false;
  }
  // checks if the op can be executed in parallel
  auto moveVector = archArg.getVector(move.first, move.second);
  return std::ranges::all_of(
      moves,
      [&moveVector, &archArg](const std::pair<AtomMove, uint32_t> opPair) {
        auto moveGroup = opPair.first;
        auto opVector = archArg.getVector(moveGroup.first, moveGroup.second);
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

void MoveToAodConverter::MoveGroup::add(const AtomMove& move,
                                        const uint32_t idx) {
  moves.emplace_back(move, idx);
  qubitsUsedByGates.emplace_back(move.second);
}

void MoveToAodConverter::AodActivationHelper::addActivation(
    std::pair<ActivationMergeType, ActivationMergeType> merge,
    const Location& origin, const AtomMove& move, MoveVector v) {
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
      allActivations.emplace_back(
          AodActivation{{x, deltaX, signX}, {y, deltaY, signY}, move});
      break;
    case ActivationMergeType::Merge:
      mergeActivationDim(Dimension::Y,
                         AodActivation{Dimension::Y, {y, deltaY, signY}, move},
                         AodActivation{Dimension::X, {x, deltaX, signX}, move});
      aodMovesY = getAodMovesFromInit(Dimension::Y, y);
      reAssignOffsets(aodMovesY, signY);
      break;
    case ActivationMergeType::Append:
      allActivations.emplace_back(
          AodActivation{{x, deltaX, signX}, {y, deltaY, signY}, move});
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
      mergeActivationDim(Dimension::X,
                         AodActivation{Dimension::X, {x, deltaX, signX}, move},
                         AodActivation{Dimension::Y, {y, deltaY, signY}, move});
      aodMovesX = getAodMovesFromInit(Dimension::X, x);
      reAssignOffsets(aodMovesX, signX);
      break;
    case ActivationMergeType::Merge:
      throw std::runtime_error("Merge in both dimensions should never happen.");
    case ActivationMergeType::Append:
      mergeActivationDim(Dimension::X,
                         AodActivation{Dimension::X, {x, deltaX, signX}, move},
                         AodActivation{Dimension::Y, {y, deltaY, signY}, move});
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
      allActivations.emplace_back(
          AodActivation{{x, deltaX, signX}, {y, deltaY, signY}, move});
      aodMovesX = getAodMovesFromInit(Dimension::X, x);
      reAssignOffsets(aodMovesX, signX);
      break;
    case ActivationMergeType::Merge:
      mergeActivationDim(Dimension::Y,
                         AodActivation{Dimension::Y, {y, deltaY, signY}, move},
                         AodActivation{Dimension::X, {x, deltaX, signX}, move});
      aodMovesX = getAodMovesFromInit(Dimension::X, x);
      reAssignOffsets(aodMovesX, signX);
      break;
    case ActivationMergeType::Append:
      allActivations.emplace_back(
          AodActivation{{x, deltaX, signX}, {y, deltaY, signY}, move});
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
          aodMoveDeactivation->init == end) {
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
    AodActivationHelper aodActivationHelper{arch, qc::OpType::AodActivate};
    AodActivationHelper aodDeactivationHelper{arch, qc::OpType::AodDeactivate};
    MoveGroup possibleNewMoveGroup;
    std::vector<AtomMove> movesToRemove;
    for (auto& movePair : groupIt->moves) {
      auto& move = movePair.first;
      auto idx = movePair.second;
      auto origin = arch.getCoordinate(move.first);
      auto target = arch.getCoordinate(move.second);
      auto v = arch.getVector(move.first, move.second);
      auto vReverse = arch.getVector(move.second, move.first);
      auto canAddX =
          canAddActivation(aodActivationHelper, aodDeactivationHelper, origin,
                           v, target, vReverse, Dimension::X);
      auto canAddY =
          canAddActivation(aodActivationHelper, aodDeactivationHelper, origin,
                           v, target, vReverse, Dimension::Y);
      auto activationCanAddXY = std::make_pair(canAddX.first, canAddY.first);
      auto deactivationCanAddXY =
          std::make_pair(canAddX.second, canAddY.second);
      if (activationCanAddXY.first == ActivationMergeType::Impossible ||
          activationCanAddXY.second == ActivationMergeType::Impossible ||
          deactivationCanAddXY.first == ActivationMergeType::Impossible ||
          deactivationCanAddXY.second == ActivationMergeType::Impossible) {
        // move could not be added as not sufficient intermediate levels
        // add new move group and add move to it
        possibleNewMoveGroup.add(move, idx);
        movesToRemove.emplace_back(move);
      } else {
        aodActivationHelper.addActivation(activationCanAddXY, origin, move, v);
        aodDeactivationHelper.addActivation(deactivationCanAddXY, target, move,
                                            vReverse);
      }
    }
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
        groupIt->processedOpsInit, groupIt->processedOpsFinal);
  }
}

AodOperation MoveToAodConverter::MoveGroup::connectAodOperations(
    const std::vector<AodOperation>& opsInit,
    const std::vector<AodOperation>& opsFinal) {
  // for each init operation find the corresponding final operation
  // and connect with an aod move operations
  // all can be done in parallel in a single move
  std::vector<SingleOperation> aodOperations;
  std::set<CoordIndex> targetQubits;

  for (const auto& opInit : opsInit) {
    if (opInit.getType() == qc::OpType::AodMove) {
      for (const auto& opFinal : opsFinal) {
        if (opFinal.getType() == qc::OpType::AodMove) {
          if (opInit.getTargets().size() <= 1 ||
              opFinal.getTargets().size() <= 1) {
            throw std::runtime_error(
                "AodScheduler::MoveGroup::connectAodOperations: "
                "AodMove operation with less than 2 targets");
          }
          if (opInit.getTargets() == opFinal.getTargets()) {
            targetQubits.insert(opInit.getTargets().begin(),
                                opInit.getTargets().end());
            // found corresponding final operation
            // connect with aod move
            const auto startXs = opInit.getEnds(Dimension::X);
            const auto endXs = opFinal.getStarts(Dimension::X);
            const auto startYs = opInit.getEnds(Dimension::Y);
            const auto endYs = opFinal.getStarts(Dimension::Y);
            if (!startXs.empty() && !endXs.empty()) {
              for (size_t i = 0; i < startXs.size(); i++) {
                const auto startX = startXs[i];
                const auto endX = endXs[i];
                if (std::abs(startX - endX) > 0.0001) {
                  aodOperations.emplace_back(Dimension::X, startX, endX);
                }
              }
            }
            if (!startYs.empty() && !endYs.empty()) {
              for (size_t i = 0; i < startYs.size(); i++) {
                const auto startY = startYs[i];
                const auto endY = endYs[i];
                if (std::abs(startY - endY) > 0.0001) {
                  aodOperations.emplace_back(Dimension::Y, startY, endY);
                }
              }
            }
          }
        }
      }
    }
  }
  std::vector<CoordIndex> targetQubitsVec;
  targetQubitsVec.reserve(targetQubits.size());
  for (const auto& qubit : targetQubits) {
    targetQubitsVec.emplace_back(qubit);
  }
  return {qc::OpType::AodMove, targetQubitsVec, aodOperations};
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
           arch.getNAodIntermediateLevels();
  }
  if (aodMovesNeighbor.empty()) {
    return getMaxOffsetAtInit(dim, init, sign) <
           arch.getNAodIntermediateLevels();
  }
  return getMaxOffsetAtInit(dim, init, sign) +
             getMaxOffsetAtInit(dim, neighborX, sign) <
         arch.getNAodIntermediateLevels();
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

std::pair<AodOperation, AodOperation>
MoveToAodConverter::AodActivationHelper::getAodOperation(
    const AodActivationHelper::AodActivation& activation) const {
  std::vector<CoordIndex> qubitsActivation;
  qubitsActivation.reserve(activation.moves.size());
  for (const auto& move : activation.moves) {
    if (type == qc::OpType::AodActivate) {
      qubitsActivation.emplace_back(move.first);
    } else {
      qubitsActivation.emplace_back(move.second);
    }
  }
  std::vector<CoordIndex> qubitsMove;
  qubitsMove.reserve(activation.moves.size() * 2);
  for (const auto& move : activation.moves) {
    if (std::ranges::find(qubitsMove, move.first) == qubitsMove.end()) {
      qubitsMove.emplace_back(move.first);
    }
    if (std::ranges::find(qubitsMove, move.second) == qubitsMove.end()) {
      qubitsMove.emplace_back(move.second);
    }
  }

  std::vector<SingleOperation> initOperations;
  std::vector<SingleOperation> offsetOperations;

  auto d = this->arch.getInterQubitDistance();
  auto interD = this->arch.getInterQubitDistance() /
                this->arch.getNAodIntermediateLevels();

  for (const auto& aodMove : activation.activateXs) {
    initOperations.emplace_back(Dimension::X,
                                static_cast<qc::fp>(aodMove->init) * d,
                                static_cast<qc::fp>(aodMove->init) * d);
    if (type == qc::OpType::AodActivate) {
      offsetOperations.emplace_back(
          Dimension::X, static_cast<qc::fp>(aodMove->init) * d,
          static_cast<qc::fp>(aodMove->init) * d +
              static_cast<qc::fp>(aodMove->offset) * interD);
    } else {
      offsetOperations.emplace_back(Dimension::X,
                                    static_cast<qc::fp>(aodMove->init) * d +
                                        static_cast<qc::fp>(aodMove->offset) *
                                            interD,
                                    static_cast<qc::fp>(aodMove->init) * d);
    }
  }
  for (const auto& aodMove : activation.activateYs) {
    initOperations.emplace_back(Dimension::Y,
                                static_cast<qc::fp>(aodMove->init) * d,
                                static_cast<qc::fp>(aodMove->init) * d);
    if (type == qc::OpType::AodActivate) {
      offsetOperations.emplace_back(
          Dimension::Y, static_cast<qc::fp>(aodMove->init) * d,
          static_cast<qc::fp>(aodMove->init) * d +
              static_cast<qc::fp>(aodMove->offset) * interD);
    } else {
      offsetOperations.emplace_back(Dimension::Y,
                                    static_cast<qc::fp>(aodMove->init) * d +
                                        static_cast<qc::fp>(aodMove->offset) *
                                            interD,
                                    static_cast<qc::fp>(aodMove->init) * d);
    }
  }

  auto initOp = AodOperation(type, qubitsActivation, initOperations);
  auto offsetOp =
      AodOperation(qc::OpType::AodMove, qubitsMove, offsetOperations);
  if (this->type == qc::OpType::AodActivate) {
    return std::make_pair(initOp, offsetOp);
  }
  return std::make_pair(offsetOp, initOp);
}

std::vector<AodOperation>
MoveToAodConverter::AodActivationHelper::getAodOperations() const {
  std::vector<AodOperation> aodOperations;
  for (const auto& activation : allActivations) {
    auto operations = getAodOperation(activation);
    aodOperations.emplace_back(std::move(operations.first));
    aodOperations.emplace_back(std::move(operations.second));
  }
  return aodOperations;
}

} // namespace na
