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
#include <string>

namespace cs {
enum class TargetMetric : std::uint8_t { Gates, TwoQubitGates, Depth };

[[maybe_unused]] static inline std::string toString(const TargetMetric target) {
  switch (target) {
  case TargetMetric::Gates:
    return "gates";
  case TargetMetric::TwoQubitGates:
    return "two_qubit_gates";
  case TargetMetric::Depth:
    return "depth";
  }
  return "Error";
}

[[maybe_unused]] static TargetMetric
targetMetricFromString(const std::string& target) {
  if (target == "gates") {
    return TargetMetric::Gates;
  }
  if (target == "two_qubit_gates") {
    return TargetMetric::TwoQubitGates;
  }
  if (target == "depth") {
    return TargetMetric::Depth;
  }
  return TargetMetric::Gates;
}
} // namespace cs
