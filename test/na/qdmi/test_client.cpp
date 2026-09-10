/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "na/qdmi/Client.hpp"
#include "qdmi/driver/Driver.hpp"

#include <algorithm>
#include <fstream>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <utility>

namespace na {
namespace {
void canonicallyOrderLatticeVectors(nlohmann::json& device) {
  for (auto& lattice : device["traps"]) {
    const auto& first = lattice["latticeVector1"];
    const auto& second = lattice["latticeVector2"];
    if (first["x"] > second["x"] ||
        (first["x"] == second["x"] && first["y"] > second["y"])) {
      std::swap(lattice["latticeVector1"], lattice["latticeVector2"]);
    }
  }
}
} // namespace

TEST(NaQdmiClient, FullJsonRoundTrip) {
  constexpr auto deviceId = "mqt.qmap.na.test";
  static_cast<void>(
      qdmi::Driver::get().registerDeviceIfAbsent({.id = deviceId,
                                                  .library = NA_DEVICE_LIBRARY,
                                                  .prefix = "MQT_QMAP_NA",
                                                  .session = {}}));

  const auto genericDevice = qdmi::Session::openDevice(deviceId);
  const auto device = Session::Device::tryCreateFromDevice(genericDevice);
  ASSERT_TRUE(device.has_value());

  std::ifstream input(NA_DEVICE_JSON);
  ASSERT_TRUE(input.is_open()) << "Failed to open " NA_DEVICE_JSON;
  auto expected = nlohmann::json::parse(input);
  nlohmann::json actual = *device;
  canonicallyOrderLatticeVectors(expected);
  canonicallyOrderLatticeVectors(actual);

  EXPECT_EQ(actual, expected);
}

TEST(NaQdmiClient, ThreeQubitGlobalOperationRoundTrip) {
  constexpr auto deviceId = "mqt.qmap.na.test.three-qubit-global";
  static_cast<void>(
      qdmi::Driver::get().registerDeviceIfAbsent({.id = deviceId,
                                                  .library = NA_DEVICE_LIBRARY,
                                                  .prefix = "MQT_QMAP_NA",
                                                  .session = {}}));

  std::ifstream input(NA_DEVICE_JSON);
  ASSERT_TRUE(input.is_open()) << "Failed to open " NA_DEVICE_JSON;
  auto expected = nlohmann::json::parse(input);
  expected["globalMultiQubitOperations"][0]["numQubits"] = 3;

  qdmi::DeviceSessionConfig overrides;
  overrides.deviceConfiguration =
      qdmi::InlineDeviceConfiguration{.json = expected.dump()};
  const auto genericDevice = qdmi::Session::openDevice(deviceId, overrides);
  const auto device = Session::Device::tryCreateFromDevice(genericDevice);
  ASSERT_TRUE(device.has_value());

  nlohmann::json actual = *device;
  canonicallyOrderLatticeVectors(expected);
  canonicallyOrderLatticeVectors(actual);
  EXPECT_EQ(actual, expected);
}

TEST(NaQdmiClient, DiscoveryPreservesDevicesWithTheSameName) {
  std::ifstream input(NA_DEVICE_JSON);
  ASSERT_TRUE(input.is_open());
  auto configuration = nlohmann::json::parse(input);
  configuration["name"] = "QMAP discovery regression";

  for (const auto capacity : {99U, 100U}) {
    configuration["numQubits"] = capacity;
    qdmi::DeviceSessionConfig session;
    session.deviceConfiguration =
        qdmi::InlineDeviceConfiguration{.json = configuration.dump()};
    static_cast<void>(qdmi::Driver::get().registerDeviceIfAbsent(
        {.id = "mqt.qmap.na.discovery." + std::to_string(capacity),
         .library = NA_DEVICE_LIBRARY,
         .prefix = "MQT_QMAP_NA",
         .session = std::move(session)}));
  }

  const auto devices = Session::getDevices();
  for (const auto capacity : {99U, 100U}) {
    EXPECT_EQ(std::ranges::count_if(devices,
                                    [capacity](const auto& device) {
                                      return device.getName() ==
                                                 "QMAP discovery regression" &&
                                             device.getQubitsNum() == capacity;
                                    }),
              1);
  }
}

} // namespace na
