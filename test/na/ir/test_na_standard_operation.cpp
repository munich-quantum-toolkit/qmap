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
#include "ir/Register.hpp"
#include "ir/operations/OpType.hpp"
#include "ir/operations/StandardOperation.hpp"
#include "na/ir/operations/AodOperation.hpp"
#include "na/ir/operations/NAStandardOperation.hpp"

#include <array>
#include <gtest/gtest.h>
#include <optional>
#include <sstream>
#include <stdexcept>

namespace na {

TEST(NAStandardOperation, Move) {
  NAStandardOperation move(NeutralAtomOpType::Move, {0, 1});

  EXPECT_EQ(move.getType(), qc::OpType::None);
  EXPECT_EQ(move.getName(), "move");
  EXPECT_EQ(move.getNeutralAtomOpType(), NeutralAtomOpType::Move);
  EXPECT_TRUE(move.isStandardOperation());
  EXPECT_TRUE(hasNeutralAtomOpType(move, NeutralAtomOpType::Move));
  EXPECT_FALSE(isAodOperation(move));
  EXPECT_NO_THROW(move.setGate(qc::OpType::None));
  EXPECT_THROW(move.setGate(qc::OpType::X), std::invalid_argument);
  EXPECT_EQ(move.getName(), "move");

  move.invert();
  EXPECT_EQ(move.getTargets(), (qc::Targets{1, 0}));
}

TEST(NAStandardOperation, Bridge) {
  const NAStandardOperation bridge(NeutralAtomOpType::Bridge, {0, 1, 2});
  const auto same = bridge.clone();
  const NAStandardOperation move(NeutralAtomOpType::Move, {0, 1, 2});

  EXPECT_TRUE(bridge == *same);
  EXPECT_FALSE(bridge == move);
  EXPECT_FALSE(bridge.commutesAtQubit(move, 0));
  EXPECT_TRUE(bridge.commutesAtQubit(*same, 0));
  const qc::Operation& bridgeBase = bridge;
  EXPECT_FALSE(bridgeBase.equals(move, {}, {}));

  const NAStandardOperation permutedBridge(NeutralAtomOpType::Bridge,
                                           {3, 4, 5});
  EXPECT_TRUE(bridgeBase.equals(permutedBridge,
                                qc::Permutation{{0, 3}, {1, 4}, {2, 5}}, {}));
  const auto inverse = bridge.getInverted();
  EXPECT_TRUE(bridge == *inverse);
  EXPECT_THROW(
      static_cast<void>(NAStandardOperation(NeutralAtomOpType::AodMove, {0})),
      std::invalid_argument);
}

TEST(NAStandardOperation, Qasm) {
  const NAStandardOperation bridge(NeutralAtomOpType::Bridge, {0, 1, 2});
  std::stringstream stream;
  const qc::QuantumRegister quantumRegister(0, 3, "q");
  qc::QubitIndexToRegisterMap qubitMap{};
  for (qc::Qubit qubit = 0; qubit < 3; ++qubit) {
    qubitMap.try_emplace(qubit, quantumRegister,
                         quantumRegister.toString(qubit));
  }

  bridge.dumpOpenQASM(stream, qubitMap, {}, 0, false);
  const NAStandardOperation move(NeutralAtomOpType::Move, {0, 2});
  move.dumpOpenQASM(stream, qubitMap, {}, 0, false);

  EXPECT_EQ(stream.str(), "bridge q[0], q[1], q[2];\nmove q[0], q[2];\n");
}

TEST(NAStandardOperation, Print) {
  const NAStandardOperation move(NeutralAtomOpType::Move, {0, 1});
  const qc::Operation& operation = move;
  std::stringstream stream;

  operation.print(stream, qc::Permutation{{0, 2}, {1, 1}}, 0, 3);

  EXPECT_EQ(stream.str(), "   |\033[0m\033[1m\033[36mmove\033[0m"
                          "\033[1m\033[36mmove\033[0m");
}

TEST(NAStandardOperation, Parsing) {
  constexpr std::array types{
      NeutralAtomOpType::None,          NeutralAtomOpType::Move,
      NeutralAtomOpType::Bridge,        NeutralAtomOpType::AodActivate,
      NeutralAtomOpType::AodDeactivate, NeutralAtomOpType::AodMove};
  for (const auto type : types) {
    EXPECT_EQ(neutralAtomOpTypeFromString(toString(type)), type);
  }
  EXPECT_THROW(static_cast<void>(neutralAtomOpTypeFromString("x")),
               std::invalid_argument);

  const qc::StandardOperation standard(0, qc::OpType::X);
  EXPECT_EQ(getNeutralAtomOpType(standard), std::nullopt);

  const NAStandardOperation unset;
  EXPECT_EQ(getNeutralAtomOpType(unset), std::nullopt);
}

TEST(NAStandardOperation, AodOperationsRemainDistinct) {
  const AodOperation first(NeutralAtomOpType::AodMove, {0},
                           {AodOperation::Dimension::X}, {0.}, {1.});
  const AodOperation same(NeutralAtomOpType::AodMove, {0},
                          {AodOperation::Dimension::X}, {0.}, {1.});
  const AodOperation different(NeutralAtomOpType::AodMove, {0},
                               {AodOperation::Dimension::X}, {0.}, {2.});

  EXPECT_TRUE(first == same);
  EXPECT_FALSE(first == different);
  const qc::Operation& firstBase = first;
  EXPECT_FALSE(firstBase.equals(different, {}, {}));
}

} // namespace na
