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
 * @brief Defines a type for representing individual atoms.
 */

#pragma once

#include <ostream>
#include <string>
#include <utility>

namespace na {
/// Represents an atom in the NAComputation.
/// @details The name of the atom is used for printing the NAComputation.
/// To maintain the uniqueness of atoms, the name of the atom should be unique.
class Atom final {
  /// The identifier of the atom.
  std::string name_;

public:
  /// Creates a new atom with the given name.
  /// @param name The name of the atom.
  explicit Atom(std::string name) : name_(std::move(name)) {}

  /// Returns the name of the atom.
  [[nodiscard]] auto getName() const -> std::string { return name_; }

  /// Prints the atom to the given output stream.
  /// @param os The output stream to print the atom to.
  /// @param obj The atom to print.
  /// @return The output stream after printing the atom.
  friend auto operator<<(std::ostream& os, const Atom& obj) -> std::ostream& {
    return os << obj.getName();
  }

  /// Compares two atoms for equality.
  /// @param other The atom to compare with.
  /// @return True if the atoms are equal, false otherwise.
  [[nodiscard]] auto operator==(const Atom& other) const -> bool {
    if (this == &other) {
      return true;
    }
    return name_ == other.name_;
  }

  /// Compares two atoms for inequality.
  /// @param other The atom to compare with.
  /// @return True if the atoms are not equal, false otherwise.
  [[nodiscard]] auto operator!=(const Atom& other) const -> bool {
    return !(*this == other);
  }
};
} // namespace na
