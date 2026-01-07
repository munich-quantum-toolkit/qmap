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
#include "hybridmap/Mapping.hpp"
#include "hybridmap/NeutralAtomArchitecture.hpp"
#include "hybridmap/NeutralAtomUtils.hpp"
#include "ir/QuantumComputation.hpp"

#include <gtest/gtest.h>
#include <stdexcept>

// These tests focus only on exception behavior in Mapping (mapping.cpp/.hpp).

TEST(MappingExceptions, CircuitExceedsHardwareThrows) {
  const auto arch =
      na::NeutralAtomArchitecture("architectures/rubidium_shuttling.json");
  // Hardware has 1 available logical qubit, circuit needs 2
  na::HardwareQubits const hw(
      arch, /*nQubits*/ 1, na::InitialCoordinateMapping::Trivial, /*seed*/ 0);
  qc::QuantumComputation qc(2);

  EXPECT_THROW((void)na::Mapping(2, na::InitialMapping::Identity, qc, hw),
               std::runtime_error);
}

TEST(MappingExceptions, GetCircQubitThrowsIfHardwareNotMapped) {
  const auto arch =
      na::NeutralAtomArchitecture("architectures/rubidium_shuttling.json");
  // Hardware has 4 logical spots, but circuit uses only 2 (identity mapping)
  na::HardwareQubits const hw(
      arch, /*nQubits*/ 4, na::InitialCoordinateMapping::Trivial, /*seed*/ 0);
  qc::QuantumComputation qc(2);
  na::Mapping const m(2, na::InitialMapping::Identity, qc, hw);

  // hw qubits 0 and 1 are mapped; 2 and 3 are not -> getCircQubit(2) should
  // throw
  EXPECT_THROW((void)m.getCircQubit(2), std::runtime_error);
}

TEST(MappingExceptions, ApplySwapThrowsIfBothHardwareQubitsUnmapped) {
  const auto arch =
      na::NeutralAtomArchitecture("architectures/rubidium_shuttling.json");
  // Hardware has 4, circuit maps only 2 via identity
  na::HardwareQubits const hw(
      arch, /*nQubits*/ 4, na::InitialCoordinateMapping::Trivial, /*seed*/ 0);
  qc::QuantumComputation qc(2);
  na::Mapping m(2, na::InitialMapping::Identity, qc, hw);

  // Swap two unmapped hardware qubits (2 and 3) -> should throw
  EXPECT_THROW(m.applySwap({2, 3}), std::runtime_error);
}

TEST(MappingExceptions, GetHwQubitThrowsOutOfRangeForInvalidCircuitIndex) {
  const auto arch =
      na::NeutralAtomArchitecture("architectures/rubidium_shuttling.json");
  na::HardwareQubits const hw(
      arch, /*nQubits*/ 2, na::InitialCoordinateMapping::Trivial, /*seed*/ 0);
  qc::QuantumComputation qc(2);
  na::Mapping const m(2, na::InitialMapping::Identity, qc, hw);

  // Access circuit qubit index outside [0, nQubits) -> std::out_of_range
  EXPECT_THROW((void)m.getHwQubit(2), std::out_of_range);
}
