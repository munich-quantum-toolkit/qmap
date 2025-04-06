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

struct TestParams {
  std::string architecture;
  bool completeRemap;
  bool alsoMap;
};

class TestParametrizedHybridSynthesisMapper
    : public ::testing::TestWithParam<TestParams> {
protected:
  std::string testArchitecturePath = "architectures/";
  std::vector<qc::QuantumComputation> circuits;

  void SetUp() override {
    testArchitecturePath += GetParam().architecture + ".json";
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
  auto mapper = HybridSynthesisMapper(arch);
  mapper.initMapping(3);
  auto best = mapper.evaluateSynthesisSteps(circuits, GetParam().completeRemap,
                                            GetParam().alsoMap);
  EXPECT_EQ(best.size(), 2);
  EXPECT_GE(best[0], 0);
  EXPECT_GE(best[1], 0);
}

INSTANTIATE_TEST_SUITE_P(
    HybridSynthesisMapperTestSuite, TestParametrizedHybridSynthesisMapper,
    ::testing::Values(TestParams{"rubidium", false, false},
                      TestParams{"rubidium", true, false},
                      TestParams{"rubidium", false, true},
                      TestParams{"rubidium", true, true},
                      TestParams{"rubidium_hybrid", false, false},
                      TestParams{"rubidium_hybrid", true, false},
                      TestParams{"rubidium_hybrid", false, true},
                      TestParams{"rubidium_hybrid", true, true},
                      TestParams{"rubidium_shuttling", false, false},
                      TestParams{"rubidium_shuttling", true, false},
                      TestParams{"rubidium_shuttling", false, true},
                      TestParams{"rubidium_shuttling", true, true}));

class TestHybridSynthesisMapper : public ::testing::Test {
protected:
  NeutralAtomArchitecture arch =
      NeutralAtomArchitecture("architectures/rubidium.json");
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

TEST_F(TestHybridSynthesisMapper, MapAppend) {
  mapper.appendWithMapping(qc, true);
  auto synthesizedQc = mapper.getSynthesizedQc();
  EXPECT_EQ(synthesizedQc.getNqubits(), 3);
  EXPECT_GE(synthesizedQc.getNops(), 3);
}

} // namespace na
