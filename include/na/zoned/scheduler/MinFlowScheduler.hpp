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
#include <queue>
#include <unordered_map>
#include <utility>
#include <vector>

namespace na::zoned {
// A wrapper to add value_type to a view that lacks it.
template <std::ranges::view R>
struct value_type_wrapper
    : public std::ranges::view_interface<value_type_wrapper<R>> {
  R r;

  // Explicitly define value_type
  using value_type = std::iter_value_t<std::ranges::iterator_t<R>>;

  value_type_wrapper() = default;
  constexpr explicit value_type_wrapper(R r) : r(std::move(r)) {}

  constexpr auto begin() { return std::ranges::begin(r); }
  constexpr auto end() { return std::ranges::end(r); }
  constexpr auto begin() const { return std::ranges::begin(r); }
  constexpr auto end() const { return std::ranges::end(r); }
};

// Deduction guide
template <typename R>
value_type_wrapper(R&&) -> value_type_wrapper<std::views::all_t<R>>;

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

public:
  /**
   * @brief A representation of a flow network that allows to compute a max-flow
   * and a min-cost max-flow.
   * @details The flow network consists of directed edges that have a capacity
   * and a (unit) cost per flow unit. This implementation uses the Compressed
   * Sparse Row (CSR) format for a compact graph representation. To this end,
   * the graph must first be initialized, i.e., all vertices and edges must be
   * added, and afterward, before any flow can be computed, the function @ref
   * build must be called once to finalize the graph representation.
   */
  class FlowNetwork {
  public:
    /// Nodes are referred to as non-negative integers.
    using VertexIndex = size_t;
    /// Edges are referred to as non-negative integers.
    using EdgeIndex = size_t;
    /**
     * Flow is expressed with integers. This type must be signed to allow
     * negative flows along residual edges and excesses during the computation
     * of the flow.
     */
    using FlowQuantity = int64_t;
    /**
     * The upper bound of a flow along an edge is expressed as a non-negative
     * integer, the capacity of the edge.
     */
    using CapacityValue = uint64_t;
    /**
     * Costs of edges are expressed as integers. While the costs themselves
     * should be non-negative, this type must be signed to allow for negative
     * reduced costs during the computation of the min-cost max-flow.
     */
    using CostValue = int64_t;

    /// Wrapper around a vector that allows to specify the index type.
    template <std::unsigned_integral I, typename T>
    class IVector : std::vector<T> {
    public:
      using typename std::vector<T>::value_type;
      using std::vector<T>::empty;
      using std::vector<T>::emplace_back;
      using std::vector<T>::clear;
      using std::vector<T>::shrink_to_fit;
      using std::vector<T>::begin;
      using std::vector<T>::cbegin;
      using std::vector<T>::end;
      using std::vector<T>::cend;
      constexpr IVector() = default;
      constexpr IVector(I n, const T& value)
          : std::vector<T>::vector(n, value) {}
      IVector(std::initializer_list<T> il) : std::vector<T>::vector(il) {}
      [[nodiscard]] constexpr auto operator[](I index) const -> const T& {
        return std::vector<T>::operator[](
            static_cast<std::vector<T>::size_type>(index));
      }
      [[nodiscard]] constexpr auto operator[](I index) -> T& {
        return std::vector<T>::operator[](
            static_cast<std::vector<T>::size_type>(index));
      }
      constexpr auto assign(I n, const T& value) -> void {
        std::vector<T>::assign(static_cast<std::vector<T>::size_type>(n),
                               value);
      }
      constexpr auto reserve(I index) -> void {
        std::vector<T>::reserve(static_cast<std::vector<T>::size_type>(index));
      }
      constexpr auto resize(I n, const T& value) -> void {
        std::vector<T>::resize(static_cast<std::vector<T>::size_type>(n),
                               value);
      }
      [[nodiscard]] constexpr auto size() const -> I {
        return I(std::vector<T>::size());
      }
    };

  private:
    //===------------------------------------------------------------------===//
    // Vertex data
    //===------------------------------------------------------------------===//
    /**
     * Used to store the excess of vertices of a pre-flow and pseudo-flow during
     * the computation of the max-flow and min-cost max-flow, respectively.
     */
    IVector<VertexIndex, FlowQuantity> vertexExcess_;
    /**
     * During the computation of the max-flow, the potential is a measure of
     * height or distance to the sink. The sink always has potential 0, and the
     * source's potential is equal to the number of vertices. During the
     * computation of the min-cost max-flow, the potential is a measure of a
     * price for that vertex used to calculate the reduced cost.
     */
    IVector<VertexIndex, CostValue> vertexPotential_;
    /**
     * For fast and cache-efficient access of all outgoing forward edges. It is
     * a vector whose values denote the index in the @ref edgeHead_ vector to
     * the first outgoing forward edge. The other outgoing forward edges are
     * placed subsequently.
     */
    IVector<VertexIndex, EdgeIndex> vertexFirstOutgoingForwardEdge_;
    /**
     * Every forward edge in the graph has a corresponding reverse backward
     * edge. The following data structure is analogue to the one above to
     * retrieve the first outgoing backward edge for a vertex.
     */
    IVector<VertexIndex, EdgeIndex> vertexFirstOutgoingBackwardEdge_;

    //===------------------------------------------------------------------===//
    // Edge data
    //===------------------------------------------------------------------===//
    /**
     * Stores the head of each forward edge, i.e., the vertex at the endpoint of
     * an edge. More precisely, the head of the i-th forward edge is stored as
     * the i-th element in the vector.
     */
    IVector<EdgeIndex, VertexIndex> forwardEdgeHead_;
    /**
     * Stores the tail of each backward edge, i.e., the vertex at the startpoint
     * of an edge. More precisely, the tail of the i-th backward edge is stored
     * as the i-th element in the vector. Note that the actual indices of
     * backward edges are shifted by the number of forward edges, i.e., they
     * start with m, where m is the number of forward edges. Hence, edges with
     * index 0 to (m - 1) are forward edges, and edges with index m to (2m - 1)
     * are backward edges, also see @ref isBackwardEdge. Also note that this
     * vector is used to retrieve the source vertex of forward edges.
     */
    IVector<EdgeIndex, VertexIndex> backwardEdgeHead_;
    /**
     * The capacity of each forward edge denotes the maximum possible flow along
     * this forward edge. The capacity of the i-th edge is stored as the i-th
     * element in this vector. Backward edges do not have a capacity. Their
     * capacity is set to 0.
     */
    IVector<EdgeIndex, FlowQuantity> edgeCapacity_;
    /**
     * The unit cost is the cost per flow unit along this forward edge. To
     * retrieve the actual cost, the unit cost is multiplied with the flow along
     * this forward edge. The unit cost of the i-th edge is the i-th element in
     * the vector. Backward edges do not have a cost.
     */
    IVector<EdgeIndex, CostValue> edgeUnitCost_;
    /**
     * Stores the finally calculated flow in the flow network. During the run of
     * the push-relabel algorithm, it contains the pre-flow or pseudo-flow.
     */
    IVector<EdgeIndex, FlowQuantity> edgeFlow_;
    /**
     * For each edge, it maps to the index of the reverse edge. Note that the
     * indices of backward edges start with m, where m is the number of forward
     * edges, also see @ref reverseEdgeTarget_.
     */
    IVector<EdgeIndex, EdgeIndex> reverseEdge_;
    /**
     * During the calculation of the flows, this queue stores all active
     * vertices, i.e., vertices with positive excess.
     */
    std::queue<VertexIndex> activeVertices_;

    //===------------------------------------------------------------------===//
    // Global variables
    //===------------------------------------------------------------------===//
    /// Total number of vertices
    VertexIndex numVertices_ = 0;
    /// Whether the graph is already finalized and the flow can be calculated.
    bool isBuilt = false;
    /// Becomes @c true when any function to compute a flow is called.
    bool hasFlow = false;
    /// The maximum cost of a forward edge.
    CostValue maxEdgeCost_ = 0;
    /**
     * The resulting total flow of the (min-cost) max-flow, i.e., the inflow
     * into the sink.
     */
    FlowQuantity maximumFlow_ = 0;
    /**
     * The min-cost max-flow algorithm uses epsilon to refine the pseudo-flow
     * gradually. During this procedure, epsilon is gradually reduced and the
     * flow is refined to an epsilon-optimal flow in each refine step.
     */
    CostValue epsilon_ = 0;

    //===------------------------------------------------------------------===//
    // Methods
    //===------------------------------------------------------------------===//
    /**
     * @returns @c true, if the flow network has zero vertices, @c false
     * otherwise.
     */
    [[nodiscard]] constexpr auto hasZeroVertices() const -> bool {
      return numVertices_ == 0;
    }
    /// @returns the number of vertices in the flow network.
    [[nodiscard]] constexpr auto getNumVertices() const -> VertexIndex {
      return numVertices_;
    }
    /// @returns the number of edges in the flow network.
    [[nodiscard]] constexpr auto getNumEdges() const -> EdgeIndex {
      return forwardEdgeHead_.size();
    }
    /**
     * Validates that the given vertex index is within bounds.
     * @param i is the index of the vertex to validate.
     * @throws std::out_of_range if the vertex index is out of bounds.
     */
    auto validateVertexIndex(VertexIndex i) const -> void;
    /**
     * Validates that the given edge index is within bounds.
     * @param i is the index of the edge to validate.
     * @throws std::out_of_range if the edge index is out of bounds.
     */
    auto validateEdgeIndex(EdgeIndex i) const -> void;
    /**
     * Ensures that the function @ref build has already been called.
     * @throws std::logic_error if the function @ref build has not been called.
     */
    auto ensureBuilt() const -> void;
    /**
     * Ensure that the function @ref build has not been called.
     * @throws std::logic_error if the function @ref build has been called.
     */
    auto ensureNotBuilt() const -> void;
    /**
     * Ensure that a flow has already been computed.
     * @throws std::logic_error if no flow has been computed yet.
     */
    auto ensureHasFlow() const -> void;
    /**
     * Converts an unsigned capacity into an equal signed value to ease
     * comparison with signed flow units.
     * @param capacity is the unsigned capacity value.
     * @throws std::invalid_argument if the capacity exceeds the maximum
     * possible positive signed value.
     * @returns capacity as a signed value.
     */
    static auto toFlowQuantityWithOverflowCheck(CapacityValue capacity)
        -> FlowQuantity;
    /// @returns a range of all outgoing forward edges.
    [[nodiscard]] auto getOutgoingForwardEdges(VertexIndex v) const
        -> std::ranges::iota_view<EdgeIndex, EdgeIndex>;
    /// @returns a range of all outgoing backward edges.
    [[nodiscard]] auto getOutgoingBackwardEdges(VertexIndex v) const
        -> std::ranges::iota_view<EdgeIndex, EdgeIndex>;
    /// @returns all outgoing forward and backward edges.
    [[nodiscard]] auto getAllOutgoingEdges(const VertexIndex v) const -> auto {
      ensureBuilt();
      validateVertexIndex(v);
      const auto forwardView = getOutgoingForwardEdges(v);
      const auto backwardView = getOutgoingBackwardEdges(v);
      // todo(yannick): replace with a more efficient concatenation view when
      //  available in the standard library or write a custom one
      std::vector buffer(forwardView.begin(), forwardView.end());
      buffer.insert(buffer.end(), backwardView.begin(), backwardView.end());
      return buffer;
    }
    /**
     * @returns @c true if the given edge is a backward edge, @c false
     * otherwise.
     */
    [[nodiscard]] constexpr auto isBackwardEdge(EdgeIndex i) const -> bool {
      return i >= getNumEdges();
    }
    /**
     * @returns the index of the reverse edge for the given forward or backward
     * edge.
     */
    [[nodiscard]] constexpr auto getReverseEdge(EdgeIndex i) const
        -> EdgeIndex {
      return reverseEdge_[i];
    }
    /// @returns the tail of the given edge without input validation.
    [[nodiscard]] auto getTailFast(EdgeIndex i) const -> VertexIndex;
    /// @returns the head of the given edge without input validation.
    [[nodiscard]] auto getHeadFast(EdgeIndex i) const -> VertexIndex;
    /**
     * @details The residual capacity of a forward edge is equal to the
     * difference of the edge's capacity minus the current flow along the edge.
     * The residual capacity of a backward edge corresponds to the flow along
     * the corresponding forward edge.
     * @returns the capacity of the given edge in the residual graph.
     */
    [[nodiscard]] auto residualEdgeCapacity(EdgeIndex e) -> FlowQuantity;
    /**
     * @details This presents a fast version of @ref residualEdgeCapacity if the
     * forward edge for a backward edge is already available.
     * @param forwardEdge is the forward edge.
     * @param forBackwardEdge whether the residual capacity should be calculated
     * for the corresponding backward edge.
     * @returns the capacity of the given edge in the residual graph.
     */
    [[nodiscard]] auto residualEdgeCapacityFast(EdgeIndex forwardEdge,
                                                bool forBackwardEdge)
        -> FlowQuantity;
    /**
     * @details The reduced cost of an edge is calculated as the sum of the
     * edge's unit cost and the difference of the tail's potential minus the
     * head's potential. For backward edges, the negative unit cost is used. The
     * function requires also the tail of the edge (even though it could be
     * derived) because it is usually already available and leads to more
     * efficient code.
     * @param e is the edge to calculate the reduced cost for.
     * @param u is the tail of the edge.
     * @param v is the head of the edge.
     * @returns the reduced cost of the edge.
     */
    [[nodiscard]] auto reducedCost(EdgeIndex e, VertexIndex u, VertexIndex v)
        -> CostValue;
    /**
     * @details This presents a fast version of @ref reducedCost if the forward
     * edge for a backward edge is already available. It also receives the
     * tail's potential and the head of the edge for situations where this
     * information is already available.
     * @param forwardEdge is the forward edge.
     * @param forBackwardEdge whether the reduced cost should be calculated for
     * the corresponding backward edge.
     * @param tailPotential is the tail's potential for the actual edge, i.e.,
     * is @p forBackwardEdge is @c true, then @code tail =
     * getHead(forwardEdge)@endcode.
     * @param head is the head of the actual edge.
     * @returns the reduced cost of the edge.
     */
    [[nodiscard]] auto reducedCostFast(EdgeIndex forwardEdge,
                                       bool forBackwardEdge,
                                       CostValue tailPotential,
                                       VertexIndex head) -> CostValue;
    /**
     * @brief Initializes the pre-flow to be refined to a maximum flow
     * afterward.
     * @details It initializes the excess and potential of vertices to 0, except
     * the potential of the source vertex, which is set to the number of
     * vertices. It initializes the flow along all edges to 0 and afterward
     * saturates the outgoing forward edges of the source vertex. Finally, the
     * queue of active vertices is initialized accordingly.
     * @param source is the source of the flow to be computed.
     */
    auto initializePreFlow(VertexIndex source) -> void;
    /**
     * @brief Pushes as much flow as the residual capacity of the edge and the
     * vertex's excess allows.
     * @note It additionally takes the tail and head for better performance
     * because it makes an additional lookup superfluous.
     * @param e the edge to push flow along.
     * @param u the edge's tail.
     * @param v the edge's head.
     */
    auto pushPreFlow(EdgeIndex e, VertexIndex u, VertexIndex v) -> void;
    /**
     * Relabel the height or potential of the vertex to 1 + the minimum height
     * of neighbors in the residual graph.
     * @param u is the vertex to relabel.
     */
    auto relabelHeight(VertexIndex u) -> void;
    /**
     * @brief Calls push and relabel operations for the vertex until it becomes
     * inactive.
     * @param u is the active vertex.
     */
    auto dischargeForMaxFlow(VertexIndex u) -> void;
    /**
     * @brief Scales all costs by multiplying the edge's costs with the number
     * of nodes.
     * @details This is necessary, such that the epsilon-optimal max–flow, where
     * epsilon is 1 in the end, is actually optimal and not just an
     * approximation.
     */
    auto scaleCosts() -> void;
    /**
     * @brief Refines the current flow to an epsilon-optimal flow.
     * @details This function iterates over all edges in the residual graph
     * and saturates all edges with negative reduced costs. The resulting flow
     * is again a pseudo-flow. The active vertices are initialized accordingly
     * and afterward processed in a FIFO order using @ref
     * dischargeForMinCostMaxFlow.
     */
    auto refine() -> void;
    /**
     * @brief Calls push and relabel operations for the vertex until it becomes
     * inactive.
     * @see refine
     * @param u is the active vertex.
     */
    auto dischargeForMinCostMaxFlow(VertexIndex u) -> void;
    /**
     * @brief Pushes as much flow as the residual capacity of the edge and the
     * vertex's excess allows.
     * @note It additionally takes the tail and head for better performance
     * because it makes an additional lookup superfluous.
     * @param e the edge to push flow along.
     * @param u the edge's tail.
     * @param v the edge's head.
     * @note The difference to @ref pushPreFlow is a different (more complex)
     * condition whether to add a vertex to the queue of active vertices.
     */
    auto pushPseudoFlow(EdgeIndex e, VertexIndex u, VertexIndex v) -> void;
    /**
     * Relabel the price or potential of the vertex.
     * @param u is the vertex to relabel.
     */
    auto relabelPrice(VertexIndex u) -> void;

  public:
    /// Constructs an empty flow network.
    FlowNetwork() = default;
    /**
     * Constructs an empty flow network and reserves memory for @p
     * reserveNumEdges. Note, in the current implementation it is not necessary
     * to reserve memory, particularly for the vertices as all corresponding
     * vectors are filled when the function @ref build is called and the exact
     * number of vertices is known to the flow network.
     */
    explicit FlowNetwork(size_t reserveNumEdges);
    /**
     * Adds a vertex to the flow network.
     * @note The returned vertex indices never change after being created.
     * @returns the index of the newly created vertex.
     */
    auto addVertex() -> VertexIndex;
    /**
     * Adds a new edge to the flow network.
     * @param source is the tail of the edge.
     * @param target is the head of the edge.
     * @param capacity is the capacitiy of the edge.
     * @param unitCost is the unit cost of the edge.
     * @returns the index of the newly created edge.
     */
    auto addEdgeWithCapacityAndUnitCost(VertexIndex source, VertexIndex target,
                                        CapacityValue capacity,
                                        CostValue unitCost) -> EdgeIndex;
    /// @returns the tail of the given edge.
    [[nodiscard]] auto getTail(EdgeIndex i) const -> VertexIndex;
    /// @returns the head of the given edge.
    [[nodiscard]] auto getHead(EdgeIndex i) const -> VertexIndex;
    /**
     * @brief Finalizes the graph structure for efficient access afterward.
     * @details This function initializes the Compressed Sparse Row (CSR) format
     * of the graph representation. More precisely, it reorders the edges such
     * that they are sorted by their tail. Furthermore, it initializes a vector
     * that carries the index to the first head for each tail. This makes
     * iteration over outgoing edges more efficient. In particular, it makes
     * access to edges and associated data, e.g., capacity more cache efficient.
     * @param permutation will be updated to contain the resulting permutation
     * of edge indices.
     * @note This function must be called before calling @ref solveMaxFlow and
     * @ref solveMinCostMaxFlow. Vice versa, @ref addVertexWithSupply and @ref
     * addEdgeWithCapacityAndUnitCost must be called before calling this
     * function.
     */
    auto build(IVector<EdgeIndex, EdgeIndex>& permutation) -> void;
    /**
     * @brief Applies a given permutation to a container.
     * @details The size of the permutation may be less than or equal to the
     * size of the container. In the former case, only the first elements up to
     * the size of the permutation are permuted.
     * @tparam T is the value type of the @ref IVector.
     * @param permutation is the permutation to apply to the data elements.
     * @param data is the container to be permuted.
     */
    template <typename T>
    static auto
    applyPermutation(const IVector<EdgeIndex, EdgeIndex>& permutation,
                     IVector<EdgeIndex, T>& data) -> void {
      IVector<EdgeIndex, T> data_copy = data;
      for (EdgeIndex i = 0; i < data.size(); ++i) {
        if (i < permutation.size()) {
          assert(permutation[i] < data.size());
          data[permutation[i]] = data_copy[i];
        }
      }
    }
    /**
     * @throws std::out_of_range if the edge index is invalid.
     * @throws std::logic_error if no flow has been computed.
     * @returns the flow along the given edge.
     */
    [[nodiscard]] auto getFlow(EdgeIndex e) const -> FlowQuantity;
    /**
     * @throws std::logic_error if no flow has been computed.
     * @returns the maximum flow of the flow network, i.e., the total inflow
     * into the sink.
     */
    [[nodiscard]] auto getMaximumFlow() const -> uint64_t;
    /**
     * @brief Computes a maximum flow from the source to the sink in the flow
     * network respecting the edge capacities.
     * @details This function implements a push-relabel algorithm to solve the
     * problem. Currently, active vertices are processed in a FIFO order. When
     * an active vertex is processed, @ref pushPreFlow operations are performed
     * as long as possible. If the vertex is still active, one @ref
     * relabelHeight operation is performed followed by a new round of @ref
     * pushPreFlow operations until the vertex becomes inactive. This procedure
     * is implemented in @ref dischargeForMaxFlow.
     * @param source is the source of the flow network.
     * @param sink is the sink of the flow network.
     */
    auto solveMaxFlow(VertexIndex source, VertexIndex sink) -> void;
    /**
     * @brief Computes a minimum cost maximum flow from source to the sink.
     * @details This function first calls @ref solveMaxFlow to compute a maximum
     * flow. The resulting flow is gradually refined to a minimum cost flow
     * using itself a push-relabel algorithm. The active vertices are processed
     * in the same fashion as for @ref solveMaxFlow.
     * @param source is the source of the flow network.
     * @param sink is the sink of the flow network.
     */
    auto solveMinCostMaxFlow(VertexIndex source, VertexIndex sink) -> void;
  };

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
  auto
  minCostFlowScheduling(const std::map<size_t, std::pair<qc::Qubit, qc::Qubit>>&
                            mGateIdxToQubitPair) const
      -> std::unordered_map<size_t, size_t>;

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
   * @param vEst is the output earliest start times of the nodes
   * @param vNodeSlack is the output slack times of the nodes
   */
  auto computeSlackAnalysis(
      size_t numNodes, const std::vector<std::vector<size_t>>& vDagAdj,
      const std::vector<std::vector<size_t>>& vDagPred,
      const std::vector<std::pair<size_t, size_t>>& vDagEdges,
      std::vector<std::pair<std::pair<size_t, size_t>, int>>&
          vEdgeClassification,
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
      -> std::vector<std::vector<size_t>>;

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
   * @param qc is the quantum computation to be scheduled.
   * @returns a pair of two vectors. The first vector contains the layers of
   * single-qubit operations. The second vector contains the layers of two-qubit
   * operations. A pair of qubits represents every two-qubit operation.
   */
  [[nodiscard]] auto schedule(const qc::QuantumComputation& qc) const
      -> std::pair<std::vector<SingleQubitGateLayer>,
                   std::vector<TwoQubitGateLayer>> override;
};
} // namespace na::zoned
