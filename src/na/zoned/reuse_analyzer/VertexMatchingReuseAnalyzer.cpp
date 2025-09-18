/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "na/zoned/reuse_analyzer/VertexMatchingReuseAnalyzer.hpp"

#include "ir/Definitions.hpp"
#include "na/zoned/Types.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <deque>
#include <numeric>
#include <optional>
#include <queue>
#include <stack>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace na::zoned {
auto VertexMatchingReuseAnalyzer::analyzeReuse(
    const std::vector<TwoQubitGateLayer>& twoQubitGateLayers)
    -> std::vector<std::unordered_set<qc::Qubit>> {
  if (twoQubitGateLayers.size() <= 1) {
    // early exit if there are no qubits to reuse between layers
    return std::vector<std::unordered_set<qc::Qubit>>{};
  }
  std::vector<std::unordered_map<qc::Qubit, size_t>> usedQubitsInLayers;
  usedQubitsInLayers.reserve(twoQubitGateLayers.size());
  const auto& firstGateLayer = twoQubitGateLayers.front();
  auto& usedQubitsInFirstLayer = usedQubitsInLayers.emplace_back();
  for (size_t gateIdx = 0; gateIdx < firstGateLayer.size(); ++gateIdx) {
    for (const auto qubit : firstGateLayer[gateIdx]) {
      usedQubitsInFirstLayer[qubit] = gateIdx;
    }
  }
  std::vector<std::unordered_set<qc::Qubit>> reuseQubits;
  reuseQubits.reserve(twoQubitGateLayers.size() - 1);
  for (auto layer = twoQubitGateLayers.begin();;) {
    const auto& twoQubitGatesInPreviousLayer = *layer;
    if (++layer == twoQubitGateLayers.end()) {
      break;
    }
    const auto& twoQubitGatesInCurrentLayer = *layer;
    std::vector matrix(twoQubitGatesInCurrentLayer.size(),
                       std::vector(twoQubitGatesInPreviousLayer.size(), false));
    const auto& usedQubitsInPreviousLayer = usedQubitsInLayers.back();
    auto& usedQubitsInCurrentLayer = usedQubitsInLayers.emplace_back();
    auto& reuseQubitsInCurrentLayer = reuseQubits.emplace_back();
    for (size_t gateIdx = 0; gateIdx < twoQubitGatesInCurrentLayer.size();
         ++gateIdx) {
      const auto& gate = twoQubitGatesInCurrentLayer[gateIdx];
      const auto& itFirst = usedQubitsInPreviousLayer.find(gate.front());
      const auto& itSecond = usedQubitsInPreviousLayer.find(gate.back());
      const auto firstReusable = itFirst != usedQubitsInPreviousLayer.end();
      const auto secondReusable = itSecond != usedQubitsInPreviousLayer.end();
      if (firstReusable && secondReusable &&
          itFirst->second == itSecond->second) {
        // If both qubits of the gate are used in the previous layer also
        // by the identical gate, then both qubits can stay at their location
        // and be reused.
        reuseQubitsInCurrentLayer.emplace(gate.front());
        reuseQubitsInCurrentLayer.emplace(gate.back());
      } else if (firstReusable) {
        matrix[gateIdx][itFirst->second] = true;
      } else if (secondReusable) {
        matrix[gateIdx][itSecond->second] = true;
      }
      usedQubitsInCurrentLayer[gate.front()] = gateIdx;
      usedQubitsInCurrentLayer[gate.back()] = gateIdx;
    }
    std::vector sparseMatrix(matrix.size(), std::vector<std::size_t>{});
    for (std::size_t r = 0; r < matrix.size(); ++r) {
      for (std::size_t c = 0; c < matrix[r].size(); ++c) {
        if (matrix[r][c]) {
          sparseMatrix[r].emplace_back(c);
        }
      }
    }
    const auto& matching = maximumBipartiteMatching(sparseMatrix);
    for (std::size_t gateIdx = 0; gateIdx < matching.size(); ++gateIdx) {
      if (const auto& reuseGateIdx = matching[gateIdx]; reuseGateIdx) {
        const auto& gate = twoQubitGatesInCurrentLayer[gateIdx];
        if (const auto& it = usedQubitsInPreviousLayer.find(gate.front());
            it != usedQubitsInPreviousLayer.end() &&
            it->second == *reuseGateIdx) {
          reuseQubitsInCurrentLayer.emplace(gate.front());
        } else {
          assert(usedQubitsInPreviousLayer.at(gate.back()) == *reuseGateIdx);
          reuseQubitsInCurrentLayer.emplace(gate.back());
        }
      }
    }
  }
  return reuseQubits;
}
auto VertexMatchingReuseAnalyzer::maximumBipartiteMatching(
    const std::vector<std::vector<std::size_t>>& sparseMatrix, bool inverted)
    -> std::vector<std::optional<std::size_t>> {
  // Conversely, to other implementations and the literature, we do NOT
  // introduce two extra nodes, one connected to all free sources and one
  // connected to all free sinks. Instead, we start the search directly from
  // free sources and end as soon as we encountered a free sink.
  const auto maxSink = std::accumulate(
      sparseMatrix.cbegin(), sparseMatrix.cend(), static_cast<std::size_t>(0),
      [](const std::size_t max, const std::vector<std::size_t>& row) {
        if (const auto maxIt = std::max_element(row.cbegin(), row.cend());
            maxIt != row.cend()) {
          return std::max(max, *maxIt);
        }
        return max;
      });
  std::vector freeSources(sparseMatrix.size(), true);
  std::vector<std::optional<std::size_t>> invMatching(maxSink + 1,
                                                      std::nullopt);
  while (true) {
    // find the reachable free sinks on shortest augmenting paths via bfs
    // for all distances, std::nullopt means "not visited yet", i.e., infinite
    // distance
    std::vector<std::optional<std::size_t>> distance(sparseMatrix.size(),
                                                     std::nullopt);
    for (std::size_t s = 0; s < freeSources.size(); ++s) {
      if (freeSources[s]) {
        distance[s] = 0;
      }
    }
    std::queue<std::size_t> queue{};
    for (std::size_t source = 0; source < freeSources.size(); ++source) {
      if (freeSources[source]) {
        queue.push(source);
      }
    }
    std::optional<std::size_t> maxDistance = std::nullopt;
    while (!queue.empty()) {
      const auto source = queue.front();
      queue.pop();
      if (!maxDistance || *distance[source] <= *maxDistance) {
        for (const auto sink : sparseMatrix[source]) {
          if (invMatching[sink]) { // a matched sink is found
            const auto nextSource = *invMatching[sink];
            if (!distance[nextSource]) { // nextSource is not visited yet
              distance[nextSource] = *distance[source] + 1;
              queue.push(nextSource);
            }
          } else { // a free sink is found
            maxDistance = distance[source];
          }
        }
      }
    }
    if (!maxDistance) { // no augmenting path exists
      break;
    }
    // find the augmenting paths via dfs and update the matching
    for (std::size_t freeSource = 0; freeSource < freeSources.size();
         ++freeSource) {
      if (freeSources[freeSource]) {
        std::stack<std::size_t, std::deque<std::size_t>> stack(
            std::deque<std::size_t>{freeSource});
        // this vector tracks the predecessors of each source, i.e., the sink
        // AND the source coming before the source in the augmenting path
        std::vector<std::optional<std::pair<std::size_t, std::size_t>>> parents(
            sparseMatrix.size(), std::nullopt);
        std::optional<std::pair<std::size_t, std::size_t>> freeSinkFound =
            std::nullopt;
        while (!freeSinkFound && !stack.empty()) {
          const auto source = stack.top();
          stack.pop();
          for (const auto sink : sparseMatrix[source]) {
            if (invMatching[sink]) { // a matched sink is found
              const auto nextSource = *invMatching[sink];
              if (distance[nextSource] &&
                  *distance[nextSource] == *distance[source] + 1) {
                // the edge from source to sink is a valid edge that was
                // encountered during the bfs
                parents[nextSource] = {source, sink};
                stack.push(nextSource);
              }
            } else { // a free sink is found
              freeSinkFound = {source, sink};
              break;
            }
          }
          distance[source] = std::nullopt; // mark source as visited
        }
        if (freeSinkFound) {
          // augment the matching
          auto source = freeSinkFound->first;
          auto sink = freeSinkFound->second;
          invMatching[sink] =
              source; // that is the additional edge in the matching
          while (source != freeSource) {
            sink = parents[source]->second;
            source = parents[source]->first;
            invMatching[sink] =
                source; // update the matching, i.e., flip the edge from the
            // successor to the predecessor
          }
          freeSources[freeSource] = false;
        }
      }
    }
  }
  // ===-------------------------------===
  if (inverted) {
    return invMatching;
  }
  // invert the matching
  std::vector<std::optional<std::size_t>> matching(sparseMatrix.size(),
                                                   std::nullopt);
  for (std::size_t i = 0; i < invMatching.size(); ++i) {
    if (invMatching[i]) {
      matching[*invMatching[i]] = i;
    }
  }
  return matching;
}
} // namespace na::zoned
