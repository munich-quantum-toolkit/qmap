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
#include "hybridmap/NeutralAtomLayer.hpp"
#include "hybridmap/NeutralAtomOperation.hpp"
#include "hybridmap/OpenQASMSerializer.hpp"
#include "ir/Definitions.hpp"
#include "ir/QuantumComputation.hpp"
#include "ir/operations/CompoundOperation.hpp"
#include "ir/operations/OpType.hpp"
#include "ir/operations/StandardOperation.hpp"

#include <gtest/gtest.h>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace na {

TEST(NeutralAtomOperation, Move) {
  NeutralAtomOperation move(NeutralAtomOperationKind::Move, {0, 1});

  EXPECT_EQ(move.getType(), qc::OpType::None);
  EXPECT_EQ(move.getName(), "move");
  EXPECT_TRUE(
      hasNeutralAtomOperationKind(move, NeutralAtomOperationKind::Move));
  EXPECT_FALSE(isAodOperation(move));

  move.invert();
  EXPECT_EQ(move.getTargets(), (qc::Targets{1, 0}));
}

TEST(NeutralAtomOperation, Bridge) {
  const NeutralAtomOperation bridge(NeutralAtomOperationKind::Bridge,
                                    {0, 1, 2});
  const auto same = bridge.clone();
  const NeutralAtomOperation move(NeutralAtomOperationKind::Move, {0, 1, 2});

  EXPECT_TRUE(bridge == *same);
  EXPECT_FALSE(bridge == move);
  EXPECT_FALSE(commuteAtQubit(&bridge, &move, 0));
  EXPECT_TRUE(commuteAtQubit(&bridge, same.get(), 0));
}

TEST(NeutralAtomOperation, Qasm) {
  qc::QuantumComputation computation(3);
  auto compound = std::make_unique<qc::CompoundOperation>();
  compound->emplace_back(makeBridgeOperation({0, 1, 2}));
  compound->emplace_back<qc::StandardOperation>(1, qc::H);
  computation.emplace_back(std::move(compound));
  std::stringstream stream;
  serializeOpenQASM(computation, stream);

  EXPECT_EQ(stream.str(), "// i 0 1 2\n"
                          "// o 0 1 2\n"
                          "OPENQASM 2.0;\n"
                          "include \"qelib1.inc\";\n"
                          "qreg q[3];\n"
                          "bridge q[0], q[1], q[2];\n"
                          "h q[1];\n");
}

TEST(NeutralAtomOperation, ConditionalQasm) {
  qc::QuantumComputation computation(3);
  const auto& controlRegister = computation.addClassicalRegister(1);
  auto thenOperation = std::make_unique<qc::CompoundOperation>();
  thenOperation->emplace_back(makeBridgeOperation({0, 1, 2}));
  thenOperation->emplace_back<qc::StandardOperation>(1, qc::H);
  computation.ifElse(
      std::move(thenOperation),
      std::make_unique<AodOperation>(NeutralAtomOperationKind::AodMove,
                                     qc::Targets{0}, std::vector{Dimension::X},
                                     std::vector{0.}, std::vector{1.}),
      controlRegister);

  std::stringstream stream;
  serializeOpenQASM(computation, stream);

  EXPECT_EQ(stream.str(), "// i 0 1 2\n"
                          "// o 0 1 2\n"
                          "OPENQASM 2.0;\n"
                          "include \"qelib1.inc\";\n"
                          "qreg q[3];\n"
                          "creg c[1];\n"
                          "if (c == 1) {\n"
                          "  bridge q[0], q[1], q[2];\n"
                          "  h q[1];\n"
                          "}\n"
                          "if (c != 1) {\n"
                          "  aod_move (0, 0, 1) q[0];\n"
                          "}\n");
}

TEST(NeutralAtomOperation, ConditionalOnBitQasm) {
  qc::QuantumComputation computation(3, 1);
  computation.ifElse(
      makeBridgeOperation({0, 1, 2}),
      std::make_unique<AodOperation>(NeutralAtomOperationKind::AodMove,
                                     qc::Targets{0}, std::vector{Dimension::X},
                                     std::vector{0.}, std::vector{1.}),
      0);

  std::stringstream stream;
  serializeOpenQASM(computation, stream);

  EXPECT_EQ(stream.str(), "// i 0 1 2\n"
                          "// o 0 1 2\n"
                          "OPENQASM 2.0;\n"
                          "include \"qelib1.inc\";\n"
                          "qreg q[3];\n"
                          "creg c[1];\n"
                          "if (c[0]) {\n"
                          "  bridge q[0], q[1], q[2];\n"
                          "}\n"
                          "if (!c[0]) {\n"
                          "  aod_move (0, 0, 1) q[0];\n"
                          "}\n");
}

TEST(NeutralAtomOperation, Parsing) {
  EXPECT_EQ(neutralAtomOperationKindFromString("aod_move"),
            NeutralAtomOperationKind::AodMove);
  EXPECT_THROW(static_cast<void>(neutralAtomOperationKindFromString("x")),
               std::invalid_argument);
}

TEST(NeutralAtomOperation, AodOperationsRemainDistinct) {
  const AodOperation first(NeutralAtomOperationKind::AodMove, {0},
                           {Dimension::X}, {0.}, {1.});
  const AodOperation same(NeutralAtomOperationKind::AodMove, {0},
                          {Dimension::X}, {0.}, {1.});
  const AodOperation different(NeutralAtomOperationKind::AodMove, {0},
                               {Dimension::X}, {0.}, {2.});

  EXPECT_TRUE(first == same);
  EXPECT_FALSE(first == different);
}

} // namespace na
