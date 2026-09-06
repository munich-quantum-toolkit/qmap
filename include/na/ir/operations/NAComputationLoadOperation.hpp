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
 * @brief Defines a class for representing load operations.
 */

#pragma once

#include "na/ir/entities/Atom.hpp"
#include "na/ir/entities/Location.hpp"
#include "na/ir/operations/NAComputationShuttlingOperation.hpp"

#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace na {
/// Represents a load operation in the NAComputation.
/// @details Before an atom can be moved, it must be loaded, i.e., transferred
/// from a static SLM to an adjustable AOD trap.
class NAComputationLoadOperation final
    : public NAComputationShuttlingOperation {
protected:
  /// The target locations to load the atoms to.
  std::optional<std::vector<Location>> targetLocations_ = std::nullopt;

public:
  /// Creates a new load operation with the given atoms and target locations.
  /// @details The target locations can be set if the loading operation contains
  /// a certain offset.
  /// @param atoms The atoms to load.
  /// @param targetLocations The target locations to load the atoms to.
  NAComputationLoadOperation(std::vector<const Atom*> atoms,
                             std::vector<Location> targetLocations)
      : NAComputationShuttlingOperation(std::move(atoms)),
        targetLocations_(std::move(targetLocations)) {
    if (atoms_.size() != targetLocations_->size()) {
      throw std::invalid_argument(
          "Number of atoms and target locations must be equal.");
    }
  }

  /// Creates a new load operation with the given atoms.
  /// @details This constructor is used if the target locations are not set,
  /// i.e., the load operation does not incorporate any offset.
  /// @param atoms The atoms to load.
  explicit NAComputationLoadOperation(std::vector<const Atom*> atoms)
      : NAComputationShuttlingOperation(std::move(atoms)) {}

  /// Creates a new load operation with the given atom and target location.
  /// @details The target locations can be set if the loading operation contains
  /// a certain offset.
  /// @param atom The atom to load.
  /// @param targetLocation The target location to load the atom to.
  NAComputationLoadOperation(const Atom& atom, const Location& targetLocation)
      : NAComputationLoadOperation({&atom}, {targetLocation}) {}

  /// Creates a new load operation with the given atom.
  /// @details This constructor is used if the target locations are not set,
  /// i.e., the load operation does not incorporate any offset.
  /// @param atom The atom to load.
  explicit NAComputationLoadOperation(const Atom& atom)
      : NAComputationLoadOperation({&atom}) {}

  /// Returns whether the load operation has target locations set.
  [[nodiscard]] auto hasTargetLocations() const -> bool override {
    return targetLocations_.has_value();
  }

  /// Returns the target locations of the load operation.
  [[nodiscard]] auto getTargetLocations() const
      -> const std::vector<Location>& override {
    if (!targetLocations_.has_value()) {
      throw std::logic_error("Operation has no target locations set.");
    }
    return *targetLocations_;
  }

  /// Returns a string representation of the load operation.
  [[nodiscard]] auto toString() const -> std::string override;
};
} // namespace na
