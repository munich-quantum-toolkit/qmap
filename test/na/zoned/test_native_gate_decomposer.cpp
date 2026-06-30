/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "ir/QuantumComputation.hpp"
#include "matcher.hpp"
#include "na/zoned/decomposer/NativeGateDecomposer.hpp"
#include "na/zoned/scheduler/ASAPScheduler.hpp"

#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

namespace na::zoned {
constexpr std::string_view architectureJson = R"({
  "name": "asap_scheduler_architecture",
  "storage_zones": [{
    "zone_id": 0,
    "slms": [{"id": 0, "site_separation": [3, 3], "r": 20, "c": 20, "location": [0, 0]}],
    "offset": [0, 0],
    "dimension": [60, 60]
  }],
  "entanglement_zones": [{
    "zone_id": 0,
    "slms": [
      {"id": 1, "site_separation": [12, 10], "r": 4, "c": 4, "location": [5, 70]},
      {"id": 2, "site_separation": [12, 10], "r": 4, "c": 4, "location": [7, 70]}
    ],
    "offset": [5, 70],
    "dimension": [50, 40]
  }],
  "aods":[{"id": 0, "site_separation": 2, "r": 20, "c": 20}],
  "rydberg_range": [[[5, 70], [55, 110]]]
})";

class DecomposerTest : public ::testing::Test {
protected:
  Architecture architecture;
  ASAPScheduler::Config schedulerConfig{.maxFillingFactor = .8};
  ASAPScheduler scheduler;
  NativeGateDecomposer::Config decomposerConfig{};
  NativeGateDecomposer decomposer;
  DecomposerTest()
      : architecture(Architecture::fromJSONString(architectureJson)),
        scheduler(architecture, schedulerConfig),
        decomposer(architecture, decomposerConfig) {}
};

// Test Translation of gates S, Sdg, T, Tdg, U2, RY, Y, Vdg, SX, SXdg,
// Unrecognized, H

TEST(NativeGateDecomposerTest, ZRotGateTranslationTest) {
  qc::StandardOperation op(0, qc::Z);
  EXPECT_THAT(
      NativeGateDecomposer::convertGateToQuaternion(
          std::reference_wrapper<const qc::Operation>(op)),
      ::testing::QuaternionNear(NativeGateDecomposer::Quaternion{0, 0, 0, 1},
                                NativeGateDecomposer::epsilon));

  op = qc::StandardOperation(0, qc::RZ, {qc::PI_2});
  EXPECT_THAT(NativeGateDecomposer::convertGateToQuaternion(
                  std::reference_wrapper<const qc::Operation>(op)),
              ::testing::QuaternionNear(
                  NativeGateDecomposer::Quaternion{1 / std::sqrt(2), 0, 0,
                                                   1 / std::sqrt(2)},
                  NativeGateDecomposer::epsilon));
  op = qc::StandardOperation(0, qc::P, {qc::PI_2});
  EXPECT_THAT(NativeGateDecomposer::convertGateToQuaternion(
                  std::reference_wrapper<const qc::Operation>(op)),
              ::testing::QuaternionNear(
                  NativeGateDecomposer::Quaternion{1 / std::sqrt(2), 0, 0,
                                                   1 / std::sqrt(2)},
                  NativeGateDecomposer::epsilon));

  op = qc::StandardOperation(0, qc::S);
  EXPECT_THAT(NativeGateDecomposer::convertGateToQuaternion(
                  std::reference_wrapper<const qc::Operation>(op)),
              ::testing::QuaternionNear(
                  NativeGateDecomposer::Quaternion{1 / std::sqrt(2), 0, 0,
                                                   1 / std::sqrt(2)},
                  NativeGateDecomposer::epsilon));

  op = qc::StandardOperation(0, qc::Sdg);
  EXPECT_THAT(NativeGateDecomposer::convertGateToQuaternion(
                  std::reference_wrapper<const qc::Operation>(op)),
              ::testing::QuaternionNear(
                  NativeGateDecomposer::Quaternion{1 / std::sqrt(2), 0, 0,
                                                   -1 / std::sqrt(2)},
                  NativeGateDecomposer::epsilon));
  op = qc::StandardOperation(0, qc::T);
  EXPECT_THAT(
      NativeGateDecomposer::convertGateToQuaternion(
          std::reference_wrapper<const qc::Operation>(op)),
      ::testing::QuaternionNear(
          NativeGateDecomposer::Quaternion{std::sqrt(2 + std::sqrt(2)) / 2, 0,
                                           0, std::sqrt(2 - std::sqrt(2)) / 2},
          NativeGateDecomposer::epsilon));

  op = qc::StandardOperation(0, qc::Tdg);
  EXPECT_THAT(
      NativeGateDecomposer::convertGateToQuaternion(
          std::reference_wrapper<const qc::Operation>(op)),
      ::testing::QuaternionNear(
          NativeGateDecomposer::Quaternion{std::sqrt(2 + std::sqrt(2)) / 2, 0,
                                           0, -std::sqrt(2 - std::sqrt(2)) / 2},
          NativeGateDecomposer::epsilon));
}

TEST(NativeGateDecomposerTest, XYRotGateTranslationTest) {
  qc::StandardOperation op = qc::StandardOperation(0, qc::X);
  EXPECT_THAT(
      NativeGateDecomposer::convertGateToQuaternion(
          std::reference_wrapper<const qc::Operation>(op)),
      ::testing::QuaternionNear(NativeGateDecomposer::Quaternion{0, 1, 0, 0},
                                NativeGateDecomposer::epsilon));

  op = qc::StandardOperation(0, qc::RX, {qc::PI_2});

  EXPECT_THAT(NativeGateDecomposer::convertGateToQuaternion(
                  std::reference_wrapper<const qc::Operation>(op)),
              ::testing::QuaternionNear(
                  NativeGateDecomposer::Quaternion{1 / std::sqrt(2),
                                                   1 / std::sqrt(2), 0, 0},
                  NativeGateDecomposer::epsilon));

  op = qc::StandardOperation(0, qc::Y);

  EXPECT_THAT(
      NativeGateDecomposer::convertGateToQuaternion(
          std::reference_wrapper<const qc::Operation>(op)),
      ::testing::QuaternionNear(NativeGateDecomposer::Quaternion{0, 0, 1, 0},
                                NativeGateDecomposer::epsilon));

  op = qc::StandardOperation(0, qc::RY, {qc::PI_2});

  EXPECT_THAT(NativeGateDecomposer::convertGateToQuaternion(
                  std::reference_wrapper<const qc::Operation>(op)),
              ::testing::QuaternionNear(
                  NativeGateDecomposer::Quaternion{1 / std::sqrt(2), 0,
                                                   1 / std::sqrt(2), 0},
                  NativeGateDecomposer::epsilon));

  op = qc::StandardOperation(0, qc::SX);

  EXPECT_THAT(NativeGateDecomposer::convertGateToQuaternion(
                  std::reference_wrapper<const qc::Operation>(op)),
              ::testing::QuaternionNear(
                  NativeGateDecomposer::Quaternion{1 / std::sqrt(2),
                                                   1 / std::sqrt(2), 0, 0},
                  NativeGateDecomposer::epsilon));

  op = qc::StandardOperation(0, qc::SXdg);

  EXPECT_THAT(NativeGateDecomposer::convertGateToQuaternion(
                  std::reference_wrapper<const qc::Operation>(op)),
              ::testing::QuaternionNear(
                  NativeGateDecomposer::Quaternion{1 / std::sqrt(2),
                                                   -1 / std::sqrt(2), 0, 0},
                  NativeGateDecomposer::epsilon));
}

TEST(NativeGateDecomposerTest, UGateTranslationTest) {
  qc::fp p = qc::PI_2;
  qc::fp t = qc::PI_4;
  qc::fp l = qc::PI_4;
  const qc::StandardOperation op1(0, qc::U, {t, p, l});
  EXPECT_THAT(NativeGateDecomposer::convertGateToQuaternion(op1),
              ::testing::QuaternionNear(
                  NativeGateDecomposer::Quaternion{
                      std::cos(p / 2) * std::cos(t / 2) * std::cos(l / 2) -
                          std::sin(p / 2) * std::cos(t / 2) * std::sin(l / 2),
                      std::cos(p / 2) * std::sin(t / 2) * std::sin(l / 2) -
                          std::sin(p / 2) * std::cos(l / 2) * std::sin(t / 2),
                      std::cos(p / 2) * std::sin(t / 2) * std::cos(l / 2) +
                          std::sin(p / 2) * std::sin(l / 2) * std::sin(t / 2),
                      std::cos(p / 2) * std::cos(t / 2) * std::sin(l / 2) +
                          std::sin(p / 2) * std::cos(l / 2) * std::cos(t / 2)},
                  NativeGateDecomposer::epsilon));

  t = qc::PI_2;
  const qc::StandardOperation op2(0, qc::U2, {p, l});
  EXPECT_THAT(NativeGateDecomposer::convertGateToQuaternion(op2),
              ::testing::QuaternionNear(
                  NativeGateDecomposer::Quaternion{
                      std::cos(p / 2) * std::cos(t / 2) * std::cos(l / 2) -
                          std::sin(p / 2) * std::cos(t / 2) * std::sin(l / 2),
                      std::cos(p / 2) * std::sin(t / 2) * std::sin(l / 2) -
                          std::sin(p / 2) * std::cos(l / 2) * std::sin(t / 2),
                      std::cos(p / 2) * std::sin(t / 2) * std::cos(l / 2) +
                          std::sin(p / 2) * std::sin(l / 2) * std::sin(t / 2),
                      std::cos(p / 2) * std::cos(t / 2) * std::sin(l / 2) +
                          std::sin(p / 2) * std::cos(l / 2) * std::cos(t / 2)},
                  NativeGateDecomposer::epsilon));

  t = -qc::PI_2;
  l = -qc::PI_2;

  const qc::StandardOperation op3(0, qc::Vdg);
  EXPECT_THAT(NativeGateDecomposer::convertGateToQuaternion(
                  std::reference_wrapper<const qc::Operation>(op3)),
              ::testing::QuaternionNear(
                  NativeGateDecomposer::Quaternion{
                      std::cos(p / 2) * std::cos(t / 2) * std::cos(l / 2) -
                          std::sin(p / 2) * std::cos(t / 2) * std::sin(l / 2),
                      std::cos(p / 2) * std::sin(t / 2) * std::sin(l / 2) -
                          std::sin(p / 2) * std::cos(l / 2) * std::sin(t / 2),
                      std::cos(p / 2) * std::sin(t / 2) * std::cos(l / 2) +
                          std::sin(p / 2) * std::sin(l / 2) * std::sin(t / 2),
                      std::cos(p / 2) * std::cos(t / 2) * std::sin(l / 2) +
                          std::sin(p / 2) * std::cos(l / 2) * std::cos(t / 2)},
                  NativeGateDecomposer::epsilon));
  const qc::StandardOperation op4(0, qc::H);
  EXPECT_THAT(NativeGateDecomposer::convertGateToQuaternion(op4),
              ::testing::QuaternionNear(
                  NativeGateDecomposer::Quaternion{0, 1 / std::sqrt(2), 0,
                                                   1 / std::sqrt(2)},
                  NativeGateDecomposer::epsilon));
}

TEST(NativeGateDecomposerTest, ThreeQuaternionCombiTest) {
  const NativeGateDecomposer::Quaternion q1{cos(qc::PI_4), 0, 0, sin(qc::PI_4)};
  const NativeGateDecomposer::Quaternion q2{cos(qc::PI_2), 0, sin(qc::PI_2), 0};
  const auto& q12 = NativeGateDecomposer::combineQuaternions(q1, q2);
  EXPECT_THAT(q12, ::testing::QuaternionNear(
                       NativeGateDecomposer::Quaternion{0, -cos(qc::PI_4),
                                                        cos(qc::PI_4), 0},
                       NativeGateDecomposer::epsilon));
  const NativeGateDecomposer::Quaternion q3{cos(qc::PI_2), 0, 0, sin(qc::PI_2)};
  const auto& q13 = NativeGateDecomposer::combineQuaternions(q12, q3);
  EXPECT_THAT(q13, ::testing::QuaternionNear(
                       NativeGateDecomposer::Quaternion{0, cos(qc::PI_4),
                                                        cos(qc::PI_4), 0},
                       NativeGateDecomposer::epsilon));
}

TEST(NativeGateDecomposerTest, ThreeQuaternionU3Test) {
  const NativeGateDecomposer::Quaternion q1{cos(qc::PI_2), 0, 0, sin(qc::PI_2)};
  const NativeGateDecomposer::Quaternion q2{cos(qc::PI_4 / 2), 0,
                                            sin(qc::PI_4 / 2), 0};
  const auto& q12 = NativeGateDecomposer::combineQuaternions(q1, q2);
  EXPECT_THAT(q12, ::testing::QuaternionNear(
                       NativeGateDecomposer::Quaternion{0, -sin(qc::PI_4 / 2),
                                                        0, cos(qc::PI_4 / 2)},
                       NativeGateDecomposer::epsilon));
  const NativeGateDecomposer::Quaternion q3{cos(qc::PI_4), 0, 0, sin(qc::PI_4)};
  const auto& q13 = NativeGateDecomposer::combineQuaternions(q12, q3);
  const auto r2 = 1 / std::sqrt(2);
  EXPECT_THAT(q13, ::testing::QuaternionNear(
                       NativeGateDecomposer::Quaternion{
                           -r2 * cos(qc::PI_4 / 2), -r2 * sin(qc::PI_4 / 2),
                           r2 * sin(qc::PI_4 / 2), r2 * cos(qc::PI_4 / 2)},
                       NativeGateDecomposer::epsilon));
}

TEST(NativeGateDecomposerTest, SingleXGateAngleTest) {
  const qc::StandardOperation op(0, qc::X);
  const auto& q = NativeGateDecomposer::convertGateToQuaternion(op);
  EXPECT_THAT(
      NativeGateDecomposer::getU3AnglesFromQuaternion(q),
      ::testing::AnglesNear(NativeGateDecomposer::Angles{qc::PI, 0, qc::PI},
                            NativeGateDecomposer::epsilon));
}

TEST(NativeGateDecomposerTest, SingleU3GateAngleTest) {
  const qc::StandardOperation op(0, qc::U, {qc::PI_4, qc::PI, qc::PI_2});
  const auto& q = NativeGateDecomposer::convertGateToQuaternion(op);
  const auto r2 = 1 / sqrt(2);
  EXPECT_THAT(q, ::testing::QuaternionNear(
                     NativeGateDecomposer::Quaternion{
                         -r2 * cos(qc::PI_4 / 2), -r2 * sin(qc::PI_4 / 2),
                         r2 * sin(qc::PI_4 / 2), r2 * cos(qc::PI_4 / 2)},
                     NativeGateDecomposer::epsilon));

  EXPECT_THAT(NativeGateDecomposer::getU3AnglesFromQuaternion(q),
              ::testing::AnglesNear(
                  NativeGateDecomposer::Angles{qc::PI_4, qc::PI, qc::PI_2},
                  NativeGateDecomposer::epsilon));
}

TEST(NativeGateDecomposerTest, ThetaPiAngleTest) {
  qc::StandardOperation op(0, qc::U, {qc::PI, qc::PI, qc::PI_2});
  const auto& q = NativeGateDecomposer::convertGateToQuaternion(op);
  const auto r2 = 1 / sqrt(2);
  EXPECT_THAT(q, ::testing::QuaternionNear(
                     NativeGateDecomposer::Quaternion{0, -r2, r2, 0},
                     NativeGateDecomposer::epsilon));
  EXPECT_THAT(
      NativeGateDecomposer::getU3AnglesFromQuaternion(q),
      ::testing::AnglesNear(NativeGateDecomposer::Angles{qc::PI, 0, -qc::PI_2},
                            NativeGateDecomposer::epsilon));
}

TEST(NativeGateDecomposerTest, ThetaZeroAngleTest) {
  const qc::StandardOperation op(0, qc::U, {0, qc::PI, qc::PI_2});
  const auto& q = NativeGateDecomposer::convertGateToQuaternion(op);
  const auto r2 = 1 / sqrt(2);
  EXPECT_THAT(q, ::testing::QuaternionNear(
                     NativeGateDecomposer::Quaternion{-r2, 0, 0, r2},
                     NativeGateDecomposer::epsilon));

  EXPECT_THAT(
      NativeGateDecomposer::getU3AnglesFromQuaternion(q),
      ::testing::AnglesNear(NativeGateDecomposer::Angles{0, 0, 3 * qc::PI_2},
                            NativeGateDecomposer::epsilon));
}

TEST(NativeGateDecomposerTest, RXDecompositionTest) {
  constexpr NativeGateDecomposer::Angles rx{qc::PI, -qc::PI_2, qc::PI_2};
  EXPECT_THAT(NativeGateDecomposer::getDecompositionAngles(rx, qc::PI),
              ::testing::AnglesNear(NativeGateDecomposer::Angles{qc::PI, 0, 0},
                                    NativeGateDecomposer::epsilon));
}

TEST(NativeGateDecomposerTest, U3DecompositionTest) {
  constexpr NativeGateDecomposer::Angles u3{qc::PI_4, qc::PI, qc::PI_2};
  EXPECT_THAT(NativeGateDecomposer::getDecompositionAngles(u3, qc::PI_4),
              ::testing::AnglesNear(
                  NativeGateDecomposer::Angles{qc::PI, qc::PI, -qc::PI_2},
                  NativeGateDecomposer::epsilon));
}

TEST(NativeGateDecomposerTest, DoubleDecompositionTest) {
  constexpr NativeGateDecomposer::Angles x1{qc::PI, -qc::PI_2, qc::PI_2};
  constexpr NativeGateDecomposer::Angles z2{0, 0, qc::PI};
  EXPECT_THAT(NativeGateDecomposer::getDecompositionAngles(x1, qc::PI),
              ::testing::AnglesNear(NativeGateDecomposer::Angles{qc::PI, 0, 0},
                                    NativeGateDecomposer::epsilon));
  EXPECT_THAT(
      NativeGateDecomposer::getDecompositionAngles(z2, qc::PI),
      ::testing::AnglesNear(NativeGateDecomposer::Angles{0, qc::PI_2, qc::PI_2},
                            NativeGateDecomposer::epsilon));
}

TEST_F(DecomposerTest, SingleRXGate) {
  //    ┌───────┐
  // q: ┤ Rx(π) ├
  //    └───────┘
  size_t n = 1;
  qc::QuantumComputation qc(n);
  qc.rx(qc::PI, 0);
  const auto& [singleQubitLayers, twoQubitLayers] = scheduler.schedule(qc);
  const auto& decompSingleQubitLayers =
      decomposer.decompose(qc.getNqubits(), singleQubitLayers, twoQubitLayers)
          .singleQubitLayers;
  EXPECT_EQ(decompSingleQubitLayers.size(), 1);
  EXPECT_EQ(decompSingleQubitLayers[0].size(), 5);
  EXPECT_EQ(decompSingleQubitLayers[0][0]->getType(), qc::RZ);
  EXPECT_THAT(decompSingleQubitLayers[0][0]->getTargets(),
              ::testing::ElementsAre(0));
  EXPECT_THAT(decompSingleQubitLayers[0][0]->getParameter(),
              ::testing::ElementsAre(::testing::DoubleNear(
                  qc::PI_2, NativeGateDecomposer::epsilon)));
  EXPECT_TRUE(decompSingleQubitLayers[0][1]->isCompoundOperation());
  EXPECT_TRUE(decompSingleQubitLayers[0][1]->isGlobal(n));
  EXPECT_EQ(decompSingleQubitLayers[0][2]->getType(), qc::RZ);
  EXPECT_THAT(decompSingleQubitLayers[0][2]->getTargets(),
              ::testing::ElementsAre(0));
  EXPECT_THAT(decompSingleQubitLayers[0][2]->getParameter(),
              ::testing::ElementsAre(::testing::DoubleNear(
                  qc::PI, NativeGateDecomposer::epsilon)));
  EXPECT_TRUE(decompSingleQubitLayers[0][3]->isCompoundOperation());
  EXPECT_TRUE(decompSingleQubitLayers[0][3]->isGlobal(n));
  EXPECT_EQ(decompSingleQubitLayers[0][4]->getType(), qc::RZ);
  EXPECT_THAT(decompSingleQubitLayers[0][4]->getTargets(),
              ::testing::ElementsAre(0));
  EXPECT_THAT(decompSingleQubitLayers[0][4]->getParameter(),
              ::testing::ElementsAre(::testing::DoubleNear(
                  qc::PI_2, NativeGateDecomposer::epsilon)));
}

TEST_F(DecomposerTest, SingleU3Gate) {
  //    ┌─────────────┐
  // q: ┤ U3(0,π,π/2) ├
  //    └─────────────┘
  size_t n = 1;
  qc::QuantumComputation qc(n);
  qc.u(0.0, qc::PI, qc::PI_2, 0);
  const auto& [singleQubitLayers, twoQubitLayers] = scheduler.schedule(qc);
  const auto& decompSingleQubitLayers =
      decomposer.decompose(qc.getNqubits(), singleQubitLayers, twoQubitLayers)
          .singleQubitLayers;
  EXPECT_EQ(decompSingleQubitLayers.size(), 1);
  EXPECT_EQ(decompSingleQubitLayers[0].size(), 5);

  EXPECT_EQ(decompSingleQubitLayers[0][0]->getType(), qc::RZ);
  EXPECT_THAT(decompSingleQubitLayers[0][0]->getTargets(),
              ::testing::ElementsAre(0));
  EXPECT_THAT(decompSingleQubitLayers[0][0]->getParameter(),
              ::testing::ElementsAre(
                  ::testing::DoubleNear(0, NativeGateDecomposer::epsilon)));
  EXPECT_TRUE(decompSingleQubitLayers[0][1]->isCompoundOperation());
  EXPECT_TRUE(decompSingleQubitLayers[0][1]->isGlobal(n));
  EXPECT_EQ(decompSingleQubitLayers[0][2]->getType(), qc::RZ);
  EXPECT_THAT(decompSingleQubitLayers[0][2]->getTargets(),
              ::testing::ElementsAre(0));
  EXPECT_THAT(decompSingleQubitLayers[0][2]->getParameter(),
              ::testing::ElementsAre(::testing::DoubleNear(
                  qc::PI, NativeGateDecomposer::epsilon)));
  EXPECT_TRUE(decompSingleQubitLayers[0][3]->isCompoundOperation());
  EXPECT_TRUE(decompSingleQubitLayers[0][3]->isGlobal(n));
  EXPECT_EQ(decompSingleQubitLayers[0][4]->getType(), qc::RZ);
  EXPECT_THAT(decompSingleQubitLayers[0][4]->getTargets(),
              ::testing::ElementsAre(0));
  EXPECT_THAT(decompSingleQubitLayers[0][4]->getParameter(),
              ::testing::ElementsAre(::testing::DoubleNear(
                  qc::PI_2, NativeGateDecomposer::epsilon)));
}

TEST_F(DecomposerTest, TwoPauliGatesOneQubit) {
  //    ┌───────┐  ┌───────┐
  // q: ┤   X   ├──┤   Z   ├
  //    └───────┘  └───────┘
  size_t n = 1;
  qc::QuantumComputation qc(n);
  qc.x(0);
  qc.z(0);
  const auto& [singleQubitLayers, twoQubitLayers] = scheduler.schedule(qc);
  const auto& decompSingleQubitLayers =
      decomposer.decompose(qc.getNqubits(), singleQubitLayers, twoQubitLayers)
          .singleQubitLayers;

  EXPECT_EQ(decompSingleQubitLayers.size(), 1);
  EXPECT_EQ(decompSingleQubitLayers[0].size(), 5);
  EXPECT_EQ(decompSingleQubitLayers[0][0]->getType(), qc::RZ);
  EXPECT_THAT(decompSingleQubitLayers[0][0]->getTargets(),
              ::testing::ElementsAre(0));
  EXPECT_THAT(decompSingleQubitLayers[0][0]->getParameter(),
              ::testing::ElementsAre(::testing::DoubleNear(
                  qc::PI_2, NativeGateDecomposer::epsilon)));
  EXPECT_TRUE(decompSingleQubitLayers[0][1]->isCompoundOperation());
  EXPECT_TRUE(decompSingleQubitLayers[0][1]->isGlobal(n));
  EXPECT_EQ(decompSingleQubitLayers[0][2]->getType(), qc::RZ);
  EXPECT_THAT(decompSingleQubitLayers[0][2]->getTargets(),
              ::testing::ElementsAre(0));
  EXPECT_THAT(decompSingleQubitLayers[0][2]->getParameter(),
              ::testing::ElementsAre(::testing::DoubleNear(
                  qc::PI, NativeGateDecomposer::epsilon)));
  EXPECT_TRUE(decompSingleQubitLayers[0][3]->isCompoundOperation());
  EXPECT_TRUE(decompSingleQubitLayers[0][3]->isGlobal(n));
  EXPECT_EQ(decompSingleQubitLayers[0][4]->getType(), qc::RZ);
  EXPECT_THAT(decompSingleQubitLayers[0][4]->getTargets(),
              ::testing::ElementsAre(0));
  EXPECT_THAT(decompSingleQubitLayers[0][4]->getParameter(),
              ::testing::ElementsAre(::testing::DoubleNear(
                  3 * qc::PI_2, NativeGateDecomposer::epsilon)));
}

TEST_F(DecomposerTest, TwoPauliGatesTwoQubits) {
  //       ┌───────┐
  // q_0: ─┤   X   ├─
  //       └───────┘
  //       ┌───────┐
  // q_1: ─┤   Z   ├─
  //       └───────┘

  size_t n = 2;
  qc::QuantumComputation qc(n);
  qc.x(0);
  qc.z(1);
  const auto& [singleQubitLayers, twoQubitLayers] = scheduler.schedule(qc);
  const auto& decompSingleQubitLayers =
      decomposer.decompose(qc.getNqubits(), singleQubitLayers, twoQubitLayers)
          .singleQubitLayers;
  EXPECT_EQ(decompSingleQubitLayers.size(), 1);
  EXPECT_EQ(decompSingleQubitLayers[0].size(), 8);

  EXPECT_EQ(decompSingleQubitLayers[0][0]->getType(), qc::RZ);
  EXPECT_THAT(decompSingleQubitLayers[0][0]->getTargets(),
              ::testing::ElementsAre(0));
  EXPECT_THAT(decompSingleQubitLayers[0][0]->getParameter(),
              ::testing::ElementsAre(::testing::DoubleNear(
                  qc::PI_2, NativeGateDecomposer::epsilon)));

  EXPECT_EQ(decompSingleQubitLayers[0][1]->getType(), qc::RZ);
  EXPECT_THAT(decompSingleQubitLayers[0][1]->getTargets(),
              ::testing::ElementsAre(1));
  EXPECT_THAT(decompSingleQubitLayers[0][1]->getParameter(),
              ::testing::ElementsAre(::testing::DoubleNear(
                  qc::PI_2, NativeGateDecomposer::epsilon)));

  EXPECT_TRUE(decompSingleQubitLayers[0][2]->isCompoundOperation());
  EXPECT_TRUE(decompSingleQubitLayers[0][2]->isGlobal(n));

  EXPECT_EQ(decompSingleQubitLayers[0][3]->getType(), qc::RZ);
  EXPECT_THAT(decompSingleQubitLayers[0][3]->getTargets(),
              ::testing::ElementsAre(0));
  EXPECT_THAT(decompSingleQubitLayers[0][3]->getParameter(),
              ::testing::ElementsAre(::testing::DoubleNear(
                  qc::PI, NativeGateDecomposer::epsilon)));

  EXPECT_EQ(decompSingleQubitLayers[0][4]->getType(), qc::RZ);
  EXPECT_THAT(decompSingleQubitLayers[0][4]->getTargets(),
              ::testing::ElementsAre(1));
  EXPECT_THAT(decompSingleQubitLayers[0][4]->getParameter(),
              ::testing::ElementsAre(
                  ::testing::DoubleNear(0, NativeGateDecomposer::epsilon)));

  EXPECT_TRUE(decompSingleQubitLayers[0][5]->isCompoundOperation());
  EXPECT_TRUE(decompSingleQubitLayers[0][5]->isGlobal(n));

  EXPECT_EQ(decompSingleQubitLayers[0][6]->getType(), qc::RZ);
  EXPECT_THAT(decompSingleQubitLayers[0][6]->getTargets(),
              ::testing::ElementsAre(0));
  EXPECT_THAT(decompSingleQubitLayers[0][6]->getParameter(),
              ::testing::ElementsAre(::testing::DoubleNear(
                  qc::PI_2, NativeGateDecomposer::epsilon)));

  EXPECT_EQ(decompSingleQubitLayers[0][7]->getType(), qc::RZ);
  EXPECT_THAT(decompSingleQubitLayers[0][7]->getTargets(),
              ::testing::ElementsAre(1));
  EXPECT_THAT(decompSingleQubitLayers[0][7]->getParameter(),
              ::testing::ElementsAre(::testing::DoubleNear(
                  qc::PI_2, NativeGateDecomposer::epsilon)));
}

TEST_F(DecomposerTest, TwoQubitsTwoLayers) {
  //       ┌───────┐       ┌───────┐
  // q_0: ─┤   X   ├───■───┤   Z   ├─
  //       └───────┘   │   └───────┘
  //                   │   ┌───────┐
  // q_1: ─────────────■───┤   X   ├─
  //                       └───────┘

  size_t n = 2;
  qc::QuantumComputation qc(n);
  qc.x(0);
  qc.cz(0, 1);
  qc.z(0);
  qc.x(1);
  const auto& [singleQubitLayers, twoQubitLayers] = scheduler.schedule(qc);
  const auto& decompSingleQubitLayers =
      decomposer.decompose(qc.getNqubits(), singleQubitLayers, twoQubitLayers)
          .singleQubitLayers;
  EXPECT_EQ(decompSingleQubitLayers.size(), 2);
  EXPECT_EQ(decompSingleQubitLayers[0].size(), 5);
  EXPECT_EQ(decompSingleQubitLayers[1].size(), 8);

  // Layer 1
  EXPECT_EQ(decompSingleQubitLayers[0][0]->getType(), qc::RZ);
  EXPECT_THAT(decompSingleQubitLayers[0][0]->getTargets(),
              ::testing::ElementsAre(0));
  EXPECT_THAT(decompSingleQubitLayers[0][0]->getParameter(),
              ::testing::ElementsAre(::testing::DoubleNear(
                  qc::PI_2, NativeGateDecomposer::epsilon)));

  EXPECT_TRUE(decompSingleQubitLayers[0][1]->isCompoundOperation());
  EXPECT_TRUE(decompSingleQubitLayers[0][1]->isGlobal(n));

  EXPECT_EQ(decompSingleQubitLayers[0][2]->getType(), qc::RZ);
  EXPECT_THAT(decompSingleQubitLayers[0][2]->getTargets(),
              ::testing::ElementsAre(0));
  EXPECT_THAT(decompSingleQubitLayers[0][2]->getParameter(),
              ::testing::ElementsAre(::testing::DoubleNear(
                  qc::PI, NativeGateDecomposer::epsilon)));

  EXPECT_TRUE(decompSingleQubitLayers[0][3]->isCompoundOperation());
  EXPECT_TRUE(decompSingleQubitLayers[0][3]->isGlobal(n));

  EXPECT_EQ(decompSingleQubitLayers[0][4]->getType(), qc::RZ);
  EXPECT_THAT(decompSingleQubitLayers[0][4]->getTargets(),
              ::testing::ElementsAre(0));
  EXPECT_THAT(decompSingleQubitLayers[0][4]->getParameter(),
              ::testing::ElementsAre(::testing::DoubleNear(
                  qc::PI_2, NativeGateDecomposer::epsilon)));

  // Layer 2

  EXPECT_EQ(decompSingleQubitLayers[1][0]->getType(), qc::RZ);
  EXPECT_THAT(decompSingleQubitLayers[1][0]->getTargets(),
              ::testing::ElementsAre(0));
  EXPECT_THAT(decompSingleQubitLayers[1][0]->getParameter(),
              ::testing::ElementsAre(::testing::DoubleNear(
                  qc::PI_2, NativeGateDecomposer::epsilon)));

  EXPECT_EQ(decompSingleQubitLayers[1][1]->getType(), qc::RZ);
  EXPECT_THAT(decompSingleQubitLayers[1][1]->getTargets(),
              ::testing::ElementsAre(1));
  EXPECT_THAT(decompSingleQubitLayers[1][1]->getParameter(),
              ::testing::ElementsAre(::testing::DoubleNear(
                  qc::PI_2, NativeGateDecomposer::epsilon)));

  EXPECT_TRUE(decompSingleQubitLayers[1][2]->isCompoundOperation());
  EXPECT_TRUE(decompSingleQubitLayers[1][2]->isGlobal(n));

  EXPECT_EQ(decompSingleQubitLayers[1][3]->getType(), qc::RZ);
  EXPECT_THAT(decompSingleQubitLayers[1][3]->getTargets(),
              ::testing::ElementsAre(0));
  EXPECT_THAT(decompSingleQubitLayers[1][3]->getParameter(),
              ::testing::ElementsAre(
                  ::testing::DoubleNear(0, NativeGateDecomposer::epsilon)));

  EXPECT_EQ(decompSingleQubitLayers[1][4]->getType(), qc::RZ);
  EXPECT_THAT(decompSingleQubitLayers[1][4]->getTargets(),
              ::testing::ElementsAre(1));
  EXPECT_THAT(decompSingleQubitLayers[1][4]->getParameter(),
              ::testing::ElementsAre(::testing::DoubleNear(
                  qc::PI, NativeGateDecomposer::epsilon)));

  EXPECT_TRUE(decompSingleQubitLayers[1][5]->isCompoundOperation());
  EXPECT_TRUE(decompSingleQubitLayers[1][5]->isGlobal(n));

  EXPECT_EQ(decompSingleQubitLayers[1][6]->getType(), qc::RZ);
  EXPECT_THAT(decompSingleQubitLayers[1][6]->getTargets(),
              ::testing::ElementsAre(0));
  EXPECT_THAT(decompSingleQubitLayers[1][6]->getParameter(),
              ::testing::ElementsAre(::testing::DoubleNear(
                  qc::PI_2, NativeGateDecomposer::epsilon)));

  EXPECT_EQ(decompSingleQubitLayers[1][7]->getType(), qc::RZ);
  EXPECT_THAT(decompSingleQubitLayers[1][7]->getTargets(),
              ::testing::ElementsAre(1));
  EXPECT_THAT(decompSingleQubitLayers[1][7]->getParameter(),
              ::testing::ElementsAre(::testing::DoubleNear(
                  qc::PI_2, NativeGateDecomposer::epsilon)));
}

} // namespace na::zoned
