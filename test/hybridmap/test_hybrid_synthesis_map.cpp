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
// See README.md or go to https://github.com/munich-quantum-toolkit/qmap for
// more information.
//

#include "hybridmap/HybridNeutralAtomMapper.hpp"
#include "hybridmap/HybridSynthesisMapper.hpp"
#include "hybridmap/NeutralAtomArchitecture.hpp"
#include "ir/QuantumComputation.hpp"
#include "na/ir/operations/NAStandardOperation.hpp"

#include <filesystem>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <vector>

namespace na {

namespace {

struct TestParams {
  std::string architecture;
  bool completeRemap;
  bool alsoMap;
};

class TestParametrizedHybridSynthesisMapper
    : public testing::TestWithParam<TestParams> {
protected:
  std::string testArchitecturePath = "architectures/";
  std::vector<qc::QuantumComputation> circuits;

  /**
   * @brief Prepare test fixtures for parametrized hybrid synthesis mapper
   * tests.
   *
   * Appends the architecture name from the test parameter plus ".json" to
   * testArchitecturePath and constructs two 3-qubit quantum circuits, which
   * are stored in the member `circuits`:
   * - Circuit 1: X on qubit 0; CX from qubit 0 to 1; CX from qubit 1 to 2.
   * - Circuit 2: Move qubit 0 to position 2; X on qubit 0.
   */
  void SetUp() override {
    testArchitecturePath += GetParam().architecture + ".json";
    qc::QuantumComputation qc1(3);
    qc1.x(0);
    qc1.cx(0, 1);
    qc1.cx(1, 2);
    circuits.push_back(qc1);

    qc::QuantumComputation qc2(3);
    qc2.emplace_back(makeMoveOperation(0, 2));
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
  const auto best = mapper.evaluateSynthesisSteps(
      circuits, GetParam().completeRemap, GetParam().alsoMap);
  EXPECT_EQ(best.size(), 2);
  EXPECT_GE(best[0], 0);
  EXPECT_GE(best[1], 0);
}

INSTANTIATE_TEST_SUITE_P(
    HybridSynthesisMapperTestSuite, TestParametrizedHybridSynthesisMapper,
    ::testing::Values(TestParams{"rubidium_gate", false, false},
                      TestParams{"rubidium_gate", true, false},
                      TestParams{"rubidium_gate", false, true},
                      TestParams{"rubidium_gate", true, true},
                      TestParams{"rubidium_hybrid", false, false},
                      TestParams{"rubidium_hybrid", true, false},
                      TestParams{"rubidium_hybrid", false, true},
                      TestParams{"rubidium_hybrid", true, true},
                      TestParams{"rubidium_shuttling", false, false},
                      TestParams{"rubidium_shuttling", true, false},
                      TestParams{"rubidium_shuttling", false, true},
                      TestParams{"rubidium_shuttling", true, true}));

class TestHybridSynthesisMapper : public testing::Test {
protected:
  NeutralAtomArchitecture arch =
      NeutralAtomArchitecture("architectures/rubidium_gate.json");
  HybridSynthesisMapper mapper = HybridSynthesisMapper(arch);
  qc::QuantumComputation qc;

  /**
   * @brief Prepare a 3-qubit test circuit and initialize the mapper's mapping
   * for three qubits.
   *
   * The circuit applies an X gate on qubit 0 followed by controlled-X gates
   * from qubit 0 to 1 and from qubit 1 to 2, and the mapper is initialized to a
   * mapping for three qubits.
   */
  void SetUp() override {
    qc = qc::QuantumComputation(3);
    qc.x(0);
    qc.cx(0, 1);
    qc.cx(1, 2);

    mapper.initMapping(3);
  }
};

TEST_F(TestHybridSynthesisMapper, MapAppend) {
  mapper.appendWithMapping(qc);
  const auto synthesizedQc = mapper.getSynthesizedQc();
  EXPECT_EQ(synthesizedQc.getNqubits(), 3);
  EXPECT_GE(synthesizedQc.getNops(), 3);
  mapper.completeRemap(false);
  mapper.completeRemap(true);
  const auto mappedQc = mapper.getMappedQc();
  EXPECT_GE(mappedQc.getNops(), synthesizedQc.getNops());
}

TEST_F(TestHybridSynthesisMapper, MappingAndSchedulingUseSeparateCircuits) {
  qc::QuantumComputation swap(3);
  swap.swap(0, 1);
  mapper.appendWithMapping(swap);
  const auto abstract = mapper.getMappedQcQasm();
  EXPECT_NE(mapper.getMappedQcAodQasm(), abstract);
  EXPECT_EQ(mapper.getMappedQcQasm(), abstract);
  EXPECT_EQ(mapper.schedule().nCZs, 3U);

  qc::QuantumComputation cz(3);
  cz.cz(0, 1);
  mapper.appendWithMapping(cz);
  EXPECT_EQ(mapper.schedule().nCZs, 4U);

  mapper.initMapping(3);
  EXPECT_EQ(mapper.getMappedQcAodQasm(), mapper.getMappedQcQasm());
}

TEST_F(TestHybridSynthesisMapper, BufferedRemapCommitsEachGateOnce) {
  HybridSynthesisMapper buffered(arch, {}, 1);
  qc::QuantumComputation gate(2);
  gate.cz(0, 1);
  buffered.appendWithMapping(gate);
  buffered.completeRemap(false);
  EXPECT_TRUE(buffered.getMappedQc().empty());
  buffered.completeRemap();
  EXPECT_EQ(buffered.schedule().nCZs, 1U);
  buffered.appendWithMapping(gate);
  NeutralAtomMapper& base = buffered;
  base.setParameters({});
  EXPECT_EQ(buffered.schedule().nCZs, 2U);
  EXPECT_EQ(buffered.getSynthesizedQc().size(), 2U);
  base.reset();
  EXPECT_EQ(buffered.schedule().nCZs, 0U);
  EXPECT_TRUE(buffered.getSynthesizedQc().empty());
}

TEST(HybridSynthesisRouting, CandidateFidelityMatchesForwardSchedule) {
  const NeutralAtomArchitecture arch("architectures/rubidium_shuttling.json");
  MapperParameters params;
  params.gateWeight = 0;
  params.usePassBy = false;

  qc::QuantumComputation prefix(4);
  prefix.cz(0, 1);
  std::vector<qc::QuantumComputation> candidates{qc::QuantumComputation(4)};
  candidates.front().x(0);
  candidates.front().cz(0, 3);
  for (const bool full : {false, true}) {
    HybridSynthesisMapper mapper(arch, params, 3);
    mapper.appendWithMapping(prefix);
    const auto scores = mapper.evaluateSynthesisSteps(candidates, full, true);
    EXPECT_DOUBLE_EQ(scores.front(), mapper.schedule().totalFidelities);
    EXPECT_EQ(mapper.getCircuitAdjacencyMatrix()(0, 3), 1U);
    if (full) {
      const auto next = mapper.evaluateSynthesisSteps(candidates, true, true);
      EXPECT_DOUBLE_EQ(next.front(), mapper.schedule().totalFidelities);
    }
  }
}

TEST_F(TestHybridSynthesisMapper, Output) {
  mapper.appendWithMapping(qc, true);
  const auto qasm = mapper.getSynthesizedQcQASM();
  EXPECT_FALSE(qasm.empty());
  const auto tempDir = std::filesystem::temp_directory_path();
  const auto qasmPath = tempDir / "test_output.qasm";
  mapper.saveSynthesizedQc(qasmPath.string());
  EXPECT_TRUE(std::filesystem::exists(qasmPath));
  EXPECT_GT(std::filesystem::file_size(qasmPath), 0);
  std::filesystem::remove(qasmPath);
}

} // namespace
} // namespace na
