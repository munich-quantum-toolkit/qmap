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

#include "na/zoned/Types.hpp"
#include "na/zoned/decomposer/DecomposerBase.hpp"

#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

namespace na::zoned {
/**
 * Decomposes a given schedule of operations into the native gate set and, if
 * `thetaOptScheduling` is enabled, re-schedules them to minimize the total
 * global rotation angle theta across the circuit
 */
class NativeGateDecomposer : public DecomposerBase {
public:
  /**
   * A struct to store the decomposition angles of a U3 gate.
   */
  struct Angles {
    qc::fp theta = 0;
    qc::fp phi = 0;
    qc::fp lambda = 0;
  };

  /**
   * A quaternion is represented by an array of four `qc::fp` values `{q0, q1,
   * q2, q3}` denoting the components of the quaternion. The default initialized
   * Quaternion denotes the identity, i.e., the neutral element, e.g., when
   * calling @ref combineQuaternions with the identity quaternion, the other
   * quaternion is returned.
   */
  struct Quaternion {
    qc::fp a = 1;
    qc::fp b = 0;
    qc::fp c = 0;
    qc::fp d = 0;
  };

  /**
   * A minimal struct to store the parameters of a U3 gate along with the qubit
   * it acts on.
   */
  struct U3Gate {
    Angles angles;
    qc::Qubit qubit;
  };

  /// A value to use as a margin of error for float equality
  constexpr static qc::fp epsilon =
      std::numeric_limits<qc::fp>::epsilon() * 1024;

  /// The configuration of the NativeGateDecomposer
  struct Config {
    bool thetaOptSchedule = false;
    bool checkFinalCond = false;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Config, thetaOptSchedule,
                                                checkFinalCond);
  };

private:
  /// The configuration of the NativeGateDecomposer
  Config config_;

public:
  /// Create a new NativeGateDecomposer.
  NativeGateDecomposer(const Architecture& /* unused */, const Config& config);

  /**
   * @brief Converts commonly used single qubit gates into their Quaternion
   * representation.
   * @details A single qubit gate R_v(phi) with rotation axis v=(v0,v1,v2)
   * and rotation angle phi can be represented as a quaternion:
   * @code quaternion(R_v(phi)) = (cos(phi/2) * I, v0 * sin(phi/2) * X, v1 *
   * sin(phi/2) * Y, v2 * sin(phi/2) * Z)@endcode with X, Y, Z Pauli Matrices.
   * @param op a reference_wrapper to the operation to be converted
   * @returns a quaternion.
   */
  static auto
  convertGateToQuaternion(std::reference_wrapper<const qc::Operation> op)
      -> Quaternion;
  /**
   * @brief Merges the quaternions representing two gates as in a matrix
   * multiplication of the gates.
   * @param q1 the first quaternion to be combined.
   * @param q2 the second quaternion to be combined.
   * @returns an quaternion.
   */
  static auto combineQuaternions(const Quaternion& q1, const Quaternion& q2)
      -> Quaternion;
  /**
   * @brief Calculates the values of the U3-gate parameters theta, phi, and
   * lambda.
   * @param quat is a quaternion representing a single qubit gate.
   * @returns an array of three `qc::fp` values `{theta, phi, lambda}` giving
   * the U3 gate angles.
   */
  static auto getU3AnglesFromQuaternion(const Quaternion& quat) -> Angles;

  /**
   * @brief Calculates the largest value of the U3-gate parameter theta from a
   * vector of operations.
   * @param layers is a vector of U3 parameters.
   * @returns the maximal value of theta in the given layer.
   */
  static auto calcThetaMax(const std::vector<U3Gate>& layers) -> qc::fp;

  /**
   * @brief Takes a vector of SingleQubitGateLayers and, for each layer,
   * transforms all gates into U3 gates represented by `StructU3` objects.
   * @details It combines all gates acting on the same qubit into a single U3
   * gate.
   * @param layers is a std::vector of SingleQubitGateLayers of a scheduled
   * circuit.
   * @param nQubits is the number of qubits in the scheduled circuit.
   * @returns a vector of vectors of StructU3 objects representing the single
   * qubit gate layers.
   */
  [[nodiscard]] static auto
  transformToU3(const std::vector<SingleQubitGateRefLayer>& layers,
                size_t nQubits) -> std::vector<std::vector<U3Gate>>;
  /**
   * @brief Calculates the decomposition angles of a U3 gate
   * @details Takes a vector of `qc::fp` representing the U3-gate angles of a
   * single-qubit gate and the maximal value of theta for the single qubit gate
   * layer and calculates the transversal decomposition angles as in Nottingham
   * et. al. 2024.
   * @param angles  `std::array` of `qc::fp` representing (theta, phi,
   * lambda).
   * @param thetaMax the maximal theta value of the single-qubit gate layer.
   * @returns an array of `qc::fp` values giving the angles (chi, gammaMinus,
   * gammaPlus).
   */
  auto static getDecompositionAngles(const Angles& angles, qc::fp thetaMax)
      -> Angles;

  [[nodiscard]] auto
  decompose(size_t nQubits,
            const std::vector<SingleQubitGateRefLayer>& singleQubitGateLayers,
            const std::vector<TwoQubitGateLayer>& twoQubitGateLayers) const
      -> DecompositionResult override;

  /**
   * A class implementing a simple DiGraph for use in the scheduling
   *        component of the native gate decomposer.
   * @tparam T is the type of object associated with each node
   */
  template <class T> class DirectedGraph {
    /// number of nodes in the graph
    size_t nNodes_;
    /// a vector containing the adjacency lists of each node
    std::vector<std::vector<std::pair<size_t, double>>> adjacencies_;
    /// a vector containing the values associated with each node
    std::vector<T> nodeValues_;

  public:
    /**
     * @brief Creates an empty graph to hold objects of type T.
     */
    DirectedGraph() {
      nNodes_ = 0;
      adjacencies_ = std::vector<std::vector<std::pair<size_t, double>>>();
      nodeValues_ = std::vector<T>();
    }

    /**
     * @brief Adds a node with a given value to the graph.
     * @param node the type T value to be added to the graph.
     * @returns the node index of the created node.
     */
    auto add_Node(T&& node) -> size_t {
      adjacencies_.emplace_back();
      nodeValues_.emplace_back(std::move(node));
      return nNodes_++;
    }

    /**
     * @brief Adds an edge between two nodes to the graph with the given weight.
     * @param from is the index of the node from which the edge originates.
     * @param to is the index of the node the edge is going to.
     * @param weight is the weight of the edge.
     * @returns a bool indicating if adding the edge was successful.
     */
    auto addEdge(const size_t from, const size_t to, const double weight)
        -> bool {
      if (from < nNodes_ && to < nNodes_ && from != to) {
        adjacencies_[from].emplace_back(to, weight);
        return true;
      }
      return false;
    }

    /**
     * @brief Adds an edge between two nodes to the graph (weight 1.0).
     * @param from is the index of the node from which the edge originates.
     * @param to is the index of the node the edge is going to.
     * @returns a bool indicating if adding the edge was successful.
     */
    auto addEdge(const size_t from, const size_t to) -> bool {
      if (from < nNodes_ && to < nNodes_ && from != to) {
        adjacencies_[from].emplace_back(to, 1.0);
        return true;
      }
      return false;
    }

    /**
     * @brief Gets the value of a given node.
     * @param node is the node index of a node in the graph.
     * @returns an object of type T contained in the given node.
     */
    auto getNodeValue(const size_t node) -> T& {
      if (node < nNodes_) {
        return nodeValues_[node];
      }
      std::ostringstream oss;
      oss << "Node Number out of range: " << node << " (nNodes_=" << nNodes_
          << ")";
      throw std::invalid_argument(oss.str());
    }

    /// @returns the number of nodes in the graph.
    [[nodiscard]] auto size() const -> size_t { return nNodes_; }

    /**
     * @brief Returns the successor nodes of a given node
     * @param node is the index of a node in the graph
     * @returns a vector containing the node indices of all nodes the passed
     * node has outgoing edges to.
     */
    [[nodiscard]] auto getAdjacent(const size_t node) const
        -> const std::vector<std::pair<size_t, double>>& {
      return adjacencies_.at(node);
    }
  };
  /**
   * @brief Converts a schedule of operations into a directional acyclic graph
   * (DAG), where each operation is a node and each edge represents a
   * dependency.
   * @details A circuit made up of U3-Gates (represented by layers of
   * StructU3's) and CZ-Gates (represented by layers of two element arrays
   * denoting control and target qubits) is transformed into a graph modeling
   * the circuit and operational dependencies. Each node contains a std::variant
   * containing either a StructU3 or an array representing a CZ-Gate. Edges
   * between nodes mean that the destination node is dependent on the source
   * node (e.g. that the operation of the source node must be executed before
   * the one of the destination node).
   * @param schedule is a pair of vectors containing layers of StructU3's
   * representing U3-Gates and TwoQubitGateLayers.
   * @param nQubits is the number of qubits in the scheduled circuit.
   * @returns a DiGraph consisting of nodes containing either a StructU3
   * representation of U3-Gates of an array representation of CZ Gates.
   */
  static auto
  convertCircuitToDAG(const std::pair<std::vector<std::vector<U3Gate>>,
                                      std::vector<TwoQubitGateLayer>>& schedule,
                      size_t nQubits)
      -> DirectedGraph<std::variant<U3Gate, std::array<qc::Qubit, 2>>>;
  /**
   * @brief Recursively finds the cheapest path to the start node of the
   * subproblem graph from a set of leaf nodes.
   * @param subproblemGraph is the subproblem graph to find the path in.
   * @param currentNode is the node of the current function call.
   * @param leafNodes is a set of nodes with no outgoing edges (aka. leaf
   * nodes).
   * @param memo is a map used to store previously computed paths and their
   * costs to avoid redundant calculations.
   * @returns a pair made up of a vector of the indices making up the cheapest
   * path and the path's total cost (the sum of the maximal theta angles of each
   * layer)
   */
  static auto cheapestPathToStart(
      const DirectedGraph<std::pair<std::vector<size_t>, std::vector<size_t>>>&
          subproblemGraph,
      size_t currentNode, const std::unordered_set<size_t>& leafNodes,
      std::unordered_map<size_t, std::pair<std::vector<size_t>, qc::fp>>& memo)
      -> std::pair<std::vector<size_t>, double>;

  /**
   * @brief Finds the cheapest (lowest cost) path from the start node to a leaf
   * node in a subproblemGraph.
   * @param subproblemGraph is the subproblem graph.
   * @param leafNodes is a vector containing the indices of all leaf nodes of
   * the graph.
   * @returns a vector containing the node indices of the cheapest path through
   * the graph.
   */
  static auto findCheapestPath(
      const DirectedGraph<std::pair<std::vector<size_t>, std::vector<size_t>>>&
          subproblemGraph,
      const std::vector<size_t>& leafNodes) -> std::vector<size_t>;

  /**
   * @brief Finds the leaf nodes (nodes with no outgoing edges) of a subproblem
   * graph.
   * @param subproblemGraph is the subproblem graph.
   * @returns a vector of node indices for the leaf nodes.
   */
  static auto findLeafNodes(
      const DirectedGraph<std::pair<std::vector<size_t>, std::vector<size_t>>>&
          subproblemGraph) -> std::vector<size_t>;

  /**
   * @brief Removes all copies of an element from a vector.
   * @param vector is the vector of size_t to remove the element from.
   * @param elem is the element to be removed from the vector.
   * @returns the vector without the element.
   */
  static auto removeElement(const std::vector<size_t>& vector, size_t elem)
      -> std::vector<size_t>;

  /**
   * @brief Returns all plausible subsets of the current layers to be
   * scheduled.
   * @param circuit is the graph representation of the quantum circuit.
   * @param currentSingleQubitGates is a vector containing the node indices of
   * the current set of single-qubit gates.
   * @param nextSubproblem is an array [twoQubitGates, singleQubitGates,
   * remainingGates] containing vectors holding the node indices of the next set
   * of two-qubit gates, single-qubit gates and all remaining gates.
   * @param checkFinalCond is a bool deciding whether to check for a strict cost
   * reduction.
   * @returns a vector holding pairs of the possible next layers to be
   * scheduled [currentSingleQubitGates, twoQubitGates, singleQubitGates,
   * remainingGates] and the layers associated.
   */
  static auto getPossibleLayers(
      DirectedGraph<std::variant<U3Gate, std::array<qc::Qubit, 2>>>& circuit,
      const std::vector<size_t>& currentSingleQubitGates,
      const std::array<std::vector<size_t>, 3>& nextSubproblem,
      bool checkFinalCond)
      -> std::vector<std::pair<std::array<std::vector<size_t>, 4>, qc::fp>>;

  /**
   * @brief Finds the maximal value of the angle theta among the given set of
   * nodes.
   * @param circuit is the passed circuit graph containing operations.
   * @param nodes is a vector of node indices for which to find the maximal
   * theta.
   * @returns the maximal theta value.
   */
  static auto maxTheta(
      DirectedGraph<std::variant<U3Gate, std::array<qc::Qubit, 2>>>& circuit,
      const std::vector<size_t>& nodes) -> qc::fp;

  /**
   * @brief returns the next two- and single-Qubit layers which can be
   * scheduled.
   * @param circuit is the quantum circuit in graph form.
   * @param remainingNodes is a vector containing all unscheduled nodes.
   * @param nQubits is the number of qubits in the circuit.
   * @returns an array containing vectors of the next single-qubit gate and
   * two-qubit gate layers which can be scheduled and the remaining nodes:
   * [twoQubitGates, singleQubitGates, remainingGates]
   */
  static auto
  sift(DirectedGraph<std::variant<U3Gate, std::array<qc::Qubit, 2>>>& circuit,
       std::vector<size_t> remainingNodes, size_t nQubits)
      -> std::array<std::vector<size_t>, 3>;

  /**
   * @brief Builds a schedule from a circuit and subproblem graph.
   * @param circuit is the circuit to be scheduled in graph form.
   * @param subproblemGraph is the subproblem graph of the circuit.
   * @returns a pair of vectors containing layers of `StructU3`'s and two
   * element arrays of qubits representing CZ gates making up a schedule.
   */
  static auto buildSchedule(
      DirectedGraph<std::variant<U3Gate, std::array<qc::Qubit, 2>>>& circuit,
      DirectedGraph<std::pair<std::vector<size_t>, std::vector<size_t>>>&
          subproblemGraph) -> std::pair<std::vector<std::vector<U3Gate>>,
                                        std::vector<TwoQubitGateLayer>>;

  /**
   * @brief Adds a node corresponding to the subproblem [twoQubitGates,
   * singleQubitGates] to the subproblem graph.
   * @param twoQubitGates is a vector of node indices making up a two-qubit gate
   * layer.
   * @param singleQubitGates is a vector of node indices making up a
   * single-qubit gate layer.
   * @param cost is the maximal theta value of operations in @p singleQubitGates
   * (aka. the cost).
   * @param subproblemGraph is a subproblem graph of a circuit.
   * @param prevNode is the node corresponding to the previous subproblem.
   * @returns the node index of the node added to the subproblem graph.
   */
  static auto addNodeToSubproblemGraph(
      const std::vector<size_t>& twoQubitGates,
      const std::vector<size_t>& singleQubitGates, qc::fp cost,
      DirectedGraph<std::pair<std::vector<size_t>, std::vector<size_t>>>&
          subproblemGraph,
      size_t prevNode) -> size_t;

  /**
   * @brief Recursively creates a subproblem graph for a given circuit.
   * @param v is the current subproblem [twoQubitGates, singleQubitGates,
   * remainingGates] for which to create a schedule.
   * @param circuit is the graph representation of the circuit to be scheduled.
   * @param subproblemGraph is the subproblem graph of the circuit to be
   * scheduled.
   * @param prevNode is the previous node in the subproblem graph.
   * @param nQubits is the number of qubits in the circuit.
   * @param checkFinalCond is a bool deciding whether the function should only
   * allow possible next layers with strictly decreasing cost.
   * @param memo is a map using subproblem hashes as keys and the actual
   * subproblem as values. A subproblem is stored as a pair of a node index in
   * the subproblem graph and an array containing the cost of the single-qubit
   * layer in the current subproblem and the total cost of the schedule
   * originating from that subproblem.
   * @returns the cost of the schedule originating from the current subproblem.
   */
  static auto scheduleRemaining(
      const std::array<std::vector<size_t>, 3>& v,
      DirectedGraph<std::variant<U3Gate, std::array<qc::Qubit, 2>>>& circuit,
      DirectedGraph<std::pair<std::vector<size_t>, std::vector<size_t>>>&
          subproblemGraph,
      size_t prevNode, size_t nQubits, bool checkFinalCond,
      std::unordered_map<size_t, std::pair<size_t, std::array<double, 2>>>&
          memo) -> double;

  /**
   * @brief Creates a schedule minimizing the total sum of the global rotation
   * angles theta across a quantum circuit.
   * @param schedule is the preliminary schedule.
   * @param nQubits is the number of qubits in the circuit.
   * @returns a schedule minimizing the total rotation angle theta
   */
  [[nodiscard]] auto
  scheduleThetaOpt(const std::pair<std::vector<std::vector<U3Gate>>,
                                   std::vector<TwoQubitGateLayer>>& schedule,
                   size_t nQubits) const
      -> std::pair<std::vector<std::vector<U3Gate>>,
                   std::vector<TwoQubitGateLayer>>;
};
} // namespace na::zoned
/**
 * A hash function for subproblems [twoQubitGates, singleQubitGates,
 * remainingGates].
 */
template <> struct std::hash<std::array<std::vector<size_t>, 3>> {
  auto
  operator()(const std::array<std::vector<size_t>, 3>& array) const noexcept
      -> size_t {
    size_t seed = 0;
    std::ranges::for_each(array, [&seed](const std::vector<size_t>& v) -> void {
      std::ranges::for_each(v, [&seed](const size_t node) -> void {
        qc::hashCombine(seed, std::hash<size_t>{}(node));
      });
    });
    return seed;
  }
};
