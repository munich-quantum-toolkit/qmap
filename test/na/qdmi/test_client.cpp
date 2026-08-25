/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "na/fomac/Device.hpp"
#include "qdmi/driver/Driver.hpp"

#include <fstream>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
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

  const auto genericDevice = qdmi_client::Session::openDevice(deviceId);
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

} // namespace na
