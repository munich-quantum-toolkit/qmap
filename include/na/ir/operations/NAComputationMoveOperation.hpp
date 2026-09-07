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
 * @brief Defines a class for representing move operations.
 */

#pragma once

#include "na/ir/entities/Atom.hpp"
#include "na/ir/entities/Location.hpp"
#include "na/ir/operations/NAComputationShuttlingOperation.hpp"

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace na {
/// Represents a move operation in the NAComputation.
class NAComputationMoveOperation final
    : public NAComputationShuttlingOperation {
protected:
  /// The target locations to move the atoms to.
  std::vector<Location> targetLocations_;

public:
  /// Creates a new move operation with the given atoms and target locations.
  /// @param atoms The atoms to move.
  /// @param targetLocations The target locations to move the atoms to.
  NAComputationMoveOperation(std::vector<const Atom*> atoms,
                             std::vector<Location> targetLocations)
      : NAComputationShuttlingOperation(std::move(atoms)),
        targetLocations_(std::move(targetLocations)) {
    if (atoms_.size() != targetLocations_.size()) {
      throw std::invalid_argument(
          "Number of atoms and target locations must be equal.");
    }
  }

  /// Creates a new move operation with the given atom and target location.
  /// @param atom The atom to move.
  /// @param targetLocation The target location to move the atom to.
  NAComputationMoveOperation(const Atom& atom, const Location& targetLocation)
      : NAComputationMoveOperation({&atom}, {targetLocation}) {}

  /// Returns whether the move operation has target locations set.
  [[nodiscard]] auto hasTargetLocations() const -> bool override {
    return true;
  }

  /// Returns the target locations of the move operation.
  [[nodiscard]] auto getTargetLocations() const -> const
      decltype(targetLocations_)& override {
    return targetLocations_;
  }

  /// Returns a string representation of the operation.
  [[nodiscard]] auto toString() const -> std::string override;
};
} // namespace na
