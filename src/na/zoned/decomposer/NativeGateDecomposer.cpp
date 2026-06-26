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

#include <variant>
#include <vector>

namespace na::zoned {

NativeGateDecomposer::NativeGateDecomposer(const Architecture&,
                                           const Config& config) {
  config_ = config;
}
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

auto NativeGateDecomposer::calcThetaMax(const std::vector<StructU3>& layers)
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
    -> std::vector<std::vector<StructU3>> {
  std::vector<std::vector<StructU3>> newLayers;
  for (const auto& layer : layers) {
    std::vector<std::vector<std::reference_wrapper<const qc::Operation>>>
        gatesPerQubit(nQubits);
    std::ranges::for_each(layer, [&gatesPerQubit](const auto& gate) -> void {
      assert(gate.get().getNqubits() != 1 &&
             "Gate has to be a single qubit gate.");
      gatesPerQubit[gate.get().getTargets().front()].emplace_back(gate);
    });
    auto& newLayer = newLayers.emplace_back();
    std::ranges::transform(
        std::views::iota(gatesPerQubit.size()) |
            std::views::filter([&gatesPerQubit](const auto i) -> bool {
              return !gatesPerQubit[i].empty();
            }),
        std::back_inserter(newLayer),
        [&gatesPerQubit](const auto i) -> StructU3 {
          const auto& gates = gatesPerQubit[i];
          const auto& quat = std::accumulate(
              gates.begin(), gates.end(), Quaternion{},
              [](Quaternion q, const auto& gate) -> Quaternion {
                return combineQuaternions(std::move(q),
                                          convertGateToQuaternion(gate));
              });
          const auto& angles = getU3AnglesFromQuaternion(quat);
          return StructU3{.angles = angles,
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
    auto schedule =
        scheduleThetaOpt(std::pair(u3Layers, twoQubitGateLayers), nQubits);
    u3Layers = schedule.first;
    newTwoQubitLayers = schedule.second;
  } else {
    newTwoQubitLayers = twoQubitGateLayers;
  }
  std::vector<SingleQubitGateLayer> newSingleQubitLayers;
  for (const auto& layer : u3Layers) {
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
    auto& newLayer = newSingleQubitLayers.emplace_back();
    if (!layer.empty()) {
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
auto disjunct(const std::unordered_set<size_t>& set1,
              const std::unordered_set<size_t>& set2) -> bool {
  return std::ranges::all_of(set1,
                             [&](auto elem) { return !set2.contains(elem); });
}

auto disjunct(const std::unordered_set<qc::Qubit>& set1,
              const std::unordered_set<qc::Qubit>& set2) -> bool {
  return std::ranges::all_of(set1,
                             [&](auto elem) { return !set2.contains(elem); });
}

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
  for (auto [target, cost] : subproblemGraph.getAdjacent(currentNode)) {
    if (leafNodes.contains(target)) {
      possiblePaths.emplace_back(std::vector{target, currentNode}, cost);
    }
  }
  // Recursive case
  if (possiblePaths.empty()) {
    for (auto [target, cost] : subproblemGraph.getAdjacent(currentNode)) {
      auto path = cheapestPathToStart(subproblemGraph, target, leafNodes, memo);
      path.first.emplace_back(currentNode);
      path.second += cost;
      possiblePaths.emplace_back(path);
    }
  }
  // Choose the cheapest path
  assert(!possiblePaths.empty() && "No path found to leaf nodes.");
  const auto& bestPathWithCost = *std::ranges::min_element(
      possiblePaths,
      [](const auto& a, const auto& b) { return a.second < b.second; });
  memo[currentNode] = bestPathWithCost;
  return bestPathWithCost;
}

auto NativeGateDecomposer::findLeafNodes(
    const DirectedGraph<std::pair<std::vector<std::size_t>,
                                  std::vector<std::size_t>>>& subproblemGraph)
    -> std::vector<std::size_t> {
  std::vector<std::size_t> leafNodes;
  std::ranges::copy(std::views::iota(subproblemGraph.size()) |
                        std::views::filter([&subproblemGraph](auto i) -> bool {
                          return subproblemGraph.getAdjacent(i).empty();
                        }),
                    std::back_inserter(leafNodes));
  return leafNodes;
}

auto NativeGateDecomposer::removeElement(const std::vector<std::size_t>& vector,
                                         const std::size_t elem)
    -> std::vector<std::size_t> {
  std::vector<std::size_t> newVector;
  for (auto element : vector) {
    if (element != elem) {
      newVector.emplace_back(element);
    }
  }
  return newVector;
}

auto NativeGateDecomposer::getPossibleLayers(
    DirectedGraph<std::variant<StructU3, std::array<qc::Qubit, 2>>>& circuit,
    const std::vector<size_t>& currentSingleQubitGates,
    const std::array<std::vector<size_t>, 3>& nextSubproblem,
    bool checkFinalCond)
    -> std::vector<std::pair<std::array<std::vector<std::size_t>, 4>, qc::fp>> {

  std::vector<size_t> vP1Star = nextSubproblem[0];
  std::vector<size_t> vP1Square = {};
  std::vector<size_t> vc1Star = nextSubproblem[1];
  std::vector<size_t> vc1Square = {};

  qc::fp vc0Cost = maxTheta(circuit, currentSingleQubitGates);
  qc::fp vc1Cost = maxTheta(circuit, nextSubproblem[1]);
  qc::fp origCombCost = vc0Cost + vc1Cost;
  qc::fp newVc1Cost = std::max(vc0Cost, vc1Cost);

  std::array<std::vector<std::size_t>, 4> vArg = {
      currentSingleQubitGates, nextSubproblem[0], nextSubproblem[1],
      nextSubproblem[2]};
  std::vector<std::pair<std::array<std::vector<std::size_t>, 4>, qc::fp>> args =
      {std::pair(vArg, vc0Cost)};
  // Sort v_0C from highest to lowest theta
  std::vector<std::size_t> vSort(currentSingleQubitGates);
  auto sortByTheta = [&circuit](std::size_t a, std::size_t b) -> bool {
    return std::fabs(std::get<StructU3>(circuit.getNodeValue(a)).angles.theta) >
           std::fabs(std::get<StructU3>(circuit.getNodeValue(b)).angles.theta);
  };
  std::ranges::sort(vSort, sortByTheta);
  // TODO: Check Condition 1
  std::vector<std::pair<std::array<std::vector<std::size_t>, 2>,
                        std::pair<std::set<qc::Qubit>, qc::fp>>>
      potentialArg = {};
  auto prevTheta = std::fabs(
      std::get<StructU3>(circuit.getNodeValue(vSort[0])).angles.theta);
  auto thisTheta = prevTheta;
  std::set<qc::Qubit> mkQubits = {
      std::get<StructU3>(circuit.getNodeValue(vSort[0])).qubit};
  for (auto i = 0; static_cast<size_t>(i) < vSort.size(); i++) {
    thisTheta = std::fabs(
        std::get<StructU3>(circuit.getNodeValue(vSort[static_cast<size_t>(i)]))
            .angles.theta);
    if (thisTheta != prevTheta) {
      std::vector<std::size_t> discarded = {vSort.begin(), vSort.begin() + i};
      std::vector<std::size_t> kept = {vSort.begin() + i, vSort.end()};
      potentialArg.emplace_back(
          std::pair<std::array<std::vector<std::size_t>, 2>,
                    std::pair<std::set<qc::Qubit>, qc::fp>>(
              {kept, discarded},
              std::pair<std::set<qc::Qubit>, qc::fp>(mkQubits, thisTheta)));
      prevTheta = thisTheta;
      mkQubits.clear();
    }
    mkQubits.insert(
        std::get<StructU3>(circuit.getNodeValue(vSort[static_cast<size_t>(i)]))
            .qubit);
  }
  std::vector<std::size_t> emplaceBackNodes = {};
  std::set<qc::Qubit> pSquareQubits = {};

  for (auto pot : potentialArg) {
    // TODO: Check Condition 2
    for (auto node : vP1Star) {
      std::set<qc::Qubit> qubits = {
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
    //  TODO: Check Condition 3
    std::set<qc::Qubit> pushQubits = pot.second.first;
    pushQubits.merge(pSquareQubits);
    emplaceBackNodes.clear();

    for (auto node : vc1Star) {
      std::set qubits = {std::get<StructU3>(circuit.getNodeValue(node)).qubit};
      if (!disjunct(qubits, pushQubits)) {
        emplaceBackNodes.emplace_back(node);
      }
    }
    for (auto node : emplaceBackNodes) {
      vc1Star = removeElement(vc1Star, node);
      vc1Square.emplace_back(node);
    }

    if (vc1Star.empty()) {
      break;
    }
    // TODO Check Condition 4
    if (!checkFinalCond || pot.second.second + newVc1Cost < origCombCost) {
      vArg = {pot.first[0], vP1Star, pot.first[1], nextSubproblem[2]};
      for (auto node : vc1Star) {
        vArg[2].emplace_back(node);
      }
      for (auto node : vc1Square) {
        vArg[3].emplace_back(node);
      }
      for (auto node : vP1Square) {
        vArg[3].emplace_back(node);
      }
      args.emplace_back(vArg, pot.second.second);
    }
  }
  return args;
}

auto NativeGateDecomposer::convertCircuitToDAG(
    const std::pair<std::vector<std::vector<StructU3>>,
                    std::vector<TwoQubitGateLayer>>& schedule,
    std::size_t nQubits)
    -> DirectedGraph<std::variant<StructU3, std::array<qc::Qubit, 2>>> {
  // std::variant<StructU3, std::array<qc::Qubit, 2>> instead of Unique_pointer
  // For Readout:
  DirectedGraph<std::variant<StructU3, std::array<qc::Qubit, 2>>> graph =
      DirectedGraph<std::variant<StructU3, std::array<qc::Qubit, 2>>>();
  std::vector<std::vector<size_t>> qubitPaths(nQubits);
  // TODO:assert that One more sql exists than mql ??
  for (size_t i = 0; i < schedule.second.size(); ++i) {
    for (const auto& s : schedule.first.at(i)) {
      size_t node = graph.add_Node(s);
      qubitPaths.at(s.qubit).emplace_back(node);
    }

    for (const auto& gate : schedule.second.at(i)) {
      size_t node = graph.add_Node(gate);
      qubitPaths.at(gate[0]).emplace_back(node);
      qubitPaths.at(gate[1]).emplace_back(node);
    }
  }
  SPDLOG_DEBUG("Added Nodes");
  for (const auto& s : schedule.first.back()) {
    size_t node = graph.add_Node(s);
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
    DirectedGraph<std::variant<StructU3, std::array<qc::Qubit, 2>>>& circuit,
    const std::vector<std::size_t>& nodes) -> qc::fp {
  qc::fp max_cost = 0;
  for (const auto node : nodes) {
    if (std::fabs(
            std::get<StructU3>(circuit.getNodeValue(node)).angles.theta) >=
        max_cost) {
      max_cost = std::fabs(
          std::get<StructU3>(circuit.getNodeValue(node)).angles.theta);
    }
  }
  return max_cost;
}
auto NativeGateDecomposer::sift(
    DirectedGraph<std::variant<StructU3, std::array<qc::Qubit, 2>>>& circuit,
    std::vector<std::size_t> remainingNodes, size_t nQubits)
    -> std::array<std::vector<size_t>, 3> {
  std::vector<size_t> vp = std::vector<size_t>();
  std::vector<size_t> v_c = std::vector<size_t>();
  std::vector<size_t> vr = std::vector<size_t>();

  std::set<std::size_t> vRemaining =
      std::set(remainingNodes.begin(), remainingNodes.end());
  std::set<size_t> removed = std::set<size_t>();

  // We traverse the graph rather than v_rem to use the graph's topological
  // ordering
  for (size_t node = 0; node < circuit.size(); node++) {
    if (vRemaining.contains(node)) {
      auto op = circuit.getNodeValue(node);
      std::set<size_t> opQubits = std::set<size_t>();

      if (std::holds_alternative<StructU3>(op)) {
        opQubits = {std::get<StructU3>(op).qubit};
      } else {
        opQubits = {std::get<std::array<qc::Qubit, 2>>(op)[0],
                    std::get<std::array<qc::Qubit, 2>>(op)[1]};
      }
      if (removed.size() < nQubits && disjunct(removed, opQubits)) {
        if (std::holds_alternative<StructU3>(op)) {
          v_c.emplace_back(node);
          removed.insert(std::get<StructU3>(op).qubit);
        } else {
          vp.emplace_back(node);
        }
      } else {
        vr.emplace_back(node);
        for (auto qubit : opQubits) {
          removed.insert(qubit);
        }
      }
    }
  }
  return std::array<std::vector<size_t>, 3>{{vp, v_c, vr}};
}

auto NativeGateDecomposer::buildSchedule(
    DirectedGraph<std::variant<StructU3, std::array<qc::Qubit, 2>>>& circuit,
    DirectedGraph<std::pair<std::vector<std::size_t>,
                            std::vector<std::size_t>>>& subproblemGraph)
    -> std::pair<std::vector<std::vector<StructU3>>,
                 std::vector<TwoQubitGateLayer>> {

  std::vector<std::size_t> leafNodes = findLeafNodes(subproblemGraph);
  std::vector<std::size_t> minimalPath =
      findCheapestPath(subproblemGraph, leafNodes);
  std::pair<std::vector<std::vector<StructU3>>, std::vector<TwoQubitGateLayer>>
      schedule = std::pair<std::vector<std::vector<StructU3>>,
                           std::vector<TwoQubitGateLayer>>{};

  std::vector<StructU3> singleQubitGates;
  std::vector<std::array<qc::Qubit, 2>> twoQubitGates;

  if (!subproblemGraph.getNodeValue(minimalPath[0]).first.empty()) {
    schedule.first.emplace_back();
  }
  std::set<qc::Qubit> usedQubits{};

  for (std::size_t i = 0; i < minimalPath.size(); i++) {
    singleQubitGates.clear();
    twoQubitGates.clear();
    usedQubits.clear();

    for (auto j : subproblemGraph.getNodeValue(minimalPath[i]).first) {
      auto op = circuit.getNodeValue(j);
      if (std::holds_alternative<std::array<qc::Qubit, 2>>(op)) {
        // TODO: Check if TWOQUBIT GATES Can be executed in parallel!!
        auto gate = std::get<std::array<qc::Qubit, 2>>(op);
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

    for (auto j : subproblemGraph.getNodeValue(minimalPath[i]).second) {
      auto op = circuit.getNodeValue(j);
      if (std::holds_alternative<StructU3>(op)) {
        singleQubitGates.emplace_back(std::get<StructU3>(op));
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
    const std::vector<size_t>& singleQubitGates, qc::fp cost,
    DirectedGraph<std::pair<std::vector<std::size_t>,
                            std::vector<std::size_t>>>& subproblemGraph,
    std::size_t prevNode) -> size_t {
  std::size_t newNode =
      subproblemGraph.add_Node(std::pair(twoQubitGates, singleQubitGates));
  subproblemGraph.addEdge(prevNode, newNode, cost);
  return newNode;
}

auto NativeGateDecomposer::scheduleRemaining(
    const std::array<std::vector<size_t>, 3>& v,
    DirectedGraph<std::variant<StructU3, std::array<qc::Qubit, 2>>>& circuit,
    DirectedGraph<std::pair<std::vector<size_t>, std::vector<size_t>>>&
        subproblemGraph,
    size_t prevNode, size_t nQubits, bool checkFinalCond,
    std::unordered_map<size_t, std::pair<size_t, std::array<double, 2>>>& memo)
    -> double {
  double cost;
  // TODO: Check if subproblem has been computed
  std::size_t id = std::hash<std::array<std::vector<size_t>, 3>>{}(v);
  if (memo.contains(id)) {
    std::size_t subNode = memo.at(id).first;
    double edgeWeight = memo.at(id).second[1];
    cost = memo.at(id).second[0];
    subproblemGraph.addEdge(prevNode, subNode, edgeWeight);
    return cost;
  }
  // TODO: Base Case-> V_rem is empty
  if (v[2].empty()) {
    // TODO:Decide if I need if to check for TWO QUBIT
    if (v[1].empty()) {
      cost = 0;
    } else {
      cost = std::fabs(
          std::get<StructU3>(circuit.getNodeValue(v[1].at(0))).angles.theta);
    }
    for (std::size_t i : v[1]) {
      if (std::get<StructU3>(circuit.getNodeValue(i)).angles.theta > cost) {
        cost =
            std::fabs(std::get<StructU3>(circuit.getNodeValue(i)).angles.theta);
      }
    }
    auto end_node =
        addNodeToSubproblemGraph(v[0], v[1], cost, subproblemGraph, prevNode);
    memo[id] = std::pair<std::size_t, std::array<double, 2>>(
        end_node, {cost, cost}); // TODO: Correct to put cost for both???
    return cost;
  }
  // TODO: Recursive Call: Only
  auto vNew = sift(circuit, v[2], nQubits);
  auto args = getPossibleLayers(circuit, v[1], vNew, checkFinalCond);
  qc::fp tempCost = 0;
  double minCost = std::numeric_limits<double>::max();
  double minWeight = std::numeric_limits<double>::max();
  std::size_t minNode;
  for (const auto& val : args) {
    auto newNode = addNodeToSubproblemGraph(v[0], val.first[0], val.second,
                                            subproblemGraph, prevNode);
    tempCost = scheduleRemaining({val.first[1], val.first[2], val.first[3]},
                                 circuit, subproblemGraph, newNode, nQubits,
                                 checkFinalCond, memo) +
               val.second;
    if (tempCost < minCost) {
      minCost = tempCost;
      minNode = newNode;
      minWeight = val.second;
    }
  }
  memo[id] = std::pair<std::size_t, std::array<double, 2>>(
      minNode, {minCost, minWeight});
  return minCost;
}

auto NativeGateDecomposer::scheduleThetaOpt(
    const std::pair<std::vector<std::vector<StructU3>>,
                    std::vector<TwoQubitGateLayer>>& schedule,
    std::size_t nQubits) const -> std::pair<std::vector<std::vector<StructU3>>,
                                            std::vector<TwoQubitGateLayer>> {

  // TODO: Convert Circuit to DAG: How to handle the unique Pointer situation???
  DirectedGraph<std::variant<StructU3, std::array<qc::Qubit, 2>>> circuit =
      convertCircuitToDAG(schedule, nQubits);
  // TODO: Get initial Moments( Not does MQB THEN SQB!! SOl to get SQB MQB??)
  std::vector<std::size_t> v_start{};
  v_start.reserve(circuit.size());
  for (size_t i = 0; i < circuit.size(); ++i) {
    v_start.emplace_back(i);
  }
  // v=(v_p,v_c,v_r)
  std::array<std::vector<size_t>, 3> v = sift(circuit, v_start, nQubits);
  // TODO: Create Subproblem Graph
  DirectedGraph<std::pair<std::vector<std::size_t>, std::vector<std::size_t>>>
      subproblemGraph = DirectedGraph<
          std::pair<std::vector<std::size_t>, std::vector<std::size_t>>>();
  // TODO: First Call of Recursive Function to create Schedule
  auto baseNode = subproblemGraph.add_Node(
      std::pair<std::vector<std::size_t>, std::vector<std::size_t>>({}, {}));
  std::map<std::size_t, std::pair<std::size_t, std::array<double, 2>>> memo =
      {};
  auto cost = scheduleRemaining(v, circuit, subproblemGraph, baseNode, nQubits,
                                config_.checkFinalCond, memo);
  // TODO: Create Schedule from Subproblem Graph
  std::pair<std::vector<std::vector<StructU3>>, std::vector<TwoQubitGateLayer>>
      finalCircuit = buildSchedule(circuit, subproblemGraph);
  return finalCircuit;
}
} // namespace na::zoned
