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
 * @brief Defines a class for representing global operations in the
 * NAComputation.
 */

#pragma once

#include "ir/Definitions.hpp"
#include "na/computation/entities/Zone.hpp"
#include "na/computation/operations/Op.hpp"

#include <string>
#include <utility>
#include <vector>

namespace na {
/// Represents a global operation in the NAComputation.
/// @details Global operations are applied to entire zones instead of to
/// individual atoms.
class GlobalOp : public Op {
protected:
  /// The name of the operation.
  std::string name_;
  /// The parameters of the operation.
  std::vector<qc::fp> params_;
  /// The zones the operation is applied to.
  std::vector<const Zone*> zones_;

  /// Creates a new global operation in the given zone with the given
  /// parameters.
  /// @param zones The zones the operation is applied to.
  /// @param params The parameters of the operation.
  GlobalOp(std::vector<const Zone*> zones, std::vector<qc::fp> params)
      : params_(std::move(params)), zones_(std::move(zones)) {}

public:
  GlobalOp() = delete;

  /// Returns the parameters of the operation.
  [[nodiscard]] auto getParams() const -> auto& { return params_; }

  /// Returns the zone the operation is applied to.
  [[nodiscard]] auto getZones() const -> auto& { return zones_; }

  /// Returns a string representation of the operation.
  [[nodiscard]] auto toString() const -> std::string override;
};
} // namespace na
