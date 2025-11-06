/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

//
// This file is part of the MQT QMAP library released under the MIT license.
// See README.md or go to https://github.com/cda-tum/qmap for more information.
//

#include "hybridmap/HybridSynthesisMapper.hpp"
#include "hybridmap/NeutralAtomArchitecture.hpp"
#include "ir/QuantumComputation.hpp"

#include <cstddef>
#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace na {
class TestParametrizedHybridSynthesisMapper
    : public ::testing::TestWithParam<std::string> {
protected:
  std::string testArchitecturePath = "architectures/";
  std::vector<qc::QuantumComputation> circuits;

  void SetUp() override {
    testArchitecturePath += GetParam() + ".json";
    qc::QuantumComputation qc1(3);
    qc1.x(0);
    qc1.cx(0, 1);
    qc1.cx(1, 2);
    circuits.push_back(qc1);

    qc::QuantumComputation qc2(3);
    qc2.move(0, 2);
    qc2.x(0);
    circuits.push_back(qc2);
  }

  // Test the HybridSynthesisMapper class
};

TEST_P(TestParametrizedHybridSynthesisMapper, AdjaencyMatrix) {
  auto arch = NeutralAtomArchitecture(testArchitecturePath);
  auto mapper = HybridSynthesisMapper(arch);
  mapper.initMapping(3);
  auto adjMatrix = mapper.getCircuitAdjacencyMatrix();
  EXPECT_EQ(adjMatrix.size(), 3);
  EXPECT_TRUE(adjMatrix(0, 2) == 0 || adjMatrix(0, 2) == 1);
}

TEST_P(TestParametrizedHybridSynthesisMapper, EvaluateSynthesisStep) {
  auto arch = NeutralAtomArchitecture(testArchitecturePath);
  auto params = MapperParameters();
  params.verbose = true;
  auto mapper = HybridSynthesisMapper(arch, params);
  mapper.initMapping(3);
  auto best = mapper.evaluateSynthesisSteps(circuits, true);
  EXPECT_EQ(best.size(), 2);
  EXPECT_GE(best[0], 0);
  EXPECT_GE(best[1], 0);
}

INSTANTIATE_TEST_SUITE_P(HybridSynthesisMapperTestSuite,
                         TestParametrizedHybridSynthesisMapper,
                         ::testing::Values("rubidium_gate", "rubidium_hybrid",
                                           "rubidium_shuttling"));

class TestHybridSynthesisMapper : public ::testing::Test {
protected:
  NeutralAtomArchitecture arch =
      NeutralAtomArchitecture("architectures/rubidium_gate.json");
  HybridSynthesisMapper mapper = HybridSynthesisMapper(arch);
  qc::QuantumComputation qc;

  void SetUp() override {
    qc = qc::QuantumComputation(3);
    qc.x(0);
    qc.cx(0, 1);
    qc.cx(1, 2);

    mapper.initMapping(3);
  }
};

TEST_F(TestHybridSynthesisMapper, DirectlyMap) {
  mapper.appendWithoutMapping(qc);
  auto synthesizedQc = mapper.getSynthesizedQc();
  EXPECT_EQ(synthesizedQc.getNqubits(), 3);
  EXPECT_EQ(synthesizedQc.getNops(), 3);
}

TEST_F(TestHybridSynthesisMapper, completelyRemap) {
  mapper.appendWithoutMapping(qc);
  mapper.appendWithoutMapping(qc);
  auto mappedQc = mapper.getMappedQc();
  EXPECT_EQ(mappedQc.getNqubitsWithoutAncillae(), arch.getNpositions());
  EXPECT_GE(mappedQc.getNops(), 3);

  mapper.completeRemap(InitialMapping::Identity);
  auto mappedQcRemapped = mapper.getMappedQc();
  EXPECT_EQ(mappedQcRemapped.getNqubitsWithoutAncillae(), arch.getNpositions());
  EXPECT_GE(mappedQcRemapped.getNops(), 3);
}

TEST_F(TestHybridSynthesisMapper, MapAppend) {
  mapper.appendWithMapping(qc);
  auto synthesizedQc = mapper.getSynthesizedQc();
  EXPECT_EQ(synthesizedQc.getNqubits(), 3);
  EXPECT_GE(synthesizedQc.getNops(), 3);
}

TEST_F(TestHybridSynthesisMapper, Output) {
  mapper.appendWithMapping(qc);
  const auto qasm = mapper.getSynthesizedQcQASM();
  EXPECT_FALSE(qasm.empty());
  mapper.saveSynthesizedQc("test_output.qasm");
}

} // namespace na
