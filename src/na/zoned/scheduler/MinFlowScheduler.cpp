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
MinFlowScheduler::FlowNetwork::FlowNetwork(const size_t reserveNumEdges) {
  forwardEdgeHead_.reserve(reserveNumEdges);
  backwardEdgeHead_.reserve(reserveNumEdges);
  edgeCapacity_.reserve(reserveNumEdges);
  edgeUnitCost_.reserve(reserveNumEdges);
}
auto MinFlowScheduler::FlowNetwork::validateVertexIndex(
    const VertexIndex i) const -> void {
  if (i >= getNumVertices()) {
    std::ostringstream ss;
    ss << "Vertex index " << i << " is out of range [0, " << getNumVertices()
       << ").";
    throw std::out_of_range(ss.str());
  }
}
auto MinFlowScheduler::FlowNetwork::validateEdgeIndex(const EdgeIndex i) const
    -> void {
  if (i >= 2 * getNumEdges()) {
    std::ostringstream ss;
    ss << "Edge index " << i << " is out of range [0, 2 * " << getNumEdges()
       << ").";
    throw std::out_of_range(ss.str());
  }
}
auto MinFlowScheduler::FlowNetwork::ensureBuilt() const -> void {
  if (!isBuilt) {
    throw std::logic_error("The flow network has not been built yet.");
  }
}
auto MinFlowScheduler::FlowNetwork::ensureNotBuilt() const -> void {
  if (isBuilt) {
    throw std::logic_error("The flow network has already been built.");
  }
}
auto MinFlowScheduler::FlowNetwork::ensureHasFlow() const -> void {
  if (!hasFlow) {
    throw std::logic_error("The flow network does not have any flow.");
  }
}
auto MinFlowScheduler::FlowNetwork::toFlowQuantityWithOverflowCheck(
    const CapacityValue capacity) -> FlowQuantity {
  constexpr CapacityValue maxCapacity =
      std::numeric_limits<FlowQuantity>::max();
  if (capacity > maxCapacity) {
    std::ostringstream ss;
    ss << "Capacity " << capacity << " exceeds maximum capacity "
       << maxCapacity;
    throw std::invalid_argument(ss.str());
  }
  return static_cast<FlowQuantity>(capacity);
}
auto MinFlowScheduler::FlowNetwork::getOutgoingForwardEdges(
    const VertexIndex v) const -> std::ranges::iota_view<EdgeIndex, EdgeIndex> {
  return std::views::iota(vertexFirstOutgoingForwardEdge_[v],
                          vertexFirstOutgoingForwardEdge_[v + 1]);
}
auto MinFlowScheduler::FlowNetwork::getOutgoingBackwardEdges(
    const VertexIndex v) const -> std::ranges::iota_view<EdgeIndex, EdgeIndex> {
  return std::views::iota(getNumEdges() + vertexFirstOutgoingBackwardEdge_[v],
                          getNumEdges() +
                              vertexFirstOutgoingBackwardEdge_[v + 1]);
}
auto MinFlowScheduler::FlowNetwork::getTailFast(const EdgeIndex i) const
    -> VertexIndex {
  return getHeadFast(getReverseEdge(i));
}
auto MinFlowScheduler::FlowNetwork::getHeadFast(const EdgeIndex i) const
    -> VertexIndex {
  if (isBackwardEdge(i)) {
    return backwardEdgeHead_[i - getNumEdges()];
  }
  return forwardEdgeHead_[i];
}
auto MinFlowScheduler::FlowNetwork::getFlow(const EdgeIndex e) const
    -> FlowQuantity {
  ensureHasFlow();
  validateEdgeIndex(e);
  return isBackwardEdge(e) ? -edgeFlow_[getReverseEdge(e)] : edgeFlow_[e];
}
auto MinFlowScheduler::FlowNetwork::getMaximumFlow() const -> uint64_t {
  ensureHasFlow();
  return static_cast<uint64_t>(maximumFlow_);
}
auto MinFlowScheduler::FlowNetwork::residualEdgeCapacity(const EdgeIndex e)
    -> FlowQuantity {
  if (isBackwardEdge(e)) {
    return residualEdgeCapacityFast(getReverseEdge(e), true);
  }
  return residualEdgeCapacityFast(e, false);
}
auto MinFlowScheduler::FlowNetwork::residualEdgeCapacityFast(
    const EdgeIndex forwardEdge, const bool forBackwardEdge) -> FlowQuantity {
  if (forBackwardEdge) {
    return edgeFlow_[forwardEdge];
  }
  return edgeCapacity_[forwardEdge] - edgeFlow_[forwardEdge];
}
auto MinFlowScheduler::FlowNetwork::reducedCost(const EdgeIndex e,
                                                const VertexIndex u,
                                                const VertexIndex v)
    -> CostValue {
  if (isBackwardEdge(e)) {
    return reducedCostFast(getReverseEdge(e), true, vertexPotential_[u], v);
  }
  return reducedCostFast(e, false, vertexPotential_[u], v);
}
auto MinFlowScheduler::FlowNetwork::reducedCostFast(
    const EdgeIndex forwardEdge, const bool forBackwardEdge,
    const CostValue tailPotential, const VertexIndex head) -> CostValue {
  return (forBackwardEdge ? -edgeUnitCost_[forwardEdge]
                          : edgeUnitCost_[forwardEdge]) +
         tailPotential - vertexPotential_[head];
}
auto MinFlowScheduler::FlowNetwork::initializePreFlow(const VertexIndex source)
    -> void {
  vertexExcess_.assign(getNumVertices(), 0);
  vertexPotential_.assign(getNumVertices(), 0);
  vertexPotential_[source] = static_cast<CostValue>(getNumVertices());
  edgeFlow_.assign(getNumEdges(), 0);
  activeVertices_ = {};
  std::ranges::for_each(getOutgoingForwardEdges(source),
                        [&](const EdgeIndex e) -> void {
                          const auto c = edgeCapacity_[e];
                          edgeFlow_[e] = c;
                          const auto v = getHeadFast(e);
                          vertexExcess_[v] = c;
                          vertexExcess_[source] -= c;
                          activeVertices_.push(v);
                        });
}
auto MinFlowScheduler::FlowNetwork::pushPreFlow(const EdgeIndex e,
                                                const VertexIndex u,
                                                const VertexIndex v) -> void {
  const auto backwardEdge = isBackwardEdge(e);
  const auto forwardEdge = backwardEdge ? getReverseEdge(e) : e;
  const auto delta = std::min(
      vertexExcess_[u], residualEdgeCapacityFast(forwardEdge, backwardEdge));
  assert(delta > 0);
  if (backwardEdge) {
    edgeFlow_[forwardEdge] -= delta;
  } else {
    edgeFlow_[forwardEdge] += delta;
  }
  vertexExcess_[u] -= delta;
  if (vertexExcess_[v] == 0) {
    activeVertices_.push(v);
  }
  vertexExcess_[v] += delta;
}
auto MinFlowScheduler::FlowNetwork::relabelHeight(const VertexIndex u) -> void {
  auto minPotential = std::numeric_limits<CostValue>::max();
  for (const auto e : getAllOutgoingEdges(u)) {
    if (residualEdgeCapacity(e) > 0) {
      const auto v = getHeadFast(e);
      minPotential = std::min(minPotential, vertexPotential_[v]);
    }
  }
  assert(minPotential < std::numeric_limits<CostValue>::max());
  vertexPotential_[u] = minPotential + 1;
}
auto MinFlowScheduler::FlowNetwork::dischargeForMaxFlow(const VertexIndex u)
    -> void {
  assert(vertexExcess_[u] > 0);
  do {
    for (const auto e : getAllOutgoingEdges(u)) {
      if (residualEdgeCapacity(e) > 0) {
        if (const auto v = getHeadFast(e);
            vertexPotential_[u] == vertexPotential_[v] + 1) {
          pushPreFlow(e, u, v);
          if (vertexExcess_[u] == 0) {
            return;
          }
        }
      }
    }
    assert(vertexExcess_[u] > 0);
    relabelHeight(u);
  } while (vertexExcess_[u] > 0);
}
auto MinFlowScheduler::FlowNetwork::solveMaxFlow(const VertexIndex source,
                                                 const VertexIndex sink)
    -> void {
  ensureBuilt();
  validateVertexIndex(source);
  validateVertexIndex(sink);
  if (source == sink) {
    throw std::invalid_argument("Source and sink cannot be the same.");
  }
  initializePreFlow(source);
  while (!activeVertices_.empty()) {
    const auto u = activeVertices_.front();
    activeVertices_.pop();
    if (u != source && u != sink) {
      dischargeForMaxFlow(u);
    }
  }
  assert(-vertexExcess_[source] == vertexExcess_[sink]);
  maximumFlow_ = vertexExcess_[sink];
  hasFlow = true;
}
auto MinFlowScheduler::FlowNetwork::refine() -> void {
  for (VertexIndex u = 0; u < getNumVertices(); ++u) {
    const auto p = vertexPotential_[u];
    for (const auto e : getAllOutgoingEdges(u)) {
      const auto v = getHeadFast(e);
      const auto isBackward = isBackwardEdge(e);
      if (const auto forwardEdge = isBackward ? getReverseEdge(e) : e;
          reducedCostFast(forwardEdge, isBackward, p, v) < 0) {
        const auto delta = residualEdgeCapacityFast(forwardEdge, isBackward);
        edgeFlow_[forwardEdge] = isBackward ? 0 : edgeCapacity_[e];
        vertexExcess_[u] -= delta;
        vertexExcess_[v] += delta;
      }
    }
  }
  activeVertices_ = {};
  for (VertexIndex u = 0; u < getNumVertices(); ++u) {
    if (vertexExcess_[u] > 0) {
      activeVertices_.push(u);
    }
  }
  while (!activeVertices_.empty()) {
    const auto u = activeVertices_.front();
    activeVertices_.pop();
    dischargeForMinCostMaxFlow(u);
  }
}
auto MinFlowScheduler::FlowNetwork::dischargeForMinCostMaxFlow(
    const VertexIndex u) -> void {
  assert(vertexExcess_[u] > 0);
  do {
    for (const auto e : getAllOutgoingEdges(u)) {
      if (residualEdgeCapacity(e) > 0) {
        if (const auto v = getHeadFast(e); reducedCost(e, u, v) < 0) {
          pushPseudoFlow(e, u, v);
          if (vertexExcess_[u] == 0) {
            return;
          }
        }
      }
    }
    assert(vertexExcess_[u] > 0);
    relabelPrice(u);
  } while (vertexExcess_[u] > 0);
}
auto MinFlowScheduler::FlowNetwork::pushPseudoFlow(const EdgeIndex e,
                                                   const VertexIndex u,
                                                   const VertexIndex v)
    -> void {
  const auto backwardEdge = isBackwardEdge(e);
  const auto forwardEdge = backwardEdge ? getReverseEdge(e) : e;
  const auto delta = std::min(
      vertexExcess_[u], residualEdgeCapacityFast(forwardEdge, backwardEdge));
  assert(delta > 0);
  if (backwardEdge) {
    edgeFlow_[forwardEdge] -= delta;
  } else {
    edgeFlow_[forwardEdge] += delta;
  }
  vertexExcess_[u] -= delta;
  if (-delta < vertexExcess_[v] && vertexExcess_[v] <= 0) {
    activeVertices_.push(v);
  }
  vertexExcess_[v] += delta;
}
auto MinFlowScheduler::FlowNetwork::relabelPrice(const VertexIndex u) -> void {
  auto maxPotential = std::numeric_limits<CostValue>::min();
  for (const auto e : getAllOutgoingEdges(u)) {
    const auto backwardEdge = isBackwardEdge(e);
    if (const auto forwardEdge = backwardEdge ? getReverseEdge(e) : e;
        residualEdgeCapacityFast(forwardEdge, backwardEdge) > 0) {
      const auto v = getHeadFast(e);
      maxPotential = std::max(maxPotential,
                              vertexPotential_[v] -
                                  (backwardEdge ? -edgeUnitCost_[forwardEdge]
                                                : edgeUnitCost_[forwardEdge]));
    }
  }
  vertexPotential_[u] = maxPotential - epsilon_;
}
auto MinFlowScheduler::FlowNetwork::solveMinCostMaxFlow(
    const VertexIndex source, const VertexIndex sink) -> void {
  // all the validation is performed by `solveMaxFlow`
  solveMaxFlow(source, sink);
  vertexPotential_.assign(getNumVertices(), 0);
  // reset the excess of the source and sink "to keep the flow in the network".
  // All other excesses are 0 after `solveMaxFlow`has returned.
  vertexExcess_[source] = 0;
  vertexExcess_[sink] = 0;
  scaleCosts();
  epsilon_ = maxEdgeCost_ * static_cast<CostValue>(getNumEdges());
  do {
    // magic number 5 is taken from the literature and seems to work well
    epsilon_ = std::max(CostValue{1}, epsilon_ / 5);
    refine();
  } while (epsilon_ > 1);
}
auto MinFlowScheduler::FlowNetwork::scaleCosts() -> void {
  std::ranges::for_each(edgeUnitCost_,
                        [this](auto& cost) { cost *= getNumVertices(); });
}
auto MinFlowScheduler::FlowNetwork::addVertex() -> VertexIndex {
  ensureNotBuilt();
  return numVertices_++;
}
auto MinFlowScheduler::FlowNetwork::addEdgeWithCapacityAndUnitCost(
    const VertexIndex source, const VertexIndex target,
    const CapacityValue capacity, const CostValue unitCost) -> EdgeIndex {
  ensureNotBuilt();
  validateVertexIndex(source);
  validateVertexIndex(target);

  backwardEdgeHead_.emplace_back(source);
  forwardEdgeHead_.emplace_back(target);
  edgeCapacity_.emplace_back(toFlowQuantityWithOverflowCheck(capacity));
  edgeUnitCost_.emplace_back(unitCost);
  maxEdgeCost_ = std::max(maxEdgeCost_, unitCost);

  return getNumEdges() - 1;
}
auto MinFlowScheduler::FlowNetwork::getTail(const EdgeIndex i) const
    -> VertexIndex {
  validateEdgeIndex(i);
  return getTailFast(i);
}
auto MinFlowScheduler::FlowNetwork::getHead(const EdgeIndex i) const
    -> VertexIndex {
  validateEdgeIndex(i);
  return getHeadFast(i);
}
auto MinFlowScheduler::FlowNetwork::build(
    IVector<EdgeIndex, EdgeIndex>& permutation) -> void {
  ensureNotBuilt();
  permutation.clear();
  isBuilt = true;
  // if the graph has zero vertices, it also has zero edges, and there is
  // nothing to build; hence, we return early.
  if (hasZeroVertices()) {
    // currently does not free unused memory in case the capacity of the
    // vectors is > 0.
    return;
  }
  // since the following vectors are considered fixed after the end of this
  // function call, we shrink the size to fit its content to release
  // unused memory.
  forwardEdgeHead_.shrink_to_fit();
  edgeCapacity_.shrink_to_fit();
  edgeUnitCost_.shrink_to_fit();
  backwardEdgeHead_.shrink_to_fit();
  // Count the edges outgoing from each vertex to initialize the
  // vertexFirstOutgoingForwardEdge_ vector, which will contain for each vertex
  // the index of the first outgoing edge in the forwardEdgeHead_ vector.
  vertexFirstOutgoingForwardEdge_.assign(getNumVertices() + 1, 0);
  vertexFirstOutgoingForwardEdge_.shrink_to_fit();
  std::ranges::for_each(backwardEdgeHead_, [&](const VertexIndex i) -> void {
    ++vertexFirstOutgoingForwardEdge_[i];
  });
  // Go through the counts and compute the exclusive prefix sum.
  std::exclusive_scan(vertexFirstOutgoingForwardEdge_.begin(),
                      vertexFirstOutgoingForwardEdge_.end(),
                      vertexFirstOutgoingForwardEdge_.begin(), EdgeIndex{0});
  // Check the sentinel value.
  assert(vertexFirstOutgoingForwardEdge_[getNumVertices()] == getNumEdges() &&
         "Sentinel value of vertexFirstOutgoingForwardEdge_ is not correct.");
  // Calculate a permutation of edges to sort them by their source vertex.
  // Note this temporarily alters the vertexFirstOutgoingForwardEdge_ vector,
  // whose state is restored afterward.
  permutation.reserve(getNumEdges());
  std::ranges::for_each(backwardEdgeHead_, [&](const VertexIndex i) {
    permutation.emplace_back(vertexFirstOutgoingForwardEdge_[i]++);
  });
  // Restore the previous state of the vertexFirstOutgoingForwardEdge_ vector.
  for (VertexIndex i = getNumVertices() - 1; i > 0; --i) {
    vertexFirstOutgoingForwardEdge_[i] = vertexFirstOutgoingForwardEdge_[i - 1];
  }
  vertexFirstOutgoingForwardEdge_[0] = 0;
  // Apply the permutation to the target vertices of the edges.
  applyPermutation(permutation, backwardEdgeHead_);
  applyPermutation(permutation, forwardEdgeHead_);
  applyPermutation(permutation, edgeCapacity_);
  applyPermutation(permutation, edgeUnitCost_);
  // Count the backward edges outgoing from each vertex to initialize the
  // vertexFirstOutgoingBackwardEdge_ vector, which will contain for each vertex
  // the index of the first outgoing backward edge in the backwardEdgeHead_
  // vector.
  vertexFirstOutgoingBackwardEdge_.assign(getNumVertices() + 1, 0);
  vertexFirstOutgoingBackwardEdge_.shrink_to_fit();
  std::ranges::for_each(forwardEdgeHead_, [&](const VertexIndex i) -> void {
    ++vertexFirstOutgoingBackwardEdge_[i];
  });
  // Go through the counts and compute the exclusive prefix sum.
  std::exclusive_scan(vertexFirstOutgoingBackwardEdge_.begin(),
                      vertexFirstOutgoingBackwardEdge_.end(),
                      vertexFirstOutgoingBackwardEdge_.begin(), EdgeIndex{0});
  // Check the sentinel value
  assert(vertexFirstOutgoingBackwardEdge_[getNumVertices()] == getNumEdges() &&
         "Sentinel value of vertexFirstOutgoingBackwardEdge_ is not correct.");
  // Calculate a permutation of reverse edges to sort them by their source
  // vertex. Note this temporarily alters the
  // vertexFirstOutgoingBackwardEdge_ vector, whose state is restored afterward.
  IVector<EdgeIndex, EdgeIndex> reversePermutation;
  reversePermutation.reserve(getNumEdges());
  std::ranges::for_each(forwardEdgeHead_, [&](const VertexIndex i) -> void {
    reversePermutation.emplace_back(vertexFirstOutgoingBackwardEdge_[i]++);
  });
  // Restore the previous state of the vertexFirstOutgoingBackwardEdge_ vector.
  for (VertexIndex i = getNumVertices() - 1; i > 0; --i) {
    vertexFirstOutgoingBackwardEdge_[i] =
        vertexFirstOutgoingBackwardEdge_[i - 1];
  }
  vertexFirstOutgoingBackwardEdge_[0] = 0;
  // Apply the permutation to the target vertices of the reverse edges.
  applyPermutation(reversePermutation, backwardEdgeHead_);
  // Initialize the vector to translate between edges and their reverse edges.
  reverseEdge_.assign(2 * getNumEdges(), EdgeIndex{0});
  for (EdgeIndex i = 0; i < getNumEdges(); ++i) {
    const auto edge = permutation[i];
    const auto reverseEdge = reversePermutation[edge] + getNumEdges();
    reverseEdge_[edge] = reverseEdge;
    reverseEdge_[reverseEdge] = edge;
  }
}
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
  std::map<size_t, std::pair<qc::Qubit, qc::Qubit>> mGateIdxToQubitPair;
  size_t gateIdx = 0;
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
  std::unordered_map<size_t, size_t> umGateToTime;
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
      const auto maxNextLayerForQubit =
          *std::ranges::max_element(std::as_const(nextLayerForQubit));
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
    const std::map<size_t, std::pair<qc::Qubit, qc::Qubit>>&
        mGateIdxToQubitPair) const -> std::unordered_map<size_t, size_t> {

  size_t numGate = mGateIdxToQubitPair.size();
  size_t source = 2 * numGate;
  size_t sink = source + 1;
  size_t sourcesSource = source + 2;

  // node idx to gate idx
  std::vector vGateIdx(numGate, 0UL);
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
  std::map<qc::Qubit, size_t> nodeMap; // Maps qubits to latest gate node

  for (size_t nodeIdx = 0; nodeIdx < mGateIdxToQubitPair.size(); nodeIdx++) {
    const auto gateIdxLocal = vGateIdx.at(nodeIdx);
    const auto [q1, q2] = mGateIdxToQubitPair.at(gateIdxLocal);

    for (const auto qubit : {q1, q2}) {
      if (nodeMap.contains(qubit)) {
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
  std::vector<std::pair<std::pair<size_t, size_t>, int>> vEdgeClassification;
  std::vector<int> vEst, vNodeSlack;
  computeSlackAnalysis(numGate, vDagAdj, vDagPred, vDagEdges,
                       vEdgeClassification, vEst, vNodeSlack);

  // 2. Get the timepoint after the function returns
  auto stop = std::chrono::high_resolution_clock::now();
  // 3. Get the difference in timepoints and cast to desired units
  auto duration =
      std::chrono::duration_cast<std::chrono::microseconds>(stop - start);
  // Output the duration
  SPDLOG_INFO("computeSlackAnalysis execution time: {} microseconds",
              duration.count());

  // construct graph for min-cost flow
  FlowNetwork g{};
  for (size_t v = 0; v < sink + 2; v++) {
    g.addVertex();
  }
  FlowNetwork::IVector<FlowNetwork::EdgeIndex, FlowNetwork::EdgeIndex>
      gateEdges;
  gateEdges.reserve(numGate);

  // Add edges with capacity and cost
  // Debug: vEdgeClassification (commented out)
  // std::cout << "Edge Classification:" << std::endl;
  // for (const auto& [costVal, edgeList] : vEdgeClassification) {
  //     std::cout << "Cost " << costVal << ": ";
  //     for (const auto& edge : edgeList) {
  //         std::cout << "(" << edge.first << ", " << edge.second << ") ";
  //     }
  //     std::cout << std::endl;
  // }
  for (const auto& [edge, cost] : vEdgeClassification) {
    gateEdges.emplace_back(g.addEdgeWithCapacityAndUnitCost(
        vOutNodes[edge.first], edge.second, 1, cost));
  }

  for (size_t v = 0; v < numGate; v++) {
    // node decomposition
    g.addEdgeWithCapacityAndUnitCost(v, vOutNodes[v], 1, 0);
    // source to in-node
    g.addEdgeWithCapacityAndUnitCost(source, v, 1, 1);
    // out-node to sink
    g.addEdgeWithCapacityAndUnitCost(vOutNodes[v], sink, 1, 1);
  }
  // source to sink
  g.addEdgeWithCapacityAndUnitCost(source, sink, numGate, 1);
  // source's source to source
  g.addEdgeWithCapacityAndUnitCost(sourcesSource, source, numGate, 0);

  start = std::chrono::high_resolution_clock::now();
  // Solve min-cost max-flow
  FlowNetwork::IVector<FlowNetwork::EdgeIndex, FlowNetwork::EdgeIndex>
      permutation;
  g.build(permutation);
  std::ranges::for_each(
      gateEdges, [&permutation](auto& e) -> void { e = permutation[e]; });
  g.solveMinCostMaxFlow(sourcesSource, sink);

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
  for (const auto e : gateEdges) {
    if (g.getFlow(e) > 0) {
      const auto u = g.getTail(e);
      const auto v = g.getHead(e);
      vFlowEdge.push_back({u - numGate, v});
      std::cout << "(vFlowEdge)" << u - numGate << ", " << v << std::endl;
    }
  }
  SPDLOG_INFO("Flow through gates: {}", vFlowEdge.size());
  start = std::chrono::high_resolution_clock::now();
  // std::cout << "Optimal count: " << vFlowEdge.size() << std::endl;
  // Build result scheduling
  std::vector<std::vector<size_t>> vResultScheduling =
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
  std::unordered_map<size_t, size_t> umGateToTime;
  for (size_t time = 0; time < vResultScheduling.size(); time++) {
    for (const auto& nodeIdx : vResultScheduling[time]) {
      const auto gateIdx = vGateIdx[nodeIdx];
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
    std::vector<std::pair<std::pair<size_t, size_t>, int>>& vEdgeClassification,
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

  // Classify edges and assign costs
  int type1 = -45, type2 = -45, type3 = -15, type4 = -30;
  vEdgeClassification.clear();

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

  // Classify each edge with per-edge cost
  for (const auto& [u, v] : vDagEdges) {
    int uSlack = vNodeSlack[u];
    int vSlack = vNodeSlack[v];
    int edgeCost;
    // two nodes can be scheduled in consecutive time steps
    if (vEst[v] <= vLst[u] + 1) {
      if (uSlack == 0 && vSlack == 0) {
        edgeCost = type1;
      } else if (uSlack > 0 && vSlack > 0) {
        edgeCost = type2;
      } else if ((uSlack == 0 && vSlack > 0 && vZeroSlackDescendant[v]) ||
                 (uSlack > 0 && vSlack == 0 && vZeroSlackAncestor[u])) {
        edgeCost = type3;
      } else {
        edgeCost = type4;
      }
      if (vEst[u] + 1 < vEst[v]) {
        edgeCost += 4; // additional cost if they may not be scheduled in the
                       // consecutive time step
      }
      if (vLst[u] + 1 < vLst[v]) {
        edgeCost += 4; // additional cost if they may not be scheduled in the
                       // consecutive time step
      }
      vEdgeClassification.push_back({{u, v}, edgeCost});
    }
  }
  // print edge cost classification for debugging
  // std::cout << "Edge classification (u, v): cost" << std::endl;
  // for (const auto& [edge, cost] : vEdgeClassification) {
  //   const auto& [u, v] = edge;
  //   std::cout << "(" << u << ", " << v << "): "  << cost << std::endl;
  // }
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
    -> std::vector<std::vector<size_t>> {
  size_t numGates = vEst.size();

  // Build flow graph
  std::vector<std::vector<size_t>> vFlowSucc(numGates);
  std::vector<std::vector<size_t>> vFlowPred(numGates);

  for (const auto& [u, v] : vFlowEdge) {
    vFlowSucc[u].push_back(v);
    vFlowPred[v].push_back(u);
  }
  // SPDLOG_INFO("find weaklyConnectedComponents");
  std::vector<std::unordered_set<size_t>> wccs =
      weaklyConnectedComponents(numGates, vFlowSucc, vFlowPred);
  // Debug: weakly connected components and EST (commented out)
  // std::cout << "wccs" << std::endl;
  // for (auto s : wccs){
  //   SPDLOG_INFO("one wccs");
  //   for (auto i : s){
  //     SPDLOG_INFO("{}", i);
  //   }
  // }
  // std::cout << "vEst" << std::endl;
  // for (auto i : vEst){
  //   std::cout << i << " ";
  // }
  // std::cout << std::endl;
  std::vector<int> vNodeTime(numGates, -1);
  std::vector<std::unordered_set<size_t>> vUnassignedReuseComps;
  // SPDLOG_INFO("Process weaklyConnectedComponents");
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
    // SPDLOG_INFO("propagate queue");
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
  // SPDLOG_INFO("Node time");
  for (size_t i = 0; i < vNodeTime.size(); i++) {
    if (vNodeTime[i] == -1 || vNodeSlack[i] == 0) {
      vNodeTime[i] = vEst[i];
      // SPDLOG_INFO("vNodeTime[{}]: {}", i, vNodeTime[i]);
    }
  }
  // Process unassigned reuse components
  if (vUnassignedReuseComps.size()) {
    // SPDLOG_INFO("Process unassigned reuse components");
    std::unordered_set<size_t> usUnassignedReuseCompIdxs;
    for (size_t i = 0; i < vUnassignedReuseComps.size(); i++) {
      usUnassignedReuseCompIdxs.insert(i);
    }

    for (int i = static_cast<int>(numGates) - 1; i >= 0; i--) {
      size_t tmp = SIZE_MAX;

      // SPDLOG_INFO("gate {}", i);
      // SPDLOG_INFO("usUnassignedReuseCompIdxs");
      for (size_t compIdx : usUnassignedReuseCompIdxs) {
        const auto& comp = vUnassignedReuseComps[compIdx];
        if (comp.find(static_cast<size_t>(i)) != comp.end()) {
          // SPDLOG_INFO("compIdx {}", compIdx);
          tmp = compIdx;
          // for (auto tmp_idx : comp){
          //   SPDLOG_INFO("node {}", tmp_idx);
          // }
          std::vector<size_t> sortedComp(comp.begin(), comp.end());
          std::sort(sortedComp.begin(), sortedComp.end());

          // SPDLOG_INFO("for loop bound {}->{}", vEst[sortedComp.back()],
          // vEst[sortedComp.back()] + vNodeSlack[sortedComp.back()]);
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
          // SPDLOG_INFO("in timeAssignment");
          for (int t : timeAssignment) {
            // SPDLOG_INFO("t: {}", t);
            vNodeTime[sortedComp.back()] = t;
            int numContinuous = 0;

            for (int j_int = static_cast<int>(sortedComp.size()) - 2;
                 j_int >= 0; j_int--) {
              size_t j = static_cast<size_t>(j_int);
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
  // SPDLOG_INFO("Build result scheduling");
  // Build result scheduling
  size_t numStage = static_cast<size_t>(
      *std::max_element(vNodeTime.begin(), vNodeTime.end()) + 1);
  std::vector<std::vector<size_t>> vResultScheduling(numStage);

  for (size_t i = 0; i < vNodeTime.size(); i++) {
    vResultScheduling[static_cast<size_t>(vNodeTime[i])].push_back(i);
  }

  // handle the case where #gates in the stage exceeds the entangling zone
  // capacity e.g., maxTwoQubitGateNumPerLayer_ = 2 stage 0: [g0, g1, g2] ->
  // stage 0: [g0, g1], stage 1: [g2]
  std::vector<std::vector<size_t>> vFinalScheduling;
  for (const auto& stage : vResultScheduling) {
    if (stage.size() <= maxTwoQubitGateNumPerLayer_) {
      vFinalScheduling.emplace_back(stage);
    } else {
      size_t totalGates = stage.size();
      size_t startIdx = 0;
      while (startIdx < totalGates) {
        size_t endIdx =
            std::min(startIdx + maxTwoQubitGateNumPerLayer_, totalGates);
        std::vector subStage(
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
