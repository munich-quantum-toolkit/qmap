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

#include "na/zoned/Types.hpp"

#include <unordered_set>
#include <vector>

namespace na::zoned {
/**
 * The Abstract Base Class for the Placer of the MQT's Zoned Neutral Atom
 * Compiler.
 */
class PlacerBase {
public:
  virtual ~PlacerBase() = default;

  /**
   * This function defines the interface of the placer.
   * @param nQubits denotes the number of qubits to be placed
   * @param twoQubitGateLayers are the qubits that must be placed for each layer
   * @param reuseQubits are the qubits that are reused in the next stage
   */
  [[nodiscard]] virtual auto
  place(size_t nQubits,
        const std::vector<TwoQubitGateLayer>& twoQubitGateLayers,
        const std::vector<std::unordered_set<qc::Qubit>>& reuseQubits)
      -> std::vector<Placement> = 0;
};
} // namespace na::zoned
