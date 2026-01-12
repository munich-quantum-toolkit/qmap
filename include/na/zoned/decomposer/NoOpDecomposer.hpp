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

#include "na/zoned/Architecture.hpp"
#include "na/zoned/Types.hpp"
#include "na/zoned/decomposer/DecomposerBase.hpp"

#include <nlohmann/json.hpp>
#include <vector>

namespace na::zoned {
/**
 * The class NoOpDecomposer implements a dummy no-op decomposer that just copies
 * every operation.
 */
class NoOpDecomposer : public DecomposerBase {

public:
  /// The configuration of the NoOpDecomposer
  struct Config {
    template <
        typename BasicJsonType,
        nlohmann::detail::enable_if_t<
            nlohmann::detail::is_basic_json<BasicJsonType>::value, int> = 0>
    friend void to_json(BasicJsonType& /* unused */,
                        const Config& /* unused */) {}

    template <
        typename BasicJsonType,
        nlohmann::detail::enable_if_t<
            nlohmann::detail::is_basic_json<BasicJsonType>::value, int> = 0>
    friend void from_json(const BasicJsonType& /* unused */,
                          Config& /* unused */) {}
  };

  /**
   * Create a new NoOpDecomposer.
   */
  NoOpDecomposer(const Architecture& /* unused */, const Config& /* unused */) {
  }

  [[nodiscard]] auto decompose(
      const std::vector<SingleQubitGateRefLayer>& singleQubitGateLayers) const
      -> std::vector<SingleQubitGateLayer> override;
};
} // namespace na::zoned
