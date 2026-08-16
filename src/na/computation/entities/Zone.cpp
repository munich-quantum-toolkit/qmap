/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "na/computation/entities/Zone.hpp"

#include "na/computation/entities/Location.hpp"

#include <stdexcept>

namespace na {

auto Zone::contains(const Location& location) const -> bool {
  if (!extent_) {
    throw std::runtime_error("Zone's extent is not set.");
  }
  return extent_->minX <= location.x && location.x <= extent_->maxX &&
         extent_->minY <= location.y && location.y <= extent_->maxY;
}
} // namespace na
