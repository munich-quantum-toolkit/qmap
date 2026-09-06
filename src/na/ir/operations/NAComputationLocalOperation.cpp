/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "na/ir/operations/NAComputationLocalOperation.hpp"

#include "ir/Definitions.hpp"
#include "na/ir/entities/Atom.hpp"

#include <iomanip>
#include <ios>
#include <sstream>
#include <string>
#include <vector>

namespace na {
namespace {
auto printLocalParams(const std::vector<qc::fp>& params, std::ostringstream& os)
    -> void {
  if (!params.empty()) {
    for (const auto& p : params) {
      os << p << " ";
    }
  }
}
} // namespace

auto NAComputationLocalOperation::toString() const -> std::string {
  std::ostringstream ss;
  ss << std::setprecision(5) << std::fixed;
  ss << "@+ " << name_ << " ";
  if (atoms_.size() == 1) {
    printLocalParams(params_, ss);
    ss << *atoms_.front();
    return ss.str();
  }
  ss << "[\n";
  for (const auto& atom : atoms_) {
    ss << "    ";
    printLocalParams(params_, ss);
    ss << *atom << "\n";
  }
  ss << "]";
  return ss.str();
}
} // namespace na
