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
 * @brief Defines a class for representing global RY operations.
 */

#pragma once

#include "ir/Definitions.hpp"
#include "na/ir/entities/Zone.hpp"
#include "na/ir/operations/NAComputationGlobalOperation.hpp"

#include <utility>
#include <vector>

namespace na {
/// Represents a global RY operation in the NAComputation.
class NAComputationGlobalRYOperation final
    : public NAComputationGlobalOperation {
public:
  /// Creates a new RY operation in the given zones with the given angle.
  /// @param zones The zones the operation is applied to.
  /// @param angle The angle of the operation.
  NAComputationGlobalRYOperation(std::vector<const Zone*> zones,
                                 const qc::fp angle)
      : NAComputationGlobalOperation(std::move(zones), {angle}) {
    name_ = "ry";
  }

  /// Creates a new RY operation in the given zone with the given angle.
  /// @param zone The zone the operation is applied to.
  /// @param angle The angle of the operation.
  NAComputationGlobalRYOperation(const Zone& zone, const qc::fp angle)
      : NAComputationGlobalRYOperation({&zone}, angle) {}
};
} // namespace na
