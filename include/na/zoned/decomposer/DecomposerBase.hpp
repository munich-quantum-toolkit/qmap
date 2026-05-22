/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#pragma once

#include "na/zoned/Types.hpp"

#include <vector>

namespace na::zoned {
/**
 * The Abstract Base Class for the Decomposer of the MQT's Zoned Neutral Atom
 * Compiler.
 */
class DecomposerBase {
public:
  virtual ~DecomposerBase() = default;
  /**
   * This function defines the interface of the decomposer.
   *
   * The decomposer may change the layering produced by the scheduler and,
   * hence, it receives the single-qubit and two-qubit gate layers.
   * @param singleQubitGateLayers are the layers of single-qubit gates that are
   * meant to be first decomposed into the native gate set.
   * @param twoQubitGateLayers are the layers of two-qubit gates that the
   * decomposer may change.
   * @return a DecompositionResult whose @p singleQubitLayers replaces the
   * scheduler's single-qubit layers (consumed by generate()) and whose @p
   * twoQubitLayers replaces the scheduler's two-qubit layers (consumed by
   * analyzeReuse() and synthesize()). There is always one single-qubit gate
   * layer more than two-qubit gate layers.
   */
  [[nodiscard]] virtual auto
  decompose(const std::vector<SingleQubitGateRefLayer>& singleQubitGateLayers,
            const std::vector<TwoQubitGateLayer>& twoQubitGateLayers) const
      -> DecompositionResult = 0;
};
} // namespace na::zoned
