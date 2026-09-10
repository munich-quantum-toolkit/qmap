/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "na/ir/OpenQASMSerializer.hpp"

#include "ir/Definitions.hpp"
#include "ir/QuantumComputation.hpp"
#include "ir/operations/Operation.hpp"
#include "na/ir/operations/AodOperation.hpp"
#include "na/ir/operations/NAStandardOperation.hpp"
#include "qasm3/Serializer.hpp"

#include <cstddef>
#include <iomanip>
#include <limits>
#include <ostream>
#include <string>

namespace na {

namespace {

constexpr std::size_t OUTPUT_INDENT_SIZE = 2U;

auto serializeOperation(
    std::ostream& output, const qc::Operation& operation,
    const qasm3::QubitIndexToRegisterMap& qubitMap,
    [[maybe_unused]] const qasm3::BitIndexToRegisterMap& bitMap,
    const std::size_t indent) -> bool {
  const auto indentation = std::string(indent * OUTPUT_INDENT_SIZE, ' ');
  if (const auto* aod = dynamic_cast<const AodOperation*>(&operation);
      aod != nullptr) {
    output << indentation
           << std::setprecision(std::numeric_limits<qc::fp>::digits10)
           << aod->getName() << " (";
    for (auto it = aod->getSegments().cbegin();
         it != aod->getSegments().cend();) {
      output << static_cast<std::size_t>(it->dimension) << ", " << it->start
             << ", " << it->end;
      if (++it != aod->getSegments().cend()) {
        output << "; ";
      }
    }
    output << ")";
    for (auto it = aod->getTargets().cbegin();
         it != aod->getTargets().cend();) {
      output << " " << qubitMap.at(*it).second;
      if (++it != aod->getTargets().cend()) {
        output << ",";
      }
    }
    output << ";\n";
    return true;
  }
  if (const auto* neutralAtom =
          dynamic_cast<const NAStandardOperation*>(&operation);
      neutralAtom != nullptr) {
    output << indentation << neutralAtom->getName();
    for (std::size_t i = 0U; i < neutralAtom->getTargets().size(); ++i) {
      output << (i == 0U ? " " : ", ")
             << qubitMap.at(neutralAtom->getTargets()[i]).second;
    }
    output << ";\n";
    return true;
  }
  return false;
}

} // namespace

void serializeOpenQASM(const qc::QuantumComputation& computation,
                       std::ostream& output) {
  qasm3::Serializer(output, qc::Format::OpenQASM2, serializeOperation)
      .serialize(computation);
}

} // namespace na
