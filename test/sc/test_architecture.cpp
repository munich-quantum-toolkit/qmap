/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "ir/operations/OpType.hpp"
#include "sc/Architecture.hpp"
#include "sc/utils.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <gtest/gtest.h>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
::testing::AssertionResult matrixNear(const Matrix& a, const Matrix& b,
                                      double delta) {
  if (a.size() != b.size()) {
    return ::testing::AssertionFailure()
           << "Matrices differ in size: " << a.size() << " != " << b.size();
  }
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (a.at(i).size() != b.at(i).size()) {
      return ::testing::AssertionFailure()
             << "Matrices differ in size in row " << i << ": " << a.at(i).size()
             << " != " << b.at(i).size();
    }
    for (std::size_t j = 0; j < a.at(i).size(); ++j) {
      if (std::abs(a.at(i).at(j) - b.at(i).at(j)) > delta) {
        return ::testing::AssertionFailure()
               << "Matrix entries in [" << i << "," << j
               << "] differ by more than " << delta << ": " << a.at(i).at(j)
               << " !~ " << b.at(i).at(j);
      }
    }
  }
  return ::testing::AssertionSuccess();
}
} // namespace

class TestArchitecture : public testing::TestWithParam<std::string> {
protected:
  std::string testArchitectureDir = "../../extern/architectures/";
  std::string testCalibrationDir = "../../extern/calibration/";
};

INSTANTIATE_TEST_SUITE_P(Architecture, TestArchitecture,
                         testing::Values("ibm_qx4.arch", "ibmq_casablanca.arch",
                                         "ibmq_london.arch",
                                         "ibmq_london.csv"));

TEST_P(TestArchitecture, QubitMap) {
  const auto& archName = GetParam();
  Architecture arch{};
  std::stringstream ss{};
  if (archName.find(".arch") != std::string::npos) {
    ss << testArchitectureDir << archName;
    arch.loadCouplingMap(ss.str());
  } else {
    ss << testCalibrationDir << archName;
    arch.loadProperties(ss.str());
  }

  EXPECT_EQ(Architecture::getQubitList(arch.getCouplingMap()).size(),
            arch.getNqubits());
}
TEST_P(TestArchitecture, GetAllConnectedSubsets) {
  const auto& archName = GetParam();
  Architecture arch{};
  std::stringstream ss{};
  if (archName.find(".arch") != std::string::npos) {
    ss << testArchitectureDir << archName;
    arch.loadCouplingMap(ss.str());
  } else {
    ss << testCalibrationDir << archName;
    arch.loadProperties(ss.str());
  }

  EXPECT_EQ(arch.getAllConnectedSubsets(arch.getNqubits()).size(), 1);
  EXPECT_EQ(arch.getAllConnectedSubsets(1).size(), arch.getNqubits());
}
TEST_P(TestArchitecture, GetHighestFidelity) {
  const auto& archName = GetParam();
  Architecture arch{};
  std::stringstream ss{};
  if (archName.find(".arch") != std::string::npos) {
    ss << testArchitectureDir << archName;
    arch.loadCouplingMap(ss.str());
  } else {
    ss << testCalibrationDir << archName;
    arch.loadProperties(ss.str());
  }
  CouplingMap cm{};

  arch.getHighestFidelityCouplingMap(arch.getNqubits(), cm);
  EXPECT_EQ(cm, arch.getCouplingMap());

  arch.getHighestFidelityCouplingMap(1U, cm);
  EXPECT_TRUE(cm.empty());
}
TEST_P(TestArchitecture, ReducedMaps) {
  const auto& archName = GetParam();
  Architecture arch{};
  std::stringstream ss{};
  if (archName.find(".arch") != std::string::npos) {
    ss << testArchitectureDir << archName;
    arch.loadCouplingMap(ss.str());
  } else {
    ss << testCalibrationDir << archName;
    arch.loadProperties(ss.str());
  }

  std::vector<CouplingMap> cms;

  arch.getReducedCouplingMaps(1, cms);

  EXPECT_EQ(cms.size(), arch.getNqubits());
}

TEST(TestArchitecture, ConnectedTest) {
  Architecture architecture{};
  CouplingMap cm{};

  cm.emplace(std::make_pair(0, 1));
  cm.emplace(std::make_pair(1, 2));
  cm.emplace(std::make_pair(2, 3));
  cm.emplace(std::make_pair(3, 4));
  cm.emplace(std::make_pair(4, 0));

  Architecture::printCouplingMap(cm, std::cout);

  architecture.loadCouplingMap(5, cm);

  std::vector<CouplingMap> cms;

  architecture.getReducedCouplingMaps(2, cms);

  EXPECT_EQ(cms.size(), 5);

  architecture.getReducedCouplingMaps(4, cms);

  EXPECT_EQ(cms.size(), 5);
}

TEST(TestArchitecture, FidelityTest) {
  Architecture architecture{};
  CouplingMap cm{};

  auto props = Architecture::Properties();
  props.setNqubits(4);
  props.setSingleQubitErrorRate(0, "x", 0.9);
  props.setSingleQubitErrorRate(1, "x", 0.9);
  props.setSingleQubitErrorRate(2, "x", 0.9);
  props.setSingleQubitErrorRate(3, "x", 0.9);

  props.setTwoQubitErrorRate(0, 1, 0.8);
  props.setTwoQubitErrorRate(1, 0, 0.8);
  props.setTwoQubitErrorRate(1, 2, 0.7);
  props.setTwoQubitErrorRate(2, 1, 0.7);
  props.setTwoQubitErrorRate(2, 3, 0.6);
  props.setTwoQubitErrorRate(3, 2, 0.6);

  architecture.loadProperties(props);
  architecture.getHighestFidelityCouplingMap(2, cm);

  const std::vector<std::uint16_t> highestFidelity{2, 3};
  auto qubitList = Architecture::getQubitList(cm);

  EXPECT_EQ(qubitList, highestFidelity);
}

TEST(TestArchitecture, FullyConnectedTest) {
  const auto cm = getFullyConnectedMap(3);

  ASSERT_TRUE(cm.size() == static_cast<std::size_t>(2 * 3));
}

TEST(TestArchitecture, MinimumNumberOfSwapsError) {
  Architecture architecture{};
  std::vector<std::uint16_t> permutation{1, 1, 2, 3, 4};
  printPi(permutation);
  std::vector<Edge> swaps{};
  EXPECT_THROW(architecture.minimumNumberOfSwaps(permutation, swaps),
               std::runtime_error);
}

TEST(TestArchitecture, TestCouplingLimitRing) {
  Architecture architecture{};
  const CouplingMap cm = {{0, 1}, {1, 0}, {1, 2}, {2, 1}, {2, 3},
                          {3, 2}, {3, 4}, {4, 3}, {4, 0}, {0, 4}};
  architecture.loadCouplingMap(5, cm);
  EXPECT_EQ(architecture.getCouplingLimit(), 2);
}

TEST(TestArchitecture, opTypeFromString) {
  Architecture arch{2, {{0, 1}}};
  auto& props = arch.getProperties();

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<> dis(0., 1.);

  const std::vector<std::pair<std::string, qc::OpType>> singleQubitGates = {
      {"i", qc::OpType::I},
      {"x", qc::OpType::X},
      {"y", qc::OpType::Y},
      {"z", qc::OpType::Z},
      {"sx", qc::OpType::SX},
      {"sxdg", qc::OpType::SXdg},
      {"h", qc::OpType::H},
      {"s", qc::OpType::S},
      {"sdg", qc::OpType::Sdg},
      {"t", qc::OpType::T},
      {"tdg", qc::OpType::Tdg},
      {"rx", qc::OpType::RX},
      {"ry", qc::OpType::RY},
      {"rz", qc::OpType::RZ},
      {"u1", qc::OpType::P},
      {"u2", qc::OpType::U2},
      {"u3", qc::OpType::U},
      {"reset", qc::OpType::Reset},
      {"measure", qc::OpType::Measure}};

  for (const auto& [opName, opType] : singleQubitGates) {
    const auto errorRate = dis(gen);

    props.setSingleQubitErrorRate(0, opName, errorRate);
    EXPECT_EQ(props.getSingleQubitErrorRate(0, opName), errorRate);
  }

  const std::vector<std::pair<std::string, qc::OpType>> twoQubitGates = {
      {"cx", qc::OpType::X},
      {"cz", qc::OpType::Z},
      {"cy", qc::OpType::Y},
      {"ch", qc::OpType::H},
      {"swap", qc::OpType::SWAP},
      {"crx", qc::OpType::RX},
      {"ry", qc::OpType::RY},
      {"crz", qc::OpType::RZ},
      {"cu1", qc::OpType::P},
      {"cu2", qc::OpType::U2},
      {"cu3", qc::OpType::U},
      {"iswap", qc::OpType::iSWAP},
      {"ecr", qc::OpType::ECR},
      {"dcx", qc::OpType::DCX},
      {"rxx", qc::OpType::RXX},
      {"rzz", qc::OpType::RZZ},
      {"ryy", qc::OpType::RYY},
      {"rzx", qc::OpType::RZX},
      {"xx_minus_yy", qc::OpType::XXminusYY},
      {"xx_plus_yy", qc::OpType::XXplusYY}};

  for (const auto& [opName, opType] : twoQubitGates) {
    const auto errorRate = dis(gen);

    props.setTwoQubitErrorRate(0, 1, errorRate, opName);
    EXPECT_EQ(props.getTwoQubitErrorRate(0, 1, opName), errorRate);
  }
}

TEST(TestArchitecture, FidelityDistanceBidirectionalTest) {
  /*
                          6 [0.03]
                          |
                        [0.9]
                          |
       [0.03] 4           5 [0.02]
              |           |
            [0.1]       [0.5]
              |           |
  0  -[0.9]-  1  -[0.5]-  2  -[0.1]-  3

[0.03]      [0.03]      [0.02]      [0.03]

  -[]- ... 2-qubit error rates
  []   ... 1-qubit error rates
  */
  Architecture architecture{};
  const CouplingMap cm = {{0, 1}, {1, 0}, {1, 2}, {2, 1}, {2, 3}, {3, 2},
                          {1, 4}, {4, 1}, {2, 5}, {5, 2}, {5, 6}, {6, 5}};
  architecture.loadCouplingMap(7, cm);

  auto props = Architecture::Properties();
  props.setSingleQubitErrorRate(0, "x", 0.03);
  props.setSingleQubitErrorRate(1, "x", 0.03);
  props.setSingleQubitErrorRate(2, "x", 0.02);
  props.setSingleQubitErrorRate(3, "x", 0.03);
  props.setSingleQubitErrorRate(4, "x", 0.03);
  props.setSingleQubitErrorRate(5, "x", 0.02);
  props.setSingleQubitErrorRate(6, "x", 0.03);

  props.setTwoQubitErrorRate(0, 1, 0.9);
  props.setTwoQubitErrorRate(1, 0, 0.9);
  props.setTwoQubitErrorRate(1, 2, 0.5);
  props.setTwoQubitErrorRate(2, 1, 0.5);
  props.setTwoQubitErrorRate(2, 3, 0.1);
  props.setTwoQubitErrorRate(3, 2, 0.1);
  props.setTwoQubitErrorRate(1, 4, 0.1);
  props.setTwoQubitErrorRate(4, 1, 0.1);
  props.setTwoQubitErrorRate(2, 5, 0.5);
  props.setTwoQubitErrorRate(5, 2, 0.5);
  props.setTwoQubitErrorRate(5, 6, 0.9);
  props.setTwoQubitErrorRate(6, 5, 0.9);

  architecture.loadProperties(props);

  const Matrix targetTable = {
      {// distance from 0 to i
       0., -3 * std::log2(1 - 0.9),
       -3 * (std::log2(1 - 0.9) + std::log2(1 - 0.5)),
       -3 * (std::log2(1 - 0.9) + std::log2(1 - 0.5) + std::log2(1 - 0.1)),
       -3 * (std::log2(1 - 0.9) + std::log2(1 - 0.1)),
       -3 * (std::log2(1 - 0.9) + std::log2(1 - 0.5) + std::log2(1 - 0.5)),
       -3 * (std::log2(1 - 0.9) + std::log2(1 - 0.5) + std::log2(1 - 0.5) +
             std::log2(1 - 0.9))},
      {// distance from 1 to i
       -3 * std::log2(1 - 0.9), 0., -3 * std::log2(1 - 0.5),
       -3 * (std::log2(1 - 0.5) + std::log2(1 - 0.1)), -3 * std::log2(1 - 0.1),
       -3 * (std::log2(1 - 0.5) + std::log2(1 - 0.5)),
       -3 * (std::log2(1 - 0.5) + std::log2(1 - 0.5) + std::log2(1 - 0.9))},
      {
          // distance from 2 to i
          -3 * (std::log2(1 - 0.9) + std::log2(1 - 0.5)),
          -3 * std::log2(1 - 0.5),
          0.,
          -3 * std::log2(1 - 0.1),
          -3 * (std::log2(1 - 0.5) + std::log2(1 - 0.1)),
          -3 * std::log2(1 - 0.5),
          -3 * (std::log2(1 - 0.5) + std::log2(1 - 0.9)),
      },
      {
          // distance from 3 to i
          -3 * (std::log2(1 - 0.1) + std::log2(1 - 0.5) + std::log2(1 - 0.9)),
          -3 * (std::log2(1 - 0.5) + std::log2(1 - 0.1)),
          -3 * std::log2(1 - 0.1),
          0.,
          -3 * (std::log2(1 - 0.1) + std::log2(1 - 0.5) + std::log2(1 - 0.1)),
          -3 * (std::log2(1 - 0.1) + std::log2(1 - 0.5)),
          -3 * (std::log2(1 - 0.1) + std::log2(1 - 0.5) + std::log2(1 - 0.9)),
      },
      {// distance from 4 to i
       -3 * (std::log2(1 - 0.1) + std::log2(1 - 0.9)), -3 * std::log2(1 - 0.1),
       -3 * (std::log2(1 - 0.1) + std::log2(1 - 0.5)),
       -3 * (std::log2(1 - 0.1) + std::log2(1 - 0.5) + std::log2(1 - 0.1)), 0.,
       -3 * (std::log2(1 - 0.1) + std::log2(1 - 0.5) + std::log2(1 - 0.5)),
       -3 * (std::log2(1 - 0.1) + std::log2(1 - 0.5) + std::log2(1 - 0.5) +
             std::log2(1 - 0.9))},
      {// distance from 5 to i
       -3 * (std::log2(1 - 0.5) + std::log2(1 - 0.5) + std::log2(1 - 0.9)),
       -3 * (std::log2(1 - 0.5) + std::log2(1 - 0.5)), -3 * std::log2(1 - 0.5),
       -3 * (std::log2(1 - 0.5) + std::log2(1 - 0.1)),
       -3 * (std::log2(1 - 0.5) + std::log2(1 - 0.5) + std::log2(1 - 0.1)), 0.,
       -3 * std::log2(1 - 0.9)},
      {
          // distance from 6 to i
          -3 * (std::log2(1 - 0.9) + std::log2(1 - 0.5) + std::log2(1 - 0.5) +
                std::log2(1 - 0.9)),
          -3 * (std::log2(1 - 0.9) + std::log2(1 - 0.5) + std::log2(1 - 0.5)),
          -3 * (std::log2(1 - 0.9) + std::log2(1 - 0.5)),
          -3 * (std::log2(1 - 0.9) + std::log2(1 - 0.5) + std::log2(1 - 0.1)),
          -3 * (std::log2(1 - 0.9) + std::log2(1 - 0.5) + std::log2(1 - 0.5) +
                std::log2(1 - 0.1)),
          -3 * std::log2(1 - 0.9),
          0.,
      }};
  EXPECT_TRUE(
      matrixNear(architecture.getFidelityDistanceTable(), targetTable, 1e-6));

  const Matrix targetTableSkip1Edge = {
      {// distance from 0 to i
       0., 0., -3 * std::log2(1 - 0.5),
       -3 * (std::log2(1 - 0.5) + std::log2(1 - 0.1)), -3 * std::log2(1 - 0.1),
       -3 * (std::log2(1 - 0.5) + std::log2(1 - 0.5)),
       -3 * (std::log2(1 - 0.5) + std::log2(1 - 0.5) + std::log2(1 - 0.9))},
      {// distance from 1 to i
       0., 0., 0., -3 * std::log2(1 - 0.1), 0., -3 * std::log2(1 - 0.5),
       -3 * (std::log2(1 - 0.5) + std::log2(1 - 0.5))},
      {
          // distance from 2 to i
          -3 * std::log2(1 - 0.5),
          0.,
          0.,
          0.,
          -3 * std::log2(1 - 0.1),
          0.,
          -3 * std::log2(1 - 0.5),
      },
      {
          // distance from 3 to i
          -3 * (std::log2(1 - 0.1) + std::log2(1 - 0.5)),
          -3 * std::log2(1 - 0.1),
          0.,
          0.,
          -3 * (std::log2(1 - 0.1) + std::log2(1 - 0.1)),
          -3 * std::log2(1 - 0.1),
          -3 * (std::log2(1 - 0.1) + std::log2(1 - 0.5)),
      },
      {// distance from 4 to i
       -3 * std::log2(1 - 0.1), 0., -3 * std::log2(1 - 0.1),
       -3 * (std::log2(1 - 0.1) + std::log2(1 - 0.1)), 0.,
       -3 * (std::log2(1 - 0.1) + std::log2(1 - 0.5)),
       -3 * (std::log2(1 - 0.1) + std::log2(1 - 0.5) + std::log2(1 - 0.5))},
      {// distance from 5 to i
       -3 * (std::log2(1 - 0.5) + std::log2(1 - 0.5)), -3 * std::log2(1 - 0.5),
       0., -3 * std::log2(1 - 0.1),
       -3 * (std::log2(1 - 0.5) + std::log2(1 - 0.1)), 0., 0.},
      {
          // distance from 6 to i
          -3 * (std::log2(1 - 0.5) + std::log2(1 - 0.5) + std::log2(1 - 0.9)),
          -3 * (std::log2(1 - 0.5) + std::log2(1 - 0.5)),
          -3 * std::log2(1 - 0.5),
          -3 * (std::log2(1 - 0.5) + std::log2(1 - 0.1)),
          -3 * (std::log2(1 - 0.5) + std::log2(1 - 0.5) + std::log2(1 - 0.1)),
          0.,
          0.,
      }};
  EXPECT_TRUE(matrixNear(architecture.getFidelityDistanceTable(1),
                         targetTableSkip1Edge, 1e-6));

  const Matrix targetTableSkip3Edges = {
      {// distance from 0 to i
       0., 0., 0., 0., 0., 0., -3 * std::log2(1 - 0.5)},
      {
          // distance from 1 to i
          0.,
          0.,
          0.,
          0.,
          0.,
          0.,
          0.,
      },
      {
          // distance from 2 to i
          0.,
          0.,
          0.,
          0.,
          0.,
          0.,
          0.,
      },
      {
          // distance from 3 to i
          0.,
          0.,
          0.,
          0.,
          0.,
          0.,
          0.,
      },
      {// distance from 4 to i
       0., 0., 0., 0., 0., 0., -3 * std::log2(1 - 0.1)},
      {
          // distance from 5 to i
          0.,
          0.,
          0.,
          0.,
          0.,
          0.,
          0.,
      },
      {
          // distance from 6 to i
          -3 * std::log2(1 - 0.5),
          0.,
          0.,
          0.,
          -3 * std::log2(1 - 0.1),
          0.,
          0.,
      }};
  EXPECT_TRUE(matrixNear(architecture.getFidelityDistanceTable(3),
                         targetTableSkip3Edges, 1e-6));

  const Matrix zeroMatrix = {
      {0., 0., 0., 0., 0., 0., 0.}, {0., 0., 0., 0., 0., 0., 0.},
      {0., 0., 0., 0., 0., 0., 0.}, {0., 0., 0., 0., 0., 0., 0.},
      {0., 0., 0., 0., 0., 0., 0.}, {0., 0., 0., 0., 0., 0., 0.},
      {0., 0., 0., 0., 0., 0., 0.}};
  EXPECT_TRUE(
      matrixNear(architecture.getFidelityDistanceTable(4), zeroMatrix, 1e-6));
  EXPECT_TRUE(
      matrixNear(architecture.getFidelityDistanceTable(5), zeroMatrix, 1e-6));
  EXPECT_TRUE(
      matrixNear(architecture.getFidelityDistanceTable(6), zeroMatrix, 1e-6));

  EXPECT_THROW(static_cast<void>(architecture.fidelityDistance(0, 7)),
               QMAPException);
  EXPECT_THROW(static_cast<void>(architecture.fidelityDistance(7, 0)),
               QMAPException);
}

TEST(TestArchitecture, FidelityDistanceSemiBidirectionalTest) {
  /*
                        6 [0.03]
                        |
                      [0.9]
                        |
     [0.03] 4           5 [0.02]
            |           ||
          [0.1]       [0.5]
            |           ||
0  =[0.9]=  1  =[0.5]=  2  =[0.1]=  3

[0.03]      [0.03]      [0.02]      [0.03]

-[]- ... 2-qubit error rates of unidirectional edge
=[]= ... 2-qubit error rates of bidirectional edge
[]   ... 1-qubit error rates
*/
  Architecture architecture{};
  const CouplingMap cm = {{0, 1}, {1, 0}, {1, 2}, {2, 1}, {2, 3},
                          {3, 2}, {1, 4}, {2, 5}, {5, 2}, {6, 5}};
  architecture.loadCouplingMap(7, cm);

  auto props = Architecture::Properties();
  props.setSingleQubitErrorRate(0, "x", 0.03);
  props.setSingleQubitErrorRate(1, "x", 0.03);
  props.setSingleQubitErrorRate(2, "x", 0.02);
  props.setSingleQubitErrorRate(3, "x", 0.03);
  props.setSingleQubitErrorRate(4, "x", 0.03);
  props.setSingleQubitErrorRate(5, "x", 0.02);
  props.setSingleQubitErrorRate(6, "x", 0.03);

  props.setTwoQubitErrorRate(0, 1, 0.9);
  props.setTwoQubitErrorRate(1, 0, 0.9);
  props.setTwoQubitErrorRate(1, 2, 0.5);
  props.setTwoQubitErrorRate(2, 1, 0.5);
  props.setTwoQubitErrorRate(2, 3, 0.1);
  props.setTwoQubitErrorRate(3, 2, 0.1);
  props.setTwoQubitErrorRate(1, 4, 0.1);
  props.setTwoQubitErrorRate(2, 5, 0.5);
  props.setTwoQubitErrorRate(5, 2, 0.5);
  props.setTwoQubitErrorRate(6, 5, 0.9);

  architecture.loadProperties(props);

  const Matrix targetTable = {
      {// distance from 0 to i
       0., -3 * std::log2(1 - 0.9),
       -3 * (std::log2(1 - 0.9) + std::log2(1 - 0.5)),
       -3 * (std::log2(1 - 0.9) + std::log2(1 - 0.5) + std::log2(1 - 0.1)),
       -3 * (std::log2(1 - 0.9) + std::log2(1 - 0.1)) -
           2 * (std::log2(1 - 0.03) + std::log2(1 - 0.03)),
       -3 * (std::log2(1 - 0.9) + std::log2(1 - 0.5) + std::log2(1 - 0.5)),
       -3 * (std::log2(1 - 0.9) + std::log2(1 - 0.5) + std::log2(1 - 0.5) +
             std::log2(1 - 0.9)) -
           2 * (std::log2(1 - 0.02) + std::log2(1 - 0.03))},
      {// distance from 1 to i
       -3 * std::log2(1 - 0.9), 0., -3 * std::log2(1 - 0.5),
       -3 * (std::log2(1 - 0.5) + std::log2(1 - 0.1)),
       -3 * std::log2(1 - 0.1) -
           2 * (std::log2(1 - 0.03) + std::log2(1 - 0.03)),
       -3 * (std::log2(1 - 0.5) + std::log2(1 - 0.5)),
       -3 * (std::log2(1 - 0.5) + std::log2(1 - 0.5) + std::log2(1 - 0.9)) -
           2 * (std::log2(1 - 0.02) + std::log2(1 - 0.03))},
      {// distance from 2 to i
       -3 * (std::log2(1 - 0.9) + std::log2(1 - 0.5)), -3 * std::log2(1 - 0.5),
       0., -3 * std::log2(1 - 0.1),
       -3 * (std::log2(1 - 0.5) + std::log2(1 - 0.1)) -
           2 * (std::log2(1 - 0.03) + std::log2(1 - 0.03)),
       -3 * std::log2(1 - 0.5),
       -3 * (std::log2(1 - 0.5) + std::log2(1 - 0.9)) -
           2 * (std::log2(1 - 0.02) + std::log2(1 - 0.03))},
      {// distance from 3 to i
       -3 * (std::log2(1 - 0.1) + std::log2(1 - 0.5) + std::log2(1 - 0.9)),
       -3 * (std::log2(1 - 0.5) + std::log2(1 - 0.1)), -3 * std::log2(1 - 0.1),
       0.,
       -3 * (std::log2(1 - 0.1) + std::log2(1 - 0.5) + std::log2(1 - 0.1)) -
           2 * (std::log2(1 - 0.03) + std::log2(1 - 0.03)),
       -3 * (std::log2(1 - 0.1) + std::log2(1 - 0.5)),
       -3 * (std::log2(1 - 0.1) + std::log2(1 - 0.5) + std::log2(1 - 0.9)) -
           2 * (std::log2(1 - 0.02) + std::log2(1 - 0.03))},
      {// distance from 4 to i
       -3 * (std::log2(1 - 0.1) + std::log2(1 - 0.9)) -
           2 * (std::log2(1 - 0.03) + std::log2(1 - 0.03)),
       -3 * std::log2(1 - 0.1) -
           2 * (std::log2(1 - 0.03) + std::log2(1 - 0.03)),
       -3 * (std::log2(1 - 0.1) + std::log2(1 - 0.5)) -
           2 * (std::log2(1 - 0.03) + std::log2(1 - 0.03)),
       -3 * (std::log2(1 - 0.1) + std::log2(1 - 0.5) + std::log2(1 - 0.1)) -
           2 * (std::log2(1 - 0.03) + std::log2(1 - 0.03)),
       0.,
       -3 * (std::log2(1 - 0.1) + std::log2(1 - 0.5) + std::log2(1 - 0.5)) -
           2 * (std::log2(1 - 0.03) + std::log2(1 - 0.03)),
       -3 * (std::log2(1 - 0.1) + std::log2(1 - 0.5) + std::log2(1 - 0.5) +
             std::log2(1 - 0.9)) -
           2 * (std::log2(1 - 0.03) + std::log2(1 - 0.03) +
                std::log2(1 - 0.02) + std::log2(1 - 0.03))},
      {
          // distance from 5 to i
          -3 * (std::log2(1 - 0.5) + std::log2(1 - 0.5) + std::log2(1 - 0.9)),
          -3 * (std::log2(1 - 0.5) + std::log2(1 - 0.5)),
          -3 * std::log2(1 - 0.5),
          -3 * (std::log2(1 - 0.5) + std::log2(1 - 0.1)),
          -3 * (std::log2(1 - 0.5) + std::log2(1 - 0.5) + std::log2(1 - 0.1)) -
              2 * (std::log2(1 - 0.03) + std::log2(1 - 0.03)),
          0.,
          -3 * std::log2(1 - 0.9) -
              2 * (std::log2(1 - 0.02) + std::log2(1 - 0.03)),
      },
      {// distance from 6 to i
       -3 * (std::log2(1 - 0.9) + std::log2(1 - 0.5) + std::log2(1 - 0.5) +
             std::log2(1 - 0.9)) -
           2 * (std::log2(1 - 0.02) + std::log2(1 - 0.03)),
       -3 * (std::log2(1 - 0.9) + std::log2(1 - 0.5) + std::log2(1 - 0.5)) -
           2 * (std::log2(1 - 0.02) + std::log2(1 - 0.03)),
       -3 * (std::log2(1 - 0.9) + std::log2(1 - 0.5)) -
           2 * (std::log2(1 - 0.02) + std::log2(1 - 0.03)),
       -3 * (std::log2(1 - 0.9) + std::log2(1 - 0.5) + std::log2(1 - 0.1)) -
           2 * (std::log2(1 - 0.02) + std::log2(1 - 0.03)),
       -3 * (std::log2(1 - 0.9) + std::log2(1 - 0.5) + std::log2(1 - 0.5) +
             std::log2(1 - 0.1)) -
           2 * (std::log2(1 - 0.02) + std::log2(1 - 0.03) +
                std::log2(1 - 0.03) + std::log2(1 - 0.03)),
       -3 * std::log2(1 - 0.9) -
           2 * (std::log2(1 - 0.02) + std::log2(1 - 0.03)),
       0.}};
  EXPECT_TRUE(
      matrixNear(architecture.getFidelityDistanceTable(), targetTable, 1e-6));

  const Matrix targetTableSkip1Edge = {
      {// distance from 0 to i
       0., 0., -3 * std::log2(1 - 0.5),
       -3 * (std::log2(1 - 0.5) + std::log2(1 - 0.1)),
       -3 * std::log2(1 - 0.1) -
           2 * (std::log2(1 - 0.03) + std::log2(1 - 0.03)),
       -3 * (std::log2(1 - 0.5) + std::log2(1 - 0.5)),
       -3 * (std::log2(1 - 0.9) + std::log2(1 - 0.5) + std::log2(1 - 0.5))},
      {// distance from 1 to i
       0., 0., 0., -3 * std::log2(1 - 0.1), 0., -3 * std::log2(1 - 0.5),
       -3 * (std::log2(1 - 0.5) + std::log2(1 - 0.5))},
      {// distance from 2 to i
       -3 * std::log2(1 - 0.5), 0., 0., 0.,
       -3 * std::log2(1 - 0.1) -
           2 * (std::log2(1 - 0.03) + std::log2(1 - 0.03)),
       0., -3 * std::log2(1 - 0.5)},
      {// distance from 3 to i
       -3 * (std::log2(1 - 0.1) + std::log2(1 - 0.5)), -3 * std::log2(1 - 0.1),
       0., 0.,
       -3 * (std::log2(1 - 0.1) + std::log2(1 - 0.1)) -
           2 * (std::log2(1 - 0.03) + std::log2(1 - 0.03)),
       -3 * std::log2(1 - 0.1), -3 * (std::log2(1 - 0.1) + std::log2(1 - 0.5))},
      {// distance from 4 to i
       -3 * std::log2(1 - 0.1) -
           2 * (std::log2(1 - 0.03) + std::log2(1 - 0.03)),
       0.,
       -3 * std::log2(1 - 0.1) -
           2 * (std::log2(1 - 0.03) + std::log2(1 - 0.03)),
       -3 * (std::log2(1 - 0.1) + std::log2(1 - 0.1)) -
           2 * (std::log2(1 - 0.03) + std::log2(1 - 0.03)),
       0.,
       -3 * (std::log2(1 - 0.1) + std::log2(1 - 0.5)) -
           2 * (std::log2(1 - 0.03) + std::log2(1 - 0.03)),
       -3 * (std::log2(1 - 0.1) + std::log2(1 - 0.5) + std::log2(1 - 0.5)) -
           2 * (std::log2(1 - 0.03) + std::log2(1 - 0.03))},
      {
          // distance from 5 to i
          -3 * (std::log2(1 - 0.5) + std::log2(1 - 0.5)),
          -3 * std::log2(1 - 0.5),
          0.,
          -3 * std::log2(1 - 0.1),
          -3 * (std::log2(1 - 0.5) + std::log2(1 - 0.1)) -
              2 * (std::log2(1 - 0.03) + std::log2(1 - 0.03)),
          0.,
          0.,
      },
      {// distance from 6 to i
       -3 * (std::log2(1 - 0.5) + std::log2(1 - 0.5) + std::log2(1 - 0.9)),
       -3 * (std::log2(1 - 0.5) + std::log2(1 - 0.5)), -3 * std::log2(1 - 0.5),
       -3 * (std::log2(1 - 0.5) + std::log2(1 - 0.1)),
       -3 * (std::log2(1 - 0.5) + std::log2(1 - 0.5) + std::log2(1 - 0.1)) -
           2 * (std::log2(1 - 0.03) + std::log2(1 - 0.03)),
       0., 0.}};
  EXPECT_TRUE(matrixNear(architecture.getFidelityDistanceTable(1),
                         targetTableSkip1Edge, 1e-6));

  const Matrix targetTableSkip3Edges = {
      {// distance from 0 to i
       0., 0., 0., 0., 0., 0., -3 * std::log2(1 - 0.5)},
      {
          // distance from 1 to i
          0.,
          0.,
          0.,
          0.,
          0.,
          0.,
          0.,
      },
      {
          // distance from 2 to i
          0.,
          0.,
          0.,
          0.,
          0.,
          0.,
          0.,
      },
      {
          // distance from 3 to i
          0.,
          0.,
          0.,
          0.,
          0.,
          0.,
          0.,
      },
      {// distance from 4 to i
       0., 0., 0., 0., 0., 0.,
       -3 * std::log2(1 - 0.1) -
           2 * (std::log2(1 - 0.03) + std::log2(1 - 0.03))},
      {
          // distance from 5 to i
          0.,
          0.,
          0.,
          0.,
          0.,
          0.,
          0.,
      },
      {
          // distance from 6 to i
          -3 * std::log2(1 - 0.5),
          0.,
          0.,
          0.,
          -3 * std::log2(1 - 0.1) -
              2 * (std::log2(1 - 0.03) + std::log2(1 - 0.03)),
          0.,
          0.,
      }};
  EXPECT_TRUE(matrixNear(architecture.getFidelityDistanceTable(3),
                         targetTableSkip3Edges, 1e-6));

  const Matrix zeroMatrix = {
      {0., 0., 0., 0., 0., 0., 0.}, {0., 0., 0., 0., 0., 0., 0.},
      {0., 0., 0., 0., 0., 0., 0.}, {0., 0., 0., 0., 0., 0., 0.},
      {0., 0., 0., 0., 0., 0., 0.}, {0., 0., 0., 0., 0., 0., 0.},
      {0., 0., 0., 0., 0., 0., 0.}};
  EXPECT_TRUE(
      matrixNear(architecture.getFidelityDistanceTable(4), zeroMatrix, 1e-6));
  EXPECT_TRUE(
      matrixNear(architecture.getFidelityDistanceTable(5), zeroMatrix, 1e-6));
  EXPECT_TRUE(
      matrixNear(architecture.getFidelityDistanceTable(6), zeroMatrix, 1e-6));
}

TEST(TestArchitecture, FidelitySwapCostTest) {
  const double tolerance = 1e-6;
  const CouplingMap cm = {{0, 1}, {1, 2}, {2, 1}, {2, 3}, {2, 4}, {4, 2}};

  auto props = Architecture::Properties();
  props.setSingleQubitErrorRate(0, "x", 0.11);
  props.setSingleQubitErrorRate(1, "x", 0.12);
  props.setSingleQubitErrorRate(2, "x", 0.13);
  props.setSingleQubitErrorRate(3, "x", 0.14);
  props.setSingleQubitErrorRate(4, "x", 0.15);

  props.setTwoQubitErrorRate(0, 1, 0.1);
  props.setTwoQubitErrorRate(1, 2, 0.2);
  props.setTwoQubitErrorRate(2, 1, 0.2);
  props.setTwoQubitErrorRate(2, 3, 0.3);
  props.setTwoQubitErrorRate(2, 4, 0.4);
  props.setTwoQubitErrorRate(4, 2, 0.4);

  const Architecture architecture(5, cm, props);

  const Matrix& swapFidCost = architecture.getSwapFidelityCosts();

  EXPECT_EQ(swapFidCost.size(), 5);
  EXPECT_EQ(swapFidCost[0].size(), 5);
  EXPECT_NEAR(swapFidCost[0][1],
              -3 * std::log2(1 - 0.1) - 2 * std::log2(1 - 0.11) -
                  2 * std::log2(1 - 0.12),
              tolerance);
  EXPECT_GT(swapFidCost[0][2], 1e20);
  EXPECT_GT(swapFidCost[0][3], 1e20);
  EXPECT_GT(swapFidCost[0][4], 1e20);
  EXPECT_EQ(swapFidCost[1].size(), 5);
  EXPECT_NEAR(swapFidCost[1][0],
              -3 * std::log2(1 - 0.1) - 2 * std::log2(1 - 0.11) -
                  2 * std::log2(1 - 0.12),
              tolerance);
  EXPECT_NEAR(swapFidCost[1][2], -3 * std::log2(1 - 0.2), tolerance);
  EXPECT_GT(swapFidCost[1][3], 1e20);
  EXPECT_GT(swapFidCost[1][4], 1e20);
  EXPECT_EQ(swapFidCost[2].size(), 5);
  EXPECT_GT(swapFidCost[2][0], 1e20);
  EXPECT_NEAR(swapFidCost[2][1], -3 * std::log2(1 - 0.2), tolerance);
  EXPECT_NEAR(swapFidCost[2][3],
              -3 * std::log2(1 - 0.3) - 2 * std::log2(1 - 0.13) -
                  2 * std::log2(1 - 0.14),
              tolerance);
  EXPECT_NEAR(swapFidCost[2][4], -3 * std::log2(1 - 0.4), tolerance);
  EXPECT_EQ(swapFidCost[3].size(), 5);
  EXPECT_GT(swapFidCost[3][0], 1e20);
  EXPECT_GT(swapFidCost[3][1], 1e20);
  EXPECT_NEAR(swapFidCost[3][2],
              -3 * std::log2(1 - 0.3) - 2 * std::log2(1 - 0.13) -
                  2 * std::log2(1 - 0.14),
              tolerance);
  EXPECT_GT(swapFidCost[3][4], 1e20);
  EXPECT_EQ(swapFidCost[4].size(), 5);
  EXPECT_GT(swapFidCost[4][0], 1e20);
  EXPECT_GT(swapFidCost[4][1], 1e20);
  EXPECT_NEAR(swapFidCost[4][2], -3 * std::log2(1 - 0.4), tolerance);
  EXPECT_GT(swapFidCost[4][3], 1e20);

  EXPECT_THROW(static_cast<void>(architecture.getSingleQubitFidelityCost(5)),
               QMAPException);
  EXPECT_THROW(static_cast<void>(architecture.getTwoQubitFidelityCost(5, 0)),
               QMAPException);
  EXPECT_THROW(static_cast<void>(architecture.getTwoQubitFidelityCost(0, 5)),
               QMAPException);
  EXPECT_THROW(static_cast<void>(architecture.getSwapFidelityCost(5, 0)),
               QMAPException);
  EXPECT_THROW(static_cast<void>(architecture.getSwapFidelityCost(0, 5)),
               QMAPException);
}

TEST(TestArchitecture, FidelityDistanceCheapestPathTest) {
  // tests if the distance measure actually finds the cheapest path and
  // not just the shortest
  Architecture architecture{};
  const CouplingMap cm = {{0, 1}, {1, 0}, {2, 1}, {2, 6}, {6, 2},
                          {0, 5}, {5, 0}, {5, 6}, {6, 5}, {0, 3},
                          {3, 0}, {3, 4}, {4, 3}, {4, 6}, {6, 4}};
  architecture.loadCouplingMap(7, cm);

  auto props = Architecture::Properties();
  props.setSingleQubitErrorRate(0, "x", 0.1);
  props.setSingleQubitErrorRate(1, "x", 0.1);
  props.setSingleQubitErrorRate(2, "x", 0.1);
  props.setSingleQubitErrorRate(3, "x", 0.1);
  props.setSingleQubitErrorRate(4, "x", 0.1);
  props.setSingleQubitErrorRate(5, "x", 0.1);
  props.setSingleQubitErrorRate(6, "x", 0.1);

  props.setTwoQubitErrorRate(0, 1, 0.1);
  props.setTwoQubitErrorRate(1, 0, 0.1);
  props.setTwoQubitErrorRate(2, 1, 0.1);
  props.setTwoQubitErrorRate(2, 6, 0.1);
  props.setTwoQubitErrorRate(6, 2, 0.1);
  props.setTwoQubitErrorRate(0, 5, 0.7);
  props.setTwoQubitErrorRate(5, 0, 0.7);
  props.setTwoQubitErrorRate(5, 6, 0.7);
  props.setTwoQubitErrorRate(6, 5, 0.7);
  props.setTwoQubitErrorRate(0, 3, 0.5);
  props.setTwoQubitErrorRate(3, 0, 0.5);
  props.setTwoQubitErrorRate(3, 4, 0.5);
  props.setTwoQubitErrorRate(4, 3, 0.5);
  props.setTwoQubitErrorRate(4, 6, 0.5);
  props.setTwoQubitErrorRate(6, 4, 0.5);

  architecture.loadProperties(props);

  const Matrix fidDistance = architecture.getFidelityDistanceTable();

  EXPECT_EQ(fidDistance.size(), 7);
  EXPECT_EQ(fidDistance[0].size(), 7);
  EXPECT_NEAR(fidDistance[0][6],
              -3 * 3 * std::log2(1 - 0.1) - 2 * 2 * std::log2(1 - 0.1), 1e-6);
}

TEST(TestArchitecture, FidelityDistanceNoFidelity) {
  const Architecture architecture(4, {{0, 1}, {1, 2}, {1, 3}});

  EXPECT_THROW(static_cast<void>(architecture.getFidelityDistanceTable()),
               QMAPException);
  EXPECT_THROW(static_cast<void>(architecture.getFidelityDistanceTable(0)),
               QMAPException);
  EXPECT_THROW(static_cast<void>(architecture.getFidelityDistanceTable(1)),
               QMAPException);
  EXPECT_THROW(static_cast<void>(architecture.getFidelityDistanceTable(2)),
               QMAPException);
  EXPECT_THROW(static_cast<void>(architecture.getFidelityDistanceTable(3)),
               QMAPException);

  EXPECT_THROW(static_cast<void>(architecture.fidelityDistance(0, 2)),
               QMAPException);
  EXPECT_THROW(static_cast<void>(architecture.fidelityDistance(0, 2, 0)),
               QMAPException);
  EXPECT_THROW(static_cast<void>(architecture.fidelityDistance(0, 2, 1)),
               QMAPException);
  EXPECT_THROW(static_cast<void>(architecture.fidelityDistance(0, 2, 2)),
               QMAPException);
  EXPECT_THROW(static_cast<void>(architecture.fidelityDistance(0, 2, 3)),
               QMAPException);

  EXPECT_THROW(static_cast<void>(architecture.getFidelityTable()),
               QMAPException);
  EXPECT_THROW(static_cast<void>(architecture.getSingleQubitFidelities()),
               QMAPException);
  EXPECT_THROW(static_cast<void>(architecture.getSingleQubitFidelityCosts()),
               QMAPException);
  EXPECT_THROW(static_cast<void>(architecture.getSingleQubitFidelityCost(0)),
               QMAPException);
  EXPECT_THROW(static_cast<void>(architecture.getTwoQubitFidelityCosts()),
               QMAPException);
  EXPECT_THROW(static_cast<void>(architecture.getTwoQubitFidelityCost(0, 1)),
               QMAPException);
  EXPECT_THROW(static_cast<void>(architecture.getSwapFidelityCosts()),
               QMAPException);
  EXPECT_THROW(static_cast<void>(architecture.getSwapFidelityCost(0, 1)),
               QMAPException);
}

TEST(TestArchitecture, DistanceCheapestPathTest) {
  // tests if the distance measure actually finds the cheapest path and
  // not just the shortest
  Architecture architecture{};

  // minimum number of unidirectional edges on a path where the same path with
  // bidirectional edges can afford at least 1 more edge and still be cheaper
  const std::uint8_t nrEdges =
      1 + static_cast<std::uint8_t>(
              std::ceil(static_cast<double>(COST_BIDIRECTIONAL_SWAP) /
                        (static_cast<double>(COST_UNIDIRECTIONAL_SWAP) -
                         static_cast<double>(COST_BIDIRECTIONAL_SWAP))));

  CouplingMap cm = {};
  for (std::uint8_t i = 0; i < nrEdges; ++i) {
    cm.insert(Edge{i + 1, i});
  }
  for (std::uint8_t i = nrEdges + 1; i < 2 * nrEdges; ++i) {
    cm.insert(Edge{i, i + 1});
    cm.insert(Edge{i + 1, i});
  }
  cm.insert(Edge{0, nrEdges + 1});
  cm.insert(Edge{nrEdges + 1, 0});
  cm.insert(Edge{2 * nrEdges, nrEdges});
  cm.insert(Edge{nrEdges, 2 * nrEdges});
  architecture.loadCouplingMap(static_cast<std::uint16_t>(2 * nrEdges + 1), cm);

  const Matrix distances = architecture.getDistanceTable();

  EXPECT_EQ(distances.size(), 2 * nrEdges + 1);
  EXPECT_EQ(distances[0].size(), 2 * nrEdges + 1);
  EXPECT_NEAR(distances[0][nrEdges], nrEdges * COST_BIDIRECTIONAL_SWAP, 1e-6);
}
