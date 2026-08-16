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
 * @brief Defines the base class for all operations in the NAComputation.
 */

#pragma once

#include <ostream>
#include <string>

namespace na {
/// This is the base class for all operations in the NAComputation.
class Op {
public:
  /// Default constructor.
  Op() = default;

  /// Virtual destructor.
  virtual ~Op() = default;

  /// Returns a string representation of the operation.
  [[nodiscard]] virtual auto toString() const -> std::string = 0;

  /// Prints the operation to the given output stream.
  /// @param os The output stream to print the operation to.
  /// @param obj The operation to print.
  /// @return The output stream after printing the operation.
  friend auto operator<<(std::ostream& os, const Op& obj) -> std::ostream& {
    return os << obj.toString(); // Using toString() method
  }

  /// Checks if the operation is of the given type.
  /// @tparam T The type to check for.
  /// @return True if the operation is of the given type, false otherwise.
  template <class T> [[nodiscard]] auto is() const -> bool {
    return dynamic_cast<const T*>(this) != nullptr;
  }

  /// Casts the operation to the given type.
  /// @tparam T The type to cast to.
  /// @return A reference to the operation as the given type.
  template <class T> [[nodiscard]] auto as() -> T& {
    return dynamic_cast<T&>(*this);
  }

  /// Casts the operation to the given type.
  /// @tparam T The type to cast to.
  /// @return A const reference to the operation as the given type.
  template <class T> [[nodiscard]] auto as() const -> const T& {
    return dynamic_cast<const T&>(*this);
  }
};
} // namespace na
