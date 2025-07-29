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
 * The Abstract Base Class for the Layout Synthesizer of the MQT's Zoned Neutral
 * Atom Compiler.
 */
class LayoutSynthsizerBase {
public:
  virtual ~LayoutSynthsizerBase() = default;
  /**
   * Collection of the placement and routing results.
   */
  struct Layout {
    std::vector<Placement> placement; ///< The placement of the qubits
    std::vector<Routing> routing;     ///< The routing of the qubits
  };
  /**
   * This function defines the interface of the layout synthesizer.
   * @param nQubits is the number of qubits in the quantum computation.
   * @param twoQubitGateLayers is a vector of two-qubit gate layers,
   * where each layer contains the two-qubit gates to be placed.
   * @param reuseQubits is a vector of qubit sets that can be reused
   * between layers.
   * @returns A Layout object containing the placement and routing results.
   */
  [[nodiscard]] virtual auto
  synthesize(size_t nQubits,
             const std::vector<TwoQubitGateLayer>& twoQubitGateLayers,
             const std::vector<std::unordered_set<qc::Qubit>>& reuseQubits)
      -> Layout = 0;
};
} // namespace na::zoned
