/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "hybridmap/HardwareQubits.hpp"
#include "hybridmap/NeutralAtomArchitecture.hpp"
#include "hybridmap/NeutralAtomDefinitions.hpp"
#include "hybridmap/NeutralAtomUtils.hpp"

#include <cstddef>
#include <gtest/gtest.h>
#include <set>
#include <stdexcept>

namespace {

TEST(HardwareQubitsExceptions, AccessEmptyCoordThrows) {
  const auto arch =
      na::NeutralAtomArchitecture("architectures/rubidium_shuttling.json");
  na::HardwareQubits const hw(arch, 2, na::InitialCoordinateMapping::Trivial,
                              0);

  constexpr auto emptyCoord = static_cast<na::CoordIndex>(3);
  EXPECT_THROW(static_cast<void>(hw.getHwQubit(emptyCoord)),
               std::runtime_error);
}

TEST(HardwareQubitsExceptions, MoveInvalidCoordinateThrows) {
  const auto arch =
      na::NeutralAtomArchitecture("architectures/rubidium_shuttling.json");
  na::HardwareQubits hw(arch, /*nQubits*/ 2,
                        na::InitialCoordinateMapping::Trivial, /*seed*/ 0);

  // Coordinate equal to Npositions is out of range
  const auto invalidCoord = arch.getNpositions();
  EXPECT_THROW(hw.move(/*hwQubit*/ 0, invalidCoord), std::runtime_error);
}

TEST(HardwareQubitsExceptions, MoveToOccupiedCoordinateThrows) {
  const auto arch =
      na::NeutralAtomArchitecture("architectures/rubidium_shuttling.json");
  na::HardwareQubits hw(arch, /*nQubits*/ 2,
                        na::InitialCoordinateMapping::Trivial, /*seed*/ 0);

  const auto occupied = hw.getCoordIndex(/*hwQubit*/ 1);
  EXPECT_THROW(hw.move(/*hwQubit*/ 0, occupied), std::runtime_error);
}

TEST(HardwareQubitsBehavior, RemoveHwQubitRemovesMappingsAndNeighbors) {
  const auto arch =
      na::NeutralAtomArchitecture("architectures/rubidium_shuttling.json");
  na::HardwareQubits hw(arch, /*nQubits*/ 3,
                        na::InitialCoordinateMapping::Trivial, /*seed*/ 0);

  // Remove middle qubit (1) and verify it is no longer addressable
  hw.removeHwQubit(/*hwQubit*/ 1);

  // getCoordIndex should throw since the qubit was erased from the mapping
  EXPECT_THROW((void)hw.getCoordIndex(1), std::out_of_range);

  // Remaining qubits neighbor lists should not contain the removed qubit
  for (na::HwQubit const q : {0U, 2U}) {
    const auto neighbors = hw.getNearbyQubits(q);
    EXPECT_TRUE(!neighbors.contains(1U));
  }
}

TEST(HardwareQubitsBehavior, RandomInitializationIsDeterministicPerSeed) {
  const auto arch =
      na::NeutralAtomArchitecture("architectures/rubidium_shuttling.json");
  constexpr na::CoordIndex nQ = 4;

  na::HardwareQubits const hw(arch, nQ, na::InitialCoordinateMapping::Random,
                              /*seed*/ 0);

  // All assigned coordinates are unique and within bounds
  std::set<na::CoordIndex> coords;
  for (na::HwQubit q = 0; q < nQ; ++q) {
    const auto c = hw.getCoordIndex(q);
    EXPECT_LT(c, arch.getNpositions());
    coords.insert(c);
  }
  EXPECT_EQ(coords.size(), static_cast<size_t>(nQ));
}

} // namespace
