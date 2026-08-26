/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "hybridmap/AodOperation.hpp"
#include "hybridmap/NeutralAtomOperation.hpp"
#include "hybridmap/OpenQASMSerializer.hpp"
#include "ir/QuantumComputation.hpp"
#include "ir/operations/Control.hpp"

#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <sstream>
#include <tuple>
#include <vector>

TEST(AodOperation, Activate) {
  // activate at position 0, dimension X, start 0.0, end 1.0
  const na::AodOperation activate(na::NeutralAtomOperationKind::AodActivate,
                                  {0}, {na::Dimension::X}, {0.0}, {1.0});
  EXPECT_EQ(activate.getNqubits(), 1);
  EXPECT_EQ(activate.getStarts(na::Dimension::X).at(0), 0.0);
  EXPECT_EQ(activate.getEnds(na::Dimension::X).at(0), 1.0);
}

TEST(AodOperation, Deactivate) {
  // deactivate at position 0,1 dimension Y, start 0.0, end 1.0
  const na::AodOperation deactivate(na::NeutralAtomOperationKind::AodDeactivate,
                                    {0, 1}, {na::Dimension::Y}, {0.0}, {1.0});
  EXPECT_EQ(deactivate.getNqubits(), 2);
  EXPECT_EQ(deactivate.getStarts(na::Dimension::Y).at(0), 0.0);
  EXPECT_EQ(deactivate.getEnds(na::Dimension::Y).at(0), 1.0);
}

TEST(AodOperation, Move) {
  // move from 0,1 to 2,3 dimension X, start 0.0, end 1.0 and dimension Y,
  // start 1.0, end 2.0
  const na::AodOperation move(na::NeutralAtomOperationKind::AodMove, {0, 1},
                              {na::Dimension::X, na::Dimension::Y}, {0.0, 1.0},
                              {1.0, 2.0});
  EXPECT_EQ(move.getNqubits(), 2);
  EXPECT_EQ(move.getStarts(na::Dimension::X).at(0), 0.0);
  EXPECT_EQ(move.getEnds(na::Dimension::X).at(0), 1.0);
  EXPECT_EQ(move.getStarts(na::Dimension::Y).at(0), 1.0);
  EXPECT_EQ(move.getEnds(na::Dimension::Y).at(0), 2.0);
}

TEST(AodOperation, Distances) {
  const na::AodOperation move(na::NeutralAtomOperationKind::AodMove, {0, 1},
                              {na::Dimension::X, na::Dimension::Y}, {0.0, 1.0},
                              {1.0, 3.0});
  EXPECT_EQ(move.getMaxDistance(na::Dimension::X), 1.0);
  EXPECT_EQ(move.getMaxDistance(na::Dimension::Y), 2.0);
}

TEST(AodOperation, Qasm) {
  qc::QuantumComputation computation(2);
  computation.emplace_back(std::make_unique<na::AodOperation>(
      na::NeutralAtomOperationKind::AodMove, qc::Targets{0, 1},
      std::vector{na::Dimension::X, na::Dimension::Y}, std::vector{0.0, 1.0},
      std::vector{1.0, 3.0}));
  std::stringstream ss;
  na::serializeOpenQASM(computation, ss);

  EXPECT_EQ(ss.str(), "// i 0 1\n"
                      "// o 0 1\n"
                      "OPENQASM 2.0;\n"
                      "include \"qelib1.inc\";\n"
                      "qreg q[2];\n"
                      "aod_move (0, 0, 1; 1, 1, 3) q[0], q[1];\n");
}

TEST(AodOperation, Constructors) {
  uint32_t const dir1 = 0;
  uint32_t const dir2 = 1;

  const na::AodOperation move(na::NeutralAtomOperationKind::AodMove, {0, 1},
                              {dir1, dir2}, {0.0, 1.0}, {1.0, 3.0});
  const na::AodOperation move2("aod_move", {0, 1}, {dir1}, {0.0}, {1.0});
  const na::AodOperation move3(na::NeutralAtomOperationKind::AodMove, {0},
                               {std::tuple{na::Dimension::X, 0.0, 1.0}});
  na::SingleOperation const singleOp(na::Dimension::X, 0.0, 1.0);
  const na::AodOperation move4(na::NeutralAtomOperationKind::AodMove, {0},
                               {singleOp});

  EXPECT_EQ(0, 0);
}

TEST(AodOperation, OverrideMethods) {
  na::AodOperation move(na::NeutralAtomOperationKind::AodMove, {0},
                        {na::Dimension::X}, {0.0}, {1.0});
  move.addControl(qc::Control(0, qc::Control::Type::Pos));
  move.removeControl(qc::Control(0, qc::Control::Type::Pos));
  move.clearControls();
  auto it = move.clone();
}

TEST(AodOperation, Invert) {
  na::AodOperation move(na::NeutralAtomOperationKind::AodMove, {0},
                        {na::Dimension::X}, {0.0}, {1.0});
  move.invert();
  EXPECT_EQ(move.getStarts(na::Dimension::X).at(0), 1.0);
  EXPECT_EQ(move.getEnds(na::Dimension::X).at(0), 0.0);

  na::AodOperation activate(na::NeutralAtomOperationKind::AodActivate, {0},
                            {na::Dimension::X}, {0.0}, {1.0});
  activate.invert();
  EXPECT_EQ(activate.getKind(), na::NeutralAtomOperationKind::AodDeactivate);
  EXPECT_EQ(activate.getName(), "aod_deactivate");

  na::AodOperation deactivate(na::NeutralAtomOperationKind::AodDeactivate, {0},
                              {na::Dimension::X}, {0.0}, {1.0});
  deactivate.invert();
  EXPECT_EQ(deactivate.getKind(), na::NeutralAtomOperationKind::AodActivate);
  EXPECT_EQ(deactivate.getName(), "aod_activate");
}
