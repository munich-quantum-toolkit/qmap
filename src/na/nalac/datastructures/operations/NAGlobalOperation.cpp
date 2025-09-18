/*
 * Copyright (c) 2025 Chair for Design Automation, TUM
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "na/nalac/datastructures/operations/NAGlobalOperation.hpp"

#include <ios>
#include <sstream>
#include <string>

namespace na::nalac {
auto NAGlobalOperation::toString() const -> std::string {
  std::stringstream ss;
  ss << std::string(ctrls_, 'c') << opType_;
  if (!params_.empty()) {
    ss << "(";
    for (const auto& p : params_) {
      ss << p << ", ";
    }
    ss.seekp(-2, std::ios_base::end);
    ss << ")";
  }
  ss << ";\n";
  return ss.str();
}
} // namespace na::nalac
