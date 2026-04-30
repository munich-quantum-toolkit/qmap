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

#include <variant>
#include <vector>

namespace na::zoned {

class NativeGateDecomposer : public DecomposerBase {

  /**
   * A quaternion is represented by an array of four `qc::fp` values `{q0, q1,
   * q2, q3}` denoting the components of the quaternion.
   */
  using Quaternion = std::array<qc::fp, 4>;
  size_t nQubits_ = 0;

  /// A value to use as a margin of error for float equality
  constexpr static qc::fp epsilon =
      std::numeric_limits<qc::fp>::epsilon() * 1024;

public:
  /// The configuration of the NativeGateDecomposer
  struct Config {
    bool thetaOptSchedule = false;
    bool checkFinalCond = false;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Config, thetaOptSchedule,
                                                checkFinalCond);
  };

  /**
   * A minimal struct to store the parameters of a U3 gate along with the qubit
   * it acts on.
   */
  struct StructU3 {
    std::array<qc::fp, 3> angles;
    qc::Qubit qubit;
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
  static auto getU3AnglesFromQuaternion(const Quaternion& quat)
      -> std::array<qc::fp, 3>;

  /**
   * @brief Calculates the largest value of the U3-gate parameter theta from a
   * vector of operations.
   * @param layer is a vector of U3 parameters.
   * @returns the maximal value of theta in the given layer.
   */
  static auto calcThetaMax(const std::vector<StructU3>& layer) -> qc::fp;

  /**
   * @brief Takes a vector of SingleQubitGateLayers and, for each layer,
   * transforms all gates into U3 gates represented by `StructU3` objects.
   * @details It combines all gates acting on the same qubit into a single U3
   * gate.
   * @param layers is a std::vector of SingleQubitGateLayers of a scheduled
   * circuit.
   * @param n_qubits the number of Qubits in the scheduled circuit
   * @returns a vector of vectors of StructU3 objects representing the single
   * qubit gate layers.
   */
  [[nodiscard]] static auto
  transformToU3(const std::vector<SingleQubitGateRefLayer>& layers,
                size_t n_qubits) -> std::vector<std::vector<StructU3>>;
  /**
   * @brief Calculates the decomposition angles of a U3 gate
   * @details Takes a vector of `qc::fp` representing the U3-gate angles of a
   * single-qubit gate and the maximal value of theta for the single qubit gate
   * layer and calculates the transversal decomposition angles as in Nottingham
   * et. al. 2024.
   * @param angles  `std::array` of `qc::fp` representing (theta, phi,
   * lambda).
   * @param theta_max the maximal theta value of the single-qubit gate layer.
   * @returns an array of `qc::fp` values giving the angles (chi, gamma_minus,
   * gamma_plus).
   */
  auto static getDecompositionAngles(const std::array<qc::fp, 3>& angles,
                                     qc::fp theta_max) -> std::array<qc::fp, 3>;

  /**
   * @brief Decomposes a given schedule of operations into the native gate set
   *       and, if theta_opt_scheduling is selected re-schedules them to
   * minimize the total global rotation angle theta across the circuit
   * @details
   * @param nQubits the number of Qubits in the scheduled circuit
   * @param schedule a pair of vectors containing SingleQubitGateRefLayers
   *       and TwoQubitGateLayers
   * @returns a pair of vectors containing SingleQubitLayers and TwoQubitLayers
   *         representing the decomposed (and rescheduled) circuit
   */
  [[nodiscard]] auto
  decompose(size_t nQubits,
            const std::pair<std::vector<SingleQubitGateRefLayer>,
                            std::vector<TwoQubitGateLayer>>& schedule)
      -> std::pair<std::vector<SingleQubitGateLayer>,
                   std::vector<TwoQubitGateLayer>> override;

  /**
   * @class A class implementing a simple DiGraph for use in the scheduling
   *        component of the native gate decomposer.
   * @tparam T , the type of object associated with each node
   */
  template <class T> class DiGraph {
    /// number of nodes in the graph
    std::size_t node_number;
    /// a vector containing the adjacency lists of each node
    std::vector<std::vector<std::pair<std::size_t, double>>> adjacencies_;
    /// a vector containing the values associated with each node
    std::vector<T> node_values_;

  public:
    /**
     * @brief Creates an empty graph to hold objects of type T
     */
    DiGraph() {
      node_number = 0;
      adjacencies_ = std::vector<std::vector<std::pair<std::size_t, double>>>();
      node_values_ = std::vector<T>();
    }

    /**
     * @brief Adds a node with given value to the graph
     * @param node the type T value to be added to the graph
     * @returns the node index of the created node
     */
    auto add_Node(T node) -> std::size_t {
      adjacencies_.emplace_back();
      node_values_.push_back(std::move(node));
      return node_number++;
    }

    /**
     * @brief Adds an edge between two nodes to the graph with given weight
     * @param from index of the node from which the edge originates
     * @param to index of the node the edge is going to
     * @param weight the weight of the edge
     * @returns a bool indicating if adding the edge was successful
     */
    auto add_Edge(std::size_t from, std::size_t to, double weight) -> bool {
      if (from < node_number && to < node_number && from != to) {
        adjacencies_[from].emplace_back(
            std::pair<std::size_t, double>(to, weight));
        return true;
      } else {
        return false;
      }
    }

    /**
     * @brief Adds an edge between two nodes to the graph (weight 1.0)
     * @param from index of the node from which the edge originates
     * @param to index of the node the edge is going to
     * @returns a bool indicating if adding the edge was successful
     */
    auto add_Edge(std::size_t from, std::size_t to) -> bool {
      if (from < node_number && to < node_number && from != to) {
        adjacencies_[from].emplace_back(
            std::pair<std::size_t, double>(to, 1.0));
        return true;
      } else {
        return false;
      }
    }

    /**
     * @brief Gets the value of a given node
     * @param node the node index of a node in the graph
     * @returns an object of type T contained in the given node
     */
    auto get_Node_Value(std::size_t node) -> T {
      if (node < node_number) {
        return node_values_[node];
      } else {
        std::ostringstream oss;
        oss << "ERROR: Node Number out of range: " << node << "\n";
        throw std::invalid_argument(oss.str());
      }
    }

    /**
     * @brief A function which returns the size/number of nodes of the graph
     * @returns the number of nodes in the graph
     */
    [[nodiscard]] auto size() const -> std::size_t { return node_number; }

    /**
     * @brief Returns the successor nodes of a given node
     * @param node the index of a node in the graph
     * @returns a vector containing the node indices of all nodes the passed
     * node has outgoing edges to.
     */
    [[nodiscard]] auto get_adjacent(std::size_t node) const
        -> std::vector<std::pair<std::size_t, double>> {
      return adjacencies_.at(node);
    }
  };
  /**
   * @brief converts a schedule of operations into a directional acyclic graph,
   *        where each operation is a node and each edge represents a dependency
   * @details A circuit made up of U3-Gates (represented by layers of
   * StructU3's) and CZ-Gates (represented by layers of two element arrays
   * denoting control and target qubits) is transformed into a graph modeling
   * the circuit and operational dependencies. Each node contains a std::variant
   * containing either a StructU3 or an array representing a CZ-Gate. Edges
   * between nodes mean that the destination node is dependent on the source
   * node (e.g. that the operation of the source node must be executed before
   * the one of the destination node).
   * @param schedule a pair of vectors containing layers of StructU3's
   * representing U3-Gates and TwoQubitGateLayers
   * @param nQubits the number of Qubits in the scheduled circuit
   * @returns a DiGraph consisting of nodes containing either a StructU3
   *         representation of U3-Gates of an array representation of CZ Gates.
   */
  static auto
  convert_circ_to_dag(const std::pair<std::vector<std::vector<StructU3>>,
                                      std::vector<TwoQubitGateLayer>>& schedule,
                      size_t nQubits)
      -> DiGraph<std::variant<StructU3, std::array<qc::Qubit, 2>>>;
  /**
   * @brief Recursively finds the cheapest path to the start node of the
   *       subproblem graph from a set of leaf nodes.
   * @details
   * @param subproblem_graph the subproblem graph to find the path in
   * @param current_node the node of the current function call
   * @param leaf_nodes a set of nodes with no outgoing edges (aka. leaf nodes)
   * @returns a pair made up of a vector of the indices making up the cheapest
   *         path and the path's total cost (the sum of the maximal theta angles
   *         of each moment)
   */
  static auto cheapest_path_to_start(
      const DiGraph<std::pair<std::vector<size_t>, std::vector<size_t>>>&
          subproblem_graph,
      std::size_t current_node, const std::set<size_t>& leaf_nodes)
      -> std::pair<std::vector<size_t>, double>;

  /**
   * @brief Finds the cheapest (lowest cost) path
   * from the start node to a leaf node in a subproblem_graph
   * @details
   * @param subproblem_graph the subproblem graph
   * @param path a vector containing the indices of all leaf nodes of the graph
   * @returns a vector containing the node inidces of the cheapest path through
   *         the graph
   */
  static auto find_cheapest_path(
      const DiGraph<std::pair<std::vector<std::size_t>,
                              std::vector<std::size_t>>>& subproblem_graph,
      const std::vector<std::size_t>& path) -> std::vector<size_t>;

  /**
   * @brief Finds the leaf nodes (Nodes with no outgoing edges) of a subproblem
   * graph
   * @details
   * @param subproblem_graph the subproblem graph
   * @returns a vectr of node indices for the leaf nodes
   */
  static auto find_leaf_nodes(
      const DiGraph<std::pair<std::vector<std::size_t>,
                              std::vector<std::size_t>>>& subproblem_graph)
      -> std::vector<std::size_t>;

  /**
   * @brief Removes all copies of an element from a vector
   * @param vector the vector of std::size_t to remove the element from
   * @param elem the element to be removed from the vector
   * @returns the vector without the element
   */
  static auto remove_element(const std::vector<std::size_t>& vector,
                             std::size_t elem) -> std::vector<std::size_t>;

  /**
   * @brief Returns all plausible subsets of the current moments to be scheduled
   * @details
   * @param circuit the graph representation of the quantum circuit
   * @param v0_c a vector containing the node indices of the current set of
   *        single Qubit operations
   * @param v_new =[v_p1,v_c1, v_rem] an array containing vectors holding the
   * node indices of the next set of two Qubit operations (v_p), single Qubit
   *        operations (v_c) and all remaining operations (v_rem)
   * @param check_final_cond a bool deciding whether to check for a strict cost
   *        reduction
   * @returns a vector holding pairs of the possible next moments to be
   * scheduled [v_c0, v_p1,vc_1,v_rem] and the moments associated
   */
  static auto get_possible_moments(
      DiGraph<std::variant<StructU3, std::array<qc::Qubit, 2>>>& circuit,
      const std::vector<size_t>& v0_c,
      const std::array<std::vector<size_t>, 3>& v_new, bool check_final_cond)
      -> std::vector<std::pair<std::array<std::vector<size_t>, 4>, qc::fp>>;

  /**
   * @brief Finds the maximal value of the angle theta among the given set of
   * nodes
   * @param circuit the passed circuit graph containing operations
   * @param nodes a vector of node indices for which to find the maximal theta
   * @returns the maximal theta value
   */
  static auto
  max_theta(DiGraph<std::variant<StructU3, std::array<qc::Qubit, 2>>>& circuit,
            const std::vector<std::size_t>& nodes) -> qc::fp;

  /**
   * @brief returns the next two- and single-Qubit moments which can be
   * scheduled
   * @details
   * @param circuit the quantum circuit in graph form
   * @param v a vector containing all unscheduled nodes
   * @param nQubits the number of qubits in the circuit
   * @returns an array containing vectors of the next two Qubit moments which
   * can be scheduled and the remaining nodes: [v_p,v_c,v_rem] v_p: next two
   * qubit gate moment v_c: next single qubit gate moment V-rem: remaining
   * unscheduled nodes
   */
  static auto
  sift(DiGraph<std::variant<StructU3, std::array<qc::Qubit, 2>>>& circuit,
       std::vector<std::size_t> v, size_t nQubits)
      -> std::array<std::vector<size_t>, 3>;

  /**
   * @brief Builds a schedule from a circuit and subproblem graph
   * @details
   * @param circuit the circuit to be scheduled in graph form
   * @param subproblem_graph the subproblem graph of the circuit
   * @returns a pair of vectrs containing layers of StructU3's and two element
   * arrays of Qubits representing CZ gates making up a schedule
   */
  static auto build_schedule(
      DiGraph<std::variant<StructU3, std::array<qc::Qubit, 2>>>& circuit,
      DiGraph<std::pair<std::vector<size_t>, std::vector<size_t>>>&
          subproblem_graph) -> std::pair<std::vector<std::vector<StructU3>>,
                                         std::vector<TwoQubitGateLayer>>;

  /**
   * @brief Adds a node corresponding to the subproblem [v_p,v_c] to the
   *        subproblem graph
   * @details
   * @param v_p a vector of node indices making up a two-Qubit gate moment
   * @param v_c a vector of node indices making up a single-Qubit gate moment
   * @param cost the maximal theta value of operations in v_c (aka. the cost)
   * @param subproblem_graph a subproblem graph of a circuit
   * @param prev_node the node corresponding to the previous subproblem
   * @returns the node index of the node added to the subproblem graph
   */
  static auto add_node_to_sub_prob_graph(
      const std::vector<size_t>& v_p, const std::vector<size_t>& v_c,
      qc::fp cost,
      DiGraph<std::pair<std::vector<size_t>, std::vector<size_t>>>&
          subproblem_graph,
      size_t prev_node) -> size_t;

  /**
   * @brief Recursively creates a subproblem graph for a given circuit
   * @details
   * @param v the current subproblem [v_p,v_c,v_rem] for which to create a
   * schedule
   * @param circuit the graph representation of the circuit to be scheduled
   * @param subproblem_graph the subproblem graph of the circuit to be scheduled
   * @param prev_node the previous node in the subproblem graph
   * @param nQubits the number of qubits in the circuit
   * @param check_final_cond a bool deciding whether the function should only
   *        allow possible next moments with strictly decreasing cost
   * @param memo a map using subproblem hashes as keys and saving as values a
   * pair of a node index in the subproblem graph and an array containing the
   *        cost of the single-Qubit layer in the current subproblem and the
   * total cost of the schedule originating from that subproblem
   * @returns
   */
  static auto schedule_remaining(
      const std::array<std::vector<size_t>, 3>& v,
      DiGraph<std::variant<StructU3, std::array<qc::Qubit, 2>>>& circuit,
      DiGraph<std::pair<std::vector<size_t>, std::vector<size_t>>>&
          subproblem_graph,
      size_t prev_node, size_t nQubits, bool check_final_cond,
      std::map<size_t, std::pair<size_t, std::array<double, 2>>>& memo)
      -> double;

  /**
   * @brief Creates a schedule minimizing the sum total of the global rotation
   * angles theta across a quantum circuit
   * @details
   * @param schedule the preliminary schedule provided
   * @param nQubits the number of qubits in the circuit
   * @returns a schedule minimizing the total rotation angle theta
   */
  [[nodiscard]] auto
  schedule_theta_opt(const std::pair<std::vector<std::vector<StructU3>>,
                                     std::vector<TwoQubitGateLayer>>& schedule,
                     std::size_t nQubits) const
      -> std::pair<std::vector<std::vector<StructU3>>,
                   std::vector<TwoQubitGateLayer>>;
};
} // namespace na::zoned
/**
 * A hash function for subproblems [v_p,v_c,v_rem]
 */
template <> struct std::hash<std::array<std::vector<std::size_t>, 3>> {
  auto operator()(const std::array<std::vector<std::size_t>, 3>& array)
      const noexcept -> std::size_t {
    std::size_t seed = 0U;
    for (const auto& v : array) {
      for (auto node : v) {
        qc::hashCombine(seed, std::hash<std::size_t>{}(node));
      }
    }
    return seed;
  }
};
