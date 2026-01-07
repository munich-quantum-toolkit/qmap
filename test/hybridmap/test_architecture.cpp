/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "hybridmap/NeutralAtomArchitecture.hpp"
#include "hybridmap/NeutralAtomDefinitions.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <system_error>

using namespace na;

TEST(NeutralAtomArchitectureMethods, GetNNTest) {
  const auto arch =
      NeutralAtomArchitecture("architectures/rubidium_shuttling.json");
  const auto cols = arch.getNcolumns();
  const auto rows = arch.getNrows();

  // Top-left corner (0): neighbors are right (1) and down (cols) when available
  const auto nn0 = arch.getNN(0);
  if (cols > 1) {
    EXPECT_NE(std::ranges::find(nn0, static_cast<CoordIndex>(1)), nn0.end());
  }
  if (rows > 1) {
    EXPECT_NE(std::ranges::find(nn0, cols), nn0.end());
  }
  // No negative indices
  EXPECT_EQ(std::ranges::find(nn0, static_cast<CoordIndex>(-1)), nn0.end());
}

TEST(NeutralAtomArchitectureMethods, GetIndexRoundTrip) {
  const auto arch =
      NeutralAtomArchitecture("architectures/rubidium_shuttling.json");
  // Round-trip index -> coordinate -> index
  const CoordIndex idx = 3 < arch.getNpositions() ? 3 : 0;
  const auto coord = arch.getCoordinate(idx);
  const auto idxBack = arch.getIndex(coord);
  EXPECT_EQ(idxBack, idx);
}

TEST(NeutralAtomArchitectureMethods, AnimationAPIsProduceContent) {
  const auto arch =
      NeutralAtomArchitecture("architectures/rubidium_shuttling.json");

  // getAnimationMachine should return a non-empty string with known sections
  const auto machine = arch.getAnimationMachine(1.0);
  EXPECT_FALSE(machine.empty());
  EXPECT_NE(machine.find("movement"), std::string::npos);
  EXPECT_NE(machine.find("time"), std::string::npos);
  EXPECT_NE(machine.find("zone hybrid"), std::string::npos);
  EXPECT_NE(machine.find("trap"), std::string::npos);

  // Save to temp files
  const std::filesystem::path tmpMachine =
      std::filesystem::temp_directory_path() / "arch_anim_machine.csv";
  arch.saveAnimationMachine(tmpMachine.string(), 1.0);
  // Verify files exist and are non-empty
  ASSERT_TRUE(std::filesystem::exists(tmpMachine));
  EXPECT_GT(std::filesystem::file_size(tmpMachine), 0U);
  // Cleanup best-effort
  std::error_code ec;
  std::filesystem::remove(tmpMachine, ec);
}

TEST(NeutralAtomArchitectureMethods, BasicCountsAndOffsetDistance) {
  const auto arch =
      NeutralAtomArchitecture("architectures/rubidium_shuttling.json");
  // Simple sanity for counts
  EXPECT_GT(arch.getNAods(), 0);
  EXPECT_GT(arch.getNAodCoordinates(), 0);

  // offset distance * levels ~= inter-qubit distance
  const auto off = arch.getOffsetDistance();
  const auto levels = arch.getNAodIntermediateLevels();
  EXPECT_GT(levels, 0);
  EXPECT_NEAR(off * levels, arch.getInterQubitDistance(), 1e-9);
}

TEST(NeutralAtomArchitectureExceptions, NonexistentFileThrows) {
  EXPECT_THROW((void)NeutralAtomArchitecture(
                   "architectures/this_file_does_not_exist.json"),
               std::runtime_error);
}

TEST(NeutralAtomArchitectureExceptions, ParseInvalidJsonThrows) {
  const auto tmp = std::filesystem::temp_directory_path() / "invalid_arch.json";
  {
    std::ofstream ofs(tmp);
    ofs << "{ invalid json }";
  }
  EXPECT_THROW((void)NeutralAtomArchitecture(tmp.string()), std::runtime_error);
  std::error_code ec;
  std::filesystem::remove(tmp, ec);
}

TEST(NeutralAtomArchitectureExceptions, TooManyQubitsThrows) {
  const auto tmp =
      std::filesystem::temp_directory_path() / "too_many_qubits.json";
  // Minimal JSON: 1x1 positions but nQubits = 2 -> should throw
  {
    const auto* content = R"JSON({
		"name": "test",
		"properties": {
			"nRows": 1,
			"nColumns": 1,
			"nAods": 1,
			"nAodCoordinates": 1,
			"interQubitDistance": 1.0,
			"interactionRadius": 1.0,
			"blockingFactor": 1.0,
			"minimalAodDistance": 1.0
		},
		"parameters": {
			"nQubits": 2
		}
	})JSON";
    std::ofstream ofs(tmp);
    ofs << content;
  }
  EXPECT_THROW((void)NeutralAtomArchitecture(tmp.string()), std::runtime_error);
  std::error_code ec;
  std::filesystem::remove(tmp, ec);
}

TEST(NeutralAtomArchitectureExceptions, GetGateTimeFallbackToNone) {
  const auto arch =
      NeutralAtomArchitecture("architectures/rubidium_shuttling.json");
  const auto fallback = arch.getGateTime("none");
  const auto missing = arch.getGateTime("this_gate_name_should_not_exist");
  EXPECT_DOUBLE_EQ(missing, fallback);
  const auto fallbackFid = arch.getGateAverageFidelity("none");
  const auto missingFid =
      arch.getGateAverageFidelity("this_gate_name_should_not_exist");
  EXPECT_DOUBLE_EQ(missingFid, fallbackFid);
}
