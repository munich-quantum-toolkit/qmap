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

#include "NeutralAtomOperationPrinting.hpp"
#include "ir/Definitions.hpp"
#include "ir/Register.hpp"
#include "ir/operations/OpType.hpp"
#include "ir/operations/Operation.hpp"

#include <cstddef>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace na {

NAStandardOperation::NAStandardOperation(
    const NeutralAtomOpType newNeutralAtomOpType, qc::Targets operationTargets)
    : qc::StandardOperation(operationTargets, qc::OpType::None),
      neutralAtomOpType(validateType(newNeutralAtomOpType)) {
  name = toString(neutralAtomOpType);
}

auto NAStandardOperation::validateType(
    const NeutralAtomOpType candidateNeutralAtomOpType) -> NeutralAtomOpType {
  if (candidateNeutralAtomOpType == NeutralAtomOpType::Move ||
      candidateNeutralAtomOpType == NeutralAtomOpType::Bridge) {
    return candidateNeutralAtomOpType;
  }
  throw std::invalid_argument(
      "A standard neutral-atom operation must be a move or bridge.");
}

auto NAStandardOperation::equals(const qc::Operation& operation,
                                 const qc::Permutation& permutation1,
                                 const qc::Permutation& permutation2) const
    -> bool {
  const auto* other = dynamic_cast<const NAStandardOperation*>(&operation);
  return other != nullptr && neutralAtomOpType == other->neutralAtomOpType &&
         qc::StandardOperation::equals(operation, permutation1, permutation2);
}

auto NAStandardOperation::equals(const qc::Operation& operation) const -> bool {
  return equals(operation, {}, {});
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

  const auto otherNeutralAtomOpType = na::getNeutralAtomOpType(other);
  if (otherNeutralAtomOpType != neutralAtomOpType ||
      targets != other.getTargets()) {
    return false;
  }
  return parameter.size() <= 1 || parameter == other.getParameter();
}

auto NAStandardOperation::print(std::ostream& os,
                                const qc::Permutation& permutation,
                                const std::size_t prefixWidth,
                                const std::size_t nQubits) const
    -> std::ostream& {
  return detail::printNeutralAtomOperation(*this, neutralAtomOpType, os,
                                           permutation, prefixWidth, nQubits);
}

void NAStandardOperation::dumpOpenQASM(
    std::ostream& of, const qc::QubitIndexToRegisterMap& qubitMap,
    [[maybe_unused]] const qc::BitIndexToRegisterMap& bitMap,
    const std::size_t indent, [[maybe_unused]] const bool openQASM3) const {
  of << std::string(indent * OUTPUT_INDENT_SIZE, ' ') << name;
  for (std::size_t i = 0; i < targets.size(); ++i) {
    of << (i == 0 ? " " : ", ") << qubitMap.at(targets[i]).second;
  }
  of << ";\n";
}

void NAStandardOperation::invert() {
  if (neutralAtomOpType == NeutralAtomOpType::Move) {
    if (targets.size() != 2) {
      throw std::invalid_argument("A move operation requires two targets.");
    }
    std::swap(targets.front(), targets.back());
  }
}

auto makeMoveOperation(const qc::Qubit origin, const qc::Qubit target)
    -> std::unique_ptr<qc::Operation> {
  return std::make_unique<NAStandardOperation>(NeutralAtomOpType::Move,
                                               qc::Targets{origin, target});
}

auto makeBridgeOperation(qc::Targets targets)
    -> std::unique_ptr<qc::Operation> {
  return std::make_unique<NAStandardOperation>(NeutralAtomOpType::Bridge,
                                               std::move(targets));
}

} // namespace na
