/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "sc/DataLogger.hpp"

#include "ir/operations/CompoundOperation.hpp"
#include "ir/operations/OpType.hpp"
#include "sc/Architecture.hpp"
#include "sc/MappingResults.hpp"
#include "sc/utils.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

void DataLogger::initLog() {
  if (dataLoggingPath.back() != '/') {
    dataLoggingPath += '/';
  }
  const std::filesystem::path dirPath(dataLoggingPath);
  if (!std::filesystem::exists(dirPath)) {
    std::filesystem::create_directories(dirPath);
  }
  clearLog();
};

void DataLogger::clearLog() {
  for (const auto& entry :
       std::filesystem::directory_iterator(dataLoggingPath)) {
    std::filesystem::remove_all(entry.path());
  }
};

void DataLogger::logArchitecture() {
  if (deactivated) {
    return;
  }

  auto of = std::ofstream(dataLoggingPath + "architecture.json");
  if (!of.good()) {
    deactivated = true;
    std::cerr << "[data-logging] Error opening file: " << dataLoggingPath
              << "architecture.json" << '\n';
    return;
  }
  nlohmann::basic_json json;
  json["name"] = architecture->getName();
  json["nqubits"] = architecture->getNqubits();
  json["coupling_map"] = architecture->getCouplingMap();
  json["distances"] = architecture->getDistanceTable();
  if (architecture->isFidelityAvailable()) {
    auto& fidelity = json["fidelity"];
    fidelity["single_qubit_fidelities"] =
        architecture->getSingleQubitFidelities();
    fidelity["two_qubit_fidelities"] = architecture->getFidelityTable();
    fidelity["single_qubit_fidelity_costs"] =
        architecture->getSingleQubitFidelityCosts();
    fidelity["two_qubit_fidelity_costs"] =
        architecture->getTwoQubitFidelityCosts();
    fidelity["swap_fidelity_costs"] = architecture->getSwapFidelityCosts();
    fidelity["fidelity_distances"] = architecture->getFidelityDistanceTables();
  }
  of << json.dump(2);
  of.close();
};

void DataLogger::openNewLayer(std::size_t layerIndex) {
  if (deactivated) {
    return;
  }

  for (std::size_t i = searchNodesLogFiles.size(); i <= layerIndex; ++i) {
    searchNodesLogFiles.emplace_back(dataLoggingPath + "nodes_layer_" +
                                     std::to_string(i) + ".csv");
    if (!searchNodesLogFiles.at(i).good()) {
      deactivated = true;
      std::cerr << "[data-logging] Error opening file: " << dataLoggingPath
                << "layer_" << i << ".json" << '\n';
      return;
    }
  }
};

void DataLogger::logFinalizeLayer(
    std::size_t layerIndex, const qc::CompoundOperation& ops,
    const std::vector<std::uint16_t>& singleQubitMultiplicity,
    const std::map<std::pair<std::uint16_t, std::uint16_t>,
                   std::pair<std::uint16_t, std::uint16_t>>&
        twoQubitMultiplicity,
    const std::vector<std::int16_t>& initialLayout, std::size_t finalNodeId,
    double finalCostFixed, double finalCostHeur, double finalLookaheadPenalty,
    const std::vector<std::int16_t>& finalLayout,
    const std::vector<Exchange>& finalSwaps, std::size_t finalSearchDepth) {
  if (deactivated) {
    return;
  }

  if (!searchNodesLogFiles.at(layerIndex).is_open()) {
    std::cerr << "[data-logging] Error: layer " << layerIndex
              << " has already been finalized" << '\n';
    return;
  }
  searchNodesLogFiles.at(layerIndex).close();

  auto of = std::ofstream(dataLoggingPath + "layer_" +
                          std::to_string(layerIndex) + ".json");
  if (!of.good()) {
    deactivated = true;
    std::cerr << "[data-logging] Error opening file: " << dataLoggingPath
              << "layer_" << layerIndex << ".json" << '\n';
    return;
  }
  nlohmann::basic_json json;
  std::stringstream qasmStream;
  ops.dumpOpenQASM3(qasmStream, qregs, cregs);
  json["qasm"] = qasmStream.str();
  if (twoQubitMultiplicity.empty()) {
    json["two_qubit_multiplicity"] = nlohmann::basic_json<>::array();
  } else {
    auto& twoMultJSON = json["two_qubit_multiplicity"];
    std::size_t j = 0;
    for (const auto& [qubits, multiplicity] : twoQubitMultiplicity) {
      twoMultJSON[j]["q1"] = qubits.first;
      twoMultJSON[j]["q2"] = qubits.second;
      twoMultJSON[j]["forward"] = multiplicity.first;
      twoMultJSON[j]["backward"] = multiplicity.second;
      ++j;
    }
  }
  json["single_qubit_multiplicity"] = singleQubitMultiplicity;
  auto& initialLayoutJSON = json["initial_layout"];
  if (initialLayout.empty()) {
    for (std::size_t i = 0; i < nqubits; ++i) {
      initialLayoutJSON[i] = -1;
    }
  } else {
    for (std::size_t i = 0; i < nqubits; ++i) {
      initialLayoutJSON[i] = initialLayout.at(i);
    }
  }
  json["final_node_id"] = finalNodeId;
  json["final_cost_fixed"] = finalCostFixed;
  json["final_cost_heur"] = finalCostHeur;
  json["final_lookahead_penalty"] = finalLookaheadPenalty;
  auto& finalLayoutJSON = json["final_layout"];
  if (finalLayout.empty()) {
    for (std::size_t i = 0; i < nqubits; ++i) {
      finalLayoutJSON[i] = -1;
    }
  } else {
    for (std::size_t i = 0; i < nqubits; ++i) {
      finalLayoutJSON[i] = finalLayout.at(i);
    }
  }
  if (finalSwaps.empty()) {
    json["final_swaps"] = nlohmann::basic_json<>::array();
  } else {
    auto& finalSwapsJSON = json["final_swaps"];
    std::size_t i = 0;
    for (const auto& swap : finalSwaps) {
      finalSwapsJSON[i][0] = swap.first;
      finalSwapsJSON[i][1] = swap.second;
      ++i;
    }
  }
  json["final_search_depth"] = finalSearchDepth;
  of << json.dump(2);
  of.close();
};

void DataLogger::splitLayer() {
  if (deactivated) {
    return;
  }

  const std::size_t layerIndex = searchNodesLogFiles.size() - 1;
  if (searchNodesLogFiles.at(layerIndex).is_open()) {
    std::cerr << "[data-logging] Error: layer " << layerIndex
              << " has not been finalized before splitting" << '\n';
    return;
  }
  searchNodesLogFiles.pop_back();
  std::size_t splitIndex = 0;
  while (std::filesystem::exists(dataLoggingPath + "nodes_layer_" +
                                 std::to_string(layerIndex) + ".presplit-" +
                                 std::to_string(splitIndex) + ".csv")) {
    ++splitIndex;
  }
  std::filesystem::rename(
      dataLoggingPath + "nodes_layer_" + std::to_string(layerIndex) + ".csv",
      dataLoggingPath + "nodes_layer_" + std::to_string(layerIndex) +
          ".presplit-" + std::to_string(splitIndex) + ".csv");
  std::filesystem::rename(
      dataLoggingPath + "layer_" + std::to_string(layerIndex) + ".json",
      dataLoggingPath + "layer_" + std::to_string(layerIndex) + ".presplit-" +
          std::to_string(splitIndex) + ".json");
}

void DataLogger::logSearchNode(std::size_t layerIndex, std::size_t nodeId,
                               std::size_t parentId, double costFixed,
                               double costHeur, double lookaheadPenalty,
                               const std::vector<std::int16_t>& qubits,
                               bool validMapping,
                               const std::vector<Exchange>& swaps,
                               std::size_t depth) {
  if (deactivated) {
    return;
  }

  if (layerIndex >= searchNodesLogFiles.size()) {
    openNewLayer(layerIndex);
  }

  auto& of = searchNodesLogFiles.at(layerIndex);
  if (!of.is_open()) {
    deactivated = true;
    std::cerr << "[data-logging] Error: layer " << layerIndex
              << " has already been finalized" << '\n';
    return;
  }
  of << nodeId << ";" << parentId << ";" << costFixed << ";" << costHeur << ";"
     << lookaheadPenalty << ";" << validMapping << ";" << depth << ";";

  if (!qubits.empty()) {
    for (std::size_t i = 0; i < nqubits; ++i) {
      of << qubits.at(i) << ",";
    }
  } else {
    for (std::size_t i = 0; i < nqubits; ++i) {
      of << "-1,";
    }
  }
  if (nqubits > 0) {
    of.seekp(-1, std::ios_base::cur); // remove last comma
  }
  of << ";";
  for (const auto& s : swaps) {
    of << s.first << " " << s.second;
    if (s.op != qc::OpType::SWAP) {
      of << " " << s.op;
      if (s.middleAncilla !=
          std::numeric_limits<decltype(s.middleAncilla)>::max()) {
        of << " " << s.middleAncilla;
      }
    }
    of << ",";
  }
  if (!swaps.empty()) {
    of.seekp(-1, std::ios_base::cur); // remove last comma
  }
  of << '\n';
};

void DataLogger::logMappingResult(MappingResults& result) {
  if (deactivated) {
    return;
  }

  // load output file
  auto of = std::ofstream(dataLoggingPath + "mapping_result.json");
  if (!of.good()) { // if loading failed, output warning and deactivate logging
    deactivated = true;
    std::cerr << "[data-logging] Error opening file: " << dataLoggingPath
              << "mapping_result.json" << '\n';
    return;
  }

  // prepare json data
  auto json = result.json();
  auto& stats = json["statistics"];
  auto& benchmark = stats["benchmark"];
  for (std::size_t i = 0; i < result.layerHeuristicBenchmark.size(); ++i) {
    benchmark["layers"][i] = result.layerHeuristicBenchmark.at(i).json();
  }

  // write json data to file
  of << json.dump(2);
  of.close();
};

void DataLogger::close() {
  for (std::size_t i = 0; i < searchNodesLogFiles.size(); ++i) {
    if (searchNodesLogFiles.at(i).is_open()) {
      std::cerr << "[data-logging] Error: layer " << i << " was not finalized"
                << '\n';
      searchNodesLogFiles.at(i).close();
    }
  }
  deactivated = true;
}
