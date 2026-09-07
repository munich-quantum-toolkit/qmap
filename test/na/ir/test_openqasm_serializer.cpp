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
#include "ir/QuantumComputation.hpp"
#include "ir/operations/CompoundOperation.hpp"
#include "ir/operations/OpType.hpp"
#include "ir/operations/StandardOperation.hpp"
#include "na/ir/OpenQASMSerializer.hpp"
#include "na/ir/operations/AodOperation.hpp"
#include "na/ir/operations/NAOpType.hpp"
#include "na/ir/operations/NAStandardOperation.hpp"

#include <gtest/gtest.h>
#include <memory>
#include <sstream>
#include <utility>
#include <vector>

namespace na {
TEST(OpenQASMSerializer, Qasm) {
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

TEST(OpenQASMSerializer, ConditionalQasm) {
  qc::QuantumComputation computation(3);
  const auto& controlRegister = computation.addClassicalRegister(1);
  auto thenOperation = std::make_unique<qc::CompoundOperation>();
  thenOperation->emplace_back(makeBridgeOperation({0, 1, 2}));
  thenOperation->emplace_back<qc::StandardOperation>(1, qc::H);
  computation.ifElse(
      std::move(thenOperation),
      std::make_unique<AodOperation>(NAOpType::AodMove, qc::Targets{0},
                                     std::vector{AodOperation::Dimension::X},
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

TEST(OpenQASMSerializer, ConditionalOnBitQasm) {
  qc::QuantumComputation computation(3, 1);
  computation.ifElse(
      makeBridgeOperation({0, 1, 2}),
      std::make_unique<AodOperation>(NAOpType::AodMove, qc::Targets{0},
                                     std::vector{AodOperation::Dimension::X},
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

TEST(OpenQASMSerializer, MoveAndAod) {
  qc::QuantumComputation computation(2);
  computation.emplace_back(makeMoveOperation(0, 1));
  computation.emplace_back<AodOperation>(
      NAOpType::AodMove, qc::Targets{0, 1},
      std::vector{AodOperation::Dimension::X, AodOperation::Dimension::Y},
      std::vector{0., 1.}, std::vector{1., 3.});
  std::stringstream stream;
  serializeOpenQASM(computation, stream);

  EXPECT_EQ(stream.str(), "// i 0 1\n"
                          "// o 0 1\n"
                          "OPENQASM 2.0;\n"
                          "include \"qelib1.inc\";\n"
                          "qreg q[2];\n"
                          "move q[0], q[1];\n"
                          "aod_move (0, 0, 1; 1, 1, 3) q[0], q[1];\n");
}

} // namespace na
