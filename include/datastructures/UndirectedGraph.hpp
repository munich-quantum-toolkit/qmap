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

#include "ir/Definitions.hpp"

#include <cstddef>
#include <functional>
#include <numeric>
#include <optional>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace qc {

/// Pairs do not provide a hash function by default, this is the replacement
template <class T, class U> struct PairHash {
  size_t operator()(const std::pair<T, U>& x) const {
    return combineHash(std::hash<T>{}(x.first), std::hash<U>{}(x.second));
  }
};

/**
 * Class representing generic undirected directed graphs.
 *
 * @tparam V the type of the vertices in the graph. Must implement `operator<<`.
 */
template <class V, class E> class UndirectedGraph final {
  static_assert(std::is_same_v<decltype(std::declval<std::ostream&>()
                                        << std::declval<V>()),
                               std::ostream&>,
                "V must support `operator<<`.");

protected:
  // the adjecency matrix works with indices
  std::vector<std::vector<std::optional<E>>> adjacencyMatrix{};
  // the mapping of vertices to indices in the graph are stored in a map
  std::unordered_map<V, std::size_t> mapping;
  // the inverse mapping is used to get the vertex from the index
  std::vector<V> invMapping;
  // the number of vertices in the graph
  std::size_t nVertices = 0;
  // the number of edges in the graph
  std::size_t nEdges = 0;
  // the degrees of the vertices in the graph
  std::vector<std::size_t> degrees;

public:
  auto addVertex(const V& v) -> void {
    // check whether the vertex is already in the graph, if so do nothing
    if (!mapping.contains(v)) {
      mapping[v] = nVertices;
      invMapping.emplace_back(v);
      ++nVertices;
      for (auto& row : adjacencyMatrix) {
        row.emplace_back(std::nullopt);
      }
      // the first param must be a 1 not nVertices since we are using an upper
      // triangular matrix as adjacency matrix instead of a square matrix
      adjacencyMatrix.emplace_back(1, std::nullopt);
      degrees.emplace_back(0);
    } else {
      std::stringstream ss;
      ss << "The vertex " << v << " is already in the graph.";
      throw std::invalid_argument(ss.str());
    }
  }
  auto addEdge(const V& u, const V& v, const E& e) -> void {
    if (!mapping.contains(u)) {
      addVertex(u);
    }
    if (!mapping.contains(v)) {
      addVertex(v);
    }
    const auto i = mapping.at(u);
    const auto j = mapping.at(v);
    if (i < j) {
      if (adjacencyMatrix[i][j - i] == std::nullopt) {
        ++degrees[i];
        if (i != j) {
          ++degrees[j];
        }
        ++nEdges;
        adjacencyMatrix[i][j - i] = e;
      } else {
        std::stringstream ss;
        ss << "The edge (" << i << ", " << j << ") is already in the graph.";
        throw std::invalid_argument(ss.str());
      }
    } else {
      if (adjacencyMatrix[j][i - j] == std::nullopt) {
        ++degrees[i];
        if (i != j) {
          ++degrees[j];
        }
        ++nEdges;
        adjacencyMatrix[j][i - j] = e;
      } else {
        std::stringstream ss;
        ss << "The edge (" << j << ", " << i << ") is already in the graph.";
        throw std::invalid_argument(ss.str());
      }
    }
  }
  [[nodiscard]] auto getNVertices() const -> std::size_t { return nVertices; }
  [[nodiscard]] auto getNEdges() const -> std::size_t { return nEdges; }
  [[nodiscard]] auto getEdge(const V& v, const V& u) const -> E {
    const auto i = mapping.at(v);
    const auto j = mapping.at(u);
    if (i < j ? adjacencyMatrix[i][j - i] != std::nullopt
              : adjacencyMatrix[j][i - j] != std::nullopt) {
      return i < j ? adjacencyMatrix[i][j - i].value()
                   : adjacencyMatrix[j][i - j].value();
    }
    std::stringstream ss;
    ss << "The edge (" << v << ", " << u << ") does not exist.";
    throw std::invalid_argument(ss.str());
  }
  [[nodiscard]] auto getAdjacentEdges(const V& v) const
      -> std::unordered_set<std::pair<V, V>, PairHash<V, V>> {
    if (!mapping.contains(v)) {
      std::stringstream ss;
      ss << "The vertex " << v << " is not in the graph.";
      throw std::invalid_argument(ss.str());
    }
    const auto i = mapping.at(v);
    std::unordered_set<std::pair<V, V>, PairHash<V, V>> result;
    for (std::size_t j = 0; j < nVertices; ++j) {
      if (i < j ? adjacencyMatrix[i][j - i] != std::nullopt
                : adjacencyMatrix[j][i - j] != std::nullopt) {
        const auto u = invMapping.at(j);
        result.emplace(std::make_pair(v, u));
      }
    }
    return result;
  }
  [[nodiscard]] auto getNeighbours(const V& v) const -> std::unordered_set<V> {
    if (!mapping.contains(v)) {
      std::stringstream ss;
      ss << "The vertex " << v << " is not in the graph.";
      throw std::invalid_argument(ss.str());
    }
    const auto i = mapping.at(v);
    std::unordered_set<V> result;
    for (std::size_t j = 0; j < nVertices; ++j) {
      if (i < j ? adjacencyMatrix[i][j - i] != std::nullopt
                : adjacencyMatrix[j][i - j] != std::nullopt) {
        result.emplace(invMapping.at(j));
      }
    }
    return result;
  }
  [[nodiscard]] auto getDegree(const V& v) const -> std::size_t {
    if (!mapping.contains(v)) {
      std::stringstream ss;
      ss << "The vertex " << v << " is not in the graph.";
      throw std::invalid_argument(ss.str());
    }
    const auto i = mapping.at(v);
    return degrees[i];
  }
  [[nodiscard]] auto getVertices() const -> std::unordered_set<V> {
    return std::accumulate(mapping.cbegin(), mapping.cend(),
                           std::unordered_set<V>(),
                           [](auto acc, const auto& v) {
                             acc.emplace(v.first);
                             return acc;
                           });
  }
  [[nodiscard]] auto isAdjacent(const V& u, const V& v) const -> bool {
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
    return (i < j && adjacencyMatrix[i][j - i] != std::nullopt) or
           (j < i && adjacencyMatrix[j][i - j] != std::nullopt);
  }
  [[nodiscard]] static auto isAdjacentEdge(const std::pair<V, V>& e,
                                           const std::pair<V, V>& f) -> bool {
    return e.first == f.first || e.first == f.second || e.second == f.first ||
           e.second == f.second;
  }
  /// Outputs a string representation of the graph in the DOT format
  [[nodiscard]] auto toString() const -> std::string {
    std::stringstream ss;
    ss << "graph {\n";
    for (const auto& [v, i] : mapping) {
      ss << "  " << i << " [label=\"" << v << "\"];\n";
    }
    for (std::size_t i = 0; i < nVertices; ++i) {
      for (std::size_t j = i + 1; j < nVertices; ++j) {
        if (adjacencyMatrix[i][j - i] != std::nullopt) {
          ss << "  " << i << " -- " << j << ";\n";
        }
      }
    }
    ss << "}\n";
    return ss.str();
  }
  friend auto operator<<(std::ostream& os, const UndirectedGraph& g)
      -> std::ostream& {
    return os << g.toString(); // Using toString() method
  }
};
} // namespace qc
