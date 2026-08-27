/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "hybridmap/NeutralAtomLayer.hpp"
#include "ir/QuantumComputation.hpp"

#include <gtest/gtest.h>

namespace na {

TEST(NeutralAtomLayer, ConstructDAGIndexesOperationsByUsedQubit) {
  qc::QuantumComputation circuit(3);
  circuit.h(0);
  circuit.cx(0, 2);
  circuit.x(1);

  const auto dag = constructDAG(circuit);

  ASSERT_EQ(dag.size(), 3U);
  ASSERT_EQ(dag.at(0).size(), 2U);
  EXPECT_EQ(dag.at(0).at(0)->get(), circuit.at(0).get());
  EXPECT_EQ(dag.at(0).at(1)->get(), circuit.at(1).get());
  ASSERT_EQ(dag.at(1).size(), 1U);
  EXPECT_EQ(dag.at(1).front()->get(), circuit.at(2).get());
  ASSERT_EQ(dag.at(2).size(), 1U);
  EXPECT_EQ(dag.at(2).front()->get(), circuit.at(1).get());
}

} // namespace na
