/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "na/zoned/scheduler/MinFlowScheduler.hpp"

#include "ir/Definitions.hpp"
#include "ir/QuantumComputation.hpp"
#include "ir/operations/OpType.hpp"
#include "ir/operations/StandardOperation.hpp"
#include "na/zoned/Architecture.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <deque>
#include <functional>
#include <iostream>
#include <memory>
#include <queue>
#include <spdlog/spdlog.h>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace na::zoned {
MinFlowScheduler::MinFlowScheduler(const Architecture& architecture,
                                   const Config& config)
    : architecture_(architecture), config_(config) {
  // Validate maxFillingFactor
  if (config_.maxFillingFactor < 0.0 || config_.maxFillingFactor > 1.0) {
    std::ostringstream oss;
    oss << "Invalid maxFillingFactor: " << config_.maxFillingFactor
        << ". Value must be in the range [0.0, 1.0].";
    throw std::invalid_argument(oss.str());
  }
  // calculate the maximum possible number of two-qubit gates per layer
  for (const auto& zone : architecture_.get().entanglementZones) {
    maxTwoQubitGateNumPerLayer_ +=
        std::max(static_cast<size_t>(1U),
                 static_cast<size_t>(config_.maxFillingFactor *
                                     static_cast<double>(zone->front().nRows *
                                                         zone->front().nCols)));
  }
  if (maxTwoQubitGateNumPerLayer_ == 0) {
    throw std::invalid_argument("Architecture must contain at least one site "
                                "in an entanglement zone");
  }
}
auto MinFlowScheduler::schedule(const qc::QuantumComputation& qc) const
    -> std::pair<std::vector<SingleQubitGateLayer>,
                 std::vector<TwoQubitGateLayer>> {
  if (qc.empty()) {
    // early exit if there are no operations to schedule
    return std::pair{std::vector<SingleQubitGateLayer>{},
                     std::vector<TwoQubitGateLayer>{}};
  }
  // collect 2q gate for scheduling
  // ! ignore barrier
  std::map<int, std::pair<int, int>> mGateIdxToQubitPair;
  int gateIdx = 0;
  for (const auto& op : qc) {
    if (op->isStandardOperation()) {
      const auto& stdOp = dynamic_cast<qc::StandardOperation&>(*op);
      if (stdOp.getType() == qc::Z && stdOp.getNtargets() == 1 &&
          stdOp.getNcontrols() == 1) {
        const auto qubit1 = stdOp.getTargets().front();
        const auto qubit2 = stdOp.getControls().cbegin()->qubit;
        mGateIdxToQubitPair[gateIdx] =
            std::pair{std::min(qubit1, qubit2), std::max(qubit1, qubit2)};
      }
    }
    gateIdx++;
  }
  std::unordered_map<int, size_t> umGateToTime;
  if (mGateIdxToQubitPair.size()) {
    umGateToTime = minCostFlowScheduling(mGateIdxToQubitPair);
  }
  size_t maxLayerSize = 0;
  for (const auto& [idx, timeStep] : umGateToTime) {
    if (timeStep > maxLayerSize) {
      maxLayerSize = static_cast<size_t>(timeStep);
    }
  }
  if (mGateIdxToQubitPair.size()) {
    maxLayerSize += 1; // since time step starts from 0
  }
  // generate scheduling based on flow formulation
  // ! ignore barrier
  std::vector<SingleQubitGateLayer> singleQubitGateLayers(maxLayerSize + 1);
  std::vector<TwoQubitGateLayer> twoQubitGateLayers(maxLayerSize);
  // the following vector contains a mapping from qubits to the layer where
  // the next two-qubit gate can be scheduled for that qubit, i.e., the layer
  // after the last layer with a two-qubit gate acting on that qubit
  std::vector<size_t> nextLayerForQubit(qc.getNqubits(), 0);
  gateIdx = 0;
  for (const auto& op : qc) {
    if (op->getType() == qc::Barrier) {
      throw std::invalid_argument("Barriers are not allowed.");
    }
    if (op->isGlobal(qc.getNqubits()) && !op->isControlled() &&
        qc.getNqubits() > 1) {
      const auto maxNextLayerForQubit = *std::max_element(
          nextLayerForQubit.cbegin(), nextLayerForQubit.cend());
      for (qc::Qubit q = 0; q < qc.getNqubits(); ++q) {
        nextLayerForQubit[q] = maxNextLayerForQubit;
      }
      singleQubitGateLayers[maxNextLayerForQubit].emplace_back(*op);
    } else if (op->isStandardOperation()) {
      const auto& stdOp = dynamic_cast<qc::StandardOperation&>(*op);
      if (stdOp.getNtargets() == 1 && stdOp.getNcontrols() == 0) {
        singleQubitGateLayers[nextLayerForQubit[stdOp.getTargets().front()]]
            .emplace_back(stdOp);
      } else if (stdOp.getType() == qc::Z && stdOp.getNtargets() == 1 &&
                 stdOp.getNcontrols() == 1) {
        const auto qubit1 = stdOp.getTargets().front();
        const auto qubit2 = stdOp.getControls().cbegin()->qubit;
        auto layer = umGateToTime[gateIdx];
        assert(layer <= twoQubitGateLayers.size());
        twoQubitGateLayers[layer].emplace_back(
            std::array{std::min(qubit1, qubit2), std::max(qubit1, qubit2)});
        nextLayerForQubit[qubit1] = layer + 1;
        nextLayerForQubit[qubit2] = layer + 1;
      } else {
        std::stringstream ss;
        ss << "Operation type not supported: " << stdOp.getType() << " with "
           << stdOp.getNcontrols() << " controls and " << stdOp.getNtargets()
           << " targets";
        throw std::invalid_argument(ss.str());
      }
    } else {
      std::stringstream ss;
      ss << "Operation type not supported: " << op->getType() << " with "
         << op->getNcontrols() << " controls and " << op->getNtargets()
         << " targets";
      throw std::invalid_argument(ss.str());
    }
    gateIdx++;
  }
  return std::pair{singleQubitGateLayers, twoQubitGateLayers};
}

// Min cost flow scheduling
auto MinFlowScheduler::minCostFlowScheduling(
    const std::map<int, std::pair<int, int>>& mGateIdxToQubitPair) const
    -> std::unordered_map<int, size_t> {

  size_t numGate = mGateIdxToQubitPair.size();
  size_t source = 2 * numGate;
  size_t sink = source + 1;

  // node idx to gate idx
  std::vector<int> vGateIdx(numGate, 0);
  size_t idx = 0;
  for (const auto& [gateIdx, qubitPair] : mGateIdxToQubitPair) {
    vGateIdx[idx] = gateIdx;
    idx++;
  }

  std::vector<size_t> vOutNodes(numGate);
  for (size_t v = 0; v < numGate; v++) {
    vOutNodes[v] = v + numGate;
  }

  // Generate DAG
  std::vector<std::vector<size_t>> vDagAdj(numGate), vDagPred(numGate);
  std::vector<std::pair<size_t, size_t>> vDagEdges;
  // generate dag
  std::map<int, size_t> nodeMap; // Maps qubits to latest gate node

  for (size_t nodeIdx = 0; nodeIdx < mGateIdxToQubitPair.size(); nodeIdx++) {
    size_t gateIdxLocal = static_cast<size_t>(vGateIdx.at(nodeIdx));
    const auto& qubitPairLocal =
        mGateIdxToQubitPair.at(static_cast<int>(gateIdxLocal));
    const auto& q1 = qubitPairLocal.first;
    const auto& q2 = qubitPairLocal.second;

    for (int qubit : {q1, q2}) {
      if (nodeMap.count(qubit)) {
        size_t prevNode = nodeMap[qubit];
        vDagAdj[prevNode].emplace_back(nodeIdx);
        vDagPred[nodeIdx].emplace_back(prevNode);
        vDagEdges.emplace_back(prevNode, nodeIdx);
      }
      nodeMap[qubit] = nodeIdx;
    }
  }

  SPDLOG_INFO("start scheduling");
  auto start = std::chrono::high_resolution_clock::now();

  // Compute slack analysis
  std::map<int, std::vector<std::pair<size_t, size_t>>> mEdgeClassification;
  std::vector<int> vEst, vNodeSlack;
  computeSlackAnalysis(numGate, vDagAdj, vDagPred, vDagEdges,
                       mEdgeClassification, vEst, vNodeSlack);

  // 2. Get the timepoint after the function returns
  auto stop = std::chrono::high_resolution_clock::now();
  // 3. Get the difference in timepoints and cast to desired units
  auto duration =
      std::chrono::duration_cast<std::chrono::microseconds>(stop - start);
  // Output the duration
  SPDLOG_INFO("computeSlackAnalysis execution time: {} microseconds",
              duration.count());

  // construct graph for min-cost flow
  size_t numNodes = sink + 1;

  // Create capacity and cost matrices
  std::vector<std::vector<int>> vCap(numNodes, std::vector<int>(numNodes, 0));
  std::vector<std::vector<int>> vCost(numNodes, std::vector<int>(numNodes, 0));
  std::vector<std::vector<int>> vFlow(numNodes, std::vector<int>(numNodes, 0));

  // Add edges with capacity and cost
  // Debug: mEdgeClassification (commented out)
  // std::cout << "Edge Classification:" << std::endl;
  // for (const auto& [costVal, edgeList] : mEdgeClassification) {
  //     std::cout << "Cost " << costVal << ": ";
  //     for (const auto& edge : edgeList) {
  //         std::cout << "(" << edge.first << ", " << edge.second << ") ";
  //     }
  //     std::cout << std::endl;
  // }
  for (const auto& [costVal, edgeList] : mEdgeClassification) {
    for (const auto& edge : edgeList) {
      vCap[vOutNodes[edge.first]][edge.second] = 1;
      vCost[vOutNodes[edge.first]][edge.second] = costVal;
    }
  }

  for (size_t v = 0; v < numGate; v++) {
    // node decomposition
    vCap[v][vOutNodes[v]] = 1;
    vCost[v][vOutNodes[v]] = 0;
    // source to in-node
    vCap[source][v] = 1;
    vCost[source][v] = 1;
    // out-node to sink
    vCap[vOutNodes[v]][sink] = 1;
    vCost[vOutNodes[v]][sink] = 1;
  }
  // source to sink
  vCap[source][sink] = static_cast<int>(numGate);
  vCost[source][sink] = 1;

  start = std::chrono::high_resolution_clock::now();
  // Solve min-cost max-flow
  bool success = solveMinCostMaxFlow(vCap, vCost, vFlow, source, sink, numGate);

  // 2. Get the timepoint after the function returns
  stop = std::chrono::high_resolution_clock::now();
  // 3. Get the difference in timepoints and cast to desired units
  duration =
      std::chrono::duration_cast<std::chrono::microseconds>(stop - start);
  // Output the duration
  SPDLOG_INFO("solveMinCostMaxFlow execution time: {} microseconds",
              duration.count());

  // Extract flow edges (simplified - direct flow analysis)
  std::vector<std::pair<size_t, size_t>> vFlowEdge;

  if (!success) {
    // std::cout << "No optimal solution found." << std::endl;
  }

  // Debug: flow matrix and mappings (commented out)
  // std::cout << "Flow matrix:" << std::endl;
  // for (size_t u = 0; u < numNodes; u++) {
  //     for (size_t v = 0; v < numNodes; v++) {
  //         std::cout << vFlow[u][v] << " ";
  //     }
  //     std::cout << std::endl;
  // }
  // std::cout << "vFlowEdge" << std::endl;
  // std::cout << "umNodeToGateIdx" << std::endl;
  // for (auto [t0, t1] : umNodeToGateIdx){
  //   std::cout << "node idx: " << t0 << ", gate idx: " << t1 << std::endl;
  // }
  for (size_t u = numGate; u < 2 * numGate; u++) {
    for (size_t v = 0; v < numGate; v++) {
      if (vFlow[u][v] > 0) {
        vFlowEdge.push_back({u - numGate, v});
        // std::cout << "(vFlowEdge)" << u << ", " << v << ", " << vFlow[u][v]
        // << std::endl; std::cout << inNodeGateIdx << ", " << outNodeGateIdx <<
        // std::endl;
      }
    }
  }
  start = std::chrono::high_resolution_clock::now();
  // std::cout << "Optimal count: " << vFlowEdge.size() << std::endl;
  // Build result scheduling
  std::vector<std::vector<int>> vResultScheduling =
      constructNodeSchedule(vDagAdj, vFlowEdge, vEst, vNodeSlack);

  // 2. Get the timepoint after the function returns
  stop = std::chrono::high_resolution_clock::now();
  // 3. Get the difference in timepoints and cast to desired units
  duration =
      std::chrono::duration_cast<std::chrono::microseconds>(stop - start);
  // Output the duration
  SPDLOG_INFO("constructNodeSchedule execution time: {} microseconds",
              duration.count());
  // Debug: vResultScheduling (commented out)
  // std::cout << "vResultScheduling: " << std::endl;
  // for (auto scheduling : vResultScheduling){
  //   for (auto g : scheduling){
  //     std::cout << g << " " ;
  //   }
  //   std::cout << std::endl;
  // }
  // map gate idx to time step
  std::unordered_map<int, size_t> umGateToTime;
  for (size_t time = 0; time < vResultScheduling.size(); time++) {
    for (const auto& nodeIdx : vResultScheduling[time]) {
      int gateIdx = vGateIdx[static_cast<size_t>(nodeIdx)];
      umGateToTime[gateIdx] = time;
    }
  }
  return umGateToTime;
}

// Min-cost max-flow solver using successive shortest paths algorithm
auto MinFlowScheduler::solveMinCostMaxFlow(std::vector<std::vector<int>>& vCap,
                                           std::vector<std::vector<int>>& vCost,
                                           std::vector<std::vector<int>>& vFlow,
                                           size_t source, size_t sink,
                                           size_t totalSupply) const -> bool {
  const int INF = std::numeric_limits<int>::max() / 4;
  size_t nNodes = vCap.size();

  if (vFlow.size() != nNodes) {
    vFlow.assign(nNodes, std::vector<int>(nNodes, 0));
  }

  int remaining = static_cast<int>(totalSupply);
  using PQItem = std::pair<int, size_t>;

  std::vector<std::vector<size_t>> vAdjacentList(nNodes);
  // construct adjacnecy list
  for (size_t u = 0; u < nNodes; ++u) {
    for (size_t v = 0; v < nNodes; ++v) {
      if (vCap[u][v] > 0) {
        vAdjacentList[u].emplace_back(v);
      }
    }
  }
  while (remaining > 0) {
    // Dijkstra on reduced costs
    std::vector<int> vDist(nNodes, INF);
    std::vector<size_t> vParent(nNodes, nNodes);

    std::priority_queue<PQItem, std::vector<PQItem>, std::greater<PQItem>> pq;
    vDist[source] = 0;
    vParent[source] = source;
    pq.push({0, source});

    while (!pq.empty()) {
      auto [d, u] = pq.top();
      pq.pop();

      if (d > vDist[u])
        continue;

      for (size_t v : vAdjacentList[u]) {
        if (vDist[u] + vCost[u][v] < vDist[v] && vCap[u][v] > 0) {
          vDist[v] = vDist[u] + vCost[u][v];
          pq.emplace(vDist[v], v);
          vParent[v] = u;
        }
      }
    }
    // Debug: vDist (commented out)
    // std::cout << "vDist: " << std::endl;
    // for (size_t u = 0; u < nNodes; u++) {
    //   std::cout << vDist[u] << " ";
    // }
    // std::cout << std::endl;
    if (vDist[sink] == INF) {
      return false; // no more augmenting path
    }

    // find bottleneck
    int amt = static_cast<int>(remaining);
    size_t v = sink;
    while (v != source) {
      size_t u = vParent[v];
      amt = std::min(amt, vCap[u][v]);
      // std::cout << "pass node " << u << " and " << v << " with vCap " <<
      // vCap[u][v] << std::endl;
      v = u;
    }

    // augment along path
    v = sink;
    while (v != source) {
      size_t u = vParent[v];
      vCap[u][v] -= amt;
      vFlow[u][v] += amt;
      v = u;
    }

    remaining -= static_cast<size_t>(amt);
    // std::cout << "amt: " << amt << std::endl;
  }
  return true;
}

// Compute slack analysis
auto MinFlowScheduler::computeSlackAnalysis(
    size_t numNodes, const std::vector<std::vector<size_t>>& vDagAdj,
    const std::vector<std::vector<size_t>>& vDagPred,
    const std::vector<std::pair<size_t, size_t>>& vDagEdges,
    std::map<int, std::vector<std::pair<size_t, size_t>>>& mEdgeClassification,
    std::vector<int>& vEst, std::vector<int>& vNodeSlack) const -> void {
  // Compute EST
  vEst.assign(numNodes, 0);
  auto sortedNodes = topologicalSort(numNodes, vDagAdj, vDagEdges);
  // Debug: DAG adjacency and sorted nodes (commented out)
  // std::cout << "vDagAdj: " << std::endl;
  // for (auto vs : vDagAdj){
  //   for (auto v : vs)
  //     std::cout << v << " ";
  //   std::cout << std::endl;
  // }
  // std::cout << "sortedNodes: ";
  // for (auto v : sortedNodes){
  //   std::cout << v << " ";
  // }
  // std::cout << std::endl;
  // record sink nodes which will be the starting point for LST computation
  std::vector<size_t> vSinkNodes;

  for (size_t node : sortedNodes) {
    const auto& successors = vDagAdj[node];
    for (size_t succ : successors) {
      vEst[succ] = std::max(vEst[succ], vEst[node] + 1);
    }
    if (successors.empty()) {
      vSinkNodes.push_back(node);
    }
  }

  // Compute LST
  int lsTime = 0;
  for (size_t node : vSinkNodes) {
    lsTime = std::max(lsTime, vEst[node]);
  }

  std::vector<int> vLst(numNodes, lsTime);
  for (auto it = sortedNodes.rbegin(); it != sortedNodes.rend(); ++it) {
    size_t node = *it;
    const auto& predecessors = vDagPred[node];
    for (size_t p : predecessors) {
      vLst[p] = std::min(vLst[node] - 1, vLst[p]);
    }
  }

  // Compute node slack
  vNodeSlack.resize(numNodes);
  for (size_t node = 0; node < numNodes; node++) {
    vNodeSlack[node] = vLst[node] - vEst[node];
  }

  // Classify edges
  int type1 = -10, type2 = -10, type3 = -2, type4 = -6;
  mEdgeClassification[type1] = {};
  mEdgeClassification[type2] = {};
  mEdgeClassification[type3] = {};
  mEdgeClassification[type4] = {};

  std::vector<bool> vZeroSlackDescendant(numNodes, false);
  std::vector<bool> vZeroSlackAncestor(numNodes, false);

  // Backward pass: Mark nodes that have a zero-slack descendant
  for (auto it = sortedNodes.rbegin(); it != sortedNodes.rend(); ++it) {
    size_t node = *it;
    if (vNodeSlack[node] == 0) {
      vZeroSlackDescendant[node] = true;
    }
    const auto& predecessors = vDagPred[node];
    if (vZeroSlackDescendant[node]) {
      for (size_t p : predecessors) {
        vZeroSlackDescendant[p] = true;
      }
    }
  }

  // Forward pass: Mark nodes that have a zero-slack ancestor
  for (size_t node : sortedNodes) {
    if (vNodeSlack[node] == 0) {
      vZeroSlackAncestor[node] = true;
    }
    const auto& successors = vDagAdj[node];
    if (vZeroSlackAncestor[node]) {
      for (size_t succ : successors) {
        vZeroSlackAncestor[succ] = true;
      }
    }
  }

  // Classify each edge
  for (const auto& [u, v] : vDagEdges) {
    int uSlack = vNodeSlack[u];
    int vSlack = vNodeSlack[v];
    // two nodes can be scheduled in consecutive time steps
    if (vEst[v] <= vLst[u] + 1) {
      if (uSlack == 0 && vSlack == 0) {
        mEdgeClassification[type1].push_back({u, v});
      } else if (uSlack > 0 && vSlack > 0) {
        mEdgeClassification[type2].push_back({u, v});
      } else if ((uSlack == 0 && vSlack > 0 && vZeroSlackDescendant[v]) ||
                 (uSlack > 0 && vSlack == 0 && vZeroSlackAncestor[u])) {
        mEdgeClassification[type3].push_back({u, v});
      } else {
        mEdgeClassification[type4].push_back({u, v});
      }
    }
  }
}

// Topological sort
auto MinFlowScheduler::topologicalSort(
    size_t numVertices, const std::vector<std::vector<size_t>>& vDagAdj,
    const std::vector<std::pair<size_t, size_t>>& vDagEdges) const
    -> std::vector<size_t> {
  std::vector<int> inDegree(numVertices, 0);

  for (const auto& [u, v] : vDagEdges) {
    inDegree[v]++;
  }

  std::deque<size_t> queue;
  for (size_t i = 0; i < numVertices; i++) {
    if (inDegree[i] == 0) {
      queue.push_back(i);
    }
  }

  std::vector<size_t> vSortedNodes;
  while (!queue.empty()) {
    size_t node = queue.front();
    queue.pop_front();
    vSortedNodes.push_back(node);

    for (size_t succ : vDagAdj[node]) {
      inDegree[succ]--;
      if (inDegree[succ] == 0) {
        queue.push_back(succ);
      }
    }
  }

  return vSortedNodes;
}

// Weakly connected components
auto MinFlowScheduler::weaklyConnectedComponents(
    size_t numVertices, const std::vector<std::vector<size_t>>& vDagSucc,
    const std::vector<std::vector<size_t>>& vDagPred) const
    -> std::vector<std::unordered_set<size_t>> {
  std::vector<bool> visited(numVertices, false);
  std::vector<std::unordered_set<size_t>> vComponents;

  for (size_t i = 0; i < numVertices; i++) {
    if (!visited[i]) {
      std::unordered_set<size_t> component;
      std::deque<size_t> queue = {i};
      visited[i] = true;

      while (!queue.empty()) {
        size_t node = queue.front();
        queue.pop_front();
        component.insert(node);

        // Check both successors and predecessors
        for (size_t neighbor : vDagSucc[node]) {
          if (!visited[neighbor]) {
            visited[neighbor] = true;
            queue.push_back(neighbor);
          }
        }
        for (size_t neighbor : vDagPred[node]) {
          if (!visited[neighbor]) {
            visited[neighbor] = true;
            queue.push_back(neighbor);
          }
        }
      }
      vComponents.push_back(component);
    }
  }

  return vComponents;
}

// Construct node schedule
auto MinFlowScheduler::constructNodeSchedule(
    const std::vector<std::vector<size_t>>& vDagAdj,
    const std::vector<std::pair<size_t, size_t>>& vFlowEdge,
    const std::vector<int>& vEst, const std::vector<int>& vNodeSlack) const
    -> std::vector<std::vector<int>> {
  size_t numGates = vEst.size();

  // Build flow graph
  std::vector<std::vector<size_t>> vFlowSucc(numGates);
  std::vector<std::vector<size_t>> vFlowPred(numGates);

  for (const auto& [u, v] : vFlowEdge) {
    vFlowSucc[u].push_back(v);
    vFlowPred[v].push_back(u);
  }

  std::vector<std::unordered_set<size_t>> wccs =
      weaklyConnectedComponents(numGates, vFlowSucc, vFlowPred);
  // Debug: weakly connected components and EST (commented out)
  // std::cout << "wccs" << std::endl;
  // for (auto s : wccs){
  //   for (auto i : s){
  //     std::cout << i << " ";
  //   }
  //   std::cout << std::endl;
  // }
  // std::cout << "vEst" << std::endl;
  // for (auto i : vEst){
  //   std::cout << i << " ";
  // }
  // std::cout << std::endl;
  std::vector<int> vNodeTime(numGates, -1);
  std::vector<std::unordered_set<size_t>> vUnassignedReuseComps;

  // Process each weakly connected component
  for (const auto& comp : wccs) {
    size_t seed = SIZE_MAX;
    for (size_t i : comp) {
      if (vNodeSlack[i] == 0) {
        seed = i;
        // std::cout << seed << std::endl;
        break;
      }
    }
    // If no zero-slack node is vFound in this component, skip propagation for
    // this comp.
    if (seed == SIZE_MAX) {
      if (comp.size() > 1) {
        vUnassignedReuseComps.push_back(comp);
      }
      continue;
    }

    bool pathBreakSucc = false, pathBreakPred = false;
    std::unordered_set<size_t> breakCompSucc, breakCompPred;
    vNodeTime[seed] = vEst[seed];
    // use a deque for BFS propagation starting from the seed.
    std::deque<size_t> queue = {seed};

    while (!queue.empty()) {
      size_t curr = queue.front();
      queue.pop_front();
      int currTime = vNodeTime[curr];

      // Forward propagation: each successor gets curr_time + 1.
      // std::cout << "curr: " << curr << std::endl;
      // std::cout << "vFlowSucc[curr]: " << std::endl;
      for (size_t succ : vFlowSucc[curr]) {
        // std::cout << succ << " " << std::endl;
        int newTime = currTime + 1;
        if (vNodeTime[succ] == -1 &&
            breakCompPred.find(succ) == breakCompPred.end() &&
            breakCompSucc.find(succ) == breakCompSucc.end()) {
          // Only update if unscheduled; if scheduled with a different time,
          // skip to avoid conflict.
          if (!pathBreakSucc && newTime >= vEst[succ] &&
              newTime <= vEst[succ] + vNodeSlack[succ]) {
            vNodeTime[succ] = newTime;
          } else {
            pathBreakSucc = true;
          }

          if (pathBreakSucc) {
            breakCompSucc.insert(succ);
          }

          queue.push_back(succ);
        }
      }

      // Backward propagation: each predecessor gets curr_time - 1
      // std::cout << "vFlowSucc[pred]: " << std::endl;
      for (size_t pred : vFlowPred[curr]) {
        // std::cout << pred << " " << std::endl;
        int newTime = currTime - 1;
        if (vNodeTime[pred] == -1 &&
            breakCompPred.find(pred) == breakCompPred.end() &&
            breakCompSucc.find(pred) == breakCompSucc.end()) {

          if (!pathBreakPred && newTime >= vEst[pred] &&
              newTime <= vEst[pred] + vNodeSlack[pred]) {
            vNodeTime[pred] = newTime;
          } else {
            pathBreakPred = true;
          }

          queue.push_back(pred);

          if (pathBreakPred) {
            breakCompPred.insert(pred);
          }
        }
      }
    }

    if (breakCompSucc.size() > 1) {
      vUnassignedReuseComps.push_back(breakCompSucc);
    }
    if (breakCompPred.size() > 1) {
      vUnassignedReuseComps.push_back(breakCompPred);
    }
  }

  // Debug: vNodeTime (commented out)
  // std::cout << "vNodeTime" << std::endl;
  // for (auto i : vNodeTime){
  //   std::cout << i << " ";
  // }
  // std::cout << std::endl;
  // Assign unscheduled gates to EST
  for (size_t i = 0; i < vNodeTime.size(); i++) {
    if (vNodeTime[i] == -1 || vNodeSlack[i] == 0) {
      vNodeTime[i] = vEst[i];
    }
  }

  // Process unassigned reuse components
  std::unordered_set<size_t> usUnassignedReuseCompIdxs;
  for (size_t i = 0; i < vUnassignedReuseComps.size(); i++) {
    usUnassignedReuseCompIdxs.insert(i);
  }

  for (int i = static_cast<int>(numGates) - 1; i >= 0; i--) {
    size_t i_size = static_cast<size_t>(i);
    size_t tmp = SIZE_MAX;

    for (size_t compIdx : usUnassignedReuseCompIdxs) {
      const auto& comp = vUnassignedReuseComps[compIdx];
      if (comp.find(i_size) != comp.end()) {
        tmp = compIdx;
        std::vector<size_t> sortedComp(comp.begin(), comp.end());
        std::sort(sortedComp.begin(), sortedComp.end());

        std::vector<int> timeAssignment = {vEst[sortedComp.back()],
                                           vEst[sortedComp.back()] +
                                               vNodeSlack[sortedComp.back()]};

        if (vNodeSlack[sortedComp.back()] > 2) {
          for (int t = vEst[sortedComp.back()] + 1;
               t < vEst[sortedComp.back()] + vNodeSlack[sortedComp.back()];
               t++) {
            timeAssignment.push_back(t);
          }
        }

        int maxContinuousGateTimes = 0;
        std::vector<int> bestGateTime;

        for (int t : timeAssignment) {
          vNodeTime[sortedComp.back()] = t;
          int numContinuous = 0;

          for (size_t j = sortedComp.size() - 2; j >= 0; j--) {
            if (vNodeTime[sortedComp[j + 1]] - 1 <=
                vNodeTime[sortedComp[j]] + vNodeSlack[sortedComp[j]]) {
              vNodeTime[sortedComp[j]] = vNodeTime[sortedComp[j + 1]] - 1;
              numContinuous++;
            }
          }

          if (numContinuous > maxContinuousGateTimes) {
            maxContinuousGateTimes = numContinuous;
            bestGateTime = vNodeTime;
          }
        }

        if (!bestGateTime.empty()) {
          vNodeTime = bestGateTime;
        }
        break;
      }
    }

    if (tmp != SIZE_MAX) {
      usUnassignedReuseCompIdxs.erase(tmp);
    }
  }
  // Fix any remaining conflicts
  for (size_t i = 0; i < numGates; i++) {
    for (size_t v : vDagAdj[i]) {
      if (vNodeTime[v] <= vNodeTime[i]) {
        if (vNodeSlack[v] == 0) {
          assert(false);
        }
        vNodeTime[v] = vNodeTime[i] + 1;
      }
    }
  }

  // Build result scheduling
  size_t numStage = static_cast<size_t>(
      *std::max_element(vNodeTime.begin(), vNodeTime.end()) + 1);
  std::vector<std::vector<int>> vResultScheduling(numStage);

  for (size_t i = 0; i < vNodeTime.size(); i++) {
    vResultScheduling[static_cast<size_t>(vNodeTime[i])].push_back(
        static_cast<int>(i));
  }

  // handle the case where #gates in the stage exceeds the entangling zone
  // capacity e.g., maxTwoQubitGateNumPerLayer_ = 2 stage 0: [g0, g1, g2] ->
  // stage 0: [g0, g1], stage 1: [g2]
  std::vector<std::vector<int>> vFinalScheduling;
  for (const auto& stage : vResultScheduling) {
    if (stage.size() <= maxTwoQubitGateNumPerLayer_) {
      vFinalScheduling.emplace_back(stage);
    } else {
      size_t totalGates = stage.size();
      size_t startIdx = 0;
      while (startIdx < totalGates) {
        size_t endIdx =
            std::min(startIdx + maxTwoQubitGateNumPerLayer_, totalGates);
        std::vector<int> subStage(
            stage.begin() + static_cast<std::ptrdiff_t>(startIdx),
            stage.begin() + static_cast<std::ptrdiff_t>(endIdx));
        vFinalScheduling.emplace_back(subStage);
        startIdx = endIdx;
      }
    }
  }

  return vFinalScheduling;
}
} // namespace na::zoned
