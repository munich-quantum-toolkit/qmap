/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "ir/QuantumComputation.hpp"
#include "ir/operations/Control.hpp"
#include "sc/Architecture.hpp"
#include "sc/configuration/AvailableArchitecture.hpp"
#include "sc/configuration/CommanderGrouping.hpp"
#include "sc/configuration/Configuration.hpp"
#include "sc/configuration/Encoding.hpp"
#include "sc/configuration/Method.hpp"
#include "sc/exact/ExactMapper.hpp"

#include <gtest/gtest.h>
#include <iostream>
#include <memory>
#include <utility>

class TestEncodings
    : public testing::TestWithParam<std::pair<Encoding, CommanderGrouping>> {
protected:
  qc::QuantumComputation qc;
  Configuration settings;
  Architecture arch;
  std::unique_ptr<ExactMapper> mapper;

  void SetUp() override {
    settings.verbose = true;
    settings.method = Method::Exact;
    settings.useSubsets = false;
  }
};

INSTANTIATE_TEST_SUITE_P(
    Encodings, TestEncodings,
    testing::Values(std::pair{Encoding::Naive, CommanderGrouping::Halves},
                    std::pair{Encoding::Commander, CommanderGrouping::Halves},
                    std::pair{Encoding::Commander, CommanderGrouping::Fixed2},
                    std::pair{Encoding::Commander, CommanderGrouping::Fixed3},
                    std::pair{Encoding::Bimander, CommanderGrouping::Halves},
                    std::pair{Encoding::Bimander, CommanderGrouping::Fixed2},
                    std::pair{Encoding::Bimander, CommanderGrouping::Fixed3}));

TEST_P(TestEncodings, ThreeToSevenQubits) {
  using namespace qc::literals;

  qc = qc::QuantumComputation(3U);
  qc.cx(1_pc, 2);
  qc.cx(0_pc, 1);

  arch.loadCouplingMap(AvailableArchitecture::IbmqCasablanca);

  mapper = std::make_unique<ExactMapper>(qc, arch);

  const auto& [encoding, grouping] = GetParam();
  settings.encoding = encoding;
  settings.commanderGrouping = grouping;

  mapper->map(settings);
  mapper->printResult(std::cout);

  ASSERT_FALSE(mapper->getResults().timeout);
  EXPECT_EQ(mapper->getResults().output.swaps, 0U);
}

TEST_P(TestEncodings, FiveToSevenQubits) {
  using namespace qc::literals;

  qc = qc::QuantumComputation(5U);
  qc.cx(0_pc, 1);
  qc.cx(0_pc, 2);
  qc.cx(0_pc, 3);
  qc.cx(0_pc, 4);

  arch.loadCouplingMap(AvailableArchitecture::IbmqCasablanca);

  mapper = std::make_unique<ExactMapper>(qc, arch);

  const auto& [encoding, grouping] = GetParam();
  settings.encoding = encoding;
  settings.commanderGrouping = grouping;

  mapper->map(settings);
  mapper->printResult(std::cout);

  ASSERT_FALSE(mapper->getResults().timeout);
  EXPECT_EQ(mapper->getResults().output.swaps, 1U);
}
