/*
 * Copyright (c) 2025 Chair for Design Automation, TUM
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "na/nalac/datastructures/operations/NAShuttlingOperation.hpp"

#include <ios>
#include <sstream>
#include <string>

namespace na::nalac {
auto NAShuttlingOperation::toString() const -> std::string {
  std::stringstream ss;
  switch (type) {
  case LOAD:
    ss << "load";
    break;
  case MOVE:
    ss << "move";
    break;
  case STORE:
    ss << "store";
    break;
  }
  ss << " ";
  for (const auto& p : start) {
    ss << *p << ", ";
  }
  ss.seekp(-2, std::ios_base::end);
  ss << " to ";
  for (const auto& p : end) {
    ss << *p << ", ";
  }
  ss.seekp(-2, std::ios_base::end);
  ss << ";\n";
  return ss.str();
}
} // namespace na::nalac
