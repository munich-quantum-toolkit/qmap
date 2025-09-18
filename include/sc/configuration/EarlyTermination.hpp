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
#include <stdexcept>
#include <string>

enum class EarlyTermination : std::uint8_t {
  None,
  ExpandedNodes,
  ExpandedNodesAfterFirstSolution,
  ExpandedNodesAfterCurrentOptimalSolution,
  SolutionNodes,
  SolutionNodesAfterCurrentOptimalSolution
};

[[maybe_unused]] static inline std::string
toString(const EarlyTermination earlyTermination) {
  switch (earlyTermination) {
  case EarlyTermination::None:
    return "none";
  case EarlyTermination::ExpandedNodes:
    return "expanded_nodes";
  case EarlyTermination::ExpandedNodesAfterFirstSolution:
    return "expanded_nodes_after_first_solution";
  case EarlyTermination::ExpandedNodesAfterCurrentOptimalSolution:
    return "expanded_nodes_after_current_optimal_solution";
  case EarlyTermination::SolutionNodes:
    return "solution_nodes";
  case EarlyTermination::SolutionNodesAfterCurrentOptimalSolution:
    return "solution_nodes_after_current_optimal_solution";
  }
  return " ";
}

[[maybe_unused]] static EarlyTermination
earlyTerminationFromString(const std::string& earlyTermination) {
  if (earlyTermination == "none" || earlyTermination == "0") {
    return EarlyTermination::None;
  }
  if (earlyTermination == "expanded_nodes" || earlyTermination == "1") {
    return EarlyTermination::ExpandedNodes;
  }
  if (earlyTermination == "expanded_nodes_after_first_solution" ||
      earlyTermination == "2") {
    return EarlyTermination::ExpandedNodesAfterFirstSolution;
  }
  if (earlyTermination == "expanded_nodes_after_current_optimal_solution" ||
      earlyTermination == "3") {
    return EarlyTermination::ExpandedNodesAfterCurrentOptimalSolution;
  }
  if (earlyTermination == "solution_nodes" || earlyTermination == "4") {
    return EarlyTermination::SolutionNodes;
  }
  if (earlyTermination == "solution_nodes_after_current_optimal_solution" ||
      earlyTermination == "5") {
    return EarlyTermination::SolutionNodesAfterCurrentOptimalSolution;
  }
  throw std::invalid_argument("Invalid method value: " + earlyTermination);
}
