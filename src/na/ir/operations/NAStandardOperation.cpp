/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "na/ir/operations/NAStandardOperation.hpp"

#include "NAOperationPrinting.hpp"
#include "ir/Definitions.hpp"
#include "ir/Permutation.hpp"
#include "ir/operations/OpType.hpp"
#include "ir/operations/Operation.hpp"
#include "na/ir/operations/NAOpType.hpp"

#include <cstddef>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace na {

NAStandardOperation::NAStandardOperation(const NAOpType newNAOpType,
                                         qc::Targets operationTargets)
    : naOpType(validateType(newNAOpType)) {
  targets = std::move(operationTargets);
  type = qc::OpType::None;
  name = toString(naOpType);
}

auto NAStandardOperation::validateType(const NAOpType candidateNAOpType)
    -> NAOpType {
  if (candidateNAOpType == NAOpType::Move ||
      candidateNAOpType == NAOpType::Bridge) {
    return candidateNAOpType;
  }
  throw std::invalid_argument(
      "A standard neutral-atom operation must be a move or bridge.");
}

auto NAStandardOperation::equals(const qc::Operation& operation) const -> bool {
  const auto* other = dynamic_cast<const NAStandardOperation*>(&operation);
  return other != nullptr && naOpType == other->naOpType &&
         qc::Operation::equals(operation);
}

void NAStandardOperation::setGate(const qc::OpType operationType) {
  if (operationType != qc::OpType::None) {
    throw std::invalid_argument(
        "A neutral-atom standard operation cannot change its gate type.");
  }
}

auto NAStandardOperation::commutesAtQubit(const qc::Operation& other,
                                          const qc::Qubit& qubit) const
    -> bool {
  if (other.isCompoundOperation()) {
    return other.commutesAtQubit(*this, qubit);
  }
  if (!actsOn(qubit) || !other.actsOn(qubit)) {
    return true;
  }
  if (controls.contains(qubit)) {
    if (other.getControls().contains(qubit)) {
      return true;
    }
    return other.isDiagonalGate();
  }
  if (other.getControls().contains(qubit)) {
    return isDiagonalGate();
  }
  if (isDiagonalGate() && other.isDiagonalGate()) {
    return true;
  }

  const auto otherNAOpType = na::getNAOpType(other);
  if (otherNAOpType != naOpType || targets != other.getTargets()) {
    return false;
  }
  return parameter.size() <= 1 || parameter == other.getParameter();
}

auto NAStandardOperation::print(std::ostream& os,
                                const qc::Permutation& permutation,
                                const std::size_t prefixWidth,
                                const std::size_t nQubits) const
    -> std::ostream& {
  return detail::printNAOperation(*this, naOpType, os, permutation, prefixWidth,
                                  nQubits);
}

void NAStandardOperation::invert() {
  if (naOpType == NAOpType::Move) {
    if (targets.size() != 2) {
      throw std::invalid_argument("A move operation requires two targets.");
    }
    std::swap(targets.front(), targets.back());
  }
}

auto makeMoveOperation(const qc::Qubit origin, const qc::Qubit target)
    -> std::unique_ptr<qc::Operation> {
  return std::make_unique<NAStandardOperation>(NAOpType::Move,
                                               qc::Targets{origin, target});
}

auto makeBridgeOperation(qc::Targets targets)
    -> std::unique_ptr<qc::Operation> {
  return std::make_unique<NAStandardOperation>(NAOpType::Bridge,
                                               std::move(targets));
}

} // namespace na
