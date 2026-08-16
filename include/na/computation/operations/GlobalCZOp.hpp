/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

/** @file
 * @brief Defines a class for representing global CZ operations.
 */

#pragma once

#include "na/computation/entities/Zone.hpp"
#include "na/computation/operations/GlobalOp.hpp"

#include <utility>
#include <vector>

namespace na {
/// Represents a global CZ operation in the NAComputation.
class GlobalCZOp final : public GlobalOp {
public:
  /// Creates a new CZ operation in the given zones.
  /// @param zones The zones the operation is applied to.
  explicit GlobalCZOp(std::vector<const Zone*> zones)
      : GlobalOp(std::move(zones), {}) {
    name_ = "cz";
  }

  /// Creates a new CZ operation in the given zone.
  /// @param zone The zone the operation is applied to.
  explicit GlobalCZOp(const Zone& zone) : GlobalCZOp({&zone}) {}
};
} // namespace na
