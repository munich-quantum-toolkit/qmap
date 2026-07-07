/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "na/zoned/decomposer/NativeGateDecomposer.hpp"

#include "ir/operations/CompoundOperation.hpp"
#include "ir/operations/Operation.hpp"
#include "ir/operations/StandardOperation.hpp"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <numeric>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
#include <variant>
#include <vector>

namespace na::zoned {
namespace {
/**
 * Helper to combine multiple lambdas into a single overload set for
 * `std::visit`.
 */
template <class... Ts> struct overloads : Ts... {
  using Ts::operator()...;
};
} // namespace
NativeGateDecomposer::NativeGateDecomposer(const Architecture&,
                                           const Config& config)
    : config_(config) {}
auto NativeGateDecomposer::convertGateToQuaternion(
    const std::reference_wrapper<const qc::Operation> op) -> Quaternion {
  assert(op.get().getNqubits() == 1 && "Works only for single-qubit gates.");
  switch (op.get().getType()) {
  case qc::RZ:
  case qc::P:
    return {cos(op.get().getParameter().front() / 2), 0, 0,
            sin(op.get().getParameter().front() / 2)};
  case qc::Z:
    return {0, 0, 0, 1};
  case qc::S:
    return {cos(qc::PI_4), 0, 0, sin(qc::PI_4)};
  case qc::Sdg:
    return {cos(-qc::PI_4), 0, 0, sin(-qc::PI_4)};
  case qc::T:
    return {cos(qc::PI_4 / 2), 0, 0, sin(qc::PI_4 / 2)};
  case qc::Tdg:
    return {cos(-qc::PI_4 / 2), 0, 0, sin(-qc::PI_4 / 2)};
  case qc::U:
    return combineQuaternions(
        combineQuaternions({cos(op.get().getParameter().at(1) / 2), 0, 0,
                            sin(op.get().getParameter().at(1) / 2)},
                           {cos(op.get().getParameter().front() / 2), 0,
                            sin(op.get().getParameter().front() / 2), 0}),
        {cos(op.get().getParameter().at(2) / 2), 0, 0,
         sin(op.get().getParameter().at(2) / 2)});
  case qc::U2:
    return combineQuaternions(
        combineQuaternions({cos(op.get().getParameter().front() / 2), 0, 0,
                            sin(op.get().getParameter().front() / 2)},
                           {cos(qc::PI_4), 0, sin(qc::PI_4), 0}),
        {cos(op.get().getParameter().at(1) / 2), 0, 0,
         sin(op.get().getParameter().at(1) / 2)});
  case qc::RX:
    return {cos(op.get().getParameter().front() / 2),
            sin(op.get().getParameter().front() / 2), 0, 0};
  case qc::RY:
    return {cos(op.get().getParameter().front() / 2), 0,
            sin(op.get().getParameter().front() / 2), 0};
  case qc::H:
    return combineQuaternions(
        combineQuaternions({1, 0, 0, 0}, {cos(qc::PI_4), 0, sin(qc::PI_4), 0}),
        {cos(qc::PI_2), 0, 0, sin(qc::PI_2)});
  case qc::X:
    return {0, 1, 0, 0};
  case qc::Y:
    return {0, 0, 1, 0};
  case qc::Vdg:
    return combineQuaternions(
        combineQuaternions({cos(qc::PI_4), 0, 0, sin(qc::PI_4)},
                           {cos(-qc::PI_4), 0, sin(-qc::PI_4), 0}),
        {cos(-qc::PI_4), 0, 0, sin(-qc::PI_4)});
  case qc::SX:
    return combineQuaternions(
        combineQuaternions({cos(-qc::PI_4), 0, 0, sin(-qc::PI_4)},
                           {cos(qc::PI_4), 0, sin(qc::PI_4), 0}),
        {cos(qc::PI_4), 0, 0, sin(qc::PI_4)});
  case qc::SXdg:
  case qc::V:
    return combineQuaternions(
        combineQuaternions({cos(-qc::PI_4), 0, 0, sin(-qc::PI_4)},
                           {cos(-qc::PI_4), 0, sin(-qc::PI_4), 0}),
        {cos(qc::PI_4), 0, 0, sin(qc::PI_4)});
  default:
    std::ostringstream oss;
    oss << "Unsupported single-qubit gate: " << op.get().getType();
    throw std::invalid_argument(oss.str());
  }
}

auto NativeGateDecomposer::combineQuaternions(const Quaternion& q1,
                                              const Quaternion& q2)
    -> Quaternion {
  return {q1.a * q2.a - q1.b * q2.b - q1.c * q2.c - q1.d * q2.d,
          q1.a * q2.b + q1.b * q2.a + q1.c * q2.d - q1.d * q2.c,
          q1.a * q2.c - q1.b * q2.d + q1.c * q2.a + q1.d * q2.b,
          q1.a * q2.d + q1.b * q2.c - q1.c * q2.b + q1.d * q2.a};
}

auto NativeGateDecomposer::getU3AnglesFromQuaternion(const Quaternion& quat)
    -> Angles {
  Angles angles;
  if (std::fabs(quat.a) > epsilon || std::fabs(quat.d) > epsilon) {
    angles.theta =
        2. * std::atan2(std::sqrt(quat.c * quat.c + quat.b * quat.b),
                        std::sqrt(quat.a * quat.a + quat.d * quat.d));
    const qc::fp alpha1 = std::atan2(quat.d, quat.a); // (phi+ lambda) /2
    if (std::fabs(quat.b) > epsilon || std::fabs(quat.c) > epsilon) {
      const qc::fp alpha2 = -1 * std::atan2(quat.b, quat.c); //(phi-lambda)/2
      angles.phi = alpha1 + alpha2;                          // phi
      angles.lambda = alpha1 - alpha2;
    } else {
      angles.phi = 0;
      angles.lambda = 2 * alpha1;
    }
  } else {
    angles.theta = qc::PI;
    if (std::fabs(quat.b) > epsilon || std::fabs(quat.c) > epsilon) {
      angles.phi = 0;
      angles.lambda = 2 * std::atan2(quat.b, quat.c);
    } else {
      throw std::invalid_argument("Invalid quaternion");
    }
  }
  return angles;
}

auto NativeGateDecomposer::calcThetaMax(const std::vector<U3Gate>& layers)
    -> qc::fp {
  assert(!layers.empty() && "Empty layer.");
  const auto thetas =
      layers | std::views::transform([](const auto& gate) -> qc::fp {
        return std::fabs(gate.angles.theta);
      });
  return *std::ranges::max_element(thetas);
}
auto NativeGateDecomposer::transformToU3(
    const std::vector<SingleQubitGateRefLayer>& layers, const size_t nQubits)
    -> std::vector<std::vector<U3Gate>> {
  std::vector<std::vector<U3Gate>> newLayers;
  for (const auto& layer : layers) {
    std::vector<std::vector<std::reference_wrapper<const qc::Operation>>>
        gatesPerQubit(nQubits);
    std::ranges::for_each(layer, [&gatesPerQubit](const auto& gate) -> void {
      // if compound operations, go instead over the contained operations
      if (gate.get().isCompoundOperation()) {
        const auto& compoundOp =
            dynamic_cast<const qc::CompoundOperation&>(gate.get());
        std::ranges::for_each(
            compoundOp, [&gatesPerQubit](const auto& subGate) -> void {
              assert(subGate->getNqubits() == 1 &&
                     "Gate has to be a single qubit gate, in particular, no "
                     "nested compound operations are allowed.");
              gatesPerQubit[subGate->getTargets().front()].emplace_back(
                  *subGate);
            });
      } else {
        assert(gate.get().getNqubits() == 1 &&
               "Gate has to be a single qubit gate.");
        gatesPerQubit[gate.get().getTargets().front()].emplace_back(gate);
      }
    });
    auto& newLayer = newLayers.emplace_back();
    std::ranges::transform(
        std::views::iota(0UL, gatesPerQubit.size()) |
            std::views::filter([&gatesPerQubit](const auto i) -> bool {
              return !gatesPerQubit[i].empty();
            }),
        std::back_inserter(newLayer), [&gatesPerQubit](const auto i) -> U3Gate {
          const auto& gates = gatesPerQubit[i];
          const auto& quat = std::accumulate(
              gates.begin(), gates.end(), Quaternion{},
              [](const Quaternion& q, const auto& gate) -> Quaternion {
                return combineQuaternions(q, convertGateToQuaternion(gate));
              });
          const auto& angles = getU3AnglesFromQuaternion(quat);
          return U3Gate{.angles = angles,
                        .qubit = gates.front().get().getTargets().front()};
        });
  }
  return newLayers;
}
auto NativeGateDecomposer::getDecompositionAngles(const Angles& angles,
                                                  const qc::fp thetaMax)
    -> Angles {
  qc::fp alpha;
  Angles decompAngles;
  // U3(theta,phi_min(phi),phi_plus(lambda))->Rz(gamma_minus)GR(theta_max/2,
  // PI_2)Rz(chi)GR(-theta_max/2,PI_2)RZ(gamma_plus)
  const auto sinSquareDiff = sin(thetaMax / 2) * sin(thetaMax / 2) -
                             sin(angles.theta / 2) * sin(angles.theta / 2);
  if (std::fabs(sinSquareDiff) < epsilon) {
    decompAngles.theta = qc::PI;
    if (std::fabs(cos(thetaMax / 2)) < epsilon) {
      alpha = 0;
    } else {
      alpha = qc::PI_2;
    }
  } else {
    const auto kappa = std::sqrt(
        (sin(angles.theta / 2) * sin(angles.theta / 2)) / sinSquareDiff);
    alpha = atan(cos(thetaMax / 2) * kappa);
    decompAngles.theta = fmod(2 * atan(kappa), qc::TAU);
  }
  const auto beta = angles.theta < 0 ? -1 * qc::PI_2 : qc::PI_2;
  // gamma_plus
  decompAngles.lambda = fmod(angles.lambda - (alpha + beta), qc::TAU);
  // gamma_minus
  decompAngles.phi = fmod(angles.phi - (alpha - beta), qc::TAU);
  return decompAngles;
}

auto NativeGateDecomposer::decompose(
    const size_t nQubits,
    const std::vector<SingleQubitGateRefLayer>& singleQubitGateLayers,
    const std::vector<TwoQubitGateLayer>& twoQubitGateLayers) const
    -> DecompositionResult {
  auto u3Layers = transformToU3(singleQubitGateLayers, nQubits);
  std::vector<TwoQubitGateLayer> newTwoQubitLayers;
  if (config_.thetaOptSchedule) {
    auto [optSingleQubitGateLayers, optTwoQubitGateLayers] =
        scheduleThetaOpt(std::pair(u3Layers, twoQubitGateLayers), nQubits);
    u3Layers = std::move(optSingleQubitGateLayers);
    newTwoQubitLayers = std::move(optTwoQubitGateLayers);
  } else {
    newTwoQubitLayers = twoQubitGateLayers;
  }
  std::vector<SingleQubitGateLayer> newSingleQubitLayers;
  for (const auto& layer : u3Layers) {
    auto& newLayer = newSingleQubitLayers.emplace_back();
    if (!layer.empty()) {
      const auto thetaMax = calcThetaMax(layer);
      SingleQubitGateLayer frontLayer;
      SingleQubitGateLayer midLayer;
      SingleQubitGateLayer backLayer;

      for (auto gate : layer) {
        const auto& [theta, phi, lambda] =
            getDecompositionAngles(gate.angles, thetaMax);
        frontLayer.emplace_back(std::make_unique<const qc::StandardOperation>(
            gate.qubit, qc::RZ, std::vector{phi}));
        midLayer.emplace_back(std::make_unique<const qc::StandardOperation>(
            gate.qubit, qc::RZ, std::vector{theta}));
        backLayer.emplace_back(std::make_unique<const qc::StandardOperation>(
            gate.qubit, qc::RZ, std::vector{lambda}));
      }
      std::vector<std::unique_ptr<qc::Operation>> globalRotation;
      std::vector<std::unique_ptr<qc::Operation>> globalReversRotation;
      for (size_t i = 0; i < nQubits; ++i) {
        globalRotation.emplace_back(std::make_unique<qc::StandardOperation>(
            i, qc::RY, std::vector{thetaMax / 2}));
        globalReversRotation.emplace_back(
            std::make_unique<qc::StandardOperation>(
                i, qc::RY, std::vector{-thetaMax / 2}));
      }
      // combine all lists into a flat list
      std::ranges::move(frontLayer, std::back_inserter(newLayer));
      newLayer.emplace_back(std::make_unique<const qc::CompoundOperation>(
          std::move(globalRotation), true));
      std::ranges::move(midLayer, std::back_inserter(newLayer));
      newLayer.emplace_back(std::make_unique<const qc::CompoundOperation>(
          std::move(globalReversRotation), true));
      std::ranges::move(backLayer, std::back_inserter(newLayer));
    }
  }
  return {.singleQubitLayers = std::move(newSingleQubitLayers),
          .twoQubitLayers = std::move(newTwoQubitLayers)};
}

auto NativeGateDecomposer::findCheapestPath(
    const DirectedGraph<std::pair<std::vector<std::size_t>,
                                  std::vector<std::size_t>>>& subproblemGraph,
    const std::vector<std::size_t>& leafNodes) -> std::vector<size_t> {
  const std::unordered_set leaves(leafNodes.begin(), leafNodes.end());
  // Memory map: Subproblem nodes as keys
  std::unordered_map<size_t, std::pair<std::vector<size_t>, qc::fp>> memo;
  auto [path, cost] = cheapestPathToStart(subproblemGraph, 0, leaves, memo);
  path.resize(path.size() - 1);
  std::ranges::reverse(path);
  return path;
}
namespace {
/// Returns true if set1 and set2 share no elements.
template <class T>
auto disjunct(const std::unordered_set<T>& set1,
              const std::unordered_set<T>& set2) -> bool {
  return std::ranges::all_of(
      set1, [&set2](const T& elem) -> bool { return !set2.contains(elem); });
}
} // namespace

auto NativeGateDecomposer::cheapestPathToStart(
    const DirectedGraph<std::pair<std::vector<std::size_t>,
                                  std::vector<std::size_t>>>& subproblemGraph,
    std::size_t currentNode, const std::unordered_set<size_t>& leafNodes,
    std::unordered_map<size_t, std::pair<std::vector<size_t>, qc::fp>>& memo)
    -> std::pair<std::vector<std::size_t>, double> {
  std::vector<std::pair<std::vector<std::size_t>, double>> possiblePaths;
  // Check the memoization map
  if (memo.contains(currentNode)) {
    return memo.at(currentNode);
  }
  // Base case
  for (const auto [target, cost] : subproblemGraph.getAdjacent(currentNode)) {
    if (leafNodes.contains(target)) {
      possiblePaths.emplace_back(std::vector{target, currentNode}, cost);
    }
  }
  // Recursive case
  if (possiblePaths.empty()) {
    for (auto [target, cost] : subproblemGraph.getAdjacent(currentNode)) {
      auto [path, accCost] =
          cheapestPathToStart(subproblemGraph, target, leafNodes, memo);
      path.emplace_back(currentNode);
      possiblePaths.emplace_back(path, accCost + cost);
    }
  }
  // Choose the cheapest path
  assert(!possiblePaths.empty() && "No path found to leaf nodes.");
  const auto& bestPathWithCost = *std::ranges::min_element(
      possiblePaths,
      [](const auto& a, const auto& b) -> bool { return a.second < b.second; });
  memo[currentNode] = bestPathWithCost;
  return bestPathWithCost;
}

auto NativeGateDecomposer::findLeafNodes(
    const DirectedGraph<std::pair<std::vector<std::size_t>,
                                  std::vector<std::size_t>>>& subproblemGraph)
    -> std::vector<std::size_t> {
  std::vector<std::size_t> leafNodes;
  std::ranges::copy(std::views::iota(0UL, subproblemGraph.size()) |
                        std::views::filter([&subproblemGraph](auto i) -> bool {
                          return subproblemGraph.getAdjacent(i).empty();
                        }),
                    std::back_inserter(leafNodes));
  return leafNodes;
}

auto NativeGateDecomposer::getPossibleLayers(
    const DirectedGraph<std::variant<U3Gate, std::array<qc::Qubit, 2>>>&
        circuit,
    const std::vector<size_t>& currentSingleQubitGates,
    const std::array<std::vector<size_t>, 3>& nextSubproblem,
    bool checkFinalCond)
    -> std::vector<std::pair<std::array<std::vector<std::size_t>, 4>, qc::fp>> {

  auto vP1Star = nextSubproblem[0];
  auto vc1Star = nextSubproblem[1];
  std::vector<size_t> vP1Square;
  std::vector<size_t> vc1Square;

  auto vc0Cost = maxTheta(circuit, currentSingleQubitGates);
  auto vc1Cost = maxTheta(circuit, nextSubproblem[1]);
  auto origCombCost = vc0Cost + vc1Cost;
  auto newVc1Cost = std::max(vc0Cost, vc1Cost);

  std::array vArg{currentSingleQubitGates, nextSubproblem[0], nextSubproblem[1],
                  nextSubproblem[2]};
  std::vector args{std::pair(vArg, vc0Cost)};
  if (currentSingleQubitGates.empty()) {
    return args;
  }
  // Sort currentSingleQubitGates from highest to lowest theta
  std::vector vSort(currentSingleQubitGates);
  std::ranges::sort(
      vSort, std::greater{}, [&circuit](const auto& gate) -> double {
        return std::fabs(
            std::get<U3Gate>(circuit.getNodeValue(gate)).angles.theta);
      });
  // Check Condition 1
  std::vector<std::pair<std::array<std::vector<std::size_t>, 2>,
                        std::pair<std::unordered_set<qc::Qubit>, qc::fp>>>
      potentialArg;
  auto prevTheta =
      std::fabs(std::get<U3Gate>(circuit.getNodeValue(vSort[0])).angles.theta);
  double thisTheta;
  std::unordered_set mkQubits{
      std::get<U3Gate>(circuit.getNodeValue(vSort[0])).qubit};
  for (size_t i = 0; i < vSort.size(); i++) {
    thisTheta = std::fabs(
        std::get<U3Gate>(circuit.getNodeValue(vSort[i])).angles.theta);
    if (thisTheta != prevTheta) {
      std::vector discarded(vSort.begin(),
                            vSort.begin() + static_cast<int64_t>(i));
      std::vector kept(vSort.begin() + static_cast<int64_t>(i), vSort.end());
      potentialArg.emplace_back(std::array{kept, discarded},
                                std::pair{mkQubits, thisTheta});
      prevTheta = thisTheta;
      mkQubits.clear();
    }
    mkQubits.insert(std::get<U3Gate>(circuit.getNodeValue(vSort[i])).qubit);
  }
  std::vector<std::size_t> emplaceBackNodes;
  std::unordered_set<qc::Qubit> pSquareQubits;

  for (auto pot : potentialArg) {
    // Check Condition 2
    emplaceBackNodes.clear();
    for (auto node : vP1Star) {
      std::unordered_set qubits = {
          std::get<std::array<qc::Qubit, 2>>(circuit.getNodeValue(node))[0],
          std::get<std::array<qc::Qubit, 2>>(circuit.getNodeValue(node))[1]};
      if (!disjunct(qubits, pot.second.first)) {
        emplaceBackNodes.emplace_back(node);
        pSquareQubits.merge(qubits);
      }
    }
    for (auto node : emplaceBackNodes) {
      const auto ret = std::ranges::remove(vP1Star, node);
      vP1Star.erase(ret.begin(), ret.end());
      vP1Square.emplace_back(node);
    }

    if (vP1Star.empty()) {
      break;
    }
    // Check Condition 3
    std::unordered_set<qc::Qubit> pushQubits = pot.second.first;
    pushQubits.insert(pSquareQubits.begin(), pSquareQubits.end());
    emplaceBackNodes.clear();

    for (auto node : vc1Star) {
      std::unordered_set qubits = {
          std::get<U3Gate>(circuit.getNodeValue(node)).qubit};
      if (!disjunct(qubits, pushQubits)) {
        emplaceBackNodes.emplace_back(node);
      }
    }
    for (auto node : emplaceBackNodes) {
      std::erase(vc1Star, node);
      vc1Square.emplace_back(node);
    }

    if (vc1Star.empty()) {
      break;
    }
    // Check Condition 4
    if (!checkFinalCond || pot.second.second + newVc1Cost < origCombCost) {
      vArg = {pot.first[0], vP1Star, pot.first[1], nextSubproblem[2]};
      vArg[2].insert(vArg[2].end(), vc1Star.begin(), vc1Star.end());
      vArg[3].insert(vArg[3].end(), vc1Square.begin(), vc1Square.end());
      vArg[3].insert(vArg[3].end(), vP1Square.begin(), vP1Square.end());
      args.emplace_back(vArg, pot.second.second);
    }
  }
  return args;
}

auto NativeGateDecomposer::convertCircuitToDAG(
    const std::pair<std::vector<std::vector<U3Gate>>,
                    std::vector<TwoQubitGateLayer>>& schedule,
    const std::size_t nQubits)
    -> DirectedGraph<std::variant<U3Gate, std::array<qc::Qubit, 2>>> {
  // std::variant<StructU3, std::array<qc::Qubit, 2>> instead of
  // Unique_pointer For Readout:
  DirectedGraph<std::variant<U3Gate, std::array<qc::Qubit, 2>>> graph;
  std::vector<std::vector<size_t>> qubitPaths(nQubits);
  // TODO:assert that One more sql exists than mql ??
  for (size_t i = 0; i < schedule.second.size(); ++i) {
    for (const auto& s : schedule.first.at(i)) {
      size_t node = graph.addNode(s);
      qubitPaths.at(s.qubit).emplace_back(node);
    }

    for (const auto& gate : schedule.second.at(i)) {
      size_t node = graph.addNode(gate);
      qubitPaths.at(gate[0]).emplace_back(node);
      qubitPaths.at(gate[1]).emplace_back(node);
    }
  }
  SPDLOG_DEBUG("Added Nodes");
  for (const auto& s : schedule.first.back()) {
    size_t node = graph.addNode(s);
    qubitPaths.at(s.qubit).emplace_back(node);
  }
  for (std::size_t i = 0; i < qubitPaths.size(); ++i) {
    if (qubitPaths.at(i).size() > 0) {
      for (std::size_t op = 0; op < (qubitPaths.at(i).size() - 1); ++op) {
        graph.addEdge(qubitPaths.at(i).at(op), qubitPaths.at(i).at(op + 1));
      }
    }
  }
  return graph;
}

auto NativeGateDecomposer::maxTheta(
    const DirectedGraph<std::variant<U3Gate, std::array<qc::Qubit, 2>>>&
        circuit,
    const std::vector<std::size_t>& nodes) -> qc::fp {
  qc::fp max_cost = 0;
  for (const auto node : nodes) {
    if (std::fabs(std::get<U3Gate>(circuit.getNodeValue(node)).angles.theta) >=
        max_cost) {
      max_cost =
          std::fabs(std::get<U3Gate>(circuit.getNodeValue(node)).angles.theta);
    }
  }
  return max_cost;
}
auto NativeGateDecomposer::sift(
    const DirectedGraph<std::variant<U3Gate, std::array<qc::Qubit, 2>>>&
        circuit,
    const std::vector<size_t>& remainingNodes, size_t nQubits)
    -> std::array<std::vector<size_t>, 3> {
  std::vector<size_t> twoQubitGates;
  std::vector<size_t> singleQubitGates;
  std::vector<size_t> remainingGates;

  std::unordered_set vRemaining(remainingNodes.begin(), remainingNodes.end());
  std::unordered_set<size_t> removed;

  // We traverse the graph rather than v_rem to use the graph's topological
  // ordering
  for (size_t node = 0; node < circuit.size(); node++) {
    if (vRemaining.contains(node)) {
      auto op = circuit.getNodeValue(node);
      if (const auto opQubits = std::visit(
              overloads{
                  [](const U3Gate& u3) -> std::unordered_set<std::size_t> {
                    return {u3.qubit};
                  },
                  [](const std::array<qc::Qubit, 2>& cz)
                      -> std::unordered_set<std::size_t> {
                    return {cz[0], cz[1]};
                  }},
              op);
          removed.size() < nQubits && disjunct(removed, opQubits)) {
        std::visit(overloads{[&singleQubitGates, &removed,
                              node](const U3Gate& u3) -> void {
                               singleQubitGates.emplace_back(node);
                               removed.emplace(u3.qubit);
                             },
                             [&twoQubitGates,
                              node](const std::array<qc::Qubit, 2>&) -> void {
                               twoQubitGates.emplace_back(node);
                             }},
                   op);
      } else {
        remainingGates.emplace_back(node);
        std::ranges::copy(opQubits, std::inserter(removed, removed.end()));
      }
    }
  }
  return {{twoQubitGates, singleQubitGates, remainingGates}};
}

auto NativeGateDecomposer::buildSchedule(
    const DirectedGraph<std::variant<U3Gate, std::array<qc::Qubit, 2>>>&
        circuit,
    const DirectedGraph<std::pair<std::vector<std::size_t>,
                                  std::vector<std::size_t>>>& subproblemGraph)
    -> std::pair<std::vector<std::vector<U3Gate>>,
                 std::vector<TwoQubitGateLayer>> {

  const auto& leafNodes = findLeafNodes(subproblemGraph);
  const auto& minimalPath = findCheapestPath(subproblemGraph, leafNodes);
  std::pair<std::vector<std::vector<U3Gate>>, std::vector<TwoQubitGateLayer>>
      schedule;

  std::vector<U3Gate> singleQubitGates;
  std::vector<std::array<qc::Qubit, 2>> twoQubitGates;

  if (!subproblemGraph.getNodeValue(minimalPath[0]).first.empty()) {
    schedule.first.emplace_back();
  }
  std::unordered_set<qc::Qubit> usedQubits;
  for (std::size_t i = 0; i < minimalPath.size(); i++) {
    singleQubitGates.clear();
    twoQubitGates.clear();
    usedQubits.clear();
    for (const auto j : subproblemGraph.getNodeValue(minimalPath[i]).first) {
      if (const auto& op = circuit.getNodeValue(j);
          std::holds_alternative<std::array<qc::Qubit, 2>>(op)) {
        // Check if two-qubit gates can be executed in parallel
        const auto& gate = std::get<std::array<qc::Qubit, 2>>(op);
        if (usedQubits.contains(gate[0]) || usedQubits.contains(gate[1])) {
          schedule.second.emplace_back(twoQubitGates);
          schedule.first.emplace_back();
          twoQubitGates.clear();
          usedQubits.clear();
        }
        usedQubits.insert(gate[0]);
        usedQubits.insert(gate[1]);
        twoQubitGates.emplace_back(gate);
      }
    }

    for (const auto j : subproblemGraph.getNodeValue(minimalPath[i]).second) {
      if (const auto& op = circuit.getNodeValue(j);
          std::holds_alternative<U3Gate>(op)) {
        singleQubitGates.emplace_back(std::get<U3Gate>(op));
      }
    }
    schedule.first.emplace_back(singleQubitGates);
    if (i != 0 || !subproblemGraph.getNodeValue(minimalPath[0]).first.empty()) {
      schedule.second.emplace_back(twoQubitGates);
    }
  }
  return schedule;
}

auto NativeGateDecomposer::addNodeToSubproblemGraph(
    const std::vector<size_t>& twoQubitGates,
    const std::vector<size_t>& singleQubitGates, const qc::fp cost,
    DirectedGraph<std::pair<std::vector<std::size_t>,
                            std::vector<std::size_t>>>& subproblemGraph,
    const std::size_t prevNode) -> size_t {
  const auto newNode =
      subproblemGraph.addNode(std::pair(twoQubitGates, singleQubitGates));
  subproblemGraph.addEdge(prevNode, newNode, cost);
  return newNode;
}

auto NativeGateDecomposer::scheduleRemaining(
    const std::array<std::vector<size_t>, 3>& subproblem,
    const DirectedGraph<std::variant<U3Gate, std::array<qc::Qubit, 2>>>&
        circuit,
    DirectedGraph<std::pair<std::vector<size_t>, std::vector<size_t>>>&
        subproblemGraph,
    const size_t prevNode, const size_t nQubits, const bool checkFinalCond,
    std::unordered_map<std::array<std::vector<size_t>, 3>,
                       std::pair<size_t, std::array<double, 2>>,
                       SubproblemHasher>& memo) -> double {
  double cost;
  // Check if a subproblem has been computed
  if (memo.contains(subproblem)) {
    const auto [to, result] = memo.at(subproblem);
    cost = result[0];
    subproblemGraph.addEdge(prevNode, to, result[1]);
    return cost;
  }
  // Base Case: remaining nodes is empty
  if (subproblem[2].empty()) {
    if (subproblem[1].empty()) {
      cost = 0;
    } else {
      cost =
          std::fabs(std::get<U3Gate>(circuit.getNodeValue(subproblem[1].at(0)))
                        .angles.theta);
    }
    for (const auto i : subproblem[1]) {
      if (std::fabs(std::get<U3Gate>(circuit.getNodeValue(i)).angles.theta) >
          cost) {
        cost =
            std::fabs(std::get<U3Gate>(circuit.getNodeValue(i)).angles.theta);
      }
    }
    const auto endNode = addNodeToSubproblemGraph(
        subproblem[0], subproblem[1], cost, subproblemGraph, prevNode);
    memo[subproblem] =
        std::pair<std::size_t, std::array<double, 2>>(endNode, {cost, cost});
    return cost;
  }
  // Recursive call
  const auto& nextSubproblem = sift(circuit, subproblem[2], nQubits);
  const auto& args =
      getPossibleLayers(circuit, subproblem[1], nextSubproblem, checkFinalCond);
  assert(!args.empty() && "No possible layers found.");
  qc::fp tempCost = 0.0;
  auto minCost = std::numeric_limits<double>::max();
  auto minWeight = std::numeric_limits<double>::max();
  std::size_t minNode;
  for (const auto& [singleQubitGates, nodeCost] : args) {
    const auto newNode =
        addNodeToSubproblemGraph(subproblem[0], singleQubitGates[0], nodeCost,
                                 subproblemGraph, prevNode);
    tempCost =
        scheduleRemaining(
            {singleQubitGates[1], singleQubitGates[2], singleQubitGates[3]},
            circuit, subproblemGraph, newNode, nQubits, checkFinalCond, memo) +
        nodeCost;
    if (tempCost < minCost) {
      minCost = tempCost;
      minNode = newNode;
      minWeight = nodeCost;
    }
  }
  memo[subproblem] = {minNode, {minCost, minWeight}};
  return minCost;
}

auto NativeGateDecomposer::scheduleThetaOpt(
    const std::pair<std::vector<std::vector<U3Gate>>,
                    std::vector<TwoQubitGateLayer>>& schedule,
    const std::size_t nQubits) const
    -> std::pair<std::vector<std::vector<U3Gate>>,
                 std::vector<TwoQubitGateLayer>> {
  // Convert circuit to DAG
  auto circuit = convertCircuitToDAG(schedule, nQubits);
  // Get initial layers
  std::vector<std::size_t> allNodes(circuit.size());
  std::iota(allNodes.begin(), allNodes.end(), 0);
  const auto& subproblem = sift(circuit, allNodes, nQubits);
  // Create subproblem graph
  DirectedGraph<std::pair<std::vector<std::size_t>, std::vector<std::size_t>>>
      subproblemGraph;
  // First call of recursive function to create schedule
  const auto baseNode = subproblemGraph.addNode({});
  std::unordered_map<std::array<std::vector<size_t>, 3>,
                     std::pair<std::size_t, std::array<double, 2>>,
                     SubproblemHasher>
      memo;
  scheduleRemaining(subproblem, circuit, subproblemGraph, baseNode, nQubits,
                    config_.checkFinalCond, memo);
  // Create a schedule from the subproblem graph
  return buildSchedule(circuit, subproblemGraph);
}
} // namespace na::zoned
