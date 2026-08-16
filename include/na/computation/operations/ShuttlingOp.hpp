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
 * @brief Defines a base class for representing shuttling operations.
 */

#pragma once

#include "na/computation/entities/Atom.hpp"
#include "na/computation/entities/Location.hpp"
#include "na/computation/operations/Op.hpp"

#include <utility>
#include <vector>

namespace na {
/// Represents a shuttling operation in the NAComputation.
/// @details A shuttling operation is the super class for all shuttling-related
/// operations, i.e., load, store, and move operations.
class ShuttlingOp : public Op {
protected:
  /// The atoms the operation is applied to.
  std::vector<const Atom*> atoms_;

  /// Creates a new shuttling operation with the given atoms.
  /// @param atoms The atoms the operation is applied to.
  explicit ShuttlingOp(std::vector<const Atom*> atoms)
      : atoms_(std::move(atoms)) {}

public:
  ShuttlingOp() = delete;

  /// Returns the atoms the operation is applied to.
  [[nodiscard]] auto getAtoms() const -> auto& { return atoms_; }

  /// Returns whether the shuttling operation has target locations set.
  [[nodiscard]] virtual auto hasTargetLocations() const -> bool = 0;

  /// Returns the target locations of the shuttling operation.
  [[nodiscard]] virtual auto getTargetLocations() const
      -> const std::vector<Location>& = 0;
};
} // namespace na
