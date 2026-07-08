/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "na/zoned/decomposer/NoOpDecomposer.hpp"

#include "ir/QuantumComputation.hpp"
#include "na/zoned/Architecture.hpp"

#include <vector>

namespace na::zoned {
auto NoOpDecomposer::decompose(
    [[maybe_unused]] const size_t nQubits,
    const std::vector<SingleQubitGateRefLayer>& singleQubitGateLayers,
    const std::vector<TwoQubitGateLayer>& twoQubitGateLayers) const
    -> DecompositionResult {
  std::vector<SingleQubitGateLayer> result;
  result.reserve(singleQubitGateLayers.size());
  for (const auto& layer : singleQubitGateLayers) {
    SingleQubitGateLayer newLayer;
    newLayer.reserve(layer.size());
    for (const auto& opRef : layer) {
      newLayer.emplace_back(opRef.get().clone());
    }
    result.emplace_back(std::move(newLayer));
  }
  return {std::move(result), twoQubitGateLayers};
}
} // namespace na::zoned
