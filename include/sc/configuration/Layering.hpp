/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

enum class Layering : std::uint8_t {
  IndividualGates,
  DisjointQubits,
  OddGates,
  QubitTriangle,
  Disjoint2qBlocks
};

[[maybe_unused]] static inline std::string toString(const Layering strategy) {
  switch (strategy) {
  case Layering::IndividualGates:
    return "individual_gates";
  case Layering::DisjointQubits:
    return "disjoint_qubits";
  case Layering::OddGates:
    return "odd_gates";
  case Layering::QubitTriangle:
    return "qubit_triangle";
  case Layering::Disjoint2qBlocks:
    return "disjoint_2q_blocks";
  }
  return " ";
}

[[maybe_unused]] static Layering
layeringFromString(const std::string& layering) {
  if (layering == "individual_gates" || layering == "0") {
    return Layering::IndividualGates;
  }
  if (layering == "disjoint_qubits" || layering == "1") {
    return Layering::DisjointQubits;
  }
  if (layering == "odd_gates" || layering == "2") {
    return Layering::OddGates;
  }
  if (layering == "qubit_triangle" || layering == "3") {
    return Layering::QubitTriangle;
  }
  if (layering == "disjoint_2q_blocks" || layering == "4") {
    return Layering::Disjoint2qBlocks;
  }
  throw std::invalid_argument("Invalid layering value: " + layering);
}
