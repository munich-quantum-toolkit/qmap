/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "datastructures/UndirectedGraph.hpp"

#include <gtest/gtest.h>
#include <stdexcept>
#include <tuple>

namespace qc {
TEST(UndirectedGraph, Numbered) {
  UndirectedGraph<int, int> g;
  g.addVertex(0);
  g.addEdge(1, 1, 0);
  g.addEdge(1, 2, 1);
  g.addEdge(2, 3, 2);
  g.addEdge(3, 1, 3);
  EXPECT_EQ(g.getNVertices(), 4);
  EXPECT_EQ(g.getNEdges(), 4);
  EXPECT_EQ(g.getEdge(1, 2), 1);
  EXPECT_EQ(g.getEdge(1, 1), 0);
  EXPECT_EQ(g.getDegree(2), 2);
  EXPECT_EQ(g.getDegree(1), 3);
  EXPECT_TRUE(g.isAdjacent(1, 2));
  EXPECT_FALSE(g.isAdjacent(1, 0));
  EXPECT_TRUE(g.isAdjacentEdge({1, 2}, {2, 3}));

  EXPECT_THROW(g.addVertex(1), std::invalid_argument);
  EXPECT_THROW(g.addEdge(1, 2, 10), std::invalid_argument);
  EXPECT_THROW(g.addEdge(2, 1, 10), std::invalid_argument);
  EXPECT_THROW(std::ignore = g.getDegree(4), std::invalid_argument);
  EXPECT_THROW(std::ignore = g.getEdge(0, 1), std::invalid_argument);
  EXPECT_THROW(std::ignore = g.isAdjacent(0, 10), std::invalid_argument);
  EXPECT_THROW(std::ignore = g.isAdjacent(10, 1), std::invalid_argument);
}
} // namespace qc
