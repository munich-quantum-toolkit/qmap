/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "na/ir/operations/AodOperation.hpp"

#include "NAOperationPrinting.hpp"
#include "ir/Definitions.hpp"
#include "ir/Register.hpp"
#include "ir/operations/OpType.hpp"
#include "ir/operations/Operation.hpp"
#include "na/ir/operations/NAOpType.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace na {

AodOperation::AodOperation(const NAOpType newNAOpType,
                           std::vector<qc::Qubit> operationTargets,
                           const std::vector<std::uint32_t>& dimensions,
                           const std::vector<qc::fp>& starts,
                           const std::vector<qc::fp>& ends)
    : AodOperation(newNAOpType, std::move(operationTargets),
                   convertToDimensions(dimensions), starts, ends) {}

auto AodOperation::Segment::toQASMString() const -> std::string {
  std::stringstream ss;
  ss << static_cast<std::size_t>(dimension) << ", " << start << ", " << end
     << "; ";
  return ss.str();
}

auto AodOperation::convertToDimensions(
    const std::vector<std::uint32_t>& dimensions)
    -> std::vector<AodOperation::Dimension> {
  std::vector<Dimension> convertedDimensions(dimensions.size());
  for (std::size_t i = 0; i < dimensions.size(); ++i) {
    convertedDimensions[i] = static_cast<Dimension>(dimensions[i]);
  }
  return convertedDimensions;
}

auto AodOperation::validateType(const NAOpType candidateNAOpType) -> NAOpType {
  if (candidateNAOpType == NAOpType::AodActivate ||
      candidateNAOpType == NAOpType::AodDeactivate ||
      candidateNAOpType == NAOpType::AodMove) {
    return candidateNAOpType;
  }
  throw std::invalid_argument("An AOD operation requires an AOD type.");
}

void AodOperation::setNAOpType(const NAOpType newNAOpType) {
  naOpType = validateType(newNAOpType);
  name = toString(naOpType);
}

AodOperation::AodOperation(const NAOpType newNAOpType,
                           std::vector<qc::Qubit> operationTargets,
                           const std::vector<Dimension>& dimensions,
                           const std::vector<qc::fp>& starts,
                           const std::vector<qc::fp>& ends)
    : naOpType(validateType(newNAOpType)) {
  if (dimensions.size() != starts.size() || starts.size() != ends.size()) {
    throw std::invalid_argument(
        "AOD dimensions, starts, and ends must have equal sizes.");
  }
  type = qc::OpType::None;
  targets = std::move(operationTargets);
  name = toString(naOpType);

  for (std::size_t i = 0; i < dimensions.size(); ++i) {
    segments.emplace_back(dimensions[i], starts[i], ends[i]);
  }
}

AodOperation::AodOperation(const std::string& typeName,
                           std::vector<qc::Qubit> operationTargets,
                           const std::vector<std::uint32_t>& dimensions,
                           const std::vector<qc::fp>& starts,
                           const std::vector<qc::fp>& ends)
    : AodOperation(naOpTypeFromString(typeName), std::move(operationTargets),
                   convertToDimensions(dimensions), starts, ends) {}

AodOperation::AodOperation(
    const NAOpType newNAOpType, std::vector<qc::Qubit> operationTargets,
    const std::vector<std::tuple<Dimension, qc::fp, qc::fp>>& operationSegments)
    : naOpType(validateType(newNAOpType)) {
  type = qc::OpType::None;
  targets = std::move(operationTargets);
  name = toString(naOpType);

  for (const auto& [dimension, start, end] : operationSegments) {
    segments.emplace_back(dimension, start, end);
  }
}

AodOperation::AodOperation(const NAOpType newNAOpType,
                           std::vector<qc::Qubit> operationTargets,
                           std::vector<Segment> operationSegments)
    : naOpType(validateType(newNAOpType)),
      segments(std::move(operationSegments)) {
  type = qc::OpType::None;
  targets = std::move(operationTargets);
  name = toString(naOpType);
}

auto AodOperation::getEnds(const Dimension dimension) const
    -> std::vector<qc::fp> {
  std::vector<qc::fp> ends;
  for (const auto& segment : segments) {
    if (segment.dimension == dimension) {
      ends.emplace_back(segment.end);
    }
  }
  return ends;
}

auto AodOperation::getStarts(const Dimension dimension) const
    -> std::vector<qc::fp> {
  std::vector<qc::fp> starts;
  for (const auto& segment : segments) {
    if (segment.dimension == dimension) {
      starts.emplace_back(segment.start);
    }
  }
  return starts;
}

auto AodOperation::getMaxDistance(const Dimension dimension) const -> qc::fp {
  const auto distances = getDistances(dimension);
  if (distances.empty()) {
    return 0;
  }
  return *std::ranges::max_element(distances);
}

auto AodOperation::getDistances(const Dimension dimension) const
    -> std::vector<qc::fp> {
  std::vector<qc::fp> distances;
  for (const auto& segment : segments) {
    if (segment.dimension == dimension) {
      distances.emplace_back(std::abs(segment.end - segment.start));
    }
  }
  return distances;
}

auto AodOperation::equals(const qc::Operation& operation,
                          const qc::Permutation& permutation1,
                          const qc::Permutation& permutation2) const -> bool {
  const auto* other = dynamic_cast<const AodOperation*>(&operation);
  return other != nullptr && naOpType == other->naOpType &&
         qc::Operation::equals(operation, permutation1, permutation2) &&
         segments == other->segments;
}

auto AodOperation::equals(const qc::Operation& operation) const -> bool {
  return equals(operation, {}, {});
}

auto AodOperation::print(std::ostream& os, const qc::Permutation& permutation,
                         const std::size_t prefixWidth,
                         const std::size_t nQubits) const -> std::ostream& {
  return detail::printNAOperation(*this, naOpType, os, permutation, prefixWidth,
                                  nQubits);
}

void AodOperation::setGate(const qc::OpType operationType) {
  if (operationType != qc::OpType::None) {
    throw std::invalid_argument(
        "An AOD operation cannot change its gate type.");
  }
}

void AodOperation::dumpOpenQASM(
    std::ostream& of, const qc::QubitIndexToRegisterMap& qubitMap,
    [[maybe_unused]] const qc::BitIndexToRegisterMap& bitMap,
    const std::size_t indent, [[maybe_unused]] const bool openQASM3) const {
  of << std::setprecision(std::numeric_limits<qc::fp>::digits10);
  of << std::string(indent * OUTPUT_INDENT_SIZE, ' ') << name << " (";

  bool first = true;
  for (const auto& segment : segments) {
    if (!first) {
      of << "; ";
    }
    first = false;
    of << static_cast<std::size_t>(segment.dimension) << ", " << segment.start
       << ", " << segment.end;
  }
  of << ")";

  bool firstQubit = true;
  for (const auto& qubit : targets) {
    if (!firstQubit) {
      of << ",";
    }
    firstQubit = false;
    of << " " << qubitMap.at(qubit).second;
  }
  of << ";\n";
}

void AodOperation::invert() {
  if (naOpType == NAOpType::AodMove) {
    for (auto& segment : segments) {
      std::swap(segment.start, segment.end);
    }
  } else if (naOpType == NAOpType::AodActivate) {
    setNAOpType(NAOpType::AodDeactivate);
  } else if (naOpType == NAOpType::AodDeactivate) {
    setNAOpType(NAOpType::AodActivate);
  }
}

} // namespace na
