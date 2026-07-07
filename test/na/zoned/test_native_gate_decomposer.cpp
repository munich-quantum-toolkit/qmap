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
#include "na/zoned/decomposer/NativeGateDecomposer.hpp"
#include "na/zoned/matcher.hpp"
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

class NativeGateDecomposerTest : public ::testing::Test {
protected:
  Architecture architecture;
  ASAPScheduler::Config schedulerConfig{.maxFillingFactor = .8};
  ASAPScheduler scheduler;
  NativeGateDecomposer::Config decomposerConfig{};
  NativeGateDecomposer decomposer;
  NativeGateDecomposerTest()
      : architecture(Architecture::fromJSONString(architectureJson)),
        scheduler(architecture, schedulerConfig),
        decomposer(architecture, decomposerConfig) {}
};

TEST_F(NativeGateDecomposerTest, TranslationZ) {
  const qc::StandardOperation op(0, qc::Z);
  EXPECT_THAT(
      NativeGateDecomposer::convertGateToQuaternion(op),
      ::testing::QuaternionNear(NativeGateDecomposer::Quaternion{0, 0, 0, 1},
                                NativeGateDecomposer::epsilon));
}

TEST_F(NativeGateDecomposerTest, TranslationRZ) {
  const qc::StandardOperation op(0, qc::RZ, {qc::PI_2});
  EXPECT_THAT(NativeGateDecomposer::convertGateToQuaternion(op),
              ::testing::QuaternionNear(
                  NativeGateDecomposer::Quaternion{1 / std::sqrt(2), 0, 0,
                                                   1 / std::sqrt(2)},
                  NativeGateDecomposer::epsilon));
}

TEST_F(NativeGateDecomposerTest, TranslationP) {
  const qc::StandardOperation op(0, qc::P, {qc::PI_2});
  EXPECT_THAT(NativeGateDecomposer::convertGateToQuaternion(op),
              ::testing::QuaternionNear(
                  NativeGateDecomposer::Quaternion{1 / std::sqrt(2), 0, 0,
                                                   1 / std::sqrt(2)},
                  NativeGateDecomposer::epsilon));
}

TEST_F(NativeGateDecomposerTest, TranslationS) {
  const qc::StandardOperation op(0, qc::S);
  EXPECT_THAT(NativeGateDecomposer::convertGateToQuaternion(op),
              ::testing::QuaternionNear(
                  NativeGateDecomposer::Quaternion{1 / std::sqrt(2), 0, 0,
                                                   1 / std::sqrt(2)},
                  NativeGateDecomposer::epsilon));
}

TEST_F(NativeGateDecomposerTest, TranslationSdg) {
  const qc::StandardOperation op(0, qc::Sdg);
  EXPECT_THAT(NativeGateDecomposer::convertGateToQuaternion(op),
              ::testing::QuaternionNear(
                  NativeGateDecomposer::Quaternion{1 / std::sqrt(2), 0, 0,
                                                   -1 / std::sqrt(2)},
                  NativeGateDecomposer::epsilon));
}

TEST_F(NativeGateDecomposerTest, TranslationT) {
  const qc::StandardOperation op(0, qc::T);
  EXPECT_THAT(
      NativeGateDecomposer::convertGateToQuaternion(op),
      ::testing::QuaternionNear(
          NativeGateDecomposer::Quaternion{std::sqrt(2 + std::sqrt(2)) / 2, 0,
                                           0, std::sqrt(2 - std::sqrt(2)) / 2},
          NativeGateDecomposer::epsilon));
}

TEST_F(NativeGateDecomposerTest, TranslationTdg) {
  const qc::StandardOperation op(0, qc::Tdg);
  EXPECT_THAT(
      NativeGateDecomposer::convertGateToQuaternion(op),
      ::testing::QuaternionNear(
          NativeGateDecomposer::Quaternion{std::sqrt(2 + std::sqrt(2)) / 2, 0,
                                           0, -std::sqrt(2 - std::sqrt(2)) / 2},
          NativeGateDecomposer::epsilon));
}

TEST_F(NativeGateDecomposerTest, TranslationX) {
  const qc::StandardOperation op(0, qc::X);
  EXPECT_THAT(
      NativeGateDecomposer::convertGateToQuaternion(op),
      ::testing::QuaternionNear(NativeGateDecomposer::Quaternion{0, 1, 0, 0},
                                NativeGateDecomposer::epsilon));
}

TEST_F(NativeGateDecomposerTest, TranslationRX) {
  const qc::StandardOperation op(0, qc::RX, {qc::PI_2});
  EXPECT_THAT(NativeGateDecomposer::convertGateToQuaternion(op),
              ::testing::QuaternionNear(
                  NativeGateDecomposer::Quaternion{1 / std::sqrt(2),
                                                   1 / std::sqrt(2), 0, 0},
                  NativeGateDecomposer::epsilon));
}

TEST_F(NativeGateDecomposerTest, TranslationY) {
  const qc::StandardOperation op(0, qc::Y);
  EXPECT_THAT(
      NativeGateDecomposer::convertGateToQuaternion(op),
      ::testing::QuaternionNear(NativeGateDecomposer::Quaternion{0, 0, 1, 0},
                                NativeGateDecomposer::epsilon));
}

TEST_F(NativeGateDecomposerTest, TranslationRY) {
  const qc::StandardOperation op(0, qc::RY, {qc::PI_2});
  EXPECT_THAT(NativeGateDecomposer::convertGateToQuaternion(op),
              ::testing::QuaternionNear(
                  NativeGateDecomposer::Quaternion{1 / std::sqrt(2), 0,
                                                   1 / std::sqrt(2), 0},
                  NativeGateDecomposer::epsilon));
}

TEST_F(NativeGateDecomposerTest, TranslationSX) {
  const qc::StandardOperation op(0, qc::SX);
  EXPECT_THAT(NativeGateDecomposer::convertGateToQuaternion(op),
              ::testing::QuaternionNear(
                  NativeGateDecomposer::Quaternion{1 / std::sqrt(2),
                                                   1 / std::sqrt(2), 0, 0},
                  NativeGateDecomposer::epsilon));
}

TEST_F(NativeGateDecomposerTest, TranslationSXdg) {
  const qc::StandardOperation op(0, qc::SXdg);
  EXPECT_THAT(NativeGateDecomposer::convertGateToQuaternion(op),
              ::testing::QuaternionNear(
                  NativeGateDecomposer::Quaternion{1 / std::sqrt(2),
                                                   -1 / std::sqrt(2), 0, 0},
                  NativeGateDecomposer::epsilon));
}

TEST_F(NativeGateDecomposerTest, TranslationU) {
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

TEST_F(NativeGateDecomposerTest, CombineQuaternion1) {
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

TEST_F(NativeGateDecomposerTest, CombineQuaternion2) {
  const NativeGateDecomposer::Quaternion q1{cos(qc::PI_2), 0, 0, sin(qc::PI_2)};
  const NativeGateDecomposer::Quaternion q2{cos(qc::PI_4 / 2), 0,
                                            sin(qc::PI_4 / 2), 0};
  const auto& q12 = NativeGateDecomposer::combineQuaternions(q1, q2);
  EXPECT_THAT(q12, ::testing::QuaternionNear(
                       NativeGateDecomposer::Quaternion{0, -sin(qc::PI_4 / 2),
                                                        0, cos(qc::PI_4 / 2)},
                       NativeGateDecomposer::epsilon));
  const NativeGateDecomposer::Quaternion q3{cos(qc::PI_4), 0, 0, sin(qc::PI_4)};
  const auto& q123 = NativeGateDecomposer::combineQuaternions(q12, q3);
  const auto r = 1 / std::sqrt(2);
  EXPECT_THAT(q123, ::testing::QuaternionNear(
                        NativeGateDecomposer::Quaternion{
                            -r * cos(qc::PI_4 / 2), -r * sin(qc::PI_4 / 2),
                            r * sin(qc::PI_4 / 2), r * cos(qc::PI_4 / 2)},
                        NativeGateDecomposer::epsilon));
}

TEST_F(NativeGateDecomposerTest, SingleXGateAngle) {
  const qc::StandardOperation op(0, qc::X);
  const auto& q = NativeGateDecomposer::convertGateToQuaternion(op);
  EXPECT_THAT(
      NativeGateDecomposer::getU3AnglesFromQuaternion(q),
      ::testing::AnglesNear(NativeGateDecomposer::Angles{qc::PI, 0, qc::PI},
                            NativeGateDecomposer::epsilon));
}

TEST_F(NativeGateDecomposerTest, SingleU3GateAngle) {
  const qc::StandardOperation op(0, qc::U, {qc::PI_4, qc::PI, qc::PI_2});
  const auto& q = NativeGateDecomposer::convertGateToQuaternion(op);
  const auto r = 1 / sqrt(2);
  EXPECT_THAT(q, ::testing::QuaternionNear(
                     NativeGateDecomposer::Quaternion{
                         -r * cos(qc::PI_4 / 2), -r * sin(qc::PI_4 / 2),
                         r * sin(qc::PI_4 / 2), r * cos(qc::PI_4 / 2)},
                     NativeGateDecomposer::epsilon));

  EXPECT_THAT(NativeGateDecomposer::getU3AnglesFromQuaternion(q),
              ::testing::AnglesNear(
                  NativeGateDecomposer::Angles{qc::PI_4, qc::PI, qc::PI_2},
                  NativeGateDecomposer::epsilon));
}

TEST_F(NativeGateDecomposerTest, ThetaPiAngle) {
  qc::StandardOperation op(0, qc::U, {qc::PI, qc::PI, qc::PI_2});
  const auto& q = NativeGateDecomposer::convertGateToQuaternion(op);
  const auto r = 1 / sqrt(2);
  EXPECT_THAT(q, ::testing::QuaternionNear(
                     NativeGateDecomposer::Quaternion{0, -r, r, 0},
                     NativeGateDecomposer::epsilon));
  EXPECT_THAT(
      NativeGateDecomposer::getU3AnglesFromQuaternion(q),
      ::testing::AnglesNear(NativeGateDecomposer::Angles{qc::PI, 0, -qc::PI_2},
                            NativeGateDecomposer::epsilon));
}

TEST_F(NativeGateDecomposerTest, ThetaZeroAngle) {
  const qc::StandardOperation op(0, qc::U, {0, qc::PI, qc::PI_2});
  const auto& q = NativeGateDecomposer::convertGateToQuaternion(op);
  const auto r = 1 / sqrt(2);
  EXPECT_THAT(q, ::testing::QuaternionNear(
                     NativeGateDecomposer::Quaternion{-r, 0, 0, r},
                     NativeGateDecomposer::epsilon));

  EXPECT_THAT(
      NativeGateDecomposer::getU3AnglesFromQuaternion(q),
      ::testing::AnglesNear(NativeGateDecomposer::Angles{0, 0, 3 * qc::PI_2},
                            NativeGateDecomposer::epsilon));
}

TEST_F(NativeGateDecomposerTest, DecompositionU3) {
  constexpr NativeGateDecomposer::Angles u3{qc::PI_4, qc::PI, qc::PI_2};
  EXPECT_THAT(NativeGateDecomposer::getDecompositionAngles(u3, qc::PI_4),
              ::testing::AnglesNear(
                  NativeGateDecomposer::Angles{qc::PI, qc::PI, -qc::PI_2},
                  NativeGateDecomposer::epsilon));
}

TEST_F(NativeGateDecomposerTest, DecompositionX) {
  constexpr NativeGateDecomposer::Angles x{qc::PI, -qc::PI_2, qc::PI_2};
  EXPECT_THAT(NativeGateDecomposer::getDecompositionAngles(x, qc::PI),
              ::testing::AnglesNear(NativeGateDecomposer::Angles{qc::PI, 0, 0},
                                    NativeGateDecomposer::epsilon));
}

TEST_F(NativeGateDecomposerTest, DecompositionZ) {
  constexpr NativeGateDecomposer::Angles z{0, 0, qc::PI};
  EXPECT_THAT(
      NativeGateDecomposer::getDecompositionAngles(z, qc::PI),
      ::testing::AnglesNear(NativeGateDecomposer::Angles{0, qc::PI_2, qc::PI_2},
                            NativeGateDecomposer::epsilon));
}

TEST_F(NativeGateDecomposerTest, OneRXOneQubit) {
  //    ┌───────┐
  // q: ┤ Rx(π) ├
  //    └───────┘
  qc::QuantumComputation qc(1);
  qc.rx(qc::PI, 0);
  const auto& [singleQubitLayers, twoQubitLayers] = scheduler.schedule(qc);
  const auto& decompSingleQubitLayers =
      decomposer.decompose(qc.getNqubits(), singleQubitLayers, twoQubitLayers)
          .singleQubitLayers;
  EXPECT_EQ(decompSingleQubitLayers.size(), 1);
  EXPECT_EQ(decompSingleQubitLayers[0].size(), 5);
  EXPECT_THAT(decompSingleQubitLayers[0][0],
              ::testing::ExpectRotationGate(qc::RZ, 0, qc::PI_2,
                                            NativeGateDecomposer::epsilon));
  EXPECT_TRUE(decompSingleQubitLayers[0][1]->isCompoundOperation());
  EXPECT_TRUE(decompSingleQubitLayers[0][1]->isGlobal(1));
  EXPECT_THAT(decompSingleQubitLayers[0][2],
              ::testing::ExpectRotationGate(qc::RZ, 0, qc::PI,
                                            NativeGateDecomposer::epsilon));
  EXPECT_TRUE(decompSingleQubitLayers[0][3]->isCompoundOperation());
  EXPECT_TRUE(decompSingleQubitLayers[0][3]->isGlobal(1));
  EXPECT_THAT(decompSingleQubitLayers[0][4],
              ::testing::ExpectRotationGate(qc::RZ, 0, qc::PI_2,
                                            NativeGateDecomposer::epsilon));
}

TEST_F(NativeGateDecomposerTest, OneU3OneQubit) {
  //    ┌─────────────┐
  // q: ┤ U3(0,π,π/2) ├
  //    └─────────────┘
  qc::QuantumComputation qc(1);
  qc.u(0.0, qc::PI, qc::PI_2, 0);
  const auto& [singleQubitLayers, twoQubitLayers] = scheduler.schedule(qc);
  const auto& decompSingleQubitLayers =
      decomposer.decompose(qc.getNqubits(), singleQubitLayers, twoQubitLayers)
          .singleQubitLayers;
  EXPECT_EQ(decompSingleQubitLayers.size(), 1);
  EXPECT_EQ(decompSingleQubitLayers[0].size(), 5);

  EXPECT_THAT(decompSingleQubitLayers[0][0],
              ::testing::ExpectRotationGate(qc::RZ, 0, 0,
                                            NativeGateDecomposer::epsilon));
  EXPECT_TRUE(decompSingleQubitLayers[0][1]->isCompoundOperation());
  EXPECT_TRUE(decompSingleQubitLayers[0][1]->isGlobal(1));
  EXPECT_THAT(decompSingleQubitLayers[0][2],
              ::testing::ExpectRotationGate(qc::RZ, 0, qc::PI,
                                            NativeGateDecomposer::epsilon));
  EXPECT_TRUE(decompSingleQubitLayers[0][3]->isCompoundOperation());
  EXPECT_TRUE(decompSingleQubitLayers[0][3]->isGlobal(1));
  EXPECT_THAT(decompSingleQubitLayers[0][4],
              ::testing::ExpectRotationGate(qc::RZ, 0, qc::PI_2,
                                            NativeGateDecomposer::epsilon));
}

TEST_F(NativeGateDecomposerTest, TwoGatesOneQubit) {
  //    ┌───────┐  ┌───────┐
  // q: ┤   X   ├──┤   Z   ├
  //    └───────┘  └───────┘
  qc::QuantumComputation qc(1);
  qc.x(0);
  qc.z(0);
  const auto& [singleQubitLayers, twoQubitLayers] = scheduler.schedule(qc);
  const auto& decompSingleQubitLayers =
      decomposer.decompose(qc.getNqubits(), singleQubitLayers, twoQubitLayers)
          .singleQubitLayers;

  EXPECT_EQ(decompSingleQubitLayers.size(), 1);
  EXPECT_EQ(decompSingleQubitLayers[0].size(), 5);
  EXPECT_THAT(decompSingleQubitLayers[0][0],
              ::testing::ExpectRotationGate(qc::RZ, 0, qc::PI_2,
                                            NativeGateDecomposer::epsilon));
  EXPECT_TRUE(decompSingleQubitLayers[0][1]->isCompoundOperation());
  EXPECT_TRUE(decompSingleQubitLayers[0][1]->isGlobal(1));
  EXPECT_THAT(decompSingleQubitLayers[0][2],
              ::testing::ExpectRotationGate(qc::RZ, 0, qc::PI,
                                            NativeGateDecomposer::epsilon));
  EXPECT_TRUE(decompSingleQubitLayers[0][3]->isCompoundOperation());
  EXPECT_TRUE(decompSingleQubitLayers[0][3]->isGlobal(1));
  EXPECT_THAT(decompSingleQubitLayers[0][4],
              ::testing::ExpectRotationGate(qc::RZ, 0, 3 * qc::PI_2,
                                            NativeGateDecomposer::epsilon));
}

TEST_F(NativeGateDecomposerTest, TwoGatesTwoQubits) {
  //       ┌───────┐
  // q_0: ─┤   X   ├─
  //       └───────┘
  //       ┌───────┐
  // q_1: ─┤   Z   ├─
  //       └───────┘
  qc::QuantumComputation qc(2);
  qc.x(0);
  qc.z(1);
  const auto& [singleQubitLayers, twoQubitLayers] = scheduler.schedule(qc);
  const auto& decompSingleQubitLayers =
      decomposer.decompose(qc.getNqubits(), singleQubitLayers, twoQubitLayers)
          .singleQubitLayers;
  EXPECT_EQ(decompSingleQubitLayers.size(), 1);
  EXPECT_EQ(decompSingleQubitLayers[0].size(), 8);

  EXPECT_THAT(decompSingleQubitLayers[0][0],
              ::testing::ExpectRotationGate(qc::RZ, 0, qc::PI_2,
                                            NativeGateDecomposer::epsilon));

  EXPECT_THAT(decompSingleQubitLayers[0][1],
              ::testing::ExpectRotationGate(qc::RZ, 1, qc::PI_2,
                                            NativeGateDecomposer::epsilon));

  EXPECT_TRUE(decompSingleQubitLayers[0][2]->isCompoundOperation());
  EXPECT_TRUE(decompSingleQubitLayers[0][2]->isGlobal(2));

  EXPECT_THAT(decompSingleQubitLayers[0][3],
              ::testing::ExpectRotationGate(qc::RZ, 0, qc::PI,
                                            NativeGateDecomposer::epsilon));

  EXPECT_THAT(decompSingleQubitLayers[0][4],
              ::testing::ExpectRotationGate(qc::RZ, 1, 0,
                                            NativeGateDecomposer::epsilon));

  EXPECT_TRUE(decompSingleQubitLayers[0][5]->isCompoundOperation());
  EXPECT_TRUE(decompSingleQubitLayers[0][5]->isGlobal(2));

  EXPECT_THAT(decompSingleQubitLayers[0][6],
              ::testing::ExpectRotationGate(qc::RZ, 0, qc::PI_2,
                                            NativeGateDecomposer::epsilon));

  EXPECT_THAT(decompSingleQubitLayers[0][7],
              ::testing::ExpectRotationGate(qc::RZ, 1, qc::PI_2,
                                            NativeGateDecomposer::epsilon));
}

TEST_F(NativeGateDecomposerTest, TwoQubitsTwoLayers) {
  //       ┌───────┐       ┌───────┐
  // q_0: ─┤   X   ├───■───┤   Z   ├─
  //       └───────┘   │   └───────┘
  //                   │   ┌───────┐
  // q_1: ─────────────■───┤   X   ├─
  //                       └───────┘
  qc::QuantumComputation qc(2);
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
  EXPECT_THAT(decompSingleQubitLayers[0][0],
              ::testing::ExpectRotationGate(qc::RZ, 0, qc::PI_2,
                                            NativeGateDecomposer::epsilon));

  EXPECT_TRUE(decompSingleQubitLayers[0][1]->isCompoundOperation());
  EXPECT_TRUE(decompSingleQubitLayers[0][1]->isGlobal(2));

  EXPECT_THAT(decompSingleQubitLayers[0][2],
              ::testing::ExpectRotationGate(qc::RZ, 0, qc::PI,
                                            NativeGateDecomposer::epsilon));

  EXPECT_TRUE(decompSingleQubitLayers[0][3]->isCompoundOperation());
  EXPECT_TRUE(decompSingleQubitLayers[0][3]->isGlobal(2));

  EXPECT_THAT(decompSingleQubitLayers[0][4],
              ::testing::ExpectRotationGate(qc::RZ, 0, qc::PI_2,
                                            NativeGateDecomposer::epsilon));

  // Layer 2
  EXPECT_THAT(decompSingleQubitLayers[1][0],
              ::testing::ExpectRotationGate(qc::RZ, 0, qc::PI_2,
                                            NativeGateDecomposer::epsilon));

  EXPECT_THAT(decompSingleQubitLayers[1][1],
              ::testing::ExpectRotationGate(qc::RZ, 1, qc::PI_2,
                                            NativeGateDecomposer::epsilon));

  EXPECT_TRUE(decompSingleQubitLayers[1][2]->isCompoundOperation());
  EXPECT_TRUE(decompSingleQubitLayers[1][2]->isGlobal(2));

  EXPECT_THAT(decompSingleQubitLayers[1][3],
              ::testing::ExpectRotationGate(qc::RZ, 0, 0,
                                            NativeGateDecomposer::epsilon));

  EXPECT_THAT(decompSingleQubitLayers[1][4],
              ::testing::ExpectRotationGate(qc::RZ, 1, qc::PI,
                                            NativeGateDecomposer::epsilon));

  EXPECT_TRUE(decompSingleQubitLayers[1][5]->isCompoundOperation());
  EXPECT_TRUE(decompSingleQubitLayers[1][5]->isGlobal(2));

  EXPECT_THAT(decompSingleQubitLayers[1][6],
              ::testing::ExpectRotationGate(qc::RZ, 0, qc::PI_2,
                                            NativeGateDecomposer::epsilon));

  EXPECT_THAT(decompSingleQubitLayers[1][7],
              ::testing::ExpectRotationGate(qc::RZ, 1, qc::PI_2,
                                            NativeGateDecomposer::epsilon));
}

} // namespace na::zoned
