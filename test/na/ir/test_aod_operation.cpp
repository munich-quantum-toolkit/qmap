/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "ir/Register.hpp"
#include "ir/operations/Control.hpp"
#include "na/ir/operations/AodOperation.hpp"

#include <cstdint>
#include <gtest/gtest.h>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <vector>

TEST(AodOperation, Activate) {
  // activate at position 0, dimension X, start 0.0, end 1.0
  const na::AodOperation activate(na::NAOpType::AodActivate, {0},
                                  {na::AodOperation::Dimension::X}, {0.0},
                                  {1.0});
  EXPECT_EQ(activate.getNqubits(), 1);
  EXPECT_EQ(activate.getStarts(na::AodOperation::Dimension::X).at(0), 0.0);
  EXPECT_EQ(activate.getEnds(na::AodOperation::Dimension::X).at(0), 1.0);
}

TEST(AodOperation, Deactivate) {
  // deactivate at position 0,1 dimension Y, start 0.0, end 1.0
  const na::AodOperation deactivate(na::NAOpType::AodDeactivate, {0, 1},
                                    {na::AodOperation::Dimension::Y}, {0.0},
                                    {1.0});
  EXPECT_EQ(deactivate.getNqubits(), 2);
  EXPECT_EQ(deactivate.getStarts(na::AodOperation::Dimension::Y).at(0), 0.0);
  EXPECT_EQ(deactivate.getEnds(na::AodOperation::Dimension::Y).at(0), 1.0);
}

TEST(AodOperation, Move) {
  // move from 0,1 to 2,3 dimension X, start 0.0, end 1.0 and dimension Y,
  // start 1.0, end 2.0
  const na::AodOperation move(
      na::NAOpType::AodMove, {0, 1},
      {na::AodOperation::Dimension::X, na::AodOperation::Dimension::Y},
      {0.0, 1.0}, {1.0, 2.0});
  EXPECT_EQ(move.getNqubits(), 2);
  EXPECT_EQ(move.getStarts(na::AodOperation::Dimension::X).at(0), 0.0);
  EXPECT_EQ(move.getEnds(na::AodOperation::Dimension::X).at(0), 1.0);
  EXPECT_EQ(move.getStarts(na::AodOperation::Dimension::Y).at(0), 1.0);
  EXPECT_EQ(move.getEnds(na::AodOperation::Dimension::Y).at(0), 2.0);
}

TEST(AodOperation, Distances) {
  const na::AodOperation move(
      na::NAOpType::AodMove, {0, 1},
      {na::AodOperation::Dimension::X, na::AodOperation::Dimension::Y},
      {0.0, 1.0}, {1.0, 3.0});
  EXPECT_EQ(move.getMaxDistance(na::AodOperation::Dimension::X), 1.0);
  EXPECT_EQ(move.getMaxDistance(na::AodOperation::Dimension::Y), 2.0);
}

TEST(AodOperation, Qasm) {
  const na::AodOperation move(
      na::NAOpType::AodMove, {0, 1},
      {na::AodOperation::Dimension::X, na::AodOperation::Dimension::Y},
      {0.0, 1.0}, {1.0, 3.0});
  std::stringstream ss;
  qc::QuantumRegister qreg(0, 2, "q");
  qc::QubitIndexToRegisterMap qubitToReg{};
  qubitToReg.try_emplace(0, qreg, qreg.toString(0));
  qubitToReg.try_emplace(1, qreg, qreg.toString(1));
  move.dumpOpenQASM(ss, qubitToReg, {}, 0, false);

  EXPECT_EQ(ss.str(), "aod_move (0, 0, 1; 1, 1, 3) q[0], q[1];\n");
}

TEST(AodOperation, Print) {
  const na::AodOperation activate(na::NAOpType::AodActivate, {1},
                                  {na::AodOperation::Dimension::X}, {0.0},
                                  {1.0});
  const qc::Operation& operation = activate;
  std::stringstream stream;

  operation.print(stream, {}, 0, 2);

  EXPECT_EQ(stream.str(), "   |\033[0m\033[1m\033[36maod_activate\033[0m");
}

TEST(AodOperation, Constructors) {
  uint32_t const dir1 = 0;
  uint32_t const dir2 = 1;

  const na::AodOperation move(na::NAOpType::AodMove, {0, 1}, {dir1, dir2},
                              {0.0, 1.0}, {1.0, 3.0});
  const na::AodOperation move2("aod_move", {0, 1}, {dir1}, {0.0}, {1.0});
  const na::AodOperation move3(
      na::NAOpType::AodMove, {0},
      {std::tuple{na::AodOperation::Dimension::X, 0.0, 1.0}});
  const na::AodOperation::Segment segment(na::AodOperation::Dimension::X, 0.0,
                                          1.0);
  const na::AodOperation move4(na::NAOpType::AodMove, {0}, {segment});

  EXPECT_EQ(move.getMaxDistance(na::AodOperation::Dimension::Y), 2.0);
  EXPECT_EQ(move2.getNAOpType(), na::NAOpType::AodMove);
  EXPECT_TRUE(move3 == move4);
  const qc::Operation& moveBase = move3;
  EXPECT_TRUE(
      moveBase.equals(na::AodOperation(na::NAOpType::AodMove, {1}, {segment}),
                      qc::Permutation{{0, 1}}, {}));
  EXPECT_THROW(static_cast<void>(na::AodOperation(
                   na::NAOpType::Move, {0}, std::vector<std::uint32_t>{dir1},
                   {0.0}, {1.0})),
               std::invalid_argument);
  EXPECT_THROW(static_cast<void>(
                   na::AodOperation(na::NAOpType::AodMove, {0},
                                    std::vector{na::AodOperation::Dimension::X,
                                                na::AodOperation::Dimension::Y},
                                    {0.0}, {1.0})),
               std::invalid_argument);
}

TEST(AodOperation, DefaultConstructionIsUnset) {
  const na::AodOperation operation;

  EXPECT_EQ(operation.getType(), qc::OpType::None);
  EXPECT_EQ(operation.getNAOpType(), na::NAOpType::None);
  EXPECT_EQ(na::getNAOpType(operation), std::nullopt);
  EXPECT_FALSE(na::hasNAOpType(operation, na::NAOpType::AodMove));
}

TEST(AodOperation, OverrideMethods) {
  na::AodOperation move(na::NAOpType::AodMove, {0},
                        {na::AodOperation::Dimension::X}, {0.0}, {1.0});
  move.addControl(qc::Control(0, qc::Control::Type::Pos));
  move.removeControl(qc::Control(0, qc::Control::Type::Pos));
  move.clearControls();
  EXPECT_NO_THROW(move.setGate(qc::OpType::None));
  EXPECT_THROW(move.setGate(qc::OpType::X), std::invalid_argument);
  EXPECT_EQ(move.getName(), "aod_move");
  const auto copy = move.clone();
  EXPECT_TRUE(move == *copy);
}

TEST(AodOperation, Invert) {
  na::AodOperation move(na::NAOpType::AodMove, {0},
                        {na::AodOperation::Dimension::X}, {0.0}, {1.0});
  move.invert();
  EXPECT_EQ(move.getStarts(na::AodOperation::Dimension::X).at(0), 1.0);
  EXPECT_EQ(move.getEnds(na::AodOperation::Dimension::X).at(0), 0.0);

  na::AodOperation activate(na::NAOpType::AodActivate, {0},
                            {na::AodOperation::Dimension::X}, {0.0}, {1.0});
  activate.invert();
  EXPECT_EQ(activate.getNAOpType(), na::NAOpType::AodDeactivate);
  EXPECT_EQ(activate.getName(), "aod_deactivate");

  na::AodOperation deactivate(na::NAOpType::AodDeactivate, {0},
                              {na::AodOperation::Dimension::X}, {0.0}, {1.0});
  deactivate.invert();
  EXPECT_EQ(deactivate.getNAOpType(), na::NAOpType::AodActivate);
  EXPECT_EQ(deactivate.getName(), "aod_activate");
}
