/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "ir/operations/StandardOperation.hpp"
#include "na/zoned/scheduler/MinFlowScheduler.hpp"

#include <gmock/gmock-matchers.h>
#include <gmock/gmock-more-matchers.h>
#include <gtest/gtest.h>
#include <utility>
#include <vector>

namespace testing {
// NOLINTNEXTLINE(modernize-use-trailing-return-type)
MATCHER_P(RefEq, value, "") { return arg.get() == value; }
} // namespace testing
namespace na::zoned {
constexpr std::string_view architectureJson = R"({
  "name": "min_flow_scheduler_architecture",
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
class MinFlowSchedulerScheduleTest : public ::testing::Test {
protected:
  Architecture architecture;
  MinFlowScheduler::Config config{.maxFillingFactor = .8};
  MinFlowScheduler scheduler;
  MinFlowSchedulerScheduleTest()
      : architecture(Architecture::fromJSONString(architectureJson)),
        scheduler(architecture, config) {}
};
TEST_F(MinFlowSchedulerScheduleTest, NoGate) {
  qc::QuantumComputation qc;
  const auto& [singleQubitGateLayers, twoQubitGateLayers] =
      scheduler.schedule(qc);
  EXPECT_THAT(singleQubitGateLayers, ::testing::IsEmpty());
  EXPECT_THAT(twoQubitGateLayers, ::testing::IsEmpty());
}
TEST_F(MinFlowSchedulerScheduleTest, SingleQubitGate) {
  //    ┌───────┐
  // q: ┤ Rz(π) ├
  //    └───────┘
  qc::QuantumComputation qc(1);
  qc.rz(qc::PI, 0);
  const auto& [singleQubitGateLayers, twoQubitGateLayers] =
      scheduler.schedule(qc);
  EXPECT_THAT(singleQubitGateLayers,
              ::testing::ElementsAre(::testing::ElementsAre(::testing::RefEq(
                  static_cast<qc::StandardOperation&>(*qc.at(0))))));
  EXPECT_THAT(twoQubitGateLayers, ::testing::IsEmpty());
}
TEST_F(MinFlowSchedulerScheduleTest, TwoQubitGate) {
  // q_0: ─■─
  //       │
  // q_1: ─■─
  qc::QuantumComputation qc(2);
  qc.cz(0, 1);
  const auto& [singleQubitGateLayers, twoQubitGateLayers] =
      scheduler.schedule(qc);
  EXPECT_THAT(
      singleQubitGateLayers,
      ::testing::ElementsAre(::testing::IsEmpty(), ::testing::IsEmpty()));
  EXPECT_THAT(twoQubitGateLayers,
              ::testing::UnorderedElementsAre(::testing::ElementsAre(
                  ::testing::UnorderedElementsAre(0U, 1U))));
}
TEST_F(MinFlowSchedulerScheduleTest, SingleQubitSandwich) {
  // q_0: ──────────■──────────
  //      ┌───────┐ │ ┌───────┐
  // q_1: ┤ Rz(π) ├─■─┤ Rz(π) ├
  //      └───────┘   └───────┘
  qc::QuantumComputation qc(2);
  qc.rz(qc::PI, 1);
  qc.cz(0, 1);
  qc.rz(qc::PI, 1);
  const auto& [singleQubitGateLayers, twoQubitGateLayers] =
      scheduler.schedule(qc);
  EXPECT_THAT(singleQubitGateLayers,
              ::testing::ElementsAre(
                  ::testing::ElementsAre(::testing::RefEq(
                      static_cast<qc::StandardOperation&>(*qc.at(0)))),
                  ::testing::ElementsAre(::testing::RefEq(
                      static_cast<qc::StandardOperation&>(*qc.at(2))))));
  EXPECT_THAT(twoQubitGateLayers,
              ::testing::ElementsAre(::testing::UnorderedElementsAre(
                  ::testing::UnorderedElementsAre(0U, 1U))));
}
TEST_F(MinFlowSchedulerScheduleTest, TwoQubitSequence) {
  // q_0: ─■───────
  //       │
  // q_1: ─■──■────
  //          │
  // q_2: ────■──■─
  //             │
  // q_3: ───────■─
  qc::QuantumComputation qc(4);
  qc.cz(0, 1);
  qc.cz(1, 2);
  qc.cz(2, 3);
  const auto& [singleQubitGateLayers, twoQubitGateLayers] =
      scheduler.schedule(qc);
  EXPECT_THAT(singleQubitGateLayers, ::testing::SizeIs(4));
  EXPECT_THAT(singleQubitGateLayers, ::testing::Each(::testing::IsEmpty()));
  EXPECT_THAT(
      twoQubitGateLayers,
      ::testing::ElementsAre(::testing::UnorderedElementsAre(
                                 ::testing::UnorderedElementsAre(0U, 1U)),
                             ::testing::UnorderedElementsAre(
                                 ::testing::UnorderedElementsAre(1U, 2U)),
                             ::testing::UnorderedElementsAre(
                                 ::testing::UnorderedElementsAre(2U, 3U))));
}
TEST_F(MinFlowSchedulerScheduleTest, Mixed) {
  //            INPUT ORDER                         SCHEDULED ORDER
  // q_0: ─■─────────────────────────  >>>  ─────────░─■─░─────────░───░─
  //       │ ┌───────┐                 >>>           ░ │ ░┌───────┐░   ░
  // q_1: ─■─┤ Rz(π) ├─────────────■─  >>>  ─────────░─■─░┤ Rz(π) ├░─■─░─
  //         └───────┘┌───────┐    │   >>>  ┌───────┐░   ░└───────┘░ │ ░
  // q_2: ────────────┤ Rz(π) ├─■──■─  >>>  ┤ Rz(π) ├░─■─░─────────░─■─░─
  //                  └───────┘ │      >>>  └───────┘░ │ ░         ░   ░
  // q_3: ──────────────────────■────  >>>  ─────────░─■─░─────────░───░─
  qc::QuantumComputation qc(4);
  qc.cz(0, 1);
  qc.rz(qc::PI, 1);
  qc.rz(qc::PI, 2);
  qc.cz(2, 3);
  qc.cz(1, 2);
  const auto& [singleQubitGateLayers, twoQubitGateLayers] =
      scheduler.schedule(qc);
  EXPECT_THAT(singleQubitGateLayers,
              ::testing::ElementsAre(
                  ::testing::ElementsAre(::testing::RefEq(
                      static_cast<qc::StandardOperation&>(*qc.at(2)))),
                  ::testing::ElementsAre(::testing::RefEq(
                      static_cast<qc::StandardOperation&>(*qc.at(1)))),
                  ::testing::IsEmpty()));
  EXPECT_THAT(
      twoQubitGateLayers,
      ::testing::ElementsAre(::testing::UnorderedElementsAre(
                                 ::testing::UnorderedElementsAre(0U, 1U),
                                 ::testing::UnorderedElementsAre(2U, 3U)),
                             ::testing::UnorderedElementsAre(
                                 ::testing::UnorderedElementsAre(1U, 2U))));
}
TEST_F(MinFlowSchedulerScheduleTest, Flow1) {
  //          INPUT ORDER                SCHEDULED ORDER
  // q_0: ──────────■────────  >>>  ───░───░───░─■────────
  //                │                  ░   ░   ░ │
  // q_1: ───────■──■────────  >>>  ───░───░─■─░─■────────
  //             │                     ░   ░ │ ░
  // q_2: ────■──■──■────────  >>>  ───░─■─░─■─░─■────────
  //          │.    │                  ░ │.░   ░ │
  // q_3: ─■──■─────│────────  >>>  ─■─░─■─░───░─│────────
  //       │        │                │ ░   ░   ░ │
  // q_4: ─■──■─────■────────  >>>  ─■─░───░─■─░─■────────
  //          │                        ░   ░ │ ░
  // q_5: ─■──■──────────────  >>>  ───░─■─░─■─░──────────
  //       │                           ░ │ ░   ░
  // q_6: ─■──■──────────────  >>>  ───░─■─░─■─░──────────
  //          │                        ░   ░ │ ░
  // q_7: ────■──────────────  >>>  ───░───░─■─░──────────
  qc::QuantumComputation qc(8);
  qc.cz(3, 4);
  qc.cz(5, 6);
  qc.cz(2, 3);
  qc.cz(4, 5);
  qc.cz(6, 7);
  qc.cz(1, 2);
  qc.cz(0, 1);
  qc.cz(2, 4);
  const auto& [singleQubitGateLayers, twoQubitGateLayers] =
      scheduler.schedule(qc);
  EXPECT_THAT(singleQubitGateLayers, ::testing::Each(::testing::IsEmpty()));
  EXPECT_THAT(
      twoQubitGateLayers,
      ::testing::ElementsAre(::testing::UnorderedElementsAre(
                                 ::testing::UnorderedElementsAre(3U, 4U)),
                             ::testing::UnorderedElementsAre(
                                 ::testing::UnorderedElementsAre(2U, 3U),
                                 ::testing::UnorderedElementsAre(5U, 6U)),
                             ::testing::UnorderedElementsAre(
                                 ::testing::UnorderedElementsAre(1U, 2U),
                                 ::testing::UnorderedElementsAre(6U, 7U),
                                 ::testing::UnorderedElementsAre(4U, 5U)),
                             ::testing::UnorderedElementsAre(
                                 ::testing::UnorderedElementsAre(0U, 1U),
                                 ::testing::UnorderedElementsAre(2U, 4U))));
}
TEST_F(MinFlowSchedulerScheduleTest, Flow2) {
  //          INPUT ORDER                SCHEDULED ORDER
  // q_0: ────────────────■──  >>>  ───░───░──░───░───░─■──
  //                      │            ░   ░  ░   ░   ░ │
  // q_1: ─────────────■──■──  >>>  ───░───░──░───░─■─░─■──
  //                   │               ░   ░  ░   ░ │ ░
  // q_2: ──────────■──■──■──  >>>  ───░───░──░─■─░─■─░─■──
  //                │     │            ░   ░  ░ │ ░   ░ │
  // q_3: ─■─────■──■─────│──  >>>  ─■─░───░─■░─■─░───░─│──
  //       │     │        │          │ ░   ░ │░   ░   ░ │
  // q_4: ─■──■──■────────│──  >>>  ─■─░─■─░─■░───░───░─│──
  //          │.          │            ░ │.░  ░   ░   ░ │
  // q_5: ─■──■──■────────│──  >>>  ─■─░─■─░──░─■─░───░─│──
  //       │     │        │          │ ░   ░  ░ │ ░   ░ │
  // q_6: ─■─────│────────│──  >>>  ─■─░───░──░─│─░───░─│──
  //             │        │            ░   ░  ░ │ ░   ░ │
  // q_7: ───────■─■──────│──  >>>  ───░───░──░─■─░─■─░─│──
  //               │      │            ░   ░  ░   ░ │ ░ │
  // q_8: ─────────■──────■──  >>>  ───░───░──░───░─■─░─■──

  qc::QuantumComputation qc(9);
  qc.cz(3, 4);
  qc.cz(5, 6);
  qc.cz(4, 5);
  qc.cz(3, 4);
  qc.cz(5, 7);
  qc.cz(2, 3);
  qc.cz(7, 8);
  qc.cz(1, 2);
  qc.cz(0, 1);
  qc.cz(2, 8);
  const auto& [singleQubitGateLayers, twoQubitGateLayers] =
      scheduler.schedule(qc);
  EXPECT_THAT(singleQubitGateLayers, ::testing::Each(::testing::IsEmpty()));
  EXPECT_THAT(
      twoQubitGateLayers,
      ::testing::ElementsAre(::testing::UnorderedElementsAre(
                                 ::testing::UnorderedElementsAre(3U, 4U),
                                 ::testing::UnorderedElementsAre(5U, 6U)),
                             ::testing::UnorderedElementsAre(
                                 ::testing::UnorderedElementsAre(4U, 5U)),
                             ::testing::UnorderedElementsAre(
                                 ::testing::UnorderedElementsAre(3U, 4U)),
                             ::testing::UnorderedElementsAre(
                                 ::testing::UnorderedElementsAre(2U, 3U),
                                 ::testing::UnorderedElementsAre(5U, 7U)),
                             ::testing::UnorderedElementsAre(
                                 ::testing::UnorderedElementsAre(1U, 2U),
                                 ::testing::UnorderedElementsAre(7U, 8U)),
                             ::testing::UnorderedElementsAre(
                                 ::testing::UnorderedElementsAre(0U, 1U),
                                 ::testing::UnorderedElementsAre(2U, 8U))));
}
// TEST_F(MinFlowSchedulerScheduleTest, Barrier) {
//   // q_0: ─■─────────░───
//   //       │┌───────┐░
//   // q_1: ─■┤ Rz(π) ├░───
//   //        └───────┘░
//   // q_2: ───────────░─■─
//   //                 ░ │
//   // q_3: ───────────░─■─
//   qc::QuantumComputation qc(4);
//   qc.cz(0, 1);
//   qc.rz(qc::PI, 1);
//   qc.barrier();
//   qc.cz(2, 3);
//   const auto& [singleQubitGateLayers, twoQubitGateLayers] =
//       scheduler.schedule(qc);
//   EXPECT_THAT(singleQubitGateLayers,
//               ::testing::ElementsAre(
//                   ::testing::IsEmpty(),
//                   ::testing::ElementsAre(::testing::RefEq(
//                       static_cast<qc::StandardOperation&>(*qc.at(1)))),
//                   ::testing::IsEmpty()));
//   EXPECT_THAT(
//       twoQubitGateLayers,
//       ::testing::ElementsAre(::testing::UnorderedElementsAre(
//                                  ::testing::UnorderedElementsAre(0U, 1U)),
//                              ::testing::UnorderedElementsAre(
//                                  ::testing::UnorderedElementsAre(2U, 3U))));
// }
TEST_F(MinFlowSchedulerScheduleTest, NonGlobalBarrier) {
  // q_0: ─░─
  //
  // q_1: ───
  qc::QuantumComputation qc(2);
  qc.emplace_back<qc::StandardOperation>(0, qc::Barrier);
  EXPECT_THROW(std::ignore = scheduler.schedule(qc), std::invalid_argument);
}
TEST_F(MinFlowSchedulerScheduleTest, NonGlobalCompound) {
  qc::QuantumComputation qc(2);
  qc::CompoundOperation compoundOp;
  compoundOp.emplace_back<qc::StandardOperation>(0, qc::RY,
                                                 std::vector{qc::PI_2});
  qc.emplace_back<qc::CompoundOperation>(compoundOp);
  EXPECT_THROW(std::ignore = scheduler.schedule(qc), std::invalid_argument);
}
TEST_F(MinFlowSchedulerScheduleTest, UnsupportedCXGate) {
  qc::QuantumComputation qc(2);
  qc.cx(0, 1);
  EXPECT_THROW(std::ignore = scheduler.schedule(qc), std::invalid_argument);
}
TEST_F(MinFlowSchedulerScheduleTest, FullEntanglementZone) {
  qc::QuantumComputation qc(26);
  for (qc::Qubit i = 0; i < 13; ++i) {
    qc.cz(2 * i, 2 * i + 1);
  }
  const auto& [singleQubitGateLayers, twoQubitGateLayers] =
      scheduler.schedule(qc);
  EXPECT_THAT(singleQubitGateLayers, ::testing::SizeIs(3));
  EXPECT_THAT(twoQubitGateLayers, ::testing::SizeIs(2));
}
TEST(MinFlowSchedulerConfigTest, InvalidMaxFillingFactor) {
  const auto architecture = Architecture::fromJSONString(architectureJson);
  constexpr MinFlowScheduler::Config config1{.maxFillingFactor = -0.1};
  EXPECT_THROW(MinFlowScheduler scheduler(architecture, config1),
               std::invalid_argument);
  constexpr MinFlowScheduler::Config config2{.maxFillingFactor = 1.1};
  EXPECT_THROW(MinFlowScheduler scheduler(architecture, config2),
               std::invalid_argument);
}
} // namespace na::zoned
