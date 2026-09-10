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

#include "na/ir/entities/Zone.hpp"
#include "na/ir/operations/NAComputationGlobalOperation.hpp"

#include <utility>
#include <vector>

namespace na {
/// Represents a global CZ operation in the NAComputation.
class NAComputationGlobalCZOperation final
    : public NAComputationGlobalOperation {
public:
  /// Creates a new CZ operation in the given zones.
  /// @param zones The zones the operation is applied to.
  explicit NAComputationGlobalCZOperation(std::vector<const Zone*> zones)
      : NAComputationGlobalOperation(std::move(zones), {}) {
    name_ = "cz";
  }

  /// Creates a new CZ operation in the given zone.
  /// @param zone The zone the operation is applied to.
  explicit NAComputationGlobalCZOperation(const Zone& zone)
      : NAComputationGlobalCZOperation({&zone}) {}
};
} // namespace na
