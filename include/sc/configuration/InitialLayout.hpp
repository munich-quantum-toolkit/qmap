/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

/// Identity: q_i -> Q_i
/// Static: first layer is mapped q_c -> Q_c and q_t -> Q_t
/// Dynamic: Layout is generated on demand upon encountering a specific gate
enum class InitialLayout : std::uint8_t { Identity, Static, Dynamic };

[[maybe_unused]] static inline std::string
toString(const InitialLayout strategy) {
  switch (strategy) {
  case InitialLayout::Identity:
    return "identity";
  case InitialLayout::Static:
    return "static";
  case InitialLayout::Dynamic:
    return "dynamic";
  }
  return " ";
}

[[maybe_unused]] static InitialLayout
initialLayoutFromString(const std::string& initialLayout) {
  if (initialLayout == "identity" || initialLayout == "0") {
    return InitialLayout::Identity;
  }
  if (initialLayout == "static" || initialLayout == "1") {
    return InitialLayout::Static;
  }
  if (initialLayout == "dynamic" || initialLayout == "2") {
    return InitialLayout::Dynamic;
  }
  throw std::invalid_argument("Invalid initial layout value: " + initialLayout);
}
