/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "sc/utils.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <queue>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

void Dijkstra::buildTable(const CouplingMap& couplingMap, Matrix& distanceTable,
                          const Matrix& edgeWeights) {
  // number of qubits
  const auto n = static_cast<std::uint16_t>(edgeWeights.size());

  distanceTable.clear();
  distanceTable.resize(n, std::vector<double>(n, -1.));

  for (std::uint16_t i = 0; i < n; ++i) {
    std::vector<Dijkstra::Node> nodes(n);
    for (std::uint16_t j = 0; j < n; ++j) {
      nodes.at(j).visited = false;
      nodes.at(j).pos = j;
      nodes.at(j).cost = -1.;
    }

    nodes.at(i).cost = 0;

    dijkstra(couplingMap, nodes, i, edgeWeights);

    for (std::uint16_t j = 0; j < n; ++j) {
      if (i == j) {
        distanceTable.at(i).at(j) = 0;
      } else {
        distanceTable.at(i).at(j) = nodes.at(j).cost;
      }
    }
  }
}

void Dijkstra::dijkstra(const CouplingMap& couplingMap,
                        std::vector<Node>& nodes, const std::uint16_t start,
                        const Matrix& edgeWeights) {
  std::priority_queue<Node*, std::vector<Node*>, NodeComparator> queue{};
  queue.push(&nodes.at(start));
  while (!queue.empty()) {
    auto* current = queue.top();
    current->visited = true;
    queue.pop();
    auto pos = current->pos;

    for (const auto& edge : couplingMap) {
      std::optional<std::uint16_t> to = std::nullopt;
      if (pos == edge.first) { // forward edge
        to = edge.second;
      } else if (pos == edge.second) { // back edge
        to = edge.first;
      }
      if (to.has_value()) {
        if (nodes.at(*to).visited) {
          continue;
        }

        Node newNode;
        newNode.cost = current->cost + edgeWeights.at(*pos).at(*to);
        newNode.pos = to;
        if (nodes.at(*to).cost < 0 || newNode < nodes.at(*to)) {
          nodes.at(*to) = newNode;
          queue.push(&nodes.at(*to));
        }
      }
    }
  }
}

void Dijkstra::buildEdgeSkipTable(const CouplingMap& couplingMap,
                                  std::vector<Matrix>& distanceTables,
                                  const Matrix& edgeWeights) {
  /* to find the cheapest distance between 2 qubits skipping any 1 edge, we
  iterate over all edges, for each assume the current edge to be the one skipped
  and are thereby able to retrieve the distance by just adding the distances
  from the source and target qubit to each of the qubits on the edge. Taking the
  minimum over all edges gives us the cheapest distance skipping any 1 edge.

  For skipping any 2 edges, we can use the same approach, but on one side of the
  edge taking not the regular distance but the previously calculated distance
  skipping 1 edge. The same approach can be used for skipping any 3 edges, etc.
  */
  distanceTables.clear();
  distanceTables.emplace_back();
  buildTable(couplingMap, distanceTables.back(), edgeWeights);
  const std::size_t n = edgeWeights.size();
  for (std::size_t k = 1; k <= n; ++k) {
    // k...number of edges to be skipped along each path
    distanceTables.emplace_back(
        n, std::vector<double>(n, std::numeric_limits<double>::max()));
    Matrix* currentTable = &distanceTables.back();
    for (std::size_t q = 0; q < n; ++q) {
      currentTable->at(q).at(q) = 0.;
    }
    bool done = false;
    for (const auto& [e1, e2] : couplingMap) { // edge to be skipped
      for (std::size_t l = 0; l < k; ++l) {
        done = true;
        // l ... number of edges to skip before edge
        for (std::size_t q1 = 0; q1 < n; ++q1) {        // q1 ... source qubit
          for (std::size_t q2 = q1 + 1; q2 < n; ++q2) { // q2 ... target qubit
            currentTable->at(q1).at(q2) =
                std::min(currentTable->at(q1).at(q2),
                         distanceTables.at(l).at(q1).at(e1) +
                             distanceTables.at(k - l - 1).at(e2).at(q2));
            currentTable->at(q1).at(q2) =
                std::min(currentTable->at(q1).at(q2),
                         distanceTables.at(l).at(q1).at(e2) +
                             distanceTables.at(k - l - 1).at(e1).at(q2));
            currentTable->at(q2).at(q1) = currentTable->at(q1).at(q2);
            if (done && currentTable->at(q2).at(q1) > 0) {
              done = false;
            }
          }
        }
      }
    }
    if (done) {
      // all distances of the last matrix where 0
      distanceTables.pop_back();
      break;
    }
  }
}

void Dijkstra::buildSingleEdgeSkipTable(const Matrix& distanceTable,
                                        const CouplingMap& couplingMap,
                                        const double reversalCost,
                                        Matrix& edgeSkipDistanceTable) {
  const std::size_t n = distanceTable.size();
  edgeSkipDistanceTable.clear();
  edgeSkipDistanceTable.resize(
      n, std::vector<double>(n, std::numeric_limits<double>::max()));
  for (std::size_t q = 0; q < n; ++q) {
    edgeSkipDistanceTable.at(q).at(q) = 0.;
  }
  for (const auto& [e1, e2] : couplingMap) {        // edge to be skipped
    for (std::size_t q1 = 0; q1 < n; ++q1) {        // q1 ... source qubit
      for (std::size_t q2 = q1 + 1; q2 < n; ++q2) { // q2 ... target qubit
        edgeSkipDistanceTable.at(q1).at(q2) =
            std::min(edgeSkipDistanceTable.at(q1).at(q2),
                     distanceTable.at(q1).at(e1) + distanceTable.at(e2).at(q2));
        edgeSkipDistanceTable.at(q1).at(q2) =
            std::min(edgeSkipDistanceTable.at(q1).at(q2),
                     distanceTable.at(q1).at(e2) + distanceTable.at(e1).at(q2) +
                         reversalCost);
        if (reversalCost == 0.) {
          edgeSkipDistanceTable.at(q2).at(q1) =
              edgeSkipDistanceTable.at(q1).at(q2);
        } else {
          edgeSkipDistanceTable.at(q2).at(q1) = std::min(
              edgeSkipDistanceTable.at(q2).at(q1),
              distanceTable.at(q2).at(e1) + distanceTable.at(e2).at(q1));
          edgeSkipDistanceTable.at(q2).at(q1) =
              std::min(edgeSkipDistanceTable.at(q2).at(q1),
                       distanceTable.at(q2).at(e2) +
                           distanceTable.at(e1).at(q1) + reversalCost);
        }
      }
    }
  }
}

/// Create a string representation of a given permutation
/// \param pi permutation
/// \return string representation of pi
std::string printPi(std::vector<std::uint16_t>& pi) {
  if (std::ranges::is_sorted(pi)) {
    return "( )";
  }

  std::stringstream perm{};
  perm << '(';
  for (std::uint64_t i = 0; i < pi.size() - 1; i++) {
    perm << pi[i] << ',';
  }
  perm << pi[pi.size() - 1] << ')';

  return perm.str();
}

/// Simple depth-first-search implementation used to check whether a given
/// subset of qubits is connected on the given architecture \param current index
/// of current qubit \param visited visited qubits \param cm coupling map of
/// architecture
void dfs(std::uint16_t current, std::set<std::uint16_t>& visited,
         const CouplingMap& rcm) {
  for (auto edge : rcm) {
    if (edge.first == current) {
      if (!visited.contains(edge.second)) {
        visited.insert(edge.second);
        dfs(edge.second, visited, rcm);
      }
    } else if (edge.second == current) {
      if (!visited.contains(edge.first)) {
        visited.insert(edge.first);
        dfs(edge.first, visited, rcm);
      }
    }
  }
}

std::vector<QubitSubset> subsets(const QubitSubset& input,
                                 const std::size_t size,
                                 const filter_function& filter) {
  const std::size_t n = input.size();
  std::vector<QubitSubset> result{};

  if (size == 0) {
    throw std::invalid_argument("Length of subset must be greater than 0");
  }

  if (size > n) {
    throw std::invalid_argument("Length of subset must be less than or equal "
                                "to the size of the input set");
  }

  if (size == 1) {
    for (const auto& item : input) {
      result.emplace_back();
      result.back().emplace(item);
    }
  } else {
    std::uint64_t i = (1U << size) - 1U;

    while ((i >> n) == 0U) {
      assert(i != 0U);
      std::set<std::uint16_t> v{};
      auto it = input.begin();

      for (std::size_t j = 0U; j < n; j++, ++it) {
        if ((i & (1U << j)) != 0U) {
          v.emplace(*it);
        }
      }
      if (filter == nullptr || filter(v)) {
        result.emplace_back(v);
      }

      // this computes the lexicographical next bitset from a set, see
      // https://graphics.stanford.edu/~seander/bithacks.html#NextBitPermutation
      const std::uint64_t t = (i | (i - 1)) + 1;
      // NOLINTNEXTLINE (clang-analyzer-core.DivideZero)
      i = t | ((((t & -t) / (i & -i)) >> 1) - 1);
    }
  }

  return result;
}

void parseLine(const std::string& line, char separator,
               const std::set<char>& escapeChars,
               const std::set<char>& ignoredChars,
               std::vector<std::string>& result) {
  result.clear();
  std::string word;
  bool inEscape = false;
  for (const char c : line) {
    if (ignoredChars.contains(c)) {
      continue;
    }
    if (inEscape) {
      if (escapeChars.contains(c)) {
        inEscape = false;
      } else {
        word += c;
      }
    } else {
      if (escapeChars.contains(c)) {
        inEscape = true;
      } else if (c == separator) {
        result.push_back(word);
        word = "";
      } else {
        word += c;
      }
    }
  }
  result.push_back(word);
}

CouplingMap getFullyConnectedMap(const std::uint16_t nQubits) {
  CouplingMap result{};
  for (std::uint16_t q = 0; q < nQubits; ++q) {
    for (std::uint16_t p = q + 1; p < nQubits; ++p) {
      result.emplace(q, p);
      result.emplace(p, q);
    }
  }
  return result;
}
