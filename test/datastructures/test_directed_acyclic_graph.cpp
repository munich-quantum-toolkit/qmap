/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "datastructures/DirectedAcyclicGraph.hpp"

#include <gtest/gtest.h>
#include <vector>

namespace qc {
TEST(DirectedAcyclicGraph, Reachable) {
  DirectedAcyclicGraph<int> g;
  g.addVertex(0);
  g.addEdge(0, 1);
  g.addEdge(1, 2);
  g.addEdge(2, 3);
  EXPECT_ANY_THROW(g.addEdge(3, 1));
  //  0 ────> 1 ———> 2 ———> 3
  //          ^             |
  //          └──────X──────┘
  EXPECT_TRUE(g.isReachable(1, 3));
  EXPECT_FALSE(g.isReachable(3, 1));
}

TEST(DirectedAcyclicGraph, TopologicalOrder) {
  DirectedAcyclicGraph<int> g;
  g.addVertex(6);
  g.addVertex(2);
  g.addVertex(5);
  g.addVertex(4);
  g.addEdge(2, 6);
  g.addEdge(2, 5);
  g.addEdge(2, 4);
  g.addEdge(5, 6);
  g.addEdge(4, 6);
  g.addEdge(4, 5);
  //         ┌─────────────┐
  //  ┌──────┼──────┐      │
  //  │      |      v      v
  //  2 ───> 4 ———> 5 ———> 6
  //  │                    ^
  //  └────────────────────┘
  const auto& actual = g.orderTopologically();
  const std::vector expected = {2, 4, 5, 6};
  EXPECT_EQ(actual, expected);
}
} // namespace qc
