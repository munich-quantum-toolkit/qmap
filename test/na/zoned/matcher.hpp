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
/**
 * @brief Computes the wraparound-aware absolute angular distance between two
 *        angles (radians), normalized into [-π, π].
 * @param a first angle.
 * @param b second angle.
 * @returns the absolute angular distance.
 */
constexpr auto angleDiff = [](const double a, const double b) -> double {
  double d = std::fmod(a - b, 2 * qc::PI);
  if (d > qc::PI) {
    d -= 2 * qc::PI;
  }
  if (d < -qc::PI) {
    d += 2 * qc::PI;
  }
  return std::fabs(d);
};
/**
 * @brief Matcher that checks if three Euler angles (theta, phi, lambda)
 *        are within a given tolerance of the expected angles.
 *
 * @param expected The expected na::zoned::NativeGateDecomposer::Angles value.
 * @param tolerance Numeric tolerance for angle comparisons (radians).
 */
MATCHER_P2(AnglesNear, expected, tolerance,
           std::string("angles ") + (negation ? "aren't" : "are") +
               " near expected") {
  if (angleDiff(arg.theta, expected.theta) <= tolerance &&
      angleDiff(arg.phi, expected.phi) <= tolerance &&
      angleDiff(arg.lambda, expected.lambda) <= tolerance) {
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
/**
 * @brief Matcher for U3 gate-like structures: checks angles and target qubit.
 *
 * @param expected Struct containing expected angles and qubit.
 * @param tolerance Tolerance passed to AnglesNear for angle comparisons.
 */
MATCHER_P2(U3GateNear, expected, tolerance,
           DescribeMatcher<na::zoned::NativeGateDecomposer::Angles>(
               AnglesNear(expected.angles, tolerance), negation) +
               (negation ? " or " : " and ") +
               DescribeMatcher<qc::Qubit>(Eq(expected.qubit), negation)) {
  return ExplainMatchResult(AnglesNear(expected.angles, tolerance), arg.angles,
                            result_listener) &&
         ExplainMatchResult(Eq(expected.qubit), arg.qubit, result_listener);
}
// Note: q and -q encode the same rotation, but the matcher only checks one
// sign. Currently, we have not implemented a canonicalized form, so equivalent
// results can fail here (but, currently, there are no false-positives).
/**
 * @brief Matcher that compares two quaternions element-wise within tolerance.
 *
 * @param expected Quaternion with components {a, b, c, d} to compare against.
 * @param tolerance Absolute tolerance for each component comparison.
 */
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
/**
 * @brief Matcher verifying a rotation gate's type, target qubit, and angle.
 *
 * @param type Expected gate type (e.g., qc::RZ).
 * @param qubit Expected target qubit index.
 * @param angle Expected rotation angle (radians).
 * @param tolerance Allowed difference between expected and actual angle.
 */
MATCHER_P4(ExpectRotationGate, type, qubit, angle, tolerance,
           DescribeMatcher<qc::OpType>(Eq(type), negation) +
               (negation ? " or " : " and ") +
               DescribeMatcher<qc::Targets>(ElementsAre(Eq(qubit)), negation) +
               (negation ? " or " : " and ") +
               DescribeMatcher<std::vector<qc::fp>>(
                   ElementsAre(DoubleNear(angle, tolerance)), negation)) {
  return ExplainMatchResult(Eq(type), arg->getType(), result_listener) &&
         ExplainMatchResult(ElementsAre(Eq(qubit)), arg->getTargets(),
                            result_listener) &&
         ExplainMatchResult(ElementsAre(DoubleNear(angle, tolerance)),
                            arg->getParameter(), result_listener);
}
/**
 * @brief Matcher verifying a global rotation gate's type and angle.
 *
 * @param type Expected gate type (e.g., qc::RZ).
 * @param nQubit Expected number of qubits the gate acts on.
 * @param angle Expected rotation angle (radians).
 * @param tolerance Allowed difference between expected and actual angle.
 */
MATCHER_P4(ExpectGlobalRotationGate, type, nQubit, angle, tolerance,
           std::string("global gate ") + (negation ? "isn't" : "is") +
               " as expected") {
  if (arg->getType() != qc::Compound) {
    *result_listener << "actual: type=" << arg->getType()
                     << ", expected: type=" << qc::Compound;
    return false;
  }
  const auto& compoundOp = dynamic_cast<const qc::CompoundOperation&>(*arg);
  if (compoundOp.size() != nQubit) {
    *result_listener << "actual: nqubits=" << compoundOp.size()
                     << ", expected: nqubits=" << nQubit;
    return false;
  }
  const auto& ops = compoundOp.getOps();
  for (size_t i = 0; i < nQubit; ++i) {
    if (ops.at(i)->getType() != type) {
      *result_listener << "actual: gate[" << i
                       << "] type=" << ops.at(i)->getType()
                       << ", expected: type=" << type;
      return false;
    }
    if (ops.at(i)->getTargets().front() != i) {
      *result_listener << "actual: gate[" << i
                       << "] target=" << ops.at(i)->getTargets().front()
                       << ", expected: target=" << i;
      return false;
    }
    if (std::fabs(ops.at(i)->getParameter().front() - angle) > tolerance) {
      *result_listener << "actual: gate[" << i
                       << "] angle=" << ops.at(i)->getParameter().front()
                       << ", expected: angle=" << angle
                       << ", tolerance=" << tolerance;
      return false;
    }
  }
  return true;
}
} // namespace testing

// NOLINTEND(modernize-use-trailing-return-type)
