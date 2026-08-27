/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "na/ir/operations/NAComputationLoadOperation.hpp"

#include <cstddef>
#include <sstream>
#include <string>

namespace na {
auto NAComputationLoadOperation::toString() const -> std::string {
  std::stringstream ss;
  ss << "@+ load";
  if (atoms_.size() == 1) {
    if (targetLocations_) {
      ss << " " << targetLocations_->front();
    }
    ss << " " << *(atoms_.front());
    return ss.str();
  }
  ss << " [\n";
  for (std::size_t i = 0; i < atoms_.size(); ++i) {
    ss << "    ";
    if (targetLocations_) {
      ss << (*targetLocations_)[i] << " ";
    }
    ss << *(atoms_[i]) << "\n";
  }
  ss << "]";
  return ss.str();
}
} // namespace na
