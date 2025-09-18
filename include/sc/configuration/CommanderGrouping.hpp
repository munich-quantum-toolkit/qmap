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

#include <cstdint>
#include <istream>
#include <ostream>
#include <stdexcept>
#include <string>

enum class CommanderGrouping : std::uint8_t {
  Halves,
  Fixed2,
  Fixed3,
  Logarithm
};

[[maybe_unused]] static inline std::string
toString(const CommanderGrouping grouping) {
  switch (grouping) {
  case CommanderGrouping::Fixed2:
    return "fixed2";
  case CommanderGrouping::Fixed3:
    return "fixed3";
  case CommanderGrouping::Logarithm:
    return "logarithm";
  case CommanderGrouping::Halves:
    return "halves";
  }
  return " ";
}

[[maybe_unused]] static CommanderGrouping
groupingFromString(const std::string& grouping) {
  if (grouping == "halves" || grouping == "0") {
    return CommanderGrouping::Halves;
  }
  if (grouping == "fixed2" || grouping == "1") {
    return CommanderGrouping::Fixed2;
  }
  if (grouping == "fixed3" || grouping == "2") {
    return CommanderGrouping::Fixed3;
  }
  if (grouping == "logarithm" || grouping == "3") {
    return CommanderGrouping::Logarithm;
  }
  throw std::invalid_argument("Invalid grouping value: " + grouping);
}

[[maybe_unused]] static inline std::ostream&
operator<<(std::ostream& os, const CommanderGrouping& grouping) {
  os << toString(grouping);
  return os;
}

[[maybe_unused]] static inline std::istream&
operator>>(std::istream& is, CommanderGrouping& grouping) {
  std::string s;
  is >> s;
  grouping = groupingFromString(s);
  return is;
}
