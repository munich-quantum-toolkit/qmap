/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "datastructures/Layer.hpp"
#include "ir/Definitions.hpp"
#include "ir/QuantumComputation.hpp"
#include "ir/operations/OpType.hpp"
#include "ir/operations/StandardOperation.hpp"

#include <algorithm>
#include <gtest/gtest.h>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <unordered_set>
#include <vector>

namespace qc {
TEST(Layer, ExecutableSet1) {
  auto qc = QuantumComputation(3);
  /* construct the following circuit
┌─────────┐┌─────────┐┌──────────┐      ┌─────────┐┌─────────┐┌──────────┐
┤         ├┤ Rz(π/4) ├┤          ├─■──■─┤         ├┤ Rz(π/4) ├┤          ├───
│         │├─────────┤│          │ │  | │         │└─────────┘│          │
┤ Ry(π/2) ├┤ Rz(π/4) ├┤ Ry(-π/2) ├─■──┼─┤ Ry(π/2) ├───────────┤ Ry(-π/2) ├─■─
│         │├─────────┤│          │    │ │         │           │          │ │
┤         ├┤ Rz(π/4) ├┤          ├────■─┤         ├───────────┤          ├─■─
└─────────┘└─────────┘└──────────┘      └─────────┘           └──────────┘
    (1)        (2)        (3)     (4)(5)    (6)        (7)        (8)     (9)
  */
  qc.emplace_back<StandardOperation>(Targets{0, 1, 2}, RY, std::vector{PI_2});
  qc.rz(PI_4, 0);
  qc.rz(PI_4, 1);
  qc.rz(PI_4, 2);
  qc.emplace_back<StandardOperation>(Targets{0, 1, 2}, RY, std::vector{-PI_2});
  qc.cz(0, 1);
  qc.cz(0, 2);
  qc.emplace_back<StandardOperation>(Targets{0, 1, 2}, RY, std::vector{PI_2});
  qc.rz(PI_4, 0);
  qc.emplace_back<StandardOperation>(Targets{0, 1, 2}, RY, std::vector{-PI_2});
  qc.cz(1, 2);

  Layer const layer(qc);
  EXPECT_EQ(layer.getExecutableSet().size(), 1); // layer (1)
  std::shared_ptr<Layer::DAGVertex> v = *(layer.getExecutableSet()).begin();
  v->execute();
  EXPECT_THROW(v->execute(), std::logic_error);
  EXPECT_EQ(layer.getExecutableSet().size(), 3); // layer (2)
  v = *(layer.getExecutableSet()).begin();
  v->execute();
  EXPECT_EQ(layer.getExecutableSet().size(), 2); // rest of layer (2)
  v = *(layer.getExecutableSet()).begin();
  v->execute();
  EXPECT_EQ(layer.getExecutableSet().size(), 1); // rest of layer (2)
  v = *(layer.getExecutableSet()).begin();
  v->execute();
  EXPECT_EQ(layer.getExecutableSet().size(), 1); // layer (3)
  v = *(layer.getExecutableSet()).begin();
  v->execute();
  EXPECT_EQ(layer.getExecutableSet().size(), 3); // layer (4), (5), (9)
  // execute layer (4) and (5), first pick those two vertices and then execute
  // them because when executing the iterator over the executable set is not
  // valid anymore
  std::vector<std::shared_ptr<Layer::DAGVertex>> vList;
  for (const auto& u : layer.getExecutableSet()) {
    if (const auto& it = (u->getOperation())->getUsedQubits(); it.contains(0)) {
      vList.emplace_back(u);
    }
  }
  std::ranges::for_each(vList, [](const auto& u) { u->execute(); });
  EXPECT_EQ(layer.getExecutableSet().size(), 2); // layer (6), (9)
}

TEST(Layer, ExecutableSet2) {
  QuantumComputation qc{};
  qc = QuantumComputation(8);
  qc.cz(1, 2);
  qc.cz(1, 6);
  qc.cz(2, 7);
  qc.cz(3, 4);
  qc.cz(3, 5);
  qc.cz(4, 5);
  qc.cz(4, 6);
  qc.cz(4, 7);
  qc.cz(5, 7);
  qc.cz(6, 7);
  const Layer layer(qc);
  EXPECT_EQ(layer.getExecutableSet().size(), 10);
}

TEST(Layer, ExecutableSetNotCommutable) {
  QuantumComputation qc{};
  qc = QuantumComputation(8);
  qc.cz(1, 7);
  qc.cz(2, 4);
  qc.cz(5, 6);
  qc.cz(1, 5);
  qc.cz(6, 7);
  qc.cz(2, 3);
  qc.cz(1, 3);
  qc.cz(4, 6);
  qc.cz(2, 5);
  const Layer layer(qc, false);
  EXPECT_EQ(layer.getExecutableSet().size(), 3);
  std::unordered_set<const Operation*> actualSet{};
  for (const auto& u : layer.getExecutableSet()) {
    actualSet.emplace(u->getOperation());
  }
  std::unordered_set<const Operation*> expectedSet{};
  expectedSet.emplace(qc.at(0).get());
  expectedSet.emplace(qc.at(1).get());
  expectedSet.emplace(qc.at(2).get());
  EXPECT_EQ(actualSet, expectedSet);
}

TEST(Layer, InteractionGraph) {
  QuantumComputation qc{};
  qc = QuantumComputation(8);
  qc.cz(1, 2);
  qc.cz(1, 6);
  qc.cz(2, 7);
  qc.cz(3, 4);
  qc.cz(3, 5);
  qc.cz(4, 5);
  qc.cz(4, 6);
  qc.cz(4, 7);
  qc.cz(5, 7);
  qc.cz(6, 7);
  const qc::Layer layer(qc);
  const auto& graph = layer.constructInteractionGraph(Z, 1);
  EXPECT_FALSE(graph.isAdjacent(1, 1));
  EXPECT_TRUE(graph.isAdjacent(1, 2));
  EXPECT_FALSE(graph.isAdjacent(1, 3));
  EXPECT_FALSE(graph.isAdjacent(1, 4));
  EXPECT_FALSE(graph.isAdjacent(1, 5));
  EXPECT_TRUE(graph.isAdjacent(1, 6));
  EXPECT_FALSE(graph.isAdjacent(1, 7));
  EXPECT_TRUE(graph.isAdjacent(2, 1));
  EXPECT_FALSE(graph.isAdjacent(2, 2));
  EXPECT_FALSE(graph.isAdjacent(2, 3));
  EXPECT_FALSE(graph.isAdjacent(2, 4));
  EXPECT_FALSE(graph.isAdjacent(2, 5));
  EXPECT_FALSE(graph.isAdjacent(2, 6));
  EXPECT_TRUE(graph.isAdjacent(2, 7));
  EXPECT_FALSE(graph.isAdjacent(3, 1));
  EXPECT_FALSE(graph.isAdjacent(3, 2));
  EXPECT_FALSE(graph.isAdjacent(3, 3));
  EXPECT_TRUE(graph.isAdjacent(3, 4));
  EXPECT_TRUE(graph.isAdjacent(3, 5));
  EXPECT_FALSE(graph.isAdjacent(3, 6));
  EXPECT_FALSE(graph.isAdjacent(3, 7));
  EXPECT_FALSE(graph.isAdjacent(4, 1));
  EXPECT_FALSE(graph.isAdjacent(4, 2));
  EXPECT_TRUE(graph.isAdjacent(4, 3));
  EXPECT_FALSE(graph.isAdjacent(4, 4));
  EXPECT_TRUE(graph.isAdjacent(4, 5));
  EXPECT_TRUE(graph.isAdjacent(4, 6));
  EXPECT_TRUE(graph.isAdjacent(4, 7));
  EXPECT_FALSE(graph.isAdjacent(5, 1));
  EXPECT_FALSE(graph.isAdjacent(5, 2));
  EXPECT_TRUE(graph.isAdjacent(5, 3));
  EXPECT_TRUE(graph.isAdjacent(5, 4));
  EXPECT_FALSE(graph.isAdjacent(5, 5));
  EXPECT_FALSE(graph.isAdjacent(5, 6));
  EXPECT_TRUE(graph.isAdjacent(5, 7));
  EXPECT_TRUE(graph.isAdjacent(6, 1));
  EXPECT_FALSE(graph.isAdjacent(6, 2));
  EXPECT_FALSE(graph.isAdjacent(6, 3));
  EXPECT_TRUE(graph.isAdjacent(6, 4));
  EXPECT_FALSE(graph.isAdjacent(6, 5));
  EXPECT_FALSE(graph.isAdjacent(6, 6));
  EXPECT_TRUE(graph.isAdjacent(6, 7));
  EXPECT_FALSE(graph.isAdjacent(7, 1));
  EXPECT_TRUE(graph.isAdjacent(7, 2));
  EXPECT_FALSE(graph.isAdjacent(7, 3));
  EXPECT_TRUE(graph.isAdjacent(7, 4));
  EXPECT_TRUE(graph.isAdjacent(7, 5));
  EXPECT_TRUE(graph.isAdjacent(7, 6));
  EXPECT_FALSE(graph.isAdjacent(7, 7));

  EXPECT_ANY_THROW(std::ignore = layer.constructInteractionGraph(Z, 2));
  EXPECT_ANY_THROW(std::ignore = layer.constructInteractionGraph(RZZ, 0));
}

TEST(Layer, ExecutableOfType) {
  auto qc = QuantumComputation(3);
  qc.cz(0, 1);
  qc.cz(0, 2);
  qc.emplace_back<qc::StandardOperation>(Targets{0, 1, 2}, RY,
                                         std::vector{PI_2});
  qc.rz(qc::PI_4, 0);
  qc.emplace_back<StandardOperation>(Targets{0, 1, 2}, RY, std::vector{-PI_2});
  qc.cz(1, 2);

  Layer const layer(qc);
  EXPECT_EQ(layer.getExecutablesOfType(Z, 1).size(), 3);
  EXPECT_EQ(layer.getExecutablesOfType(RZ, 0).size(), 0);
}
} // namespace qc
