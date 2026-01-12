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
   * @param singleQubitGateLayers are the layers of single-qubit gates that are
   * meant to be first decomposed into the native gate set.
   * @return the new single-qubit gate layers
   */
  [[nodiscard]] virtual auto decompose(
      const std::vector<SingleQubitGateRefLayer>& singleQubitGateLayers) const
      -> std::vector<SingleQubitGateLayer> = 0;
};
} // namespace na::zoned
