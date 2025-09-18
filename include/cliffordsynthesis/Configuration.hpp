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

#include "TargetMetric.hpp"

#include <cstddef>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <ostream>
#include <plog/Severity.h>
#include <string>
#include <thread>
#include <unordered_map>
#include <variant>

namespace cs {

using SolverParameter = std::variant<bool, std::uint32_t, double, std::string>;
using SolverParameterMap = std::unordered_map<std::string, SolverParameter>;

struct Configuration {
  Configuration() = default;

  /// General configuration for the synthesis algorithm
  std::size_t initialTimestepLimit = 0U;
  std::size_t minimalTimesteps = 0U;
  bool useMaxSAT = false;
  bool linearSearch = false;
  TargetMetric target = TargetMetric::Gates;
  bool useSymmetryBreaking = true;
  bool dumpIntermediateResults = false;
  std::string intermediateResultsPath = "./";
  plog::Severity verbosity = plog::Severity::warning;

  /// Settings for the SAT solver
  SolverParameterMap solverParameters;

  /// Settings for depth-optimal synthesis
  bool minimizeGatesAfterDepthOptimization = false;

  /// Settings for two-qubit gate-optimal synthesis
  bool tryHigherGateLimitForTwoQubitGateOptimization = false;
  double gateLimitFactor = 1.1;
  bool minimizeGatesAfterTwoQubitGateOptimization = false;

  // Settings for the heuristic solver
  bool heuristic = false;
  std::size_t splitSize = 5U;
  std::size_t nThreadsHeuristic = std::thread::hardware_concurrency();

  [[nodiscard]] nlohmann::basic_json<> json() const {
    nlohmann::basic_json j;
    j["initial_timestep_limit"] = initialTimestepLimit;
    j["minimal_timesteps"] = minimalTimesteps;
    j["use_max_sat"] = useMaxSAT;
    j["linear_search"] = linearSearch;
    j["target_metric"] = toString(target);
    j["use_symmetry_breaking"] = useSymmetryBreaking;
    j["minimize_gates_after_depth_optimization"] =
        minimizeGatesAfterDepthOptimization;
    j["try_higher_gate_limit_for_two_qubit_gate_optimization"] =
        tryHigherGateLimitForTwoQubitGateOptimization;
    j["gate_limit_factor"] = gateLimitFactor;
    j["minimize_gates_after_two_qubit_gate_optimization"] =
        minimizeGatesAfterTwoQubitGateOptimization;
    j["heuristic"] = heuristic;
    j["split_size"] = splitSize;
    j["n_threads_heuristic"] = nThreadsHeuristic;
    if (!solverParameters.empty()) {
      nlohmann::basic_json solverParametersJson;
      for (const auto& entry : solverParameters) {
        std::visit(
            [&solverParametersJson, &entry](const auto& v) {
              solverParametersJson[entry.first] = v;
            },
            entry.second);
      }
      j["solver_parameters"] = solverParametersJson;
    }
    return j;
  }

  friend std::ostream& operator<<(std::ostream& os,
                                  const Configuration& config) {
    os << config.json().dump(2);
    return os;
  }
};
} // namespace cs
