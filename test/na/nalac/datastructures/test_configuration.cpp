/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "na/nalac/datastructures/Configuration.hpp"

#include <gtest/gtest.h>
#include <sstream>
#include <stdexcept>
#include <tuple>

TEST(Configuration, MethodOfString) {
  EXPECT_EQ(na::nalac::getMethodOfString("naive"),
            na::nalac::NAMappingMethod::Naive);
  EXPECT_EQ(na::nalac::getMethodOfString("maximize parallelism"),
            na::nalac::NAMappingMethod::MaximizeParallelismHeuristic);
  EXPECT_EQ(na::nalac::getMethodOfString("NaIvE"),
            na::nalac::NAMappingMethod::Naive);
  EXPECT_EQ(na::nalac::getMethodOfString("mAxImIzE pArAllElIsm"),
            na::nalac::NAMappingMethod::MaximizeParallelismHeuristic);
  EXPECT_THROW(std::ignore = na::nalac::getMethodOfString("unsupported"),
               std::invalid_argument);
}

TEST(Configuration, Import) {
  EXPECT_THROW(na::nalac::Configuration("nonexistent.json"),
               std::runtime_error);
  std::istringstream configIS(R"(
    {
      "patch": {
        "rows": 2,
        "cols": 3
      },
      "method": "maximize parallelism"
    }
  )");
  const na::nalac::Configuration config(configIS);
  EXPECT_EQ(config.getPatchRows(), 2);
  EXPECT_EQ(config.getPatchCols(), 3);
  EXPECT_EQ(config.getMethod(),
            na::nalac::NAMappingMethod::MaximizeParallelismHeuristic);
  std::istringstream invalidJson("{name: invalid}");
  EXPECT_THROW(const na::nalac::Configuration ignore(invalidJson),
               std::runtime_error);
}
