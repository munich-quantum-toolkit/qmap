/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
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
#include "hybridmap/NeutralAtomUtils.hpp"
#include "ir/QuantumComputation.hpp"

#include <filesystem>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <vector>

namespace na {
class TestParametrizedHybridSynthesisMapper
    : public testing::TestWithParam<std::string> {
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

TEST_P(TestParametrizedHybridSynthesisMapper, AdjacencyMatrix) {
  const auto arch = NeutralAtomArchitecture(testArchitecturePath);
  auto mapper = HybridSynthesisMapper(arch);
  mapper.initMapping(3);
  auto adjMatrix = mapper.getCircuitAdjacencyMatrix();
  EXPECT_EQ(adjMatrix.size(), 3);
  EXPECT_TRUE(adjMatrix(0, 2) == 0 || adjMatrix(0, 2) == 1);
}

TEST_P(TestParametrizedHybridSynthesisMapper, EvaluateSynthesisStep) {
  const auto arch = NeutralAtomArchitecture(testArchitecturePath);
  auto params = MapperParameters();
  params.verbose = true;
  auto mapper = HybridSynthesisMapper(arch, params);
  // Intentionally not initializing the mapper to test error handling
  EXPECT_THROW(static_cast<void>(mapper.getCircuitAdjacencyMatrix()),
               std::runtime_error);
  // Initializing with too many qubits to test error handling
  EXPECT_THROW(mapper.initMapping(50), std::runtime_error);
  const auto best = mapper.evaluateSynthesisSteps(circuits, true);
  EXPECT_EQ(best.size(), 2);
  EXPECT_GE(best[0], 0);
  EXPECT_GE(best[1], 0);
}

INSTANTIATE_TEST_SUITE_P(HybridSynthesisMapperTestSuite,
                         TestParametrizedHybridSynthesisMapper,
                         ::testing::Values("rubidium_gate", "rubidium_hybrid",
                                           "rubidium_shuttling"));

class TestHybridSynthesisMapper : public testing::Test {
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
  const auto synthesizedQc = mapper.getSynthesizedQc();
  EXPECT_EQ(synthesizedQc.getNqubits(), 3);
  EXPECT_EQ(synthesizedQc.getNops(), 3);
}

TEST_F(TestHybridSynthesisMapper, completelyRemap) {
  mapper.appendWithoutMapping(qc);
  mapper.appendWithoutMapping(qc);
  const auto mappedQc = mapper.getMappedQc();
  EXPECT_EQ(mappedQc.getNqubitsWithoutAncillae(), arch.getNpositions());
  EXPECT_GE(mappedQc.getNops(), 3);

  mapper.completeRemap(Identity);
  const auto mappedQcRemapped = mapper.getMappedQc();
  EXPECT_EQ(mappedQcRemapped.getNqubitsWithoutAncillae(), arch.getNpositions());
  EXPECT_GE(mappedQcRemapped.getNops(), 3);
}

TEST_F(TestHybridSynthesisMapper, MapAppend) {
  mapper.appendWithMapping(qc);
  const auto synthesizedQc = mapper.getSynthesizedQc();
  EXPECT_EQ(synthesizedQc.getNqubits(), 3);
  EXPECT_GE(synthesizedQc.getNops(), 3);
}

TEST_F(TestHybridSynthesisMapper, Output) {
  mapper.appendWithMapping(qc);
  const auto qasm = mapper.getSynthesizedQcQASM();
  EXPECT_FALSE(qasm.empty());
  const auto tempDir = std::filesystem::temp_directory_path();
  const auto qasmPath = tempDir / "test_output.qasm";
  mapper.saveSynthesizedQc(qasmPath.string());
  EXPECT_TRUE(std::filesystem::exists(qasmPath));
  EXPECT_GT(std::filesystem::file_size(qasmPath), 0);
  std::filesystem::remove(qasmPath);
}

} // namespace na
