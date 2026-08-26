/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "circuit_optimizer/CircuitOptimizer.hpp"
#include "datastructures/CircuitOptimizations.hpp"
#include "ir/Definitions.hpp"
#include "ir/QuantumComputation.hpp"
#include "ir/operations/CompoundOperation.hpp"
#include "ir/operations/Control.hpp"
#include "ir/operations/OpType.hpp"

#include <cstddef>
#include <gtest/gtest.h>

namespace qmap {

TEST(DecomposeSWAP, UndirectedArchitecture) {
  qc::QuantumComputation circuit(2, 2);
  circuit.swap(0, 1);

  decomposeSWAP(circuit, false);

  ASSERT_EQ(circuit.size(), 3U);
  auto it = circuit.begin();
  EXPECT_EQ((*it)->getType(), qc::X);
  EXPECT_EQ((*it)->getControls().begin()->qubit, 0);
  EXPECT_EQ((*it)->getTargets().at(0), 1);
  ++it;
  EXPECT_EQ((*it)->getType(), qc::X);
  EXPECT_EQ((*it)->getControls().begin()->qubit, 1);
  EXPECT_EQ((*it)->getTargets().at(0), 0);
  ++it;
  EXPECT_EQ((*it)->getType(), qc::X);
  EXPECT_EQ((*it)->getControls().begin()->qubit, 0);
  EXPECT_EQ((*it)->getTargets().at(0), 1);
}

TEST(DecomposeSWAP, DirectedArchitecture) {
  qc::QuantumComputation circuit(2);
  circuit.swap(0, 1);

  decomposeSWAP(circuit, true);

  ASSERT_EQ(circuit.size(), 7U);
  auto it = circuit.begin();
  EXPECT_EQ((*it)->getType(), qc::X);
  EXPECT_EQ((*it)->getControls().begin()->qubit, 0);
  EXPECT_EQ((*it)->getTargets().at(0), 1);

  ++it;
  EXPECT_EQ((*it)->getType(), qc::H);
  EXPECT_EQ((*it)->getTargets().at(0), 1);
  ++it;
  EXPECT_EQ((*it)->getType(), qc::H);
  EXPECT_EQ((*it)->getTargets().at(0), 0);
  ++it;
  EXPECT_EQ((*it)->getType(), qc::X);
  EXPECT_EQ((*it)->getControls().begin()->qubit, 0);
  EXPECT_EQ((*it)->getTargets().at(0), 1);
  ++it;
  EXPECT_EQ((*it)->getType(), qc::H);
  EXPECT_EQ((*it)->getTargets().at(0), 1);
  ++it;
  EXPECT_EQ((*it)->getType(), qc::H);
  EXPECT_EQ((*it)->getTargets().at(0), 0);
  ++it;
  EXPECT_EQ((*it)->getType(), qc::X);
  EXPECT_EQ((*it)->getControls().begin()->qubit, 0);
  EXPECT_EQ((*it)->getTargets().at(0), 1);
}

TEST(DecomposeSWAP, CompoundOperation) {
  qc::QuantumComputation operation(2);
  operation.swap(0, 1);
  operation.swap(0, 1);
  operation.swap(0, 1);
  qc::QuantumComputation circuit(2);
  circuit.emplace_back(operation.asOperation());

  decomposeSWAP(circuit, false);

  ASSERT_TRUE(circuit.front()->isCompoundOperation());
  const auto* compound =
      dynamic_cast<const qc::CompoundOperation*>(circuit.front().get());
  ASSERT_NE(compound, nullptr);
  EXPECT_EQ(compound->size(), 9U);
}

TEST(DecomposeSWAP, DirectedCompoundOperation) {
  qc::QuantumComputation operation(2);
  operation.swap(0, 1);
  operation.swap(0, 1);
  operation.swap(0, 1);
  qc::QuantumComputation circuit(2);
  circuit.emplace_back(operation.asOperation());

  decomposeSWAP(circuit, true);

  ASSERT_TRUE(circuit.front()->isCompoundOperation());
  const auto* compound =
      dynamic_cast<const qc::CompoundOperation*>(circuit.front().get());
  ASSERT_NE(compound, nullptr);
  EXPECT_EQ(compound->size(), 21U);
}

TEST(CancelCNOTs, IdenticalCNOTs) {
  qc::QuantumComputation circuit(2);
  circuit.cx(1, 0);
  circuit.cx(1, 0);

  cancelCNOTs(circuit);

  EXPECT_TRUE(circuit.empty());
}

TEST(CancelCNOTs, IdenticalSWAPs) {
  qc::QuantumComputation circuit(2);
  circuit.swap(0, 1);
  circuit.swap(1, 0);

  cancelCNOTs(circuit);

  EXPECT_TRUE(circuit.empty());
}

TEST(CancelCNOTs, SWAPFollowedByCNOT) {
  qc::QuantumComputation circuit(2);
  circuit.swap(0, 1);
  circuit.cx(1, 0);

  cancelCNOTs(circuit);

  ASSERT_EQ(circuit.size(), 2U);
  EXPECT_EQ(circuit.front()->getType(), qc::X);
  EXPECT_EQ(circuit.front()->getTargets().front(), 0U);
  EXPECT_EQ(circuit.front()->getControls().begin()->qubit, 1U);
  EXPECT_EQ(circuit.back()->getType(), qc::X);
  EXPECT_EQ(circuit.back()->getTargets().front(), 1U);
  EXPECT_EQ(circuit.back()->getControls().begin()->qubit, 0U);
}

TEST(CancelCNOTs, CNOTFollowedBySWAP) {
  qc::QuantumComputation circuit(2);
  circuit.cx(1, 0);
  circuit.swap(0, 1);

  cancelCNOTs(circuit);

  ASSERT_EQ(circuit.size(), 2U);
  EXPECT_EQ(circuit.front()->getType(), qc::X);
  EXPECT_EQ(circuit.front()->getTargets().front(), 1U);
  EXPECT_EQ(circuit.front()->getControls().begin()->qubit, 0U);
  EXPECT_EQ(circuit.back()->getType(), qc::X);
  EXPECT_EQ(circuit.back()->getTargets().front(), 0U);
  EXPECT_EQ(circuit.back()->getControls().begin()->qubit, 1U);
}

TEST(CancelCNOTs, ThreeCNOTsBecomeSWAP) {
  qc::QuantumComputation circuit(2);
  circuit.cx(1, 0);
  circuit.cx(0, 1);
  circuit.cx(1, 0);

  cancelCNOTs(circuit);

  ASSERT_EQ(circuit.size(), 1U);
  EXPECT_EQ(circuit.front()->getType(), qc::SWAP);
  EXPECT_EQ(circuit.front()->getTargets().front(), 0U);
  EXPECT_EQ(circuit.front()->getTargets().back(), 1U);
}

TEST(ReplaceMCXWithMCZ, CX) {
  qc::QuantumComputation circuit(2U);
  circuit.cx(0, 1);

  replaceMCXWithMCZ(circuit);

  ASSERT_EQ(circuit.getNops(), 3U);
  EXPECT_EQ(circuit.at(0)->getType(), qc::H);
  EXPECT_EQ(circuit.at(0)->getTargets()[0], 1U);
  EXPECT_EQ(circuit.at(1)->getType(), qc::Z);
  EXPECT_EQ(circuit.at(1)->getTargets()[0], 1U);
  EXPECT_EQ(*circuit.at(1)->getControls().begin(), 0U);
  EXPECT_EQ(circuit.at(2)->getType(), qc::H);
  EXPECT_EQ(circuit.at(2)->getTargets()[0], 1U);
}

TEST(ReplaceMCXWithMCZ, CCX) {
  constexpr std::size_t nqubits = 3U;
  const qc::Controls controls = {0, 1};
  constexpr qc::Qubit target = 2U;
  qc::QuantumComputation circuit(nqubits);
  circuit.mcx(controls, target);

  replaceMCXWithMCZ(circuit);

  ASSERT_EQ(circuit.getNops(), 3U);
  EXPECT_EQ(circuit.at(0)->getType(), qc::H);
  EXPECT_EQ(circuit.at(0)->getTargets()[0], target);
  EXPECT_EQ(circuit.at(1)->getType(), qc::Z);
  EXPECT_EQ(circuit.at(1)->getTargets()[0], target);
  EXPECT_EQ(circuit.at(1)->getControls(), controls);
  EXPECT_EQ(circuit.at(2)->getType(), qc::H);
  EXPECT_EQ(circuit.at(2)->getTargets()[0], target);
}

TEST(ReplaceMCXWithMCZ, CompoundOperation) {
  qc::QuantumComputation operation(2U);
  operation.cx(0, 1);
  qc::QuantumComputation circuit(2U);
  circuit.emplace_back(operation.asCompoundOperation());

  replaceMCXWithMCZ(circuit);
  qc::CircuitOptimizer::flattenOperations(circuit);

  ASSERT_EQ(circuit.getNops(), 3U);
  EXPECT_EQ(circuit.at(0)->getType(), qc::H);
  EXPECT_EQ(circuit.at(1)->getType(), qc::Z);
  EXPECT_EQ(circuit.at(2)->getType(), qc::H);
}

TEST(ReplaceMCXWithMCZ, ToffoliSequenceSimplification) {
  constexpr std::size_t nqubits = 3U;
  const qc::Controls controls = {0, 1};
  constexpr qc::Qubit target = 2U;
  qc::QuantumComputation circuit(nqubits);
  circuit.cx(0, target);
  circuit.mcx(controls, target);

  replaceMCXWithMCZ(circuit);
  qc::CircuitOptimizer::singleQubitGateFusion(circuit);
  qc::CircuitOptimizer::flattenOperations(circuit);

  qc::QuantumComputation reference(nqubits);
  reference.h(target);
  reference.cz(0, target);
  reference.mcz(controls, target);
  reference.h(target);

  ASSERT_EQ(circuit.getNops(), reference.getNops());
  for (std::size_t i = 0; i < reference.getNops(); ++i) {
    EXPECT_EQ(circuit.at(i)->getType(), reference.at(i)->getType());
    EXPECT_EQ(circuit.at(i)->getTargets(), reference.at(i)->getTargets());
    EXPECT_EQ(circuit.at(i)->getControls(), reference.at(i)->getControls());
  }
}

} // namespace qmap
