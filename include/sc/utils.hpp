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

#include "ir/operations/OpType.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using Matrix = std::vector<std::vector<double>>;
using Edge = std::pair<std::uint16_t, std::uint16_t>;
using CouplingMap = std::set<Edge>;
using QubitSubset = std::set<std::uint16_t>;

struct Exchange {
  Exchange(const std::uint16_t f, const std::uint16_t s, const qc::OpType type)
      : first(f), second(s),
        middleAncilla(std::numeric_limits<decltype(middleAncilla)>::max()),
        op(type) {}
  Exchange(const std::uint16_t f, const std::uint16_t s,
           const std::uint16_t middleAnc, const qc::OpType type)
      : first(f), second(s), middleAncilla(middleAnc), op(type) {}
  std::uint16_t first;
  std::uint16_t second;
  std::uint16_t middleAncilla;
  qc::OpType op;
};

class QMAPException : public std::runtime_error {
  std::string msg;

public:
  explicit QMAPException(std::string m)
      : std::runtime_error("QMAP Exception"), msg(std::move(m)) {}

  [[nodiscard]] const char* what() const noexcept override {
    return msg.c_str();
  }
};

class Dijkstra {
public:
  struct Node {
    /** true if the node has already been expanded */
    bool visited = false;
    /** current qubit */
    std::optional<std::uint16_t> pos = std::nullopt;
    /** current cost of the path */
    double cost = -1.;
  };

  /**
   * @brief builds a distance table containing the minimal costs for moving
   * logical qubits from one physical qubit to another (along the cheapest path)
   *
   * e.g. cost of moving qubit q1 onto q2:
   * distanceTable[q1][q2]
   *
   * @param couplingMap coupling map specifying all edges in the architecture
   * @param distanceTable target table
   * @param edgeWeights matrix containing costs for swapping any two, connected
   * qubits (this might be uniform for all edges or different for each edge, as
   * e.g. in the case of fidelity-aware distances or distances on
   * mixed bi/unidirectional architectures)
   */
  static void buildTable(const CouplingMap& couplingMap, Matrix& distanceTable,
                         const Matrix& edgeWeights);
  /**
   * @brief builds a 3d matrix containing the distance tables giving the minimal
   * distances between 2 qubit when upto k edges can be skipped.
   *
   * e.g. cost of moving qubit q1 onto q2 skipping upto 3 edges:
   * distanceTables[3][q1][q2]
   *
   * if k > distanceTables.size() a cost of 0 can be assumed
   *
   * @param couplingMap coupling map specifying all edges in the architecture
   * @param distanceTables vector to fill with target tables (from 0 skips in
   * the first entry to k skips in the last entry, where k is the last index
   * not containing a matrix of pure 0s i.e. k+1 = diameter of the coupling
   * graph)
   * @param edgeWeights matrix containing costs for swapping any two, connected
   * qubits (this might be uniform for all edges or different for each edge, as
   * e.g. in the case of fidelity-aware distances or distances on
   * mixed bi/unidirectional architectures)
   */
  static void buildEdgeSkipTable(const CouplingMap& couplingMap,
                                 std::vector<Matrix>& distanceTables,
                                 const Matrix& edgeWeights);
  /**
   * @brief builds a distance table containing the minimal costs for moving
   * logical qubits from one physical qubit to another (along the cheapest path)
   * while skipping a single edge, i.e. equivalent to buildEdgeSkipTable(...)[1]
   *
   * An additional reversal cost can be specified, which is added to the cost if
   * the skipped edge is a back edge
   *
   * @param distanceTable 2d matrix containing distances between any 2 qubits:
   * distanceTable[source][target]
   * @param couplingMap coupling map specifying all edges in the architecture
   * @param reversalCost cost for reversing an edge
   * @param edgeSkipDistanceTable target distance table
   */
  static void buildSingleEdgeSkipTable(const Matrix& distanceTable,
                                       const CouplingMap& couplingMap,
                                       double reversalCost,
                                       Matrix& edgeSkipDistanceTable);

protected:
  static void dijkstra(const CouplingMap& couplingMap, std::vector<Node>& nodes,
                       std::uint16_t start, const Matrix& edgeWeights);

  struct NodeComparator {
    bool operator()(const Node* x, const Node* y) { return x->cost > y->cost; }
  };
};

inline bool operator<(const Dijkstra::Node& x, const Dijkstra::Node& y) {
  return x.cost < y.cost;
}

/// Iterating routine through all combinations
/// \tparam Iterator iterator type
/// \param first iterator to beginning
/// \param k current iterator
/// \param last iterator to end
/// \return true if another combination was found
template <typename Iterator>
bool nextCombination(Iterator first, Iterator k, Iterator last) {
  /* Credits: Thomas Draper */
  if ((first == last) || (first == k) || (last == k)) {
    return false;
  }
  Iterator itr1 = first;
  Iterator itr2 = last;
  ++itr1;
  if (last == itr1) {
    return false;
  }
  itr1 = last;
  --itr1;
  itr1 = k;
  --itr2;
  while (first != itr1) {
    if (*--itr1 < *itr2) {
      Iterator j = k;
      while (!(*itr1 < *j)) {
        ++j;
      }
      std::iter_swap(itr1, j);
      ++itr1;
      ++j;
      itr2 = k;
      std::rotate(itr1, j, last);
      while (last != j) {
        ++j;
        ++itr2;
      }
      std::rotate(k, itr2, last);
      return true;
    }
  }
  std::rotate(first, k, last);
  return false;
}

/// Create a string representation of a given permutation
/// \param pi permutation
/// \return string representation of pi
std::string printPi(std::vector<std::uint16_t>& pi);

/// Simple depth-first-search implementation used to check whether a given
/// subset of qubits is connected on the given architecture
///
/// \param current index of current qubit
/// \param visited visited qubits
/// \param cm coupling map of architecture
void dfs(std::uint16_t current, std::set<std::uint16_t>& visited,
         const CouplingMap& rcm);

using filter_function = std::function<bool(const QubitSubset&)>;
std::vector<QubitSubset> subsets(const QubitSubset& input, std::size_t size,
                                 const filter_function& filter = nullptr);

void parseLine(const std::string& line, char separator,
               const std::set<char>& escapeChars,
               const std::set<char>& ignoredChars,
               std::vector<std::string>& result);
CouplingMap getFullyConnectedMap(std::uint16_t nQubits);
