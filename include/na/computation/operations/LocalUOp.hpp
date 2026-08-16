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
 * @brief Defines a class for representing local U3 operations.
 */

#pragma once

#include "ir/Definitions.hpp"
#include "na/computation/entities/Atom.hpp"
#include "na/computation/operations/LocalOp.hpp"

#include <string>
#include <utility>
#include <vector>

namespace na {
/// Represents a local U3 operation in the NAComputation.
class LocalUOp final : public LocalOp {
public:
  /// Creates a new U3 operation with the given atoms and angles.
  /// @param atoms The atoms the operation is applied to.
  /// @param theta The first parameter of the operation.
  /// @param phi The second parameter of the operation.
  /// @param lambda The third parameter of the operation.
  LocalUOp(std::vector<const Atom*> atoms, const qc::fp theta, const qc::fp phi,
           const qc::fp lambda)
      : LocalOp(std::move(atoms), {theta, phi, lambda}) {
    name_ = "u";
  }

  /// Creates a new U3 operation with the given atom and angle.
  /// @param atom The atom the operation is applied to.
  /// @param theta The first parameter of the operation.
  /// @param phi The second parameter of the operation.
  /// @param lambda The third parameter of the operation.
  LocalUOp(const Atom& atom, const qc::fp theta, const qc::fp phi,
           const qc::fp lambda)
      : LocalUOp({&atom}, theta, phi, lambda) {}
};
} // namespace na
