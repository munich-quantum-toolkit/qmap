/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "hybridmap/NeutralAtomScheduler.hpp"

#include <gtest/gtest.h>

TEST(NeutralAtomSchedulerTests, SchedulerResultsToMapForPython) {
  // Given some arbitrary scheduler result values
  na::SchedulerResults const res(/*executionTime*/ 10.5,
                                 /*idleTime*/ 2.0,
                                 /*gateFidelities*/ 0.9,
                                 /*fidelities*/ 0.85,
                                 /*nCZs*/ 3,
                                 /*nAodActivate*/ 4,
                                 /*nAodMove*/ 5);

  const auto m = res.toMap();

  // Only the documented keys are exported
  ASSERT_EQ(m.size(), 7U);
  EXPECT_TRUE(m.count("totalExecutionTime"));
  EXPECT_TRUE(m.count("totalIdleTime"));
  EXPECT_TRUE(m.count("totalGateFidelities"));
  EXPECT_TRUE(m.count("totalFidelities"));
  EXPECT_TRUE(m.count("nCZs"));
  EXPECT_TRUE(m.count("nAodActivate"));
  EXPECT_TRUE(m.count("nAodMove"));

  // Values preserved exactly
  EXPECT_DOUBLE_EQ(m.at("totalExecutionTime"), 10.5);
  EXPECT_DOUBLE_EQ(m.at("totalIdleTime"), 2.0);
  EXPECT_DOUBLE_EQ(m.at("totalGateFidelities"), 0.9);
  EXPECT_DOUBLE_EQ(m.at("totalFidelities"), 0.85);
  EXPECT_DOUBLE_EQ(m.at("nCZs"), 3.0);
  EXPECT_DOUBLE_EQ(m.at("nAodActivate"), 4.0);
  EXPECT_DOUBLE_EQ(m.at("nAodMove"), 5.0);
}
