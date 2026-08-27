/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "na/ir/operations/NeutralAtomOpType.hpp"

#include "NeutralAtomOperationPrinting.hpp"
#include "na/ir/operations/AodOperation.hpp"
#include "na/ir/operations/NAStandardOperation.hpp"

#include <algorithm>
#include <cstddef>
#include <iomanip>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace na {

std::string_view toString(const NeutralAtomOpType type) {
  switch (type) {
  case NeutralAtomOpType::None:
    return "none";
  case NeutralAtomOpType::Move:
    return "move";
  case NeutralAtomOpType::Bridge:
    return "bridge";
  case NeutralAtomOpType::AodActivate:
    return "aod_activate";
  case NeutralAtomOpType::AodDeactivate:
    return "aod_deactivate";
  case NeutralAtomOpType::AodMove:
    return "aod_move";
  }
  throw std::invalid_argument("Unknown neutral-atom operation type.");
}

NeutralAtomOpType neutralAtomOpTypeFromString(const std::string_view name) {
  if (name == "none") {
    return NeutralAtomOpType::None;
  }
  if (name == "move") {
    return NeutralAtomOpType::Move;
  }
  if (name == "bridge") {
    return NeutralAtomOpType::Bridge;
  }
  if (name == "aod_activate") {
    return NeutralAtomOpType::AodActivate;
  }
  if (name == "aod_deactivate") {
    return NeutralAtomOpType::AodDeactivate;
  }
  if (name == "aod_move") {
    return NeutralAtomOpType::AodMove;
  }
  throw std::invalid_argument("Unknown neutral-atom operation name: " +
                              std::string(name));
}

std::optional<NeutralAtomOpType>
getNeutralAtomOpType(const qc::Operation& operation) {
  if (const auto* standardOperation =
          dynamic_cast<const NAStandardOperation*>(&operation)) {
    if (standardOperation->getNeutralAtomOpType() != NeutralAtomOpType::None) {
      return standardOperation->getNeutralAtomOpType();
    }
    return std::nullopt;
  }
  if (const auto* aodOperation =
          dynamic_cast<const AodOperation*>(&operation)) {
    if (aodOperation->getNeutralAtomOpType() != NeutralAtomOpType::None) {
      return aodOperation->getNeutralAtomOpType();
    }
  }
  return std::nullopt;
}

bool hasNeutralAtomOpType(const qc::Operation& operation,
                          const NeutralAtomOpType type) {
  const auto operationType = getNeutralAtomOpType(operation);
  return operationType.has_value() && *operationType == type;
}

bool isAodOperation(const qc::Operation& operation) {
  return dynamic_cast<const AodOperation*>(&operation) != nullptr;
}

std::ostream& detail::printNeutralAtomOperation(
    const qc::Operation& operation, const NeutralAtomOpType operationType,
    std::ostream& os, const qc::Permutation& permutation,
    [[maybe_unused]] const std::size_t prefixWidth, const std::size_t nQubits) {
  const auto precisionBefore = os.precision(20);
  const auto& actualControls = permutation.apply(operation.getControls());
  const auto& actualTargets = permutation.apply(operation.getTargets());

  for (std::size_t i = 0; i < nQubits; ++i) {
    const auto qubit = static_cast<qc::Qubit>(i);
    if (std::ranges::find(actualTargets, qubit) != actualTargets.cend()) {
      os << "\033[1m\033[36m" << std::setw(4) << toString(operationType)
         << "\033[0m";
      continue;
    }

    if (const auto control = std::ranges::find_if(
            actualControls,
            [qubit](const qc::Control& c) { return c.qubit == qubit; });
        control != actualControls.cend()) {
      if (control->type == qc::Control::Type::Pos) {
        os << "\033[32m";
      } else {
        os << "\033[31m";
      }
      os << std::setw(4) << "c"
         << "\033[0m";
      continue;
    }

    os << std::setw(4) << "|"
       << "\033[0m";
  }

  operation.printParameters(os);
  os.precision(precisionBefore);
  return os;
}

} // namespace na
