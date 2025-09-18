//
// This file is part of the MQT QMAP library released under the MIT license.
// See README.md or go to https://github.com/cda-tum/qmap for more information.
//

#include "configuration/Configuration.hpp"
#include "configuration/Method.hpp"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

#pragma once

struct MappingResults {
  struct CircuitInfo {
    // general info
    std::string name;
    std::uint16_t qubits = 0;
    std::size_t gates = 0;
    std::size_t singleQubitGates = 0;
    std::size_t cnots = 0;
    std::size_t layers = 0;
    double totalFidelity = 1.;
    // higher precision than totalFidelity because larger part of double's
    // representation space is used
    double totalLogFidelity = 0.;

    // info in output circuit
    std::size_t swaps = 0;
    std::size_t directionReverse = 0;
  };

  struct HeuristicBenchmarkInfo {
    std::size_t expandedNodes = 0;
    std::size_t generatedNodes = 0;
    double secondsPerNode = 0.;
    double averageBranchingFactor = 0.;
    double effectiveBranchingFactor = 0.;

    [[nodiscard]] nlohmann::basic_json<> json() const {
      nlohmann::basic_json resultJSON{};
      resultJSON["expanded_nodes"] = expandedNodes;
      resultJSON["generated_nodes"] = generatedNodes;
      resultJSON["seconds_per_node"] = secondsPerNode;
      resultJSON["average_branching_factor"] = averageBranchingFactor;
      resultJSON["effective_branching_factor"] = effectiveBranchingFactor;
      return resultJSON;
    }
  };

  struct LayerHeuristicBenchmarkInfo {
    std::size_t expandedNodes = 0;
    std::size_t generatedNodes = 0;
    std::size_t expandedNodesAfterFirstSolution = 0;
    std::size_t expandedNodesAfterOptimalSolution = 0;
    std::size_t solutionNodes = 0;
    std::size_t solutionNodesAfterOptimalSolution = 0;
    std::size_t solutionDepth = 0;
    double secondsPerNode = 0.;
    double averageBranchingFactor = 0.;
    double effectiveBranchingFactor = 0.;
    bool earlyTermination = false;

    [[nodiscard]] nlohmann::basic_json<> json() const {
      nlohmann::basic_json resultJSON{};
      resultJSON["expanded_nodes"] = expandedNodes;
      resultJSON["generated_nodes"] = generatedNodes;
      resultJSON["expanded_nodes_after_first_solution"] =
          expandedNodesAfterFirstSolution;
      resultJSON["expanded_nodes_after_optimal_solution"] =
          expandedNodesAfterOptimalSolution;
      resultJSON["solution_nodes"] = solutionNodes;
      resultJSON["solution_nodes_after_optimal_solution"] =
          solutionNodesAfterOptimalSolution;
      resultJSON["solution_depth"] = solutionDepth;
      resultJSON["seconds_per_node"] = secondsPerNode;
      resultJSON["average_branching_factor"] = averageBranchingFactor;
      resultJSON["effective_branching_factor"] = effectiveBranchingFactor;
      resultJSON["early_termination"] = earlyTermination;
      return resultJSON;
    }
  };

  CircuitInfo input{};

  std::string architecture;
  Configuration config{};

  double time = 0.0;
  bool timeout = true;

  CircuitInfo output{};
  std::string mappedCircuit;

  std::string wcnf;

  HeuristicBenchmarkInfo heuristicBenchmark{};
  std::vector<LayerHeuristicBenchmarkInfo> layerHeuristicBenchmark;

  MappingResults() = default;
  virtual ~MappingResults() = default;

  virtual void copyInput(const MappingResults& mappingResults) {
    input = mappingResults.input;
    architecture = mappingResults.architecture;
    config = mappingResults.config;
    output = mappingResults.output;
    wcnf = mappingResults.wcnf;
    heuristicBenchmark = mappingResults.heuristicBenchmark;
    layerHeuristicBenchmark = mappingResults.layerHeuristicBenchmark;
  }

  [[nodiscard]] std::string toString() const { return json().dump(2); }

  [[nodiscard]] virtual nlohmann::basic_json<> json() const {
    nlohmann::basic_json resultJSON{};

    auto& circuit = resultJSON["circuit"];
    circuit["name"] = input.name;
    circuit["qubits"] = input.qubits;
    circuit["gates"] = input.gates;
    circuit["single_qubit_gates"] = input.singleQubitGates;
    circuit["cnots"] = input.cnots;

    auto& mappedCirc = resultJSON["mapped_circuit"];
    mappedCirc["name"] = output.name;
    mappedCirc["qubits"] = output.qubits;
    mappedCirc["gates"] = output.gates;
    mappedCirc["single_qubit_gates"] = output.singleQubitGates;
    mappedCirc["cnots"] = output.cnots;
    if (!mappedCircuit.empty()) {
      mappedCirc["qasm"] = mappedCircuit;
    }

    resultJSON["config"] = config.json();

    auto& stats = resultJSON["statistics"];
    stats["timeout"] = timeout;
    stats["mapping_time"] = time;
    stats["arch"] = architecture;
    stats["layers"] = input.layers;
    stats["swaps"] = output.swaps;
    stats["total_fidelity"] = output.totalFidelity;
    stats["total_log_fidelity"] = output.totalLogFidelity;
    if (config.method == Method::Exact) {
      stats["direction_reverse"] = output.directionReverse;
      if (config.includeWCNF && !wcnf.empty()) {
        stats["WCNF"] = wcnf;
      }
    } else if (config.method == Method::Heuristic) {
      stats["benchmark"] = heuristicBenchmark.json();
    }
    stats["additional_gates"] =
        static_cast<std::make_signed_t<decltype(output.gates)>>(output.gates) -
        static_cast<std::make_signed_t<decltype(input.gates)>>(input.gates);

    return resultJSON;
  }
};
