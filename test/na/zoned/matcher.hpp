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

#include "na/zoned/decomposer/NativeGateDecomposer.hpp"

#include <gmock/gmock-matchers.h>

// NOLINTBEGIN(modernize-use-trailing-return-type)

namespace testing {
constexpr auto angleDiff = [](const double a, const double b) {
  double d = std::fmod(a - b, 2 * qc::PI);
  if (d > qc::PI) {
    d -= 2 * qc::PI;
  }
  if (d < -qc::PI) {
    d += 2 * qc::PI;
  }
  return std::fabs(d);
};
MATCHER_P2(AnglesNear, expected, tolerance,
           std::string("angles ") + (negation ? "aren't" : "are") +
               " near expected") {
  if (angleDiff(arg.theta - expected.theta) <= tolerance &&
      angleDiff(arg.phi - expected.phi) <= tolerance &&
      angleDiff(arg.lambda - expected.lambda) <= tolerance) {
    return true;
  }
  *result_listener << "actual: {theta=" << arg.theta << ", phi=" << arg.phi
                   << ", lambda=" << arg.lambda << "} "
                   << "expected: {theta=" << expected.theta
                   << ", phi=" << expected.phi << ", lambda=" << expected.lambda
                   << "} "
                   << "(tolerance=" << tolerance << ")";
  return false;
}
MATCHER_P2(U3GateNear, expected, tolerance,
           DescribeMatcher<na::zoned::NativeGateDecomposer::Angles>(
               AnglesNear(expected.angles, tolerance), negation) +
               (negation ? " or " : " and ") +
               DescribeMatcher<qc::Qubit>(Eq(expected.qubit), negation)) {
  return ExplainMatchResult(AnglesNear(expected.angles, tolerance), arg.angles,
                            result_listener) &&
         ExplainMatchResult(Eq(expected.qubit), arg.qubit, result_listener);
}
MATCHER_P2(QuaternionNear, expected, tolerance,
           std::string("quaternion ") + (negation ? "isn't" : "is") +
               " near expected") {
  if (std::fabs(arg.a - expected.a) <= tolerance &&
      std::fabs(arg.b - expected.b) <= tolerance &&
      std::fabs(arg.c - expected.c) <= tolerance &&
      std::fabs(arg.d - expected.d) <= tolerance) {
    return true;
  }
  *result_listener << "actual: {a=" << arg.a << ", b=" << arg.b
                   << ", c=" << arg.c << ", d=" << arg.d << "} "
                   << "expected: {a=" << expected.a << ", b=" << expected.b
                   << ", c=" << expected.c << ", d=" << expected.d << "} "
                   << "(tolerance=" << tolerance << ")";
  return false;
}
MATCHER_P4(ExpectRotationGate, type, qubit, angle, tolerance,
           DescribeMatcher<qc::OpType>(Eq(type), negation) +
               (negation ? " or " : " and ") +
               DescribeMatcher<qc::Qubit>(ElementsAre(Eq(qubit)), negation) +
               (negation ? " or " : " and ") +
               DescribeMatcher<qc::fp>(
                   ElementsAre(DoubleNear(angle, tolerance)), negation)) {
  return ExplainMatchResult(Eq(type), arg.getType(), result_listener) &&
         ExplainMatchResult(ElementsAre(Eq(qubit)), arg.getTargets(),
                            result_listener) &&
         ExplainMatchResult(ElementsAre(DoubleNear(angle, tolerance)),
                            arg.getParameter(), result_listener);
}
} // namespace testing

// NOLINTEND(modernize-use-trailing-return-type)
