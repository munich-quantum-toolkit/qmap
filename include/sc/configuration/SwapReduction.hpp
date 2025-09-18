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

enum class SwapReduction : std::uint8_t {
  None,
  CouplingLimit,
  Custom,
  Increasing
};

[[maybe_unused]] static inline std::string
toString(const SwapReduction strategy) {
  switch (strategy) {
  case SwapReduction::CouplingLimit:
    return "coupling_limit";
  case SwapReduction::Custom:
    return "custom";
  case SwapReduction::None:
    return "none";
  case SwapReduction::Increasing:
    return "increasing";
  }
  return " ";
}

[[maybe_unused]] static SwapReduction
swapReductionFromString(const std::string& reduction) {
  if (reduction == "none" || reduction == "0") {
    return SwapReduction::None;
  }
  if (reduction == "coupling_limit" || reduction == "1") {
    return SwapReduction::CouplingLimit;
  }
  if (reduction == "custom" || reduction == "2") {
    return SwapReduction::Custom;
  }
  if (reduction == "increasing" || reduction == "3") {
    return SwapReduction::Increasing;
  }
  throw std::invalid_argument("Invalid swap reduction value: " + reduction);
}
