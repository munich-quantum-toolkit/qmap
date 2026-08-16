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
 * @brief Defines a class for representing store operations.
 */

#pragma once

#include "na/computation/entities/Atom.hpp"
#include "na/computation/entities/Location.hpp"
#include "na/computation/operations/ShuttlingOp.hpp"

#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace na {
/// Represents a store operation in the NAComputation.
class StoreOp final : public ShuttlingOp {
protected:
  /// The target locations to store the atoms to.
  std::optional<std::vector<Location>> targetLocations_ = std::nullopt;

public:
  /// Creates a new store operation with the given atoms and target locations.
  /// @details The target locations can be used if the store operation
  /// incorporates some offset.
  /// @param atoms The atoms to store.
  /// @param targetLocations The target locations to store the atoms to.
  StoreOp(std::vector<const Atom*> atoms, std::vector<Location> targetLocations)
      : ShuttlingOp(std::move(atoms)),
        targetLocations_(std::move(targetLocations)) {
    if (atoms_.size() != targetLocations_->size()) {
      throw std::invalid_argument(
          "Number of atoms and target locations must be equal.");
    }
  }

  /// Creates a new store operation with the given atoms and target locations.
  /// @details Here, the target locations are not used, i.e., this store does
  /// not contain any offset.
  /// @param atoms The atoms to store.
  explicit StoreOp(std::vector<const Atom*> atoms)
      : ShuttlingOp(std::move(atoms)) {}

  /// Creates a new store operation with the given atom and target location.
  /// @details The target location can be used if the store operation
  /// incorporates some offset.
  /// @param atom The atom to store.
  /// @param targetLocation The target location to store the atom to.
  StoreOp(const Atom& atom, const Location& targetLocation)
      : StoreOp({&atom}, {targetLocation}) {}

  /// Creates a new store operation with the given atom and target locations.
  /// @details Here, the target locations are not used, i.e., this store does
  /// not contain any offset.
  /// @param atom The atom to store.
  explicit StoreOp(const Atom& atom) : StoreOp({&atom}) {}

  /// Returns whether the store operation has target locations set.
  [[nodiscard]] auto hasTargetLocations() const -> bool override {
    return targetLocations_.has_value();
  }

  /// Returns the target locations of the store operation.
  [[nodiscard]] auto getTargetLocations() const
      -> const std::vector<Location>& override {
    if (!targetLocations_.has_value()) {
      throw std::logic_error("Operation has no target locations set.");
    }
    return *targetLocations_;
  }

  /// Returns a string representation of the operation.
  [[nodiscard]] auto toString() const -> std::string override;
};
} // namespace na
