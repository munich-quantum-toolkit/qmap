/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#pragma once

#include <gmock/gmock-matchers.h>

namespace testing {
MATCHER_P2(AnglesNear, expected, tolerance, "angles near expected") {
  return std::fabs(arg.theta - expected.theta) <= tolerance &&
         std::fabs(arg.phi - expected.phi) <= tolerance &&
         std::fabs(arg.lambda - expected.lambda) <= tolerance;
}
MATCHER_P2(U3GateNear, expected, tolerance, "angles near expected") {
  return std::fabs(arg.angles.theta - expected.angles.theta) <= tolerance &&
         std::fabs(arg.angles.phi - expected.angles.phi) <= tolerance &&
         std::fabs(arg.angles.lambda - expected.angles.lambda) <= tolerance &&
         arg.qubit == expected.qubit;
}
MATCHER_P2(QuaternionNear, expected, tolerance, "quaternion near expected") {
  return std::fabs(arg.a - expected.a) <= tolerance &&
         std::fabs(arg.b - expected.b) <= tolerance &&
         std::fabs(arg.c - expected.c) <= tolerance &&
         std::fabs(arg.d - expected.d) <= tolerance;
}
} // namespace testing
