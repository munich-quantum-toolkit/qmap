/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "ir/Definitions.hpp"
#include "ir/operations/OpType.hpp"
#include "na/nalac/datastructures/NAComputation.hpp"
#include "na/nalac/datastructures/NADefinitions.hpp"
#include "na/nalac/datastructures/operations/NAGlobalOperation.hpp"
#include "na/nalac/datastructures/operations/NALocalOperation.hpp"
#include "na/nalac/datastructures/operations/NAShuttlingOperation.hpp"

#include <gtest/gtest.h>
#include <memory>
#include <sstream>
#include <vector>

namespace na::nalac {
TEST(NAComputation, General) {
  auto qc = NAComputation();
  qc.emplaceInitialPosition(std::make_shared<Point>(0, 0));
  qc.emplaceInitialPosition(std::make_shared<Point>(1, 0));
  qc.emplaceInitialPosition(std::make_shared<Point>(2, 0));
  qc.emplaceBack(std::make_unique<NALocalOperation>(
      qc::RZ, 0, std::vector{qc::PI_2}, std::make_shared<Point>(0, 0)));
  qc.emplaceBack(
      std::make_unique<NAGlobalOperation>(qc::RY, 0, std::vector{qc::PI_2}));
  qc.emplaceBack(std::make_unique<NAShuttlingOperation>(
      LOAD,
      std::vector{std::make_shared<Point>(0, 0), std::make_shared<Point>(1, 0)},
      std::vector{std::make_shared<Point>(0, 1),
                  std::make_shared<Point>(1, 1)}));
  qc.emplaceBack(std::make_unique<NAShuttlingOperation>(
      MOVE,
      std::vector{std::make_shared<Point>(0, 1), std::make_shared<Point>(1, 1)},
      std::vector{std::make_shared<Point>(4, 1),
                  std::make_shared<Point>(5, 1)}));
  qc.emplaceBack(std::make_unique<NAShuttlingOperation>(
      STORE,
      std::vector{std::make_shared<Point>(4, 1), std::make_shared<Point>(5, 1)},
      std::vector{std::make_shared<Point>(4, 0),
                  std::make_shared<Point>(5, 0)}));
  std::stringstream ss;
  ss << qc;
  EXPECT_EQ(ss.str(), "init at (0, 0), (1, 0), (2, 0);\n"
                      "rz(1.5708) at (0, 0);\n"
                      "ry(1.5708);\n"
                      "load (0, 0), (1, 0) to (0, 1), (1, 1);\n"
                      "move (0, 1), (1, 1) to (4, 1), (5, 1);\n"
                      "store (4, 1), (5, 1) to (4, 0), (5, 0);\n");
}

TEST(NAComputation, EmptyPrint) {
  const NAComputation qc;
  std::stringstream ss;
  ss << qc;
  EXPECT_EQ(ss.str(), "init at;\n");
}

TEST(NAComputation, ValidateAODConstraints) {
  auto qc = NAComputation();
  qc.emplaceInitialPosition(std::make_shared<Point>(0, 0));
  qc.emplaceInitialPosition(std::make_shared<Point>(1, 0));
  qc.emplaceInitialPosition(std::make_shared<Point>(0, 2));
  qc.emplaceInitialPosition(std::make_shared<Point>(1, 2));
  qc.emplaceBack(std::make_unique<NAShuttlingOperation>(
      LOAD,
      std::vector{std::make_shared<Point>(0, 0), std::make_shared<Point>(1, 0)},
      std::vector{std::make_shared<Point>(0, 1),
                  std::make_shared<Point>(1, 1)}));
  EXPECT_TRUE(qc.validateAODConstraints());
  qc.clear(false);
  qc.emplaceBack(std::make_unique<NAShuttlingOperation>(
      LOAD,
      std::vector{std::make_shared<Point>(0, 0), std::make_shared<Point>(0, 0)},
      std::vector{std::make_shared<Point>(0, 1),
                  std::make_shared<Point>(1, 0)}));
  EXPECT_FALSE(qc.validateAODConstraints());
  qc.clear(false);
  qc.emplaceBack(std::make_unique<NAShuttlingOperation>(
      LOAD,
      std::vector{std::make_shared<Point>(0, 0), std::make_shared<Point>(1, 0)},
      std::vector{std::make_shared<Point>(0, 1),
                  std::make_shared<Point>(0, 1)}));
  EXPECT_FALSE(qc.validateAODConstraints());
  qc.clear(false);
  qc.emplaceBack(std::make_unique<NAShuttlingOperation>(
      LOAD,
      std::vector{std::make_shared<Point>(0, 0), std::make_shared<Point>(1, 0)},
      std::vector{std::make_shared<Point>(0, 1),
                  std::make_shared<Point>(1, 0)}));
  EXPECT_FALSE(qc.validateAODConstraints());
  qc.clear(false);
  qc.emplaceBack(std::make_unique<NAShuttlingOperation>(
      LOAD,
      std::vector{std::make_shared<Point>(0, 0), std::make_shared<Point>(1, 0)},
      std::vector{std::make_shared<Point>(1, 1),
                  std::make_shared<Point>(0, 1)}));
  EXPECT_FALSE(qc.validateAODConstraints());
  qc.clear(false);
  qc.emplaceBack(std::make_unique<NAShuttlingOperation>(
      LOAD,
      std::vector{std::make_shared<Point>(1, 0), std::make_shared<Point>(0, 0)},
      std::vector{std::make_shared<Point>(0, 1),
                  std::make_shared<Point>(1, 1)}));
  EXPECT_FALSE(qc.validateAODConstraints());
  qc.clear(false);
  qc.emplaceBack(std::make_unique<NAShuttlingOperation>(
      LOAD,
      std::vector{std::make_shared<Point>(0, 0), std::make_shared<Point>(0, 2)},
      std::vector{std::make_shared<Point>(1, 0),
                  std::make_shared<Point>(0, 1)}));
  EXPECT_FALSE(qc.validateAODConstraints());
  qc.clear(false);
  qc.emplaceBack(std::make_unique<NAShuttlingOperation>(
      LOAD,
      std::vector{std::make_shared<Point>(0, 0), std::make_shared<Point>(1, 2)},
      std::vector{std::make_shared<Point>(0, 2),
                  std::make_shared<Point>(1, 0)}));
  EXPECT_FALSE(qc.validateAODConstraints());
  qc.clear(false);
  qc.emplaceBack(std::make_unique<NAShuttlingOperation>(
      LOAD,
      std::vector{std::make_shared<Point>(1, 2), std::make_shared<Point>(0, 0)},
      std::vector{std::make_shared<Point>(1, 0),
                  std::make_shared<Point>(0, 2)}));
  EXPECT_FALSE(qc.validateAODConstraints());
  qc.clear(false);
  qc.emplaceBack(std::make_unique<NALocalOperation>(
      qc::RZ, 0, std::vector{qc::PI_2},
      std::vector{std::make_shared<Point>(0, 0),
                  std::make_shared<Point>(0, 0)}));
  EXPECT_FALSE(qc.validateAODConstraints());
}
} // namespace na::nalac
