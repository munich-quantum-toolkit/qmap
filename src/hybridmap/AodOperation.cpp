/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "hybridmap/AodOperation.hpp"

#include "hybridmap/NeutralAtomOperation.hpp"
#include "ir/Definitions.hpp"
#include "ir/Register.hpp"

#include <algorithm>
#include <cassert>
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
AodOperation::AodOperation(const NeutralAtomOperationKind kind,
                           std::vector<qc::Qubit> qubits,
                           const std::vector<uint32_t>& dirs,
                           const std::vector<qc::fp>& start,
                           const std::vector<qc::fp>& end)
    : AodOperation(kind, std::move(qubits), convertToDimension(dirs), start,
                   end) {}

std::string SingleOperation::toQASMString() const {
  std::stringstream ss;
  ss << static_cast<std::size_t>(dir) << ", " << start << ", " << end << "; ";
  return ss.str();
}

std::vector<Dimension>
AodOperation::convertToDimension(const std::vector<uint32_t>& dirs) {
  std::vector<Dimension> dirsEnum(dirs.size());
  for (size_t i = 0; i < dirs.size(); ++i) {
    dirsEnum[i] = static_cast<Dimension>(dirs[i]);
  }
  return dirsEnum;
}

NeutralAtomOperationKind
AodOperation::validateKind(const NeutralAtomOperationKind kind) {
  if (kind == NeutralAtomOperationKind::AodActivate ||
      kind == NeutralAtomOperationKind::AodDeactivate ||
      kind == NeutralAtomOperationKind::AodMove) {
    return kind;
  }
  throw std::invalid_argument("An AOD operation requires an AOD kind.");
}

AodOperation::AodOperation(const NeutralAtomOperationKind kind,
                           std::vector<qc::Qubit> qubits,
                           const std::vector<Dimension>& dirs,
                           const std::vector<qc::fp>& start,
                           const std::vector<qc::fp>& end)
    : NeutralAtomOperation(validateKind(kind), std::move(qubits),
                           std::in_place) {
  assert(dirs.size() == start.size() && start.size() == end.size());

  for (size_t i = 0; i < dirs.size(); ++i) {
    operations.emplace_back(dirs[i], start[i], end[i]);
  }
}

AodOperation::AodOperation(const std::string& typeName,
                           std::vector<qc::Qubit> qubits,
                           const std::vector<uint32_t>& dirs,
                           const std::vector<qc::fp>& start,
                           const std::vector<qc::fp>& end)
    : AodOperation(neutralAtomOperationKindFromString(typeName),
                   std::move(qubits), convertToDimension(dirs), start, end) {}

AodOperation::AodOperation(
    const NeutralAtomOperationKind kind, std::vector<qc::Qubit> qubits,
    const std::vector<std::tuple<Dimension, qc::fp, qc::fp>>& ops)
    : NeutralAtomOperation(validateKind(kind), std::move(qubits),
                           std::in_place) {

  for (const auto& [dir, index, param] : ops) {
    operations.emplace_back(dir, index, param);
  }
}

AodOperation::AodOperation(const NeutralAtomOperationKind kind,
                           std::vector<qc::Qubit> targets,
                           std::vector<SingleOperation> ops)
    : NeutralAtomOperation(validateKind(kind), std::move(targets),
                           std::in_place),
      operations(std::move(ops)) {}

std::vector<qc::fp> AodOperation::getEnds(const Dimension dir) const {
  std::vector<qc::fp> ends;
  for (const auto& op : operations) {
    if (op.dir == dir) {
      ends.emplace_back(op.end);
    }
  }
  return ends;
}

std::vector<qc::fp> AodOperation::getStarts(const Dimension dir) const {
  std::vector<qc::fp> starts;
  for (const auto& op : operations) {
    if (op.dir == dir) {
      starts.emplace_back(op.start);
    }
  }
  return starts;
}

qc::fp AodOperation::getMaxDistance(const Dimension dir) const {
  const auto distances = getDistances(dir);
  if (distances.empty()) {
    return 0;
  }
  return *std::ranges::max_element(distances);
}

std::vector<qc::fp> AodOperation::getDistances(const Dimension dir) const {
  std::vector<qc::fp> params;
  for (const auto& op : operations) {
    if (op.dir == dir) {
      params.emplace_back(std::abs(op.end - op.start));
    }
  }
  return params;
}

bool AodOperation::equals(const qc::Operation& operation) const {
  const auto* other = dynamic_cast<const AodOperation*>(&operation);
  return other != nullptr && NeutralAtomOperation::equals(operation) &&
         operations == other->operations;
}

void AodOperation::dumpOpenQASM(
    std::ostream& of, const qc::QubitIndexToRegisterMap& qubitMap,
    [[maybe_unused]] const qc::BitIndexToRegisterMap& bitMap,
    const size_t indent, bool /*openQASM3*/) const {
  of << std::setprecision(std::numeric_limits<qc::fp>::digits10);
  of << std::string(indent * OUTPUT_INDENT_SIZE, ' ') << name << " (";

  // Write AOD operations with separator logic
  bool first = true;
  for (const auto& op : operations) {
    if (!first) {
      of << "; ";
    }
    first = false;
    of << static_cast<std::size_t>(op.dir) << ", " << op.start << ", "
       << op.end;
  }
  of << ")";

  // Write qubits with separator logic
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
  if (getKind() == NeutralAtomOperationKind::AodMove) {
    for (auto& op : operations) {
      std::swap(op.start, op.end);
    }
  } else if (getKind() == NeutralAtomOperationKind::AodActivate) {
    setKind(NeutralAtomOperationKind::AodDeactivate);
  } else if (getKind() == NeutralAtomOperationKind::AodDeactivate) {
    setKind(NeutralAtomOperationKind::AodActivate);
  }
}
} // namespace na
