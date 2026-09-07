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
 * @brief Defines a type for representing zones in NA architectures.
 */

#pragma once

#include <optional>
#include <ostream>
#include <string>
#include <utility>

namespace na {

struct Location;

/// Represents a zone in the NAComputation.
/// @details The name of the zone is used for printing the NAComputation.
/// To maintain the uniqueness of zones, the name of the zone should be unique
/// for this zone.
class Zone final {
public:
  /// A simple struct to represent the extent of a zone.
  struct Extent {
    double minX = 0.0;
    double minY = 0.0;
    double maxX = 0.0;
    double maxY = 0.0;
  };

private:
  /// The identifier of the zone.
  std::string name_;
  /// The extent of the zone.
  std::optional<Extent> extent_;

public:
  /// Creates a new zone with the given name.
  /// @param name The name of the zone.
  explicit Zone(std::string name) : name_(std::move(name)) {}

  /// Creates a new zone with the given name.
  /// @param name The name of the zone.
  /// @param extent The extent of the zone.
  Zone(std::string name, const Extent& extent)
      : name_(std::move(name)), extent_(extent) {}

  /// Returns the name of the zone.
  [[nodiscard]] auto getName() const -> std::string { return name_; }

  /// Returns the extent of the zone.
  [[nodiscard]] const std::optional<Extent>& getExtent() const {
    return extent_;
  }

  /// Sets the extent of the zone.
  void setExtent(const Extent& extent) { extent_ = extent; }

  [[nodiscard]] auto contains(const Location& location) const -> bool;

  /// Prints the zone to the given output stream.
  /// @param os The output stream to print the zone to.
  /// @param obj The zone to print.
  /// @return The output stream after printing the zone.
  friend auto operator<<(std::ostream& os, const Zone& obj) -> std::ostream& {
    return os << obj.getName();
  }

  /// Compares two zones for equality.
  /// @param other The zone to compare with.
  /// @return True if the zones are equal, false otherwise.
  [[nodiscard]] auto operator==(const Zone& other) const -> bool {
    if (this == &other) {
      return true;
    }
    return name_ == other.name_;
  }

  /// Compares two zones for inequality.
  /// @param other The zone to compare with.
  /// @return True if the zones are not equal, false otherwise.
  [[nodiscard]] auto operator!=(const Zone& other) const -> bool {
    return !(*this == other);
  }
};
} // namespace na
