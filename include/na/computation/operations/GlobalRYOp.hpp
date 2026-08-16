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
#include "na/computation/entities/Zone.hpp"
#include "na/computation/operations/GlobalOp.hpp"

#include <utility>
#include <vector>

namespace na {
/// Represents a global RY operation in the NAComputation.
class GlobalRYOp final : public GlobalOp {
public:
  /// Creates a new RY operation in the given zones with the given angle.
  /// @param zones The zones the operation is applied to.
  /// @param angle The angle of the operation.
  GlobalRYOp(std::vector<const Zone*> zones, const qc::fp angle)
      : GlobalOp(std::move(zones), {angle}) {
    name_ = "ry";
  }

  /// Creates a new RY operation in the given zone with the given angle.
  /// @param zone The zone the operation is applied to.
  /// @param angle The angle of the operation.
  GlobalRYOp(const Zone& zone, const qc::fp angle)
      : GlobalRYOp({&zone}, angle) {}
};
} // namespace na
