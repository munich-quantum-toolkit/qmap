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
#include "ir/Definitions.hpp"
#include "ir/Register.hpp"
#include "ir/operations/OpType.hpp"

#include <gtest/gtest.h>
#include <sstream>
#include <stdexcept>

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
  const NeutralAtomOperation bridge(NeutralAtomOperationKind::Bridge,
                                    {0, 1, 2});
  std::stringstream stream;
  const qc::QuantumRegister quantumRegister(0, 3, "q");
  qc::QubitIndexToRegisterMap qubitMap{};
  for (qc::Qubit qubit = 0; qubit < 3; ++qubit) {
    qubitMap.try_emplace(qubit, quantumRegister,
                         quantumRegister.toString(qubit));
  }

  bridge.dumpOpenQASM(stream, qubitMap, {}, 0, false);

  EXPECT_EQ(stream.str(), "bridge q[0], q[1], q[2];\n");
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
