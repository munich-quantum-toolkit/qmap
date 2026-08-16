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
 * @brief Defines a type for representing two-dimensional locations.
 */

#pragma once

#include "ir/Definitions.hpp"

#include <cmath>
#include <cstddef>
#include <functional>
#include <iomanip>
#include <ios>
#include <ostream>
#include <sstream>
#include <string>

namespace na {
/// Class to store two-dimensional coordinates of type double.
struct Location final {
  /// The x-coordinate of the location.
  double x = 0;
  /// The y-coordinate of the location.
  double y = 0;

  /// Subtracts the coordinates of the given location from this location.
  /// @param loc The location to subtract.
  /// @return The location resulting from the subtraction.
  Location operator-(const Location& loc) const {
    return {x - loc.x, y - loc.y};
  }

  /// Adds the coordinates of the given location to this location.
  /// @param loc The location to add.
  /// @return The location resulting from the addition.
  Location operator+(const Location& loc) const {
    return {x + loc.x, y + loc.y};
  }

  /// Returns the length of the vector from the origin to this location.
  [[nodiscard]] auto length() const -> double { return std::hypot(x, y); }

  /// Returns the distance between two locations.
  [[nodiscard]] static auto distance(const Location& loc1, const Location& loc2)
      -> double {
    return (loc1 - loc2).length();
  }

  /// Returns a string representation of the location in the format "(x, y)".
  [[nodiscard]] auto toString() const -> std::string {
    std::stringstream ss;
    ss << std::setprecision(3) << std::fixed;
    ss << "(" << x << ", " << y << ")";
    return ss.str();
  }

  /// Prints the location to the given output stream.
  /// @param os The output stream to print the location to.
  /// @param obj The location to print.
  /// @return The output stream after printing the location.
  friend auto operator<<(std::ostream& os, const Location& obj)
      -> std::ostream& {
    return os << obj.toString();
  }

  /// Compares two locations for equality.
  /// @param other The location to compare with.
  /// @return True if the locations are equal, false otherwise.
  [[nodiscard]] auto operator==(const Location& other) const -> bool {
    return x == other.x && y == other.y;
  }

  /// Compares two locations for inequality.
  /// @param other The location to compare with.
  /// @return True if the locations are not equal, false otherwise.
  [[nodiscard]] auto operator!=(const Location& other) const -> bool {
    return !(*this == other);
  }

  /// Compares two locations for less than.
  /// @param other The location to compare with.
  /// @return True if this location is less than the other location, false
  [[nodiscard]] auto operator<(const Location& other) const -> bool {
    return x < other.x || (x == other.x && y < other.y);
  }

  /// Compares two locations for greater than.
  /// @param other The location to compare with.
  /// @return True if this location is greater than the other location, false
  [[nodiscard]] auto operator>(const Location& other) const -> bool {
    return other < *this;
  }

  /// Compares two locations for greater than or equal.
  /// @param other The location to compare with.
  /// @return True if this location is greater than or equal to the other
  [[nodiscard]] auto operator>=(const Location& other) const -> bool {
    return !(other < *this);
  }

  /// Compares two locations for less than or equal.
  /// @param other The location to compare with.
  /// @return True if this location is less than or equal to the other location,
  [[nodiscard]] auto operator<=(const Location& other) const -> bool {
    return *this >= other;
  }

  /// Computes the Euclidean distance between this location and the given
  /// location.
  /// @param loc The location to compute the distance to.
  /// @return The Euclidean distance between the two locations.
  [[nodiscard]] auto getEuclideanDistance(const Location& loc) const -> double {
    return (*this - loc).length();
  }

  /// Computes the horizontal distance between this location and the given
  /// location.
  /// @param loc The location to compute the distance to.
  /// @return The horizontal distance between the two locations.
  [[nodiscard]] auto getManhattanDistanceX(const Location& loc) const
      -> double {
    return std::abs(x - loc.x);
  }

  /// Computes the vertical distance between this location and the given
  /// location.
  /// @param loc The location to compute the distance to.
  /// @return The vertical distance between the two locations.
  [[nodiscard]] auto getManhattanDistanceY(const Location& loc) const
      -> double {
    return std::abs(y - loc.y);
  }
};
} // namespace na

/// @brief Specialization of std::hash for na::Location.
template <> struct std::hash<na::Location> {
  /// @brief Hashes a pair of qc::OpType and size_t values.
  auto operator()(const na::Location& loc) const noexcept -> size_t {
    const size_t h1 = std::hash<double>{}(loc.x);
    const size_t h2 = std::hash<double>{}(loc.y);
    return qc::combineHash(h1, h2);
  }
};
