/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "na/qdmi/Configuration.hpp"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <gtest/gtest.h>
#include <limits>
#include <nlohmann/json.hpp> // NOLINT(misc-include-cleaner)
#include <nlohmann/json_fwd.hpp>
#include <stdexcept>
#include <utility>
#include <vector>

namespace na {
TEST(NaConfigurationTest, ParsesBundledDeviceStrictly) {
  const auto device = readJSON(NA_DEVICE_JSON);
  EXPECT_EQ(device.schemaVersion, 1);
  EXPECT_EQ(device.name, "MQT QMAP NA Default QDMI Device");
  EXPECT_EQ(device.numQubits, 100);
}

TEST(NaConfigurationTest, RejectsUnknownMissingAndInvalidValues) {
  std::ifstream input(NA_DEVICE_JSON);
  ASSERT_TRUE(input);
  nlohmann::json json;
  input >> json;

  auto unknown = json;
  unknown["unknown"] = true;
  EXPECT_THROW(static_cast<void>(readJSON(unknown.dump(), "inline")),
               std::invalid_argument);

  auto missing = json;
  missing.erase("traps");
  EXPECT_THROW(static_cast<void>(readJSON(missing.dump(), "inline")),
               std::invalid_argument);

  auto invalidFidelity = json;
  invalidFidelity["localSingleQubitOperations"][0]["fidelity"] = 2.;
  EXPECT_THROW(static_cast<void>(readJSON(invalidFidelity.dump(), "inline")),
               std::invalid_argument);

  auto singleQubitGlobalMulti = json;
  singleQubitGlobalMulti["globalMultiQubitOperations"][0]["numQubits"] = 1;
  EXPECT_THROW(
      static_cast<void>(readJSON(singleQubitGlobalMulti.dump(), "inline")),
      std::invalid_argument);

  auto invalidUnit = json;
  invalidUnit["durationUnit"]["unit"] = "ts";
  EXPECT_THROW(static_cast<void>(readJSON(invalidUnit.dump(), "inline")),
               std::invalid_argument);

  auto negativeExtent = json;
  negativeExtent["traps"][0]["extent"]["size"]["width"] = -1;
  EXPECT_THROW(static_cast<void>(readJSON(negativeExtent.dump(), "inline")),
               std::invalid_argument);

  auto unknownNested = json;
  unknownNested["traps"][0]["extent"]["unexpected"] = true;
  EXPECT_THROW(static_cast<void>(readJSON(unknownNested.dump(), "inline")),
               std::invalid_argument);

  auto missingNested = json;
  missingNested["localSingleQubitOperations"][0].erase("duration");
  EXPECT_THROW(static_cast<void>(readJSON(missingNested.dump(), "inline")),
               std::invalid_argument);

  auto unsupportedSchema = json;
  unsupportedSchema["schema-version"] = 2;
  EXPECT_THROW(static_cast<void>(readJSON(unsupportedSchema.dump(), "inline")),
               std::invalid_argument);

  auto incorrectType = json;
  incorrectType["numQubits"] = "many";
  EXPECT_THROW(static_cast<void>(readJSON(incorrectType.dump(), "inline")),
               std::invalid_argument);

  auto outOfRangeCoordinate = json;
  outOfRangeCoordinate["traps"][0]["latticeOrigin"]["x"] =
      static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) + 1;
  EXPECT_THROW(
      static_cast<void>(readJSON(outOfRangeCoordinate.dump(), "inline")),
      std::invalid_argument);

  auto degenerateLattice = json;
  degenerateLattice["traps"][0]["latticeVector2"] =
      degenerateLattice["traps"][0]["latticeVector1"];
  EXPECT_THROW(static_cast<void>(readJSON(degenerateLattice.dump(), "inline")),
               std::invalid_argument);

  auto duplicateCoordinates = json;
  duplicateCoordinates["traps"].push_back(duplicateCoordinates["traps"][0]);
  EXPECT_THROW(
      static_cast<void>(readJSON(duplicateCoordinates.dump(), "inline")),
      std::invalid_argument);

  auto overflowingExtent = json;
  overflowingExtent["traps"][0]["extent"]["size"]["width"] =
      std::numeric_limits<uint64_t>::max();
  EXPECT_THROW(static_cast<void>(readJSON(overflowingExtent.dump(), "inline")),
               std::invalid_argument);

  auto excessiveSiteCount = json;
  excessiveSiteCount["traps"][0]["extent"]["size"]["width"] =
      std::numeric_limits<int64_t>::max();
  EXPECT_THROW(static_cast<void>(readJSON(excessiveSiteCount.dump(), "inline")),
               std::invalid_argument);

  auto excessivePairWork = json;
  excessivePairWork["traps"][0]["extent"]["size"]["width"] = 2499;
  excessivePairWork["traps"][0]["extent"]["size"]["height"] = 1;
  excessivePairWork["localMultiQubitOperations"][0]["region"] =
      excessivePairWork["traps"][0]["extent"];
  EXPECT_THROW(static_cast<void>(readJSON(excessivePairWork.dump(), "inline")),
               std::invalid_argument);

  auto emptyOffsets = json;
  emptyOffsets["traps"][0]["sublatticeOffsets"] = nlohmann::json::array();
  EXPECT_THROW(static_cast<void>(readJSON(emptyOffsets.dump(), "inline")),
               std::invalid_argument);
}

TEST(NaConfigurationTest, PreservesCoordinatesNearSignedMaximum) {
  std::ifstream input(NA_DEVICE_JSON);
  ASSERT_TRUE(input);
  nlohmann::json json;
  input >> json;

  constexpr auto maximum = std::numeric_limits<int64_t>::max();
  constexpr auto origin = maximum - 2;
  auto& trap = json["traps"][0];
  trap["latticeOrigin"] = {{"x", origin}, {"y", origin}};
  trap["latticeVector1"] = {{"x", 1}, {"y", 0}};
  trap["latticeVector2"] = {{"x", 0}, {"y", 1}};
  trap["sublatticeOffsets"] = {{{"x", 0}, {"y", 0}}};
  trap["extent"] = {{"origin", {{"x", origin}, {"y", origin}}},
                    {"size", {{"width", 2}, {"height", 2}}}};
  json["numQubits"] = 9;

  const auto device = readJSON(json.dump(), "inline");
  std::vector<std::pair<int64_t, int64_t>> coordinates;
  forEachRegularSites(device.traps, [&](const SiteInfo& site) {
    coordinates.emplace_back(site.x, site.y);
  });

  EXPECT_EQ(coordinates.size(), 9);
  EXPECT_NE(std::ranges::find(coordinates, std::pair{maximum, maximum}),
            coordinates.end());
}

TEST(NaConfigurationTest, AcceptsLargeLatticeVectors) {
  std::ifstream input(NA_DEVICE_JSON);
  ASSERT_TRUE(input);
  nlohmann::json json;
  input >> json;

  auto& trap = json["traps"][0];
  trap["latticeOrigin"] = {{"x", 0}, {"y", 0}};
  trap["latticeVector1"] = {{"x", 4'000'000'000}, {"y", 0}};
  trap["latticeVector2"] = {{"x", 0}, {"y", 4'000'000'000}};
  trap["sublatticeOffsets"] = {{{"x", 0}, {"y", 0}}};
  trap["extent"] = {{"origin", {{"x", 0}, {"y", 0}}},
                    {"size", {{"width", 1}, {"height", 1}}}};
  json["numQubits"] = 1;

  const auto device = readJSON(json.dump(), "inline");
  std::vector<std::pair<int64_t, int64_t>> coordinates;
  forEachRegularSites(device.traps, [&](const SiteInfo& site) {
    coordinates.emplace_back(site.x, site.y);
  });

  EXPECT_EQ(coordinates, std::vector({std::pair<int64_t, int64_t>{0, 0}}));
}

} // namespace na
