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
 * The Abstract Base Class for the Reuse Analyzer of the MQT's Zoned Neutral
 * Atom Compiler.
 */
class ReuseAnalyzerBase {
public:
  virtual ~ReuseAnalyzerBase() = default;
  /**
   * This function defines the interface of the reuse analyzer.
   * @param twoQubitGateLayers are the pairs of qubits to execute CZ-gates on
   * for each layer
   * @return a set of qubits for each layer that can be reused in the next layer
   */
  [[nodiscard]] virtual auto
  analyzeReuse(const std::vector<TwoQubitGateLayer>& twoQubitGateLayers)
      -> std::vector<std::unordered_set<qc::Qubit>> = 0;
};
} // namespace na::zoned
