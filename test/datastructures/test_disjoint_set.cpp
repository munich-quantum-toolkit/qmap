/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "datastructures/DisjointSet.hpp"

#include <gtest/gtest.h>
#include <vector>

namespace qc {
TEST(DisjointSet, FindSet) {
  std::vector elements = {1, 2, 3};
  DisjointSet<int> ds(elements.begin(), elements.end());
  EXPECT_EQ(ds.findSet(1), ds.findSet(1));
  EXPECT_NE(ds.findSet(1), ds.findSet(2));
}

TEST(DisjointSet, UnionSet) {
  std::vector elements = {1, 2, 3};
  DisjointSet<int> ds(elements.begin(), elements.end());
  ds.unionSet(1, 2);
  EXPECT_EQ(ds.findSet(1), ds.findSet(2));
  EXPECT_NE(ds.findSet(1), ds.findSet(3));
  EXPECT_NE(ds.findSet(2), ds.findSet(3));
  ds.unionSet(1, 3);
  EXPECT_EQ(ds.findSet(1), ds.findSet(2));
  EXPECT_EQ(ds.findSet(1), ds.findSet(3));
  EXPECT_EQ(ds.findSet(2), ds.findSet(3));
}
} // namespace qc
