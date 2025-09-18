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

#include <vector>

namespace na::zoned {

/**
 * The Abstract Base Class for the Router of the MQT's Zoned Neutral Atom
 * Compiler.
 */
class RouterBase {
public:
  virtual ~RouterBase() = default;
  /**
   * This function defines the interface of the router.
   * @param placement is a vector of the atoms' placement at every layer
   * @return the routing, i.e., for every transition between two placements a
   * vector of groups containing atoms that can be moved simultaneously
   */
  [[nodiscard]] virtual auto
  route(const std::vector<Placement>& placement) const
      -> std::vector<Routing> = 0;
};
} // namespace na::zoned
