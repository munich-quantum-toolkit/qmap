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

#include "DirectedGraph.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <vector>

namespace qc {

template <class V> class DirectedAcyclicGraph final : public DirectedGraph<V> {
protected:
  // transitive closure matrix to detect cycles
  std::vector<std::vector<bool>> closureMatrix;

public:
  auto addVertex(const V& v) -> void override {
    DirectedGraph<V>::addVertex(v);
    for (auto& row : closureMatrix) {
      row.emplace_back(false);
    }
    closureMatrix.emplace_back(this->nVertices, false);
    const auto i = this->mapping.at(v);
    closureMatrix[i][i] = true;
  }
  auto addEdge(const V& u, const V& v) -> void override {
    if (!this->mapping.contains(u)) {
      addVertex(u);
    }
    if (!this->mapping.contains(v)) {
      addVertex(v);
    }
    std::size_t const i = this->mapping.at(u);
    std::size_t const j = this->mapping.at(v);
    if (closureMatrix[j][i]) {
      std::ostringstream oss;
      oss << "Adding edge (" << u << ", " << v << ") would create a cycle.";
      throw std::logic_error(oss.str());
    }
    DirectedGraph<V>::addEdge(u, v);
    for (std::size_t k = 0; k < this->nVertices; ++k) {
      if (closureMatrix[k][i]) {
        closureMatrix[k][j] = true;
      }
      if (closureMatrix[j][k]) {
        closureMatrix[i][k] = true;
      }
    }
  }
  [[nodiscard]] auto isReachable(const V& u, const V& v) const -> bool {
    if (!this->mapping.contains(u)) {
      throw std::invalid_argument("Vertex u not in graph.");
    }
    if (!this->mapping.contains(v)) {
      throw std::invalid_argument("Vertex v not in graph.");
    }
    return closureMatrix[this->mapping.at(u)][this->mapping.at(v)];
  }
  /// Perform a depth-first search on the graph and return the nodes in a
  /// topological order
  [[nodiscard]] auto orderTopologically() const -> std::vector<V> {
    std::stack<std::size_t> stack{};
    std::vector<std::size_t> result;
    result.reserve(this->nVertices);
    std::vector visited(this->nVertices, false);
    // visitedInDegree is used to count the incoming edges that have been
    // visited already such that the resulting order of the nodes is one that
    // satisfies a topological ordering
    std::vector<std::size_t> visitedInDegree(this->nVertices, 0);
    // Push nodes with 0 indegree onto the stack
    for (std::size_t k = 0; k < this->nVertices; ++k) {
      if (this->inDegrees[k] == 0) {
        stack.push(k);
        visited[k] = true;
      }
    }
    // Perform DFS
    while (!stack.empty()) {
      const auto u = stack.top();
      stack.pop();
      result.emplace_back(u);

      for (std::size_t k = 0; k < this->nVertices; ++k) {
        if (this->adjacencyMatrix[u][k]) {
          if (!visited[k]) {
            if (++visitedInDegree[k] == this->inDegrees[k]) {
              stack.push(k);
              visited[k] = true;
            }
          }
        }
      }
    }
    // Otherwise graph has a cycle
    assert(result.size() == this->nVertices);
    std::vector<V> vertices;
    vertices.reserve(this->nVertices);
    std::transform(result.cbegin(), result.cend(), std::back_inserter(vertices),
                   [&](const auto i) { return this->invMapping.at(i); });
    return vertices;
  }
};
} // namespace qc
