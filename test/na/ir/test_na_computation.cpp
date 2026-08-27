/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "ir/Definitions.hpp"
#include "na/ir/NAComputation.hpp"
#include "na/ir/entities/Atom.hpp"
#include "na/ir/entities/Location.hpp"
#include "na/ir/entities/Zone.hpp"
#include "na/ir/operations/NAComputationGlobalCZOperation.hpp"
#include "na/ir/operations/NAComputationGlobalRYOperation.hpp"
#include "na/ir/operations/NAComputationLoadOperation.hpp"
#include "na/ir/operations/NAComputationLocalRZOperation.hpp"
#include "na/ir/operations/NAComputationLocalUOperation.hpp"
#include "na/ir/operations/NAComputationMoveOperation.hpp"
#include "na/ir/operations/NAComputationStoreOperation.hpp"

#include <gtest/gtest.h>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <vector>

namespace na {
TEST(NAComputation, Atom) {
  const auto atom = Atom("atom");
  EXPECT_EQ(atom.getName(), "atom");
  std::stringstream ss;
  ss << atom;
  EXPECT_EQ(ss.str(), "atom");
}

TEST(NAComputation, Zone) {
  const auto zone = Zone("zone");
  EXPECT_EQ(zone.getName(), "zone");
  std::stringstream ss;
  ss << zone;
  EXPECT_EQ(ss.str(), "zone");
}
TEST(NAComputation, ZonesExtent) {
  const auto zone = Zone("zone", {.minX = 0, .minY = 0, .maxX = 2, .maxY = 2});
  EXPECT_TRUE(zone.contains({1., 1.}));
  EXPECT_FALSE(zone.contains({1., 3.}));
  EXPECT_THROW(std::ignore = Zone("zone").contains({0., 0.}),
               std::runtime_error);
}

TEST(NAComputation, Location) {
  constexpr Location loc{.x = 3, .y = 4};
  EXPECT_EQ(loc, (Location{3, 4}));
  EXPECT_LE((Location{2, 4}), loc);
  EXPECT_LE(loc, loc);
  EXPECT_FALSE((Location{4, 3}) <= loc);
  std::stringstream ss;
  ss << loc;
  EXPECT_EQ(ss.str(), "(3.000, 4.000)");
  EXPECT_DOUBLE_EQ((Location{0, 0}).getEuclideanDistance(loc), 5.0);
  EXPECT_DOUBLE_EQ((Location{0, 0}).getManhattanDistanceX(loc), 3);
  EXPECT_DOUBLE_EQ((Location{0, 0}).getManhattanDistanceY(loc), 4);
}

TEST(NAComputation, LocalUOperation) {
  const Atom atom("atom");
  const NAComputationLocalUOperation op(atom, 0.0, 0.0, 0.0);
  EXPECT_EQ(op.toString(), "@+ u 0.00000 0.00000 0.00000 atom");
}

TEST(NAComputation, LocalRZOperation) {
  const Atom atom("atom");
  const NAComputationLocalRZOperation op(atom, 0.0);
  EXPECT_EQ(op.toString(), "@+ rz 0.00000 atom");
}

TEST(NAComputation, General) {
  auto qc = NAComputation();
  const auto& atom0 = qc.emplaceBackAtom("atom0");
  const auto& atom1 = qc.emplaceBackAtom("atom1");
  const auto& atom2 = qc.emplaceBackAtom("atom2");
  const auto& globalZone = qc.emplaceBackZone("global");
  qc.emplaceInitialLocation(atom0, 0, 0);
  qc.emplaceInitialLocation(atom1, 1, 0);
  qc.emplaceInitialLocation(atom2, 2, 0);
  qc.emplaceBack<NAComputationLocalRZOperation>(atom0, qc::PI_2);
  qc.emplaceBack<NAComputationLocalRZOperation>(std::vector{&atom1, &atom2},
                                                qc::PI_2);
  qc.emplaceBack<NAComputationGlobalRYOperation>(globalZone, qc::PI_2);
  qc.emplaceBack<NAComputationLoadOperation>(
      std::vector{&atom0, &atom1},
      std::vector{Location{.x = 0, .y = 1}, Location{.x = 1, .y = 1}});
  qc.emplaceBack<NAComputationMoveOperation>(
      std::vector{&atom0, &atom1},
      std::vector{Location{.x = 4, .y = 1}, Location{.x = 5, .y = 1}});
  qc.emplaceBack<NAComputationStoreOperation>(
      std::vector{&atom0, &atom1},
      std::vector{Location{.x = 4, .y = 0}, Location{.x = 5, .y = 0}});
  qc.emplaceBack(NAComputationGlobalCZOperation(globalZone));
  std::stringstream ss;
  ss << qc;
  EXPECT_EQ(ss.str(), "atom (0.000, 0.000) atom0\n"
                      "atom (1.000, 0.000) atom1\n"
                      "atom (2.000, 0.000) atom2\n"
                      "@+ rz 1.57080 atom0\n"
                      "@+ rz [\n"
                      "    1.57080 atom1\n"
                      "    1.57080 atom2\n"
                      "]\n"
                      "@+ ry 1.57080 global\n"
                      "@+ load [\n"
                      "    (0.000, 1.000) atom0\n"
                      "    (1.000, 1.000) atom1\n"
                      "]\n"
                      "@+ move [\n"
                      "    (4.000, 1.000) atom0\n"
                      "    (5.000, 1.000) atom1\n"
                      "]\n"
                      "@+ store [\n"
                      "    (4.000, 0.000) atom0\n"
                      "    (5.000, 0.000) atom1\n"
                      "]\n"
                      "@+ cz global\n");
}

TEST(NAComputation, EmptyPrint) {
  const NAComputation qc;
  std::stringstream ss;
  ss << qc;
  EXPECT_EQ(ss.str(), "");
}

namespace {

class NAComputationValidateAODConstraints : public ::testing::Test {
protected:
  NAComputation qc;
  const Atom* atom0 = nullptr;
  const Atom* atom1 = nullptr;
  const Atom* atom2 = nullptr;
  const Atom* atom3 = nullptr;

  auto SetUp() -> void override {
    atom0 = &qc.emplaceBackAtom("atom0");
    atom1 = &qc.emplaceBackAtom("atom1");
    atom2 = &qc.emplaceBackAtom("atom2");
    atom3 = &qc.emplaceBackAtom("atom3");
    qc.emplaceInitialLocation(*atom0, 0, 0);
    qc.emplaceInitialLocation(*atom1, 1, 2);
    qc.emplaceInitialLocation(*atom2, 1, 0);
    qc.emplaceInitialLocation(*atom3, 0, 2);
  }
};

} // namespace

TEST_F(NAComputationValidateAODConstraints, AtomAlreadyLoaded) {
  qc.emplaceBack<NAComputationLoadOperation>(
      std::vector{atom0, atom2},
      std::vector{Location{.x = 0, .y = 1}, Location{.x = 1, .y = 1}});
  EXPECT_TRUE(qc.validate().first);
  qc.emplaceBack<NAComputationLoadOperation>(*atom0, Location{.x = 0, .y = 1});
  EXPECT_FALSE(qc.validate().first);
}
TEST_F(NAComputationValidateAODConstraints, AtomNotLoaded) {
  qc.emplaceBack<NAComputationMoveOperation>(*atom0, Location{.x = 0, .y = 1});
  EXPECT_FALSE(qc.validate().first);
}
TEST_F(NAComputationValidateAODConstraints, DuplicateAtomsInShuttle) {
  qc.emplaceBack<NAComputationLoadOperation>(std::vector{atom0, atom0});
  qc.emplaceBack<NAComputationMoveOperation>(
      std::vector{atom0, atom0},
      std::vector{Location{.x = 0, .y = 1}, Location{.x = 1, .y = 1}});
  EXPECT_FALSE(qc.validate().first);
}
TEST_F(NAComputationValidateAODConstraints, DuplicateEndPoints) {
  qc.emplaceBack<NAComputationLoadOperation>(std::vector{atom0, atom2});
  qc.emplaceBack<NAComputationMoveOperation>(
      std::vector{atom0, atom2},
      std::vector{Location{.x = 0, .y = 1}, Location{.x = 0, .y = 1}});
  EXPECT_FALSE(qc.validate().first);
}
TEST_F(NAComputationValidateAODConstraints, ColumnPreserving1) {
  qc.emplaceBack<NAComputationLoadOperation>(std::vector{atom2, atom1});
  qc.emplaceBack<NAComputationMoveOperation>(
      std::vector{atom2, atom1},
      std::vector{Location{.x = 0, .y = 1}, Location{.x = 2, .y = 2}});
  EXPECT_FALSE(qc.validate().first);
}
TEST_F(NAComputationValidateAODConstraints, RowPreserving1) {
  qc.emplaceBack<NAComputationLoadOperation>(std::vector{atom0, atom2});
  qc.emplaceBack<NAComputationMoveOperation>(
      std::vector{atom0, atom2},
      std::vector{Location{.x = 0, .y = 1}, Location{.x = 1, .y = -1}});
  EXPECT_FALSE(qc.validate().first);
}
TEST_F(NAComputationValidateAODConstraints, ColumnPreserving2) {
  qc.emplaceBack<NAComputationLoadOperation>(std::vector{atom0, atom1});
  qc.emplaceBack<NAComputationMoveOperation>(
      std::vector{atom0, atom1},
      std::vector{Location{.x = 1, .y = 1}, Location{.x = 0, .y = 1}});
  EXPECT_FALSE(qc.validate().first);
}
TEST_F(NAComputationValidateAODConstraints, RowPreserving2) {
  // row order not preserved
  qc.emplaceBack<NAComputationLoadOperation>(std::vector{atom0, atom1});
  qc.emplaceBack<NAComputationMoveOperation>(
      std::vector{atom0, atom1},
      std::vector{Location{.x = 0, .y = 1}, Location{.x = 2, .y = 0}});
  EXPECT_FALSE(qc.validate().first);
}
TEST_F(NAComputationValidateAODConstraints, ColumnPreserving3) {
  qc.emplaceBack<NAComputationLoadOperation>(std::vector{atom3, atom2});
  qc.emplaceBack<NAComputationMoveOperation>(
      std::vector{atom2, atom3},
      std::vector{Location{.x = 0, .y = 1}, Location{.x = 1, .y = 3}});
  EXPECT_FALSE(qc.validate().first);
}
TEST_F(NAComputationValidateAODConstraints, RowPreserving3) {
  qc.emplaceBack<NAComputationLoadOperation>(std::vector{atom3, atom2});
  qc.emplaceBack<NAComputationMoveOperation>(
      std::vector{atom3, atom2},
      std::vector{Location{.x = 0, .y = 1}, Location{.x = 2, .y = 2}});
  EXPECT_FALSE(qc.validate().first);
}
TEST_F(NAComputationValidateAODConstraints, DuplicateAtomsInRz) {
  qc.emplaceBack<NAComputationLocalRZOperation>(std::vector{atom0, atom0},
                                                qc::PI_2);
  EXPECT_FALSE(qc.validate().first);
}
TEST_F(NAComputationValidateAODConstraints, DuplicateAtoms) {
  // store unloaded atom
  qc.emplaceBack<NAComputationStoreOperation>(*atom0);
  EXPECT_FALSE(qc.validate().first);
}
TEST_F(NAComputationValidateAODConstraints, RowPreserving4) {
  qc.emplaceBack<NAComputationLoadOperation>(std::vector{atom2, atom3});
  qc.emplaceBack<NAComputationStoreOperation>(
      std::vector{atom2, atom3},
      std::vector{Location{.x = 1, .y = 3}, Location{.x = 0, .y = 2}});
  EXPECT_FALSE(qc.validate().first);
}
TEST_F(NAComputationValidateAODConstraints, StoreStoredAtom) {
  qc.emplaceBack<NAComputationLoadOperation>(*atom2);
  qc.emplaceBack<NAComputationStoreOperation>(*atom2);
  qc.emplaceBack<NAComputationStoreOperation>(*atom2);
  EXPECT_FALSE(qc.validate().first);
}

TEST(NAComputation, GetPositionOfAtomAfterOperation) {
  auto qc = NAComputation();
  const auto& atom0 = qc.emplaceBackAtom("atom0");
  qc.emplaceInitialLocation(atom0, 0, 0);
  qc.emplaceBack<NAComputationLoadOperation>(atom0);
  qc.emplaceBack<NAComputationMoveOperation>(atom0, Location{.x = 1, .y = 1});
  qc.emplaceBack<NAComputationStoreOperation>(atom0);
  EXPECT_EQ(qc.getLocationOfAtomAfterOperation(atom0, qc[0]), (Location{0, 0}));
  EXPECT_EQ(qc.getLocationOfAtomAfterOperation(atom0, qc[2]), (Location{1, 1}));
}

TEST(NAComputation, NonMovingAtomsViolateRowOrderConstraint) {
  auto qc = NAComputation();
  const auto& atom0 = qc.emplaceBackAtom("atom0");
  qc.emplaceInitialLocation(atom0, 0, 0);
  const auto& atom1 = qc.emplaceBackAtom("atom1");
  qc.emplaceInitialLocation(atom1, 3, 0);
  qc.emplaceBack<NAComputationLoadOperation>(std::vector{&atom0, &atom1});
  qc.emplaceBack<NAComputationMoveOperation>(atom0, Location{.x = 4, .y = 4});
  qc.emplaceBack<NAComputationStoreOperation>(std::vector{&atom0, &atom1});
  const auto [ok, msg] = qc.validate();
  EXPECT_FALSE(ok);
  // Optional: verify row-preservation is the cause triggered here
  EXPECT_NE(msg.find("rows not preserved"), std::string::npos);
}
} // namespace na
