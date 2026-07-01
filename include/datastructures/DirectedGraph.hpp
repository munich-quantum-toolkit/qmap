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

#include <cstddef>
#include <numeric>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace qc {

/**
 * Class representing generic directed graphs.
 *
 * @tparam V the type of the vertices in the graph. Must implement `operator<<`.
 */
template <class V> class DirectedGraph {
  static_assert(std::is_same_v<decltype(std::declval<std::ostream&>()
                                        << std::declval<V>()),
                               std::ostream&>,
                "V must support `operator<<`.");

protected:
  // the adjecency matrix works with indices
  std::vector<std::vector<bool>> adjacencyMatrix;
  // the mapping of vertices to indices in the graph are stored in a map
  std::unordered_map<V, std::size_t> mapping;
  // the inverse mapping is used to get the vertex from the index
  std::vector<V> invMapping;
  // the number of vertices in the graph
  std::size_t nVertices = 0;
  // the number of edges in the graph
  std::size_t nEdges = 0;
  // the in-degrees of the vertices in the graph
  std::vector<std::size_t> inDegrees;
  // the out-degrees of the vertices in the graph
  std::vector<std::size_t> outDegrees;

public:
  DirectedGraph() = default;
  DirectedGraph(const DirectedGraph&) = default;
  DirectedGraph(DirectedGraph&&) = default;
  DirectedGraph& operator=(const DirectedGraph&) = default;
  DirectedGraph& operator=(DirectedGraph&&) = default;
  virtual ~DirectedGraph() = default;
  virtual auto addVertex(const V& v) -> void {
    // check whether the vertex is already in the graph, if so do nothing
    if (mapping.contains(v)) {
      std::stringstream ss;
      ss << "The vertex " << v << " is already in the graph.";
      throw std::invalid_argument(ss.str());
    }
    mapping[v] = nVertices;
    invMapping.emplace_back(v);
    ++nVertices;
    for (auto& row : adjacencyMatrix) {
      row.emplace_back(false);
    }
    adjacencyMatrix.emplace_back(nVertices, false);
    inDegrees.emplace_back(0);
    outDegrees.emplace_back(0);
  }
  virtual auto addEdge(const V& u, const V& v) -> void {
    if (!mapping.contains(u)) {
      addVertex(u);
    }
    if (!mapping.contains(v)) {
      addVertex(v);
    }
    const auto i = mapping.at(u);
    const auto j = mapping.at(v);
    if (!adjacencyMatrix[i][j]) {
      adjacencyMatrix[i][j] = true;
      ++outDegrees[i];
      ++inDegrees[j];
      ++nEdges;
    }
  }
  [[nodiscard]] auto getNVertices() const -> std::size_t { return nVertices; }
  [[nodiscard]] auto getNEdges() const -> std::size_t { return nEdges; }
  [[nodiscard]] auto getInDegree(const V& v) const -> std::size_t {
    if (!mapping.contains(v)) {
      std::stringstream ss;
      ss << "The vertex " << v << " is not in the graph.";
      throw std::invalid_argument(ss.str());
    }
    const auto i = mapping.at(v);
    return inDegrees[i];
  }
  [[nodiscard]] auto getOutDegree(const V& v) const -> std::size_t {
    if (!mapping.contains(v)) {
      std::stringstream ss;
      ss << "The vertex " << v << " is not in the graph.";
      throw std::invalid_argument(ss.str());
    }
    const auto i = mapping.at(v);
    return outDegrees[i];
  }
  [[nodiscard]] auto getVertices() const -> std::unordered_set<V> {
    return std::accumulate(mapping.cbegin(), mapping.cend(),
                           std::unordered_set<V>(),
                           [](auto acc, const auto& v) {
                             acc.emplace(v.first);
                             return acc;
                           });
  }
  [[nodiscard]] auto isEdge(const V& u, const V& v) const -> bool {
    if (!mapping.contains(u)) {
      std::stringstream ss;
      ss << "The vertex " << u << " is not in the graph.";
      throw std::invalid_argument(ss.str());
    }
    if (!mapping.contains(v)) {
      std::stringstream ss;
      ss << "The vertex " << v << " is not in the graph.";
      throw std::invalid_argument(ss.str());
    }
    const auto i = mapping.at(u);
    const auto j = mapping.at(v);
    return adjacencyMatrix[i][j];
  }
  /// Outputs a string representation of the graph in the DOT format
  [[nodiscard]] auto toString() const -> std::string {
    std::stringstream ss;
    ss << "digraph {\n";
    for (const auto& [v, i] : mapping) {
      ss << "  " << i << " [label=\"" << v << "\"];\n";
    }
    for (std::size_t i = 0; i < nVertices; ++i) {
      for (std::size_t j = 0; j < nVertices; ++j) {
        if (adjacencyMatrix[i][j]) {
          ss << "  " << i << " -> " << j << ";\n";
        }
      }
    }
    ss << "}\n";
    return ss.str();
  }
  friend auto operator<<(std::ostream& os, const DirectedGraph& g)
      -> std::ostream& {
    return os << g.toString(); // Using toString() method
  }
};
} // namespace qc
