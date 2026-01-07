/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "hybridmap/HybridNeutralAtomMapper.hpp"
#include "hybridmap/NeutralAtomArchitecture.hpp"
#include "hybridmap/NeutralAtomUtils.hpp"
#include "ir/Definitions.hpp"
#include "ir/QuantumComputation.hpp"
#include "qasm3/Importer.hpp"

#include <cstdint>
#include <filesystem>
#include <gtest/gtest.h>
#include <iostream>
#include <stdexcept>
#include <string>
#include <tuple>

class NeutralAtomArchitectureTest : public testing::TestWithParam<std::string> {
protected:
  std::string testArchitecturePath = "architectures/";

  void SetUp() override { testArchitecturePath += GetParam() + ".json"; }
};

TEST_P(NeutralAtomArchitectureTest, LoadArchitectures) {
  std::cout << "wd: " << std::filesystem::current_path() << '\n';
  const auto arch = na::NeutralAtomArchitecture(testArchitecturePath);

  // Test get properties
  EXPECT_LE(arch.getNqubits(), arch.getNpositions());
  EXPECT_EQ(arch.getNpositions(), arch.getNrows() * arch.getNcolumns());
  // Test precomputed values
  const auto c1 = arch.getCoordinate(0);
  const auto c2 = arch.getCoordinate(1);
  EXPECT_GE(arch.getSwapDistance(c1, c2), 0);
  EXPECT_GE(arch.getNAodIntermediateLevels(), 1);
  // Test get parameters
  EXPECT_GE(arch.getGateTime("cz"), 0);
  EXPECT_GE(arch.getGateAverageFidelity("cz"), 0);
  // Test distance functions
  EXPECT_GE(arch.getEuclideanDistance(c1, c2), 0);
  // Test MoveVector functions
  const auto mv = arch.getVector(0, 1);
  EXPECT_GE(arch.getVectorShuttlingTime(mv), 0);
}

INSTANTIATE_TEST_SUITE_P(NeutralAtomArchitectureTestSuite,
                         NeutralAtomArchitectureTest,
                         ::testing::Values("rubidium_gate", "rubidium_hybrid",
                                           "arch_minimal"));
class NeutralAtomMapperTestParams
    // parameters are architecture, circuit, gateWeight, shuttlingWeight,
    // lookAheadWeight, dynamicMappingWeight, initialCoordinateMapping
    : public testing::TestWithParam<
          std::tuple<std::string, std::string, qc::fp, qc::fp, qc::fp, qc::fp,
                     na::InitialCoordinateMapping>> {
protected:
  std::string testArchitecturePath = "architectures/";
  std::string testQcPath = "circuits/";
  qc::fp gateWeight = 1;
  qc::fp shuttlingWeight = 1;
  qc::fp lookAheadWeight = 1;
  qc::fp dynamicMappingWeight = 1;
  na::InitialCoordinateMapping initialCoordinateMapping =
      na::InitialCoordinateMapping::Random;
  // fixed
  qc::fp decay = 0.1;
  qc::fp shuttlingTimeWeight = 0.1;
  uint32_t seed = 42;

  void SetUp() override {
    const auto& params = GetParam();
    testArchitecturePath += std::get<0>(params) + ".json";
    testQcPath += std::get<1>(params) + ".qasm";
    gateWeight = std::get<2>(params);
    shuttlingWeight = std::get<3>(params);
    lookAheadWeight = std::get<4>(params);
    dynamicMappingWeight = std::get<5>(params);
    initialCoordinateMapping = std::get<6>(params);
  }
};

TEST_P(NeutralAtomMapperTestParams, MapCircuitsIdentity) {
  const auto arch = na::NeutralAtomArchitecture(testArchitecturePath);
  constexpr na::InitialMapping initialMapping = na::InitialMapping::Identity;
  na::NeutralAtomMapper mapper(arch);
  na::MapperParameters mapperParameters;
  mapperParameters.initialCoordMapping = initialCoordinateMapping;
  mapperParameters.lookaheadWeightSwaps = lookAheadWeight;
  mapperParameters.lookaheadWeightMoves = lookAheadWeight;
  mapperParameters.decay = decay;
  mapperParameters.shuttlingTimeWeight = shuttlingTimeWeight;
  mapperParameters.gateWeight = gateWeight;
  mapperParameters.shuttlingWeight = shuttlingWeight;
  mapperParameters.dynamicMappingWeight = dynamicMappingWeight;
  mapperParameters.seed = seed;
  mapperParameters.verbose = true;
  mapperParameters.maxBridgeDistance = 2;
  mapperParameters.numFlyingAncillas = 1;
  mapperParameters.usePassBy = false;
  mapperParameters.limitShuttlingLayer = 1;
  mapper.setParameters(mapperParameters);

  auto qc = qasm3::Importer::importf(testQcPath);
  const auto qcMapped = mapper.map(qc, initialMapping);
  ASSERT_GE(qcMapped.size(), qc.size());
  mapper.convertToAod();

  const auto scheduleResults = mapper.schedule(true, true);

  ASSERT_GT(scheduleResults.totalFidelities, 0);
  ASSERT_GT(scheduleResults.totalIdleTime, 0);
  ASSERT_GT(scheduleResults.totalExecutionTime, 0);
}

INSTANTIATE_TEST_SUITE_P(
    NeutralAtomMapperTestSuite, NeutralAtomMapperTestParams,
    ::testing::Combine(
        ::testing::Values("rubidium_gate", "rubidium_hybrid"),
        ::testing::Values("dj_nativegates_rigetti_qiskit_opt3_10", "modulo_2",
                          "multiply_2",
                          "qft_nativegates_rigetti_qiskit_opt3_10",
                          "random_nativegates_rigetti_qiskit_opt3_10"),
        ::testing::Values(1, 0.), ::testing::Values(1, 0.),
        ::testing::Values(0.1), ::testing::Values(0),
        ::testing::Values(na::InitialCoordinateMapping::Trivial)));

class NeutralAtomMapperTest : public testing::Test {
protected:
  std::string testArchitecturePath = "architectures/rubidium_shuttling.json";
  const na::NeutralAtomArchitecture arch =
      na::NeutralAtomArchitecture(testArchitecturePath);
  na::InitialMapping const initialMapping = na::InitialMapping::Graph;
  na::MapperParameters mapperParameters;
  na::NeutralAtomMapper mapper{arch, mapperParameters};
  qc::QuantumComputation qc;

  void SetUp() override {
    mapperParameters.initialCoordMapping =
        na::InitialCoordinateMapping::Trivial;
    mapperParameters.lookaheadDepth = 1;
    mapperParameters.lookaheadWeightSwaps = 0.1;
    mapperParameters.lookaheadWeightMoves = 0.5;
    mapperParameters.decay = 0;
    mapperParameters.shuttlingTimeWeight = 0.1;
    mapperParameters.gateWeight = 1;
    mapperParameters.shuttlingWeight = 0;
    mapperParameters.seed = 43;
    mapperParameters.verbose = false;
    mapperParameters.numFlyingAncillas = 1;
    mapperParameters.limitShuttlingLayer = 1;
    mapperParameters.usePassBy = true;
    mapper = na::NeutralAtomMapper(arch, mapperParameters);
    qc = qasm3::Importer::importf(
        "circuits/dj_nativegates_rigetti_qiskit_opt3_10.qasm");
  }
};

TEST_F(NeutralAtomMapperTest, Output) {
  auto qcMapped = mapper.map(qc, initialMapping);
  // write to file
  const auto tempDir = std::filesystem::temp_directory_path();
  const auto qasmPath = tempDir / "test.qasm";
  mapper.saveMappedQcQasm(qasmPath.string());
  const auto qcMappedFromFile = mapper.getMappedQcQasm();
  EXPECT_GT(qcMappedFromFile.size(), 0);
  EXPECT_TRUE(std::filesystem::exists(qasmPath));
  EXPECT_GT(std::filesystem::file_size(qasmPath), 0);
  std::filesystem::remove(qasmPath);

  const auto aodQasmPath = tempDir / "test_aod.qasm";
  mapper.saveMappedQcAodQasm(aodQasmPath.string());
  const auto qcMappedAod = mapper.getMappedQcAodQasm();
  EXPECT_GT(qcMappedAod.size(), 0);
  EXPECT_TRUE(std::filesystem::exists(aodQasmPath));
  EXPECT_GT(std::filesystem::file_size(aodQasmPath), 0);
  std::filesystem::remove(aodQasmPath);

  const auto mapperStats = mapper.getStats();
  EXPECT_GE(mapperStats.nSwaps + mapperStats.nBridges + mapperStats.nFAncillas +
                mapperStats.nMoves + mapperStats.nPassBy,
            0);
  const auto mapperStatsMap = mapper.getStatsMap();
  EXPECT_GE(mapperStatsMap.at("nSwaps") + mapperStatsMap.at("nBridges") +
                mapperStatsMap.at("nFAncillas") + mapperStatsMap.at("nMoves") +
                mapperStatsMap.at("nPassBy"),
            0);
  const auto initHwPos = mapper.getInitHwPos();
  EXPECT_EQ(initHwPos.size(), arch.getNqubits() - 1 /* flying ancilla */);

  const auto scheduleResults = mapper.schedule(true, true);
  const auto animationViz = mapper.getAnimationViz();
  EXPECT_GT(animationViz.size(), 0);
  const auto animationPath = tempDir / "test";
  mapper.saveAnimationFiles(animationPath.string());
  const auto machinePath = animationPath.string() + ".namachine";
  const auto vizPath = animationPath.string() + ".naviz";
  EXPECT_TRUE(std::filesystem::exists(machinePath));
  EXPECT_GT(std::filesystem::file_size(machinePath), 0);
  std::filesystem::remove(machinePath);
  EXPECT_TRUE(std::filesystem::exists(vizPath));
  EXPECT_GT(std::filesystem::file_size(vizPath), 0);
  std::filesystem::remove(vizPath);

  std::cout << scheduleResults.toCsv();

  ASSERT_GT(scheduleResults.totalFidelities, 0);
}

// Exception tests for HybridNeutralAtomMapper

TEST(NeutralAtomMapperExceptions, NotEnoughQubitsForCircuitAndAncillas) {
  const auto arch =
      na::NeutralAtomArchitecture("architectures/rubidium_hybrid.json");
  na::MapperParameters p;
  p.initialCoordMapping = na::InitialCoordinateMapping::Trivial;
  p.shuttlingWeight = 0;   // avoid free-coords check interference
  p.numFlyingAncillas = 1; // allowed by ctor
  na::NeutralAtomMapper mapper(arch, p);

  // Circuit uses exactly all hardware qubits; +1 ancilla should trigger
  qc::QuantumComputation qc1((arch.getNqubits()));
  EXPECT_THROW((void)mapper.map(qc1, na::InitialMapping::Identity),
               std::runtime_error);

  // Circuit bigger than architecture should throw
  qc::QuantumComputation qc2(arch.getNqubits() + 1);
  EXPECT_THROW((void)mapper.map(qc2, na::InitialMapping::Identity),
               std::runtime_error);
}

// for now, only one flying ancilla is supported
TEST(NeutralAtomMapperExceptions, OnlyOneFlyingAncillaSupported) {
  const auto arch =
      na::NeutralAtomArchitecture("architectures/rubidium_hybrid.json");
  na::MapperParameters p;
  p.initialCoordMapping = na::InitialCoordinateMapping::Trivial;
  p.shuttlingWeight = 0;   // avoid free-coords check interference
  p.numFlyingAncillas = 2; // should be rejected
  EXPECT_THROW((void)na::NeutralAtomMapper(arch, p), std::runtime_error);
}

TEST(NeutralAtomMapperExceptions, NoFreeCoordsForShuttlingConstructor) {
  // Create minimal arch JSON: 1x1 positions, nQubits = 1 => no free coords
  const auto arch =
      na::NeutralAtomArchitecture("architectures/arch_minimal.json");

  na::MapperParameters p;
  p.initialCoordMapping = na::InitialCoordinateMapping::Trivial;
  p.shuttlingWeight = 1.0; // triggers constructor check
  EXPECT_THROW((void)na::NeutralAtomMapper(arch, p), std::runtime_error);

  na::MapperParameters p1 = p;
  p1.shuttlingWeight = 0.0; // construct ok
  na::NeutralAtomMapper mapper(arch, p1);
  p1.shuttlingWeight = 0.5; // triggers setParameters check
  EXPECT_THROW(mapper.setParameters(p1), std::runtime_error);
}
TEST(NeutralAtomMapperExceptions, NoMultiQubitSpace) {
  // Test that mapping throws when multi-qubit gates cannot be executed
  const auto arch =
      na::NeutralAtomArchitecture("architectures/rubidium_gate.json");
  constexpr na::MapperParameters p;
  na::NeutralAtomMapper mapper(arch, p);
  qc::QuantumComputation qc =
      qasm3::Importer::importf("circuits/multi_qubit.qasm");
  EXPECT_THROW(static_cast<void>(mapper.map(qc, na::InitialMapping::Identity)),
               std::runtime_error);
}

TEST(NeutralAtomMapperExceptions, LongShuttling) {
  const auto arch =
      na::NeutralAtomArchitecture("architectures/rubidium_shuttling.json");
  na::MapperParameters p;
  p.gateWeight = 0.0;
  p.verbose = true;
  na::NeutralAtomMapper mapper(arch, p);
  qc::QuantumComputation qc =
      qasm3::Importer::importf("circuits/long_random.qasm");
  const auto circ = mapper.map(qc, na::InitialMapping::Graph);
  mapper.convertToAod();
}
