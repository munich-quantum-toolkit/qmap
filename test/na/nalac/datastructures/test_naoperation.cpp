/*
 * Copyright (c) 2025 Chair for Design Automation, TUM
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "ir/Definitions.hpp"
#include "ir/operations/OpType.hpp"
#include "na/nalac/datastructures/NADefinitions.hpp"
#include "na/nalac/datastructures/operations/NAGlobalOperation.hpp"
#include "na/nalac/datastructures/operations/NALocalOperation.hpp"
#include "na/nalac/datastructures/operations/NAShuttlingOperation.hpp"

#include <gtest/gtest.h>
#include <memory>
#include <sstream>
#include <vector>

namespace na::nalac {
TEST(NAOperation, ShuttlingOperation) {
  const NAShuttlingOperation shuttling(
      LOAD,
      std::vector{std::make_shared<Point>(0, 0), std::make_shared<Point>(1, 0)},
      std::vector{std::make_shared<Point>(0, 1),
                  std::make_shared<Point>(1, 1)});
  EXPECT_TRUE(shuttling.isShuttlingOperation());
  EXPECT_FALSE(shuttling.isLocalOperation());
  EXPECT_EQ(shuttling.getStart()[1]->x, 1);
  EXPECT_EQ(shuttling.getEnd()[0]->x, 0);
  EXPECT_ANY_THROW(NAShuttlingOperation(
      ShuttleType::STORE, std::vector{std::make_shared<Point>(0, 0)},
      std::vector{std::make_shared<Point>(0, 1),
                  std::make_shared<Point>(1, 1)}));
}

TEST(NAOperation, GlobalOperation) {
  const NAGlobalOperation op(qc::RY, 0, std::vector{qc::PI_2});
  EXPECT_FALSE(op.isShuttlingOperation());
  EXPECT_FALSE(op.isLocalOperation());
  EXPECT_TRUE(op.isGlobalOperation());
  EXPECT_DOUBLE_EQ(op.getParams()[0], qc::PI_2);
  EXPECT_ANY_THROW(NAGlobalOperation(qc::ECR, 0));
}

TEST(NAOperation, LocalOperation) {
  const NALocalOperation op(qc::RY, 0, std::vector{qc::PI_2},
                            std::make_shared<Point>(0, 0));
  EXPECT_FALSE(op.isShuttlingOperation());
  EXPECT_FALSE(op.isGlobalOperation());
  EXPECT_TRUE(op.isLocalOperation());
  EXPECT_EQ(op.getType(), (std::pair{qc::RY, size_t{0U}}));
  EXPECT_DOUBLE_EQ(op.getParams()[0], qc::PI_2);
  EXPECT_EQ(op.getPositions()[0]->x, 0);
  EXPECT_ANY_THROW(NALocalOperation(qc::ECR, 0, std::make_shared<Point>(0, 0)));
  EXPECT_ANY_THROW(NALocalOperation(qc::RY, 1, std::make_shared<Point>(0, 0)));
}

TEST(NAOperation, EmptyPrint) {
  const NALocalOperation op(qc::RY, 0, std::vector{qc::PI_2},
                            std::vector<std::shared_ptr<Point>>{});
  std::stringstream ss;
  ss << op;
  EXPECT_EQ(ss.str(), "ry(1.5708) at;\n");
}
} // namespace na::nalac
