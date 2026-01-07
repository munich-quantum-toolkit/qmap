/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "hybridmap/NeutralAtomDefinitions.hpp"
#include "hybridmap/NeutralAtomUtils.hpp"

#include <gtest/gtest.h>
#include <stdexcept>

using namespace na;

TEST(NeutralAtomUtils, InitialCoordinateMappingFromStringTest) {
  EXPECT_EQ(initialCoordinateMappingFromString("trivial"),
            InitialCoordinateMapping::Trivial);
  EXPECT_EQ(initialCoordinateMappingFromString("0"),
            InitialCoordinateMapping::Trivial);
  EXPECT_EQ(initialCoordinateMappingFromString("random"),
            InitialCoordinateMapping::Random);
  EXPECT_EQ(initialCoordinateMappingFromString("1"),
            InitialCoordinateMapping::Random);
}

TEST(NeutralAtomUtils, InitialCoordinateMappingFromStringThrow) {
  EXPECT_THROW((void)initialCoordinateMappingFromString("foobar"),
               std::invalid_argument);
}

TEST(NeutralAtomUtils, InitialMappingFromString) {
  EXPECT_EQ(initialMappingFromString("identity"), InitialMapping::Identity);
  EXPECT_EQ(initialMappingFromString("0"), InitialMapping::Identity);
  EXPECT_EQ(initialMappingFromString("graph"), InitialMapping::Graph);
  EXPECT_EQ(initialMappingFromString("1"), InitialMapping::Graph);
}

TEST(NeutralAtomUtils, InitialMappingFromStringThrow) {
  EXPECT_THROW((void)initialMappingFromString("baz"), std::invalid_argument);
}

TEST(NeutralAtomUtils, MoveCombConstructorsAndEquality) {
  constexpr AtomMove m1{.c1 = 1, .c2 = 2, .load1 = true, .load2 = false};
  constexpr AtomMove m2{.c1 = 3, .c2 = 4, .load1 = false, .load2 = true};
  const CoordIndices pos{5, 6};

  // vector-based constructor
  const MoveComb cvec({m1, m2}, /*cost*/ 1.23, /*op*/ nullptr, pos);
  EXPECT_FALSE(cvec.empty());
  EXPECT_EQ(cvec.moves.size(), 2U);
  EXPECT_DOUBLE_EQ(cvec.cost, 1.23);
  EXPECT_EQ(cvec.op, nullptr);
  EXPECT_EQ(cvec.bestPos, pos);

  // single-move constructor
  const MoveComb cone(m1, /*cost*/ 0.5, /*op*/ nullptr, CoordIndices{7});
  EXPECT_FALSE(cone.empty());
  EXPECT_EQ(cone.moves.size(), 1U);
  EXPECT_DOUBLE_EQ(cone.cost, 0.5);
  EXPECT_EQ(cone.op, nullptr);
  EXPECT_EQ(cone.bestPos, (CoordIndices{7}));

  // equality compares only the moves vector (per operator== definition)
  const MoveComb cvecSameMoves(
      {m1, m2}, /*cost*/ 9.99,
      /*op*/ reinterpret_cast<const qc::Operation*>(0x1), CoordIndices{42});
  EXPECT_TRUE(cvec == cvecSameMoves);
  EXPECT_FALSE(cvec != cvecSameMoves);

  const MoveComb cvecDiffMoves({m2, m1}, /*cost*/ 1.23, /*op*/ nullptr, pos);
  EXPECT_FALSE(cvec == cvecDiffMoves);
  EXPECT_TRUE(cvec != cvecDiffMoves);
}

TEST(NeutralAtomUtils, MoveCombEmpty) {
  const MoveComb emptyComb;
  EXPECT_TRUE(emptyComb.empty());
}

TEST(NeutralAtomUtils, MoveVectorLengthAndDirection) {
  // 3-4-5 triangle
  const MoveVector mv(0, 0, 3, 4);
  EXPECT_DOUBLE_EQ(mv.getLength(), 5.0);
  // Direction should be positive in both axes
  EXPECT_TRUE(mv.direction.x);
  EXPECT_TRUE(mv.direction.y);
}

TEST(NeutralAtomUtils, MoveVectorOverlapAndInclude) {
  // Overlap: same row (y=0), x-ranges sharing points
  const MoveVector a(0, 0, 5, 0);
  const MoveVector b(3, 0, 10, 0);
  EXPECT_TRUE(a.overlap(b));
  EXPECT_TRUE(b.overlap(a));
  EXPECT_TRUE(a.sameDirection(b));
  EXPECT_TRUE(b.sameDirection(a));

  // No overlap in either X or Y ranges -> should be false
  const MoveVector c(0, 0, 0, 2);   // vertical at x=0, y in [0,2]
  const MoveVector d(10, 5, 12, 5); // horizontal at y=5, x in [10,12]
  EXPECT_FALSE(c.overlap(d));
  EXPECT_FALSE(d.overlap(c));

  // Include
  const MoveVector inner(2, 0, 4, 0);
  const MoveVector outer(1, 0, 5, 0);
  EXPECT_TRUE(inner.include(outer));
  EXPECT_FALSE(outer.include(inner));

  // Non-include: disjoint segments
  const MoveVector e(0, 0, 2, 0);
  const MoveVector f(3, 0, 5, 0);
  EXPECT_FALSE(e.include(f));
  EXPECT_FALSE(f.include(e));
}
