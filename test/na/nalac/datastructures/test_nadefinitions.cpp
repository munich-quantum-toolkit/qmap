/*
 * Copyright (c) 2025 Chair for Design Automation, TUM
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "ir/QuantumComputation.hpp"
#include "ir/operations/OpType.hpp"
#include "na/nalac/datastructures/NADefinitions.hpp"
#include "qasm3/Importer.hpp"

#include <gtest/gtest.h>
#include <sstream>
#include <string>
#include <unordered_map>

namespace na::nalac {
TEST(NADefinitions, Point) {
  const Point p(-1, 2);
  EXPECT_EQ(p.x, -1);
  EXPECT_EQ(p.y, 2);
  EXPECT_EQ(p.length(), 2);
  EXPECT_EQ(p.toString(), "(-1, 2)");
  std::stringstream ss;
  ss << p;
  EXPECT_EQ(ss.str(), "(-1, 2)");
  EXPECT_EQ(p, Point(-1, 2));
  EXPECT_FALSE(p == Point(1, 2));
  EXPECT_EQ(p - Point(1, 2), Point(-2, 0));
  EXPECT_EQ(Point(1, 2) + p, Point(0, 4));
}

TEST(NADefinitions, PointDistances) {
  const Point p1(0, 0);
  const Point p2(3, 4);
  EXPECT_EQ(p1.getEuclideanDistance(p2), 5);
  EXPECT_EQ(p1.getManhattanDistanceX(p2), 3);
  EXPECT_EQ(p1.getManhattanDistanceY(p2), 4);
  EXPECT_EQ(p2.getManhattanDistanceX(p1), 3);
  EXPECT_EQ(p2.getManhattanDistanceY(p1), 4);
}

TEST(NADefinitions, IsGlobal) {
  const std::string testfile = "OPENQASM 3.0;\n"
                               "include \"stdgates.inc\";\n"
                               "qubit[3] q;\n"
                               "rz(pi/4) q[0];\n"
                               "ry(pi/2) q;\n";
  const auto qc = qasm3::Importer::imports(testfile);
  EXPECT_EQ(qc.getHighestLogicalQubitIndex(), 2);
  EXPECT_FALSE(isGlobal(*qc.at(0), 3));
  EXPECT_TRUE(isGlobal(*qc.at(1), 3));
}

TEST(NADefinitions, OpTypeHash) {
  std::unordered_map<std::pair<qc::OpType, std::size_t>, int> map;
  map.emplace(std::pair{qc::OpType::X, 1}, 1);
  map.emplace(std::pair{qc::OpType::X, 2}, 2);
  map.emplace(std::pair{qc::OpType::Y, 1}, 3);
  map.emplace(std::pair{qc::OpType::Y, 2}, 4);
  EXPECT_EQ(map.at(std::pair{qc::OpType::X, 1}), 1);
  EXPECT_EQ(map.at(std::pair{qc::OpType::X, 2}), 2);
  EXPECT_EQ(map.at(std::pair{qc::OpType::Y, 1}), 3);
  EXPECT_EQ(map.at(std::pair{qc::OpType::Y, 2}), 4);
}
} // namespace na::nalac
