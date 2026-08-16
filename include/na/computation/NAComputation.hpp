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
 * @brief Defines a class for representing neutral atom computations.
 */

#pragma once

#include "na/computation/entities/Atom.hpp"
#include "na/computation/entities/Location.hpp"
#include "na/computation/entities/Zone.hpp"
#include "na/computation/operations/Op.hpp"

#include <cstddef>
#include <memory>
#include <ostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace na {
/// Represents a neutral atom computation.
class NAComputation final {
protected:
  /// The operations in the NA computation.
  std::vector<std::unique_ptr<Op>> operations_;
  /// The atoms used in the NA computation.
  std::vector<std::unique_ptr<Atom>> atoms_;
  /// The zones used in the NA computation.
  std::vector<std::unique_ptr<Zone>> zones_;
  /// The initial locations of the atoms.
  std::unordered_map<const Atom*, Location> initialLocations_;

public:
  /// Returns an iterator to the beginning of the operations.
  [[nodiscard]] auto begin() -> auto { return operations_.begin(); }

  /// Returns an iterator to the beginning of the operations.
  [[nodiscard]] auto begin() const -> auto { return operations_.begin(); }

  /// Returns an iterator to the end of the operations.
  [[nodiscard]] auto end() -> auto { return operations_.end(); }

  /// Returns an iterator to the end of the operations.
  [[nodiscard]] auto end() const -> auto { return operations_.end(); }

  /// Returns the number of operations in the NAComputation.
  [[nodiscard]] auto size() const -> std::size_t { return operations_.size(); }

  /// Returns a reference to the operation at the given index.
  /// @param i The index of the operation.
  /// @return A reference to the operation at the given index.
  [[nodiscard]] auto operator[](const std::size_t i) -> Op& {
    return *operations_[i];
  }

  /// Returns a const reference to the operation at the given index.
  /// @param i The index of the operation.
  /// @return A const reference to the operation at the given index.
  [[nodiscard]] auto operator[](const std::size_t i) const -> const Op& {
    return *operations_[i];
  }

  /// Clears the operations in the NAComputation.
  auto clear() -> void { operations_.clear(); }

  /// Returns the number of atoms used in the NAComputation.
  [[nodiscard]] auto getAtomsSize() const -> std::size_t {
    return atoms_.size();
  }

  /// Returns the atoms used in the NAComputation.
  [[nodiscard]] auto getAtoms() const -> auto& { return atoms_; }

  /// Returns the zones used in global operations within the NAComputation.
  [[nodiscard]] auto getZones() const -> auto& { return zones_; }

  /// Returns the initial locations of the atoms.
  [[nodiscard]] auto getInitialLocations() const -> auto& {
    return initialLocations_;
  }

  /// Returns the location of the given atom after the given operation.
  /// @param atom The atom to get the location for.
  /// @param op The operation to get the location after.
  /// @return The location of the atom after the operation.
  [[nodiscard]] auto getLocationOfAtomAfterOperation(const Atom& atom,
                                                     const Op& op) const
      -> Location;

  /// Emplaces a new atom with the given name and returns a reference to the
  /// newly created atom.
  /// @param name The name of the atom.
  /// @return A reference to the newly created atom.
  auto emplaceBackAtom(std::string name) -> const Atom& {
    return *atoms_.emplace_back(std::make_unique<Atom>(std::move(name)));
  }

  /// Emplaces a new zone with the given name and returns a reference to the
  /// newly created zone.
  /// @param args The name of the zone and, optionally, the extent of the zone.
  /// @return A reference to the newly created zone.
  template <typename... Args>
  auto emplaceBackZone(Args&&... args) -> const Zone& {
    return *zones_.emplace_back(
        std::make_unique<Zone>(std::forward<Args>(args)...));
  }

  /// Emplaces a new initial location for the given atom with the given location
  /// and returns a reference to the newly created location.
  /// @param atom The atom to set the initial location for.
  /// @param loc The location of the atom.
  /// @return A reference to the newly created location.
  auto emplaceInitialLocation(const Atom& atom, const Location& loc)
      -> const Location& {
    return initialLocations_.emplace(&atom, loc).first->second;
  }

  /// Emplaces a new initial location for the given atom with the given
  /// arguments and returns a reference to the newly created location.
  /// @param atom The atom to set the initial location for.
  /// @param loc The parameters for the location of the atom.
  /// @return A reference to the newly created location.
  template <typename... Args>
  auto emplaceInitialLocation(const Atom& atom, Args&&... loc)
      -> const Location& {
    return initialLocations_
        .emplace(&atom,
                 Location{static_cast<double>(std::forward<Args>(loc))...})
        .first->second;
  }

  /// Emplaces a new operation of type T with the given operation and returns a
  /// reference to the newly created operation.
  /// @tparam T The concrete type of the operation.
  /// @param op The operation to emplace.
  /// @return A reference to the newly created operation.
  template <class T> auto emplaceBack(T&& op) -> const Op& {
    return *operations_.emplace_back(std::make_unique<T>(std::forward<T>(op)));
  }

  /// Emplaces a new operation of type T with the given arguments and returns a
  /// reference to the newly created operation.
  /// @tparam T The concrete type of the operation.
  /// @param args The arguments for the operation.
  /// @return A reference to the newly created operation.
  template <class T, typename... Args>
  auto emplaceBack(Args&&... args) -> const Op& {
    return *operations_.emplace_back(
        std::make_unique<T>(std::forward<Args>(args)...));
  }

  /// Returns a string representation of the NAComputation.
  [[nodiscard]] auto toString() const -> std::string;
  /// Outputs the NAComputation to the given output stream, i.e., the string
  /// returned by toString().
  /// @param os The output stream to print the NAComputation to.
  /// @param qc The NAComputation to print.
  /// @return The output stream after printing the NAComputation.
  friend auto operator<<(std::ostream& os, const NAComputation& qc)
      -> std::ostream& {
    return os << qc.toString();
  }

  /// Validates the NAComputation and checks whether all AOD constraints are
  /// fulfilled.
  /// Specifically,
  /// - each atom is loaded before it is moved
  /// - the relative order of loaded atoms is preserved
  /// - each atom is loaded before it is stored
  /// - each atom is stored before it is loaded (again)
  /// @returns a pair of a Boolean indicating whether the NAComputation is valid
  /// and a string containing the error message if the NAComputation is invalid.
  [[nodiscard]] auto validate() const -> std::pair<bool, std::string>;
};
} // namespace na
