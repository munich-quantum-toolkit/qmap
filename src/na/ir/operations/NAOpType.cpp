/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "na/ir/operations/NAOpType.hpp"

#include "NAOperationPrinting.hpp"
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

auto toString(const NAOpType type) -> std::string_view {
  switch (type) {
  case NAOpType::None:
    return "none";
  case NAOpType::Move:
    return "move";
  case NAOpType::Bridge:
    return "bridge";
  case NAOpType::AodActivate:
    return "aod_activate";
  case NAOpType::AodDeactivate:
    return "aod_deactivate";
  case NAOpType::AodMove:
    return "aod_move";
  }
  throw std::invalid_argument("Unknown neutral-atom operation type.");
}

auto naOpTypeFromString(const std::string_view name) -> NAOpType {
  if (name == "none") {
    return NAOpType::None;
  }
  if (name == "move") {
    return NAOpType::Move;
  }
  if (name == "bridge") {
    return NAOpType::Bridge;
  }
  if (name == "aod_activate") {
    return NAOpType::AodActivate;
  }
  if (name == "aod_deactivate") {
    return NAOpType::AodDeactivate;
  }
  if (name == "aod_move") {
    return NAOpType::AodMove;
  }
  throw std::invalid_argument("Unknown neutral-atom operation name: " +
                              std::string(name));
}

auto getNAOpType(const qc::Operation& operation) -> std::optional<NAOpType> {
  if (const auto* standardOperation =
          dynamic_cast<const NAStandardOperation*>(&operation)) {
    if (standardOperation->getNAOpType() != NAOpType::None) {
      return standardOperation->getNAOpType();
    }
    return std::nullopt;
  }
  if (const auto* aodOperation =
          dynamic_cast<const AodOperation*>(&operation)) {
    if (aodOperation->getNAOpType() != NAOpType::None) {
      return aodOperation->getNAOpType();
    }
  }
  return std::nullopt;
}

auto hasNAOpType(const qc::Operation& operation, const NAOpType type) -> bool {
  const auto operationType = getNAOpType(operation);
  return operationType.has_value() && *operationType == type;
}

auto isAodOperation(const qc::Operation& operation) -> bool {
  return dynamic_cast<const AodOperation*>(&operation) != nullptr;
}

auto detail::printNAOperation(const qc::Operation& operation,
                              const NAOpType operationType, std::ostream& os,
                              const qc::Permutation& permutation,
                              [[maybe_unused]] const std::size_t prefixWidth,
                              const std::size_t nQubits) -> std::ostream& {
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
