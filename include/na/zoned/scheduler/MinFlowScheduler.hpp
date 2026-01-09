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

#include "ir/QuantumComputation.hpp"
#include "na/zoned/Architecture.hpp"
#include "na/zoned/Types.hpp"
#include "na/zoned/scheduler/SchedulerBase.hpp"

#include <functional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace na::zoned {
/**
 * The class MinFlowScheduler implements the min-cost flow scheduling
 * strategy for the zoned neutral atom compiler.
 */
class MinFlowScheduler : public SchedulerBase {
  /// A reference to the zoned neutral atom architecture
  std::reference_wrapper<const Architecture> architecture_;
  /**
   * This value is calculated based on the architecture and indicates the
   * entanglement zone.
   */
  size_t maxTwoQubitGateNumPerLayer_ = 0;

public:
  /// The configuration of the MinFlowScheduler
  struct Config {
    /// The maximal share of traps that are used in the entanglement zone.
    double maxFillingFactor = 0.9;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Config, maxFillingFactor);
  };

private:
  /// The configuration of the MinFlowScheduler
  Config config_;

private:
  /**
   * @brief Generates the scheduling based on min-cost flow
   * layers.
   * @details Every two-qubit gate operation is scheduled based on min-cost flow
   * to optimize for qubit reuse. The function splits the operations into
   * layers. Every layer (except for the last one) contains some single-qubit
   * operations and two-qubit operations. The single-qubit operations are
   * executed before the two-qubit operations. For every layer, all two-qubit
   * operations can be executed in parallel, i.e., every qubit is involved in at
   * most one two-qubit operation. The last layer contains only the remaining
   * single-qubit operations.
   * @param mGateIdxToQubitPair is a map from gate indices to qubit pairs
   * @return a map from gate indices to time steps (layers)
   */
  auto minCostFlowScheduling(
      const std::map<int, std::pair<int, int>>& mGateIdxToQubitPair) const
      -> std::unordered_map<int, size_t>;

  /**
   * @brief solve the min-cost max-flow problem
   * @param vCap is the capacity matrix
   * @param vCost is the cost matrix
   * @param vFlow is the flow matrix
   * @param source is the source node index
   * @param sink is the sink node index
   * @param totalSupply is the total supply to be sent from source to sink
   * @return true if the flow is successfully found, false otherwise
   */
  auto solveMinCostMaxFlow(std::vector<std::vector<int>>& vCap,
                           std::vector<std::vector<int>>& vCost,
                           std::vector<std::vector<int>>& vFlow, size_t source,
                           size_t sink, size_t totalSupply) const -> bool;

  /**
   * @brief Computes slack analysis for the given DAG
   * @param numNodes is the number of nodes in the DAG
   * @param vDagAdj is the adjacency list of the DAG
   * @param vDagPred is the predecessor list of the DAG
   * @param vDagEdges is the edge list of the DAG
   * @param mEdgeClassification is the output edge classification map
   * @param vEst is the output earliest start times of the nodes
   * @param vNodeSlack is the output slack times of the nodes
   */
  auto computeSlackAnalysis(
      size_t numNodes, const std::vector<std::vector<size_t>>& vDagAdj,
      const std::vector<std::vector<size_t>>& vDagPred,
      const std::vector<std::pair<size_t, size_t>>& vDagEdges,
      std::map<int, std::vector<std::pair<size_t, size_t>>>&
          mEdgeClassification,
      std::vector<int>& vEst, std::vector<int>& vNodeSlack) const -> void;

  /**
   * @brief Performs topological sort on the given DAG
   * @param numVertices is the number of vertices in the DAG
   * @param vDagAdj is the adjacency list of the DAG
   * @param vDagEdges is the edge list of the DAG
   * @return a vector of node indices in topological order
   */
  auto
  topologicalSort(size_t numVertices,
                  const std::vector<std::vector<size_t>>& vDagAdj,
                  const std::vector<std::pair<size_t, size_t>>& vDagEdges) const
      -> std::vector<size_t>;

  /**
   * @brief constructs the node scheduling based on the flow results
   * @param vDagAdj is the adjacency list of the DAG
   * @param vFlowEdge is the edge list of the flow solution
   * @param vEst is the earliest start times of the nodes
   * @param vNodeSlack is the slack times of the nodes
   * @return a vector of vectors representing the node scheduling: layer ->
   * nodes
   */
  auto
  constructNodeSchedule(const std::vector<std::vector<size_t>>& vDagAdj,
                        const std::vector<std::pair<size_t, size_t>>& vFlowEdge,
                        const std::vector<int>& vEst,
                        const std::vector<int>& vNodeSlack) const
      -> std::vector<std::vector<int>>;

  /**
   * @brief constructs weakly connected components of the given DAG
   * @param numVertices is the number of vertices in the DAG
   * @param vDagSucc is the successor list of the DAG
   * @param vDagPred is the predecessor list of the DAG
   * @return a vector of sets representing the weakly connected components
   */
  auto weaklyConnectedComponents(
      size_t numVertices, const std::vector<std::vector<size_t>>& vDagSucc,
      const std::vector<std::vector<size_t>>& vDagPred) const
      -> std::vector<std::unordered_set<size_t>>;

public:
  /**
   * Create a new MinFlowScheduler.
   * @param architecture is the architecture of the neutral atom system
   * @param config is the configuration for the scheduler
   */
  MinFlowScheduler(const Architecture& architecture, const Config& config);
  /**
   * This function schedules the operations of a quantum computation.
   * @details Two-qubit gates are scheduled based on min-cost flow to optimize
   * for qubit reuse while single-qubit gates are scheduled based on the
   * as-soon-as-possible strategy. The function splits the operations into
   * layers. Every layer (except for the last one) contains some single-qubit
   * operations and two-qubit operations. The single-qubit operations are
   * executed before the two-qubit operations. For every layer, all two-qubit
   * operations can be executed in parallel, i.e., every qubit is involved in at
   * most one two-qubit operation. The last layer contains only the remaining
   * single-qubit operations.
   * @param qc is the quantum computation
   * @return a pair of two vectors. The first vector contains the layers of
   * single-qubit operations. The second vector contains the layers of two-qubit
   * operations. A pair of qubits represents every two-qubit operation.
   */
  [[nodiscard]] auto schedule(const qc::QuantumComputation& qc) const
      -> std::pair<std::vector<SingleQubitGateLayer>,
                   std::vector<TwoQubitGateLayer>>;
};
} // namespace na::zoned
