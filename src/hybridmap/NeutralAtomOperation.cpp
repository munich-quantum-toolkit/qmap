/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "hybridmap/NeutralAtomOperation.hpp"

#include "ir/Definitions.hpp"
#include "ir/operations/OpType.hpp"
#include "ir/operations/Operation.hpp"

#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace na {

std::string_view toString(const NeutralAtomOperationKind kind) {
  switch (kind) {
  case NeutralAtomOperationKind::Move:
    return "move";
  case NeutralAtomOperationKind::Bridge:
    return "bridge";
  case NeutralAtomOperationKind::AodActivate:
    return "aod_activate";
  case NeutralAtomOperationKind::AodDeactivate:
    return "aod_deactivate";
  case NeutralAtomOperationKind::AodMove:
    return "aod_move";
  }
  throw std::invalid_argument("Unknown neutral-atom operation kind.");
}

NeutralAtomOperationKind
neutralAtomOperationKindFromString(const std::string_view name) {
  if (name == "move") {
    return NeutralAtomOperationKind::Move;
  }
  if (name == "bridge") {
    return NeutralAtomOperationKind::Bridge;
  }
  if (name == "aod_activate") {
    return NeutralAtomOperationKind::AodActivate;
  }
  if (name == "aod_deactivate") {
    return NeutralAtomOperationKind::AodDeactivate;
  }
  if (name == "aod_move") {
    return NeutralAtomOperationKind::AodMove;
  }
  throw std::invalid_argument("Unknown neutral-atom operation name: " +
                              std::string(name));
}

NeutralAtomOperation::NeutralAtomOperation(
    const NeutralAtomOperationKind operationKind, qc::Targets operationTargets)
    : NeutralAtomOperation(validateKind(operationKind),
                           std::move(operationTargets), std::in_place) {}

NeutralAtomOperation::NeutralAtomOperation(
    const NeutralAtomOperationKind operationKind, qc::Targets operationTargets,
    [[maybe_unused]] std::in_place_t tag)
    : kind(operationKind) {
  type = qc::OpType::None;
  targets = std::move(operationTargets);
  name = toString(kind);
}

NeutralAtomOperationKind NeutralAtomOperation::validateKind(
    const NeutralAtomOperationKind operationKind) {
  if (operationKind == NeutralAtomOperationKind::Move ||
      operationKind == NeutralAtomOperationKind::Bridge) {
    return operationKind;
  }
  throw std::invalid_argument(
      "A generic neutral-atom operation must be a move or bridge.");
}

void NeutralAtomOperation::setKind(const NeutralAtomOperationKind newKind) {
  kind = newKind;
  name = toString(kind);
}

bool NeutralAtomOperation::equals(const qc::Operation& operation) const {
  if (typeid(*this) != typeid(operation)) {
    return false;
  }
  const auto* other = dynamic_cast<const NeutralAtomOperation*>(&operation);
  return other != nullptr && kind == other->kind &&
         qc::Operation::equals(operation);
}

void NeutralAtomOperation::invert() {
  if (kind == NeutralAtomOperationKind::Move) {
    if (targets.size() != 2) {
      throw std::invalid_argument("A move operation requires two targets.");
    }
    std::swap(targets.front(), targets.back());
  }
}

bool hasNeutralAtomOperationKind(const qc::Operation& operation,
                                 const NeutralAtomOperationKind kind) {
  const auto* neutralAtomOperation =
      dynamic_cast<const NeutralAtomOperation*>(&operation);
  return neutralAtomOperation != nullptr &&
         neutralAtomOperation->getKind() == kind;
}

bool isAodOperation(const qc::Operation& operation) {
  const auto* neutralAtomOperation =
      dynamic_cast<const NeutralAtomOperation*>(&operation);
  if (neutralAtomOperation == nullptr) {
    return false;
  }
  const auto kind = neutralAtomOperation->getKind();
  return kind == NeutralAtomOperationKind::AodActivate ||
         kind == NeutralAtomOperationKind::AodDeactivate ||
         kind == NeutralAtomOperationKind::AodMove;
}

std::unique_ptr<qc::Operation> makeMoveOperation(const qc::Qubit origin,
                                                 const qc::Qubit target) {
  return std::make_unique<NeutralAtomOperation>(NeutralAtomOperationKind::Move,
                                                qc::Targets{origin, target});
}

std::unique_ptr<qc::Operation> makeBridgeOperation(qc::Targets targets) {
  return std::make_unique<NeutralAtomOperation>(
      NeutralAtomOperationKind::Bridge, std::move(targets));
}

} // namespace na
