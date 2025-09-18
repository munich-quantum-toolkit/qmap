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

enum class Method : std::uint8_t { None, Exact, Heuristic };

[[maybe_unused]] static inline std::string toString(const Method method) {
  switch (method) {
  case Method::None:
    return "none";
  case Method::Exact:
    return "exact";
  case Method::Heuristic:
    return "heuristic";
  }
  return " ";
}

[[maybe_unused]] static Method methodFromString(const std::string& method) {
  if (method == "none" || method == "0") {
    return Method::None;
  }
  if (method == "exact" || method == "1") {
    return Method::Exact;
  }
  if (method == "heuristic" || method == "2") {
    return Method::Heuristic;
  }
  throw std::invalid_argument("Invalid method value: " + method);
}
