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

#include <variant>
#include <vector>

namespace na::zoned {

auto NativeGateDecomposer::convertGateToQuaternion(
    std::reference_wrapper<const qc::Operation> op) -> Quaternion {
  assert(op.get().getNqubits() == 1);
  Quaternion quat{};
  if (op.get().getType() == qc::RZ || op.get().getType() == qc::P) {
    quat = {cos(op.get().getParameter().front() / 2), 0, 0,
            sin(op.get().getParameter().front() / 2)};
  } else if (op.get().getType() == qc::Z) {
    quat = {0, 0, 0, 1};
  } else if (op.get().getType() == qc::S) {
    quat = {cos(qc::PI_4), 0, 0, sin(qc::PI_4)};
  } else if (op.get().getType() == qc::Sdg) {
    quat = {cos(-qc::PI_4), 0, 0, sin(-qc::PI_4)};
  } else if (op.get().getType() == qc::T) {
    quat = {cos(qc::PI_4 / 2), 0, 0, sin(qc::PI_4 / 2)};
  } else if (op.get().getType() == qc::Tdg) {
    quat = {cos(-qc::PI_4 / 2), 0, 0, sin(-qc::PI_4 / 2)};
  } else if (op.get().getType() == qc::U) {
    quat = combineQuaternions(
        combineQuaternions({cos(op.get().getParameter().at(1) / 2), 0, 0,
                            sin(op.get().getParameter().at(1) / 2)},
                           {cos(op.get().getParameter().front() / 2), 0,
                            sin(op.get().getParameter().front() / 2), 0}),
        {cos(op.get().getParameter().at(2) / 2), 0, 0,
         sin(op.get().getParameter().at(2) / 2)});
  } else if (op.get().getType() == qc::U2) {
    quat = combineQuaternions(
        combineQuaternions({cos(op.get().getParameter().front() / 2), 0, 0,
                            sin(op.get().getParameter().front() / 2)},
                           {cos(qc::PI_4), 0, sin(qc::PI_4), 0}),
        {cos(op.get().getParameter().at(1) / 2), 0, 0,
         sin(op.get().getParameter().at(1) / 2)});
  } else if (op.get().getType() == qc::RX) {
    quat = {cos(op.get().getParameter().front() / 2),
            sin(op.get().getParameter().front() / 2), 0, 0};
  } else if (op.get().getType() == qc::RY) {
    quat = {cos(op.get().getParameter().front() / 2), 0,
            sin(op.get().getParameter().front() / 2), 0};
  } else if (op.get().getType() == qc::H) {
    quat = combineQuaternions(
        combineQuaternions({1, 0, 0, 0}, {cos(qc::PI_4), 0, sin(qc::PI_4), 0}),
        {cos(qc::PI_2), 0, 0, sin(qc::PI_2)});
  } else if (op.get().getType() == qc::X) {
    quat = {0, 1, 0, 0};
  } else if (op.get().getType() == qc::Y) {
    quat = {0, 0, 1, 0};
  } else if (op.get().getType() == qc::Vdg) {
    quat = combineQuaternions(
        combineQuaternions({cos(qc::PI_4), 0, 0, sin(qc::PI_4)},
                           {cos(-qc::PI_4), 0, sin(-qc::PI_4), 0}),
        {cos(-qc::PI_4), 0, 0, sin(-qc::PI_4)});
  } else if (op.get().getType() == qc::SX) {
    quat = combineQuaternions(
        combineQuaternions({cos(-qc::PI_4), 0, 0, sin(-qc::PI_4)},
                           {cos(qc::PI_4), 0, sin(qc::PI_4), 0}),
        {cos(qc::PI_4), 0, 0, sin(qc::PI_4)});
  } else if (op.get().getType() == qc::SXdg || op.get().getType() == qc::V) {
    quat = combineQuaternions(
        combineQuaternions({cos(-qc::PI_4), 0, 0, sin(-qc::PI_4)},
                           {cos(-qc::PI_4), 0, sin(-qc::PI_4), 0}),
        {cos(qc::PI_4), 0, 0, sin(qc::PI_4)});
  } else {
    // if the gate type is not recognized, an error is printed and the
    // gate is not included in the output.
    std::ostringstream oss;
    oss << "ERROR: Unsupported single-qubit gate: " << op.get().getType()
        << "\n";
    throw std::invalid_argument(oss.str());
  }
  return quat;
}

auto NativeGateDecomposer::combineQuaternions(const Quaternion& q1,
                                              const Quaternion& q2)
    -> Quaternion {
  Quaternion new_quat{};
  new_quat[0] = q1[0] * q2[0] - q1[1] * q2[1] - q1[2] * q2[2] - q1[3] * q2[3];
  new_quat[1] = q1[0] * q2[1] + q1[1] * q2[0] + q1[2] * q2[3] - q1[3] * q2[2];
  new_quat[2] = q1[0] * q2[2] - q1[1] * q2[3] + q1[2] * q2[0] + q1[3] * q2[1];
  new_quat[3] = q1[0] * q2[3] + q1[1] * q2[2] - q1[2] * q2[1] + q1[3] * q2[0];
  return new_quat;
}

auto NativeGateDecomposer::getU3AnglesFromQuaternion(const Quaternion& quat)
    -> std::array<qc::fp, 3> {
  qc::fp theta;
  qc::fp phi;
  qc::fp lambda;
  if (std::fabs(quat[0]) > epsilon || std::fabs(quat[3]) > epsilon) {
    theta = 2. * std::atan2(std::sqrt(quat[2] * quat[2] + quat[1] * quat[1]),
                            std::sqrt(quat[0] * quat[0] + quat[3] * quat[3]));
    qc::fp alpha_1 = std::atan2(quat[3], quat[0]); // (phi+ lambda) /2
    if (std::fabs(quat[1]) > epsilon || std::fabs(quat[2]) > epsilon) {
      qc::fp alpha_2 = -1 * std::atan2(quat[1], quat[2]); //(phi-lambda)/2
      phi = alpha_1 + alpha_2;                            // phi
      lambda = alpha_1 - alpha_2;
    } else {
      phi = 0;
      lambda = 2 * alpha_1;
    }
  } else {
    theta = qc::PI;
    if (std::fabs(quat[1]) > epsilon || std::fabs(quat[2]) > epsilon) {
      phi = 0;
      lambda = 2 * std::atan2(quat[1], quat[2]);
    } else {
      // This should never happen! Exception??
      phi = 0.;
      lambda = 0.;
    }
  }
  return {theta, phi, lambda};
}

auto NativeGateDecomposer::calcThetaMax(const std::vector<StructU3>& layer)
    -> qc::fp {
  qc::fp theta_max = 0;
  for (auto gate : layer) {
    if (std::fabs(gate.angles[0]) > theta_max) {
      theta_max = std::fabs(gate.angles[0]);
    }
  }
  return theta_max;
}
auto NativeGateDecomposer::transformToU3(
    const std::vector<SingleQubitGateRefLayer>& layers, size_t n_qubits)
    -> std::vector<std::vector<StructU3>> {
  std::vector<std::vector<StructU3>> new_layers;
  for (const auto& layer : layers) {
    std::vector<std::vector<std::reference_wrapper<const qc::Operation>>> gates(
        n_qubits);
    std::vector<StructU3> new_layer;
    for (auto gate : layer) {
      // WHat are operations with empty targets doing??
      if (!gate.get().getTargets().empty()) {
        gates[gate.get().getTargets().front()].push_back(gate);
      }
    }

    for (auto qubit_gates : gates) {
      if (!qubit_gates.empty()) {
        std::array<qc::fp, 4> quat = convertGateToQuaternion(qubit_gates[0]);
        for (size_t i = 1; i < qubit_gates.size(); i++) {
          quat =
              combineQuaternions(quat, convertGateToQuaternion(qubit_gates[i]));
        }
        std::array<qc::fp, 3> angles = getU3AnglesFromQuaternion(quat);
        new_layer.emplace_back(
            StructU3{angles, qubit_gates[0].get().getTargets().front()});
      }
    }
    new_layers.push_back(new_layer);
  }
  return new_layers;
}
auto NativeGateDecomposer::getDecompositionAngles(
    const std::array<qc::fp, 3>& angles, qc::fp theta_max)
    -> std::array<qc::fp, 3> {
  qc::fp alpha, chi;
  // U3(theta,phi_min(phi),phi_plus(lambda)->Rz(gamma_minus)GR(theta_max/2,
  // PI_2)Rz(chi)GR(-theta_max/2,PI_2)RZ(gamma_plus)
  qc::fp sin_sq_diff = (sin(theta_max / 2) * sin(theta_max / 2) -
                        (sin(angles[0] / 2) * sin(angles[0] / 2)));
  if (std::fabs(sin_sq_diff) < epsilon) {
    chi = qc::PI;
    if (std::fabs(cos(theta_max / 2)) < epsilon) { // Periodicity covered?
      alpha = 0;
    } else {
      alpha = qc::PI_2;
    }
  } else {
    qc::fp kappa =
        std::sqrt((sin(angles[0] / 2) * sin(angles[0] / 2)) / sin_sq_diff);
    alpha = atan(cos(theta_max / 2) * kappa);
    chi = fmod(2 * atan(kappa), qc::TAU);
  }
  qc::fp beta = angles[0] < 0 ? -1 * qc::PI_2 : qc::PI_2;
  qc::fp gamma_plus = fmod(angles[2] - (alpha + beta), qc::TAU);
  qc::fp gamma_minus = fmod(angles[1] - (alpha - beta), qc::TAU);

  return {chi, gamma_minus, gamma_plus};
}

auto NativeGateDecomposer::decompose(
    size_t nQubits, const std::pair<std::vector<SingleQubitGateRefLayer>,
                                    std::vector<TwoQubitGateLayer>>& schedule)
    -> std::pair<std::vector<SingleQubitGateLayer>,
                 std::vector<TwoQubitGateLayer>> {
  nQubits_ = nQubits;

  std::vector<std::vector<StructU3>> U3Layers =
      transformToU3(schedule.first, nQubits);
  std::vector<TwoQubitGateLayer> NewTwoQubitGateLayers = schedule.second;
  if (config_.theta_opt_schedule) {
    auto thetaOptSchedule =
        schedule_theta_opt(std::pair(U3Layers, NewTwoQubitGateLayers), nQubits);
    U3Layers = thetaOptSchedule.first;
    NewTwoQubitGateLayers = thetaOptSchedule.second;
  }
  std::vector<SingleQubitGateLayer> NewSingleQubitLayers =
      std::vector<SingleQubitGateLayer>{};

  for (const auto& layer : U3Layers) {
    qc::fp theta_max = calcThetaMax(layer);
    SingleQubitGateLayer FrontLayer;
    SingleQubitGateLayer MidLayer;
    SingleQubitGateLayer BackLayer;
    SingleQubitGateLayer NewLayer = {};

    for (auto gate : layer) {
      std::array<qc::fp, 3> decomp_angles =
          getDecompositionAngles(gate.angles, theta_max);

      // GR(theta_max/2, PI_2)==Global Y due to PI_2
      FrontLayer.emplace_back(std::make_unique<const qc::StandardOperation>(
          qc::StandardOperation(gate.qubit, qc::RZ, {decomp_angles[1]})));

      MidLayer.emplace_back(std::make_unique<const qc::StandardOperation>(
          qc::StandardOperation(gate.qubit, qc::RZ, {decomp_angles[0]})));

      BackLayer.emplace_back(std::make_unique<const qc::StandardOperation>(
          qc::StandardOperation(gate.qubit, qc::RZ, {decomp_angles[2]})));
    } // gate::layer
    if (!layer.empty()) {
      std::vector<std::unique_ptr<qc::Operation>> GR_plus;
      std::vector<std::unique_ptr<qc::Operation>> GR_minus;

      for (size_t i = 0; i < this->nQubits_; ++i) {
        GR_plus.emplace_back(std::make_unique<qc::StandardOperation>(
            i, qc::RY, std::initializer_list<qc::fp>{theta_max / 2}));
        GR_minus.emplace_back(std::make_unique<qc::StandardOperation>(
            i, qc::RY, std::initializer_list<qc::fp>{-1 * theta_max / 2}));
      }

      for (auto&& gate : FrontLayer) {
        NewLayer.push_back(std::move(gate));
      }

      NewLayer.emplace_back(std::make_unique<const qc::CompoundOperation>(
          qc::CompoundOperation(std::move(GR_plus), true)));

      for (auto&& gate : MidLayer) {
        NewLayer.push_back(std::move(gate));
      }
      NewLayer.emplace_back(std::make_unique<const qc::CompoundOperation>(
          qc::CompoundOperation(std::move(GR_minus), true)));

      for (auto&& gate : BackLayer) {
        NewLayer.push_back(std::move(gate));
      }
    }
    NewSingleQubitLayers.push_back(std::move(NewLayer));
  } // layer::SingleQubitLayers
  return {std::move(NewSingleQubitLayers), NewTwoQubitGateLayers};
}

auto NativeGateDecomposer::find_shortest_path(
    const DiGraph<std::pair<std::vector<std::size_t>,
                            std::vector<std::size_t>>>& subproblem_graph,
    const std::vector<std::size_t>& leaf_nodes) -> std::vector<size_t> {
  std::set<std::size_t> leaves =
      std::set<std::size_t>(leaf_nodes.begin(), leaf_nodes.end());
  std::pair<std::vector<std::size_t>, double> leaf_path =
      shortest_path_to_start(subproblem_graph, 0, leaves);
  std::ranges::reverse(leaf_path.first);

  return {leaf_path.first.begin() + 1, leaf_path.first.end()};
}
auto disjunct(const std::set<size_t>& set1, const std::set<size_t>& set2)
    -> bool {
  for (auto elem : set1) {
    if (set2.contains(elem)) {
      return false;
    }
  }
  return true;
}

auto disjunct(const std::set<qc::Qubit>& set1, const std::set<qc::Qubit>& set2)
    -> bool {
  for (auto elem : set1) {
    if (set2.contains(elem)) {
      return false;
    }
  }
  return true;
}

auto NativeGateDecomposer::shortest_path_to_start(
    const DiGraph<std::pair<std::vector<std::size_t>,
                            std::vector<std::size_t>>>& subproblem_graph,
    std::size_t current_node, const std::set<std::size_t>& leaf_nodes)
    -> std::pair<std::vector<std::size_t>, double> {
  std::vector<std::pair<std::vector<std::size_t>, double>> possible_paths = {};
  // Check if leaf nodes are reached

  // TODO: Check if Path cost takes into account edge weight from current node
  // to next node!!!
  for (auto edge : subproblem_graph.get_adjacent(current_node)) {
    if (leaf_nodes.contains(edge.first)) {
      possible_paths.push_back({std::pair<std::vector<std::size_t>, double>(
          {edge.first, current_node}, edge.second)});
    }
  }
  // Recursive Case
  if (possible_paths.empty()) {
    for (auto edge : subproblem_graph.get_adjacent(current_node)) {
      auto path =
          shortest_path_to_start(subproblem_graph, edge.first, leaf_nodes);
      path.first.push_back(current_node);
      path.second += edge.second;
      possible_paths.push_back(path);
    }
  }
  // Base Case:
  // Choose shortest Paths
  auto min_length = possible_paths.at(0).first.size();
  std::vector<std::pair<std::vector<std::size_t>, double>> shortest_paths = {};
  for (const auto& key : possible_paths | std::views::keys) {
    if (key.size() < min_length) {
      min_length = key.size();
    }
  }
  for (const auto& path : possible_paths) {
    if (path.first.size() == min_length) {
      shortest_paths.push_back(path);
    }
  }
  if (shortest_paths.size() == 1) {
    return shortest_paths.at(0);
  }
  // Find shortest path with minimal cost
  auto min_cost = shortest_paths.at(0).second;
  auto best_path = shortest_paths.at(0);
  for (const auto& path : shortest_paths) {
    if (path.second < min_cost) {
      min_cost = path.second;
      best_path = path;
    }
  }
  return best_path;
}

auto NativeGateDecomposer::find_leaf_nodes(
    const DiGraph<std::pair<std::vector<std::size_t>,
                            std::vector<std::size_t>>>& subproblem_graph)
    -> std::vector<std::size_t> {
  std::vector<std::size_t> end_nodes = std::vector<std::size_t>{};
  for (size_t i = 0; i < subproblem_graph.size(); i++) {
    if (subproblem_graph.get_adjacent(i).empty()) {
      end_nodes.push_back(i);
    }
  }
  return end_nodes;
}

auto NativeGateDecomposer::remove_element(
    const std::vector<std::size_t>& vector, std::size_t elem)
    -> std::vector<std::size_t> {
  std::vector<std::size_t> new_vector = {};
  for (auto element : vector) {
    if (element != elem) {
      new_vector.push_back(element);
    }
  }
  return new_vector;
}

auto NativeGateDecomposer::get_possible_moments(
    DiGraph<std::variant<StructU3, std::array<qc::Qubit, 2>>>& circuit,
    const std::vector<size_t>& v0_c,
    const std::array<std::vector<size_t>, 3>& v_new, bool check_final_cond)
    -> std::vector<std::pair<std::array<std::vector<std::size_t>, 4>, qc::fp>> {

  std::vector<size_t> v_p1_star = v_new[0];
  std::vector<size_t> v_p1_square = {};
  std::vector<size_t> v_c1_star = v_new[1];
  std::vector<size_t> v_c1_square = {};

  qc::fp v_c0_cost = max_theta(circuit, v0_c);
  qc::fp v_c1_cost = max_theta(circuit, v_new[1]);
  qc::fp orig_comb_cost = v_c0_cost + v_c1_cost;
  qc::fp new_v_c1_cost = std::max(v_c0_cost, v_c1_cost);

  std::array<std::vector<std::size_t>, 4> v_arg = {v0_c, v_new[0], v_new[1],
                                                   v_new[2]};
  std::vector<std::pair<std::array<std::vector<std::size_t>, 4>, qc::fp>> args =
      {std::pair(v_arg, v_c0_cost)};
  // Sort v_0C from highest to lowest theta
  std::vector<std::size_t> v_sort(v0_c);
  auto sort_by_theta = [&circuit](std::size_t a, std::size_t b) -> bool {
    // TODO: IF CHECK???
    return std::get<StructU3>(circuit.get_Node_Value(a)).angles[0] >
           std::get<StructU3>(circuit.get_Node_Value(b)).angles[0];
  };
  std::ranges::sort(v_sort, sort_by_theta);
  // TODO: Check Condition 1
  std::vector<std::pair<std::array<std::vector<std::size_t>, 2>,
                        std::pair<std::set<qc::Qubit>, qc::fp>>>
      potential_arg = {};
  auto prev_theta =
      std::get<StructU3>(circuit.get_Node_Value(v_sort[0])).angles[0];
  auto this_theta = prev_theta;
  std::set<qc::Qubit> mk_qubits = {
      std::get<StructU3>(circuit.get_Node_Value(v_sort[0])).qubit};
  for (auto i = 0; static_cast<size_t>(i) < v_sort.size(); i++) {
    this_theta = std::get<StructU3>(
                     circuit.get_Node_Value(v_sort[static_cast<size_t>(i)]))
                     .angles[0];
    if (this_theta != prev_theta) {
      std::vector<std::size_t> discarded = {v_sort.begin(), v_sort.begin() + i};
      std::vector<std::size_t> kept = {v_sort.begin() + i, v_sort.end()};
      potential_arg.push_back(std::pair<std::array<std::vector<std::size_t>, 2>,
                                        std::pair<std::set<qc::Qubit>, qc::fp>>(
          {kept, discarded},
          std::pair<std::set<qc::Qubit>, qc::fp>(mk_qubits, this_theta)));
      prev_theta = this_theta;
      mk_qubits.clear();
    }
    mk_qubits.insert(std::get<StructU3>(
                         circuit.get_Node_Value(v_sort[static_cast<size_t>(i)]))
                         .qubit);
  }
  std::vector<std::size_t> push_back_nodes = {};
  std::set<qc::Qubit> p_square_qubits = {};

  for (auto pot : potential_arg) {
    // TODO: Check Condition 2
    for (auto node : v_p1_star) {
      std::set<qc::Qubit> qubits = {
          std::get<std::array<qc::Qubit, 2>>(circuit.get_Node_Value(node))[0],
          std::get<std::array<qc::Qubit, 2>>(circuit.get_Node_Value(node))[1]};
      if (!disjunct(qubits, pot.second.first)) {
        push_back_nodes.emplace_back(node);
        p_square_qubits.merge(qubits);
      }
    }
    for (auto node : push_back_nodes) {
      v_p1_star = remove_element(v_p1_star, node);
      v_p1_square.emplace_back(node);
    }

    if (v_p1_star.empty()) {
      break;
    }
    //  TODO: Check Condition 3
    std::set<qc::Qubit> push_qubits = pot.second.first;
    push_qubits.merge(p_square_qubits);
    push_back_nodes.clear();

    for (auto node : v_c1_star) {
      std::set<qc::Qubit> qubits = {
          std::get<StructU3>(circuit.get_Node_Value(node)).qubit};
      if (!disjunct(qubits, push_qubits)) {
        push_back_nodes.emplace_back(node);
      }
    }
    for (auto node : push_back_nodes) {
      v_c1_star = remove_element(v_c1_star, node);
      v_c1_square.emplace_back(node);
    }

    if (v_c1_star.empty()) {
      break;
    }
    // TODO Check Condition 4
    if (!check_final_cond ||
        pot.second.second + new_v_c1_cost < orig_comb_cost) {
      v_arg = {pot.first[0], v_p1_star, pot.first[1], v_new[2]};
      for (auto node : v_c1_star) {
        v_arg[2].push_back(node);
      }
      for (auto node : v_c1_square) {
        v_arg[3].push_back(node);
      }
      for (auto node : v_p1_square) {
        v_arg[3].push_back(node);
      }
      args.emplace_back(v_arg, pot.second.second);
    }
  }
  return args;
}

auto NativeGateDecomposer::convert_circ_to_dag(
    const std::pair<std::vector<std::vector<StructU3>>,
                    std::vector<TwoQubitGateLayer>>& schedule,
    std::size_t nQubits)
    -> DiGraph<std::variant<StructU3, std::array<qc::Qubit, 2>>> {
  // std::variant<StructU3, std::array<qc::Qubit, 2>> instead of Unique_pointer
  // For Readout:
  DiGraph<std::variant<StructU3, std::array<qc::Qubit, 2>>> graph =
      DiGraph<std::variant<StructU3, std::array<qc::Qubit, 2>>>();
  std::vector<std::vector<size_t>> qubit_paths(nQubits);
  // TODO:assert that One more sql exists than mql ??
  for (size_t i = 0; i < schedule.second.size(); ++i) {
    for (const auto& s : schedule.first.at(i)) {
      size_t node = graph.add_Node(s);
      qubit_paths.at(s.qubit).push_back(node);
    }

    for (const auto& gate : schedule.second.at(i)) {
      size_t node = graph.add_Node(gate);
      qubit_paths.at(gate[0]).push_back(node);
      qubit_paths.at(gate[1]).push_back(node);
    }
  }
  for (const auto& s : schedule.first.back()) {
    size_t node = graph.add_Node(s);
    qubit_paths.at(s.qubit).push_back(node);
  }

  for (std::size_t i = 0; i < qubit_paths.size(); ++i) {
    for (std::size_t op = 0; op < qubit_paths.at(i).size() - 1; ++op) {
      graph.add_Edge(qubit_paths.at(i).at(op), qubit_paths.at(i).at(op + 1));
    }
  }
  return graph;
}

auto NativeGateDecomposer::max_theta(
    DiGraph<std::variant<StructU3, std::array<qc::Qubit, 2>>>& circuit,
    const std::vector<std::size_t>& nodes) -> qc::fp {
  qc::fp max_cost = 0;
  for (const auto node : nodes) {
    if (std::get<StructU3>(circuit.get_Node_Value(node)).angles[0] >=
        max_cost) {
      max_cost = std::get<StructU3>(circuit.get_Node_Value(node)).angles[0];
    }
  }
  return max_cost;
}
auto NativeGateDecomposer::sift(
    DiGraph<std::variant<StructU3, std::array<qc::Qubit, 2>>>& circuit,
    std::vector<std::size_t> v, size_t nQubits)
    -> std::array<std::vector<size_t>, 3> {
  std::vector<size_t> v_p = std::vector<size_t>();
  std::vector<size_t> v_c = std::vector<size_t>();
  std::vector<size_t> v_r = std::vector<size_t>();

  std::set<std::size_t> v_rem = std::set(v.begin(), v.end());
  std::set<size_t> removed = std::set<size_t>();

  // We traverse the graph rather than v_rem to use the graph's topological
  // ordering
  for (size_t node = 0; node < circuit.size(); node++) {
    if (v_rem.contains(node)) {
      auto op = circuit.get_Node_Value(node);
      std::set<size_t> op_qubits = std::set<size_t>();

      if (std::holds_alternative<StructU3>(op)) {
        op_qubits = {std::get<StructU3>(op).qubit};
      } else {
        op_qubits = {std::get<std::array<qc::Qubit, 2>>(op)[0],
                     std::get<std::array<qc::Qubit, 2>>(op)[1]};
      }
      if (removed.size() < nQubits && disjunct(removed, op_qubits)) {
        if (std::holds_alternative<StructU3>(op)) {
          v_c.push_back(node);
          removed.insert(std::get<StructU3>(op).qubit);
        } else {
          v_p.push_back(node);
        }
      } else {
        v_r.push_back(node);
        for (auto qubit : op_qubits) {
          removed.insert(qubit);
        }
      }
    }
  }
  return std::array<std::vector<size_t>, 3>({v_p, v_c, v_r});
}

auto NativeGateDecomposer::build_schedule(
    DiGraph<std::variant<StructU3, std::array<qc::Qubit, 2>>>& circuit,
    DiGraph<std::pair<std::vector<std::size_t>, std::vector<std::size_t>>>&
        subproblem_graph) -> std::pair<std::vector<std::vector<StructU3>>,
                                       std::vector<TwoQubitGateLayer>> {

  std::vector<std::size_t> leaf_nodes = find_leaf_nodes(subproblem_graph);
  std::vector<std::size_t> minimal_path =
      find_shortest_path(subproblem_graph, leaf_nodes);
  std::pair<std::vector<std::vector<StructU3>>, std::vector<TwoQubitGateLayer>>
      schedule = std::pair<std::vector<std::vector<StructU3>>,
                           std::vector<TwoQubitGateLayer>>{};
  // !!!!TODO:ADD in the check that makes sure SQGL's sandwich schedule: If 1st
  // v_p empty skip adding it . If not add empty SQGL

  std::vector<StructU3> singleQubitGates;
  std::vector<std::array<qc::Qubit, 2>> twoQubitGates;

  if (!subproblem_graph.get_Node_Value(minimal_path[0]).first.empty()) {
    schedule.first.push_back({});
  }
  std::set<qc::Qubit> used_qubits{};

  for (std::size_t i = 0; i < minimal_path.size(); i++) {
    singleQubitGates.clear();
    twoQubitGates.clear();
    used_qubits.clear();

    for (auto j : subproblem_graph.get_Node_Value(minimal_path[i]).first) {
      auto op = circuit.get_Node_Value(j);
      if (std::holds_alternative<std::array<qc::Qubit, 2>>(op)) {
        // TODO: Check if TWOQUBIT GATES Can be executed in parallel!!
        auto gate = std::get<std::array<qc::Qubit, 2>>(op);
        if (used_qubits.contains(gate[0]) || used_qubits.contains(gate[1])) {
          schedule.second.push_back(twoQubitGates);
          schedule.first.push_back({});
          twoQubitGates.clear();
          used_qubits.clear();
        }
        used_qubits.insert(gate[0]);
        used_qubits.insert(gate[1]);
        twoQubitGates.emplace_back(gate);
      }
    }

    for (auto j : subproblem_graph.get_Node_Value(minimal_path[i]).second) {
      auto op = circuit.get_Node_Value(j);
      if (std::holds_alternative<StructU3>(op)) {
        singleQubitGates.emplace_back(std::get<StructU3>(op));
      }
    }
    schedule.first.push_back(singleQubitGates);
    if (i != 0 ||
        !subproblem_graph.get_Node_Value(minimal_path[0]).first.empty()) {
      schedule.second.push_back(twoQubitGates);
    }
  }
  return schedule;
}

auto NativeGateDecomposer::add_node_to_sub_prob_graph(
    const std::vector<size_t>& v_p, const std::vector<size_t>& v_c, qc::fp cost,
    DiGraph<std::pair<std::vector<std::size_t>, std::vector<std::size_t>>>&
        subproblem_graph,
    std::size_t prev_node) -> size_t {
  std::size_t new_node = subproblem_graph.add_Node(std::pair(v_p, v_c));
  subproblem_graph.add_Edge(prev_node, new_node, cost);
  return new_node;
}

auto NativeGateDecomposer::schedule_remaining(
    const std::array<std::vector<size_t>, 3>& v,
    DiGraph<std::variant<StructU3, std::array<qc::Qubit, 2>>>& circuit,
    DiGraph<std::pair<std::vector<size_t>, std::vector<size_t>>>&
        subproblem_graph,
    size_t prev_node, size_t nQubits, bool check_final_cond,
    std::map<size_t, std::pair<size_t, std::array<double, 2>>>& memo)
    -> double {
  double cost;
  // TODO: Check if subproblem has been computed
  std::size_t id = std::hash<std::array<std::vector<size_t>, 3>>{}(v);
  if (memo.contains(id)) {
    std::size_t sub_node = memo.at(id).first;
    double edge_weight = memo.at(id).second[1];
    cost = memo.at(id).second[0];
    subproblem_graph.add_Edge(prev_node, sub_node, edge_weight);
    return cost;
  }
  // TODO: Base Case-> V_rem is empty
  if (v[2].empty()) {
    // TODO:DEcide if I need if to check for TWO QUBIT
    cost = std::get<StructU3>(circuit.get_Node_Value(v[1].at(0))).angles[0];
    for (std::size_t i : v[1]) {
      if (std::get<StructU3>(circuit.get_Node_Value(i)).angles[0] > cost) {
        cost = std::get<StructU3>(circuit.get_Node_Value(i)).angles[0];
      }
    }
    auto end_node = add_node_to_sub_prob_graph(v[0], v[1], cost,
                                               subproblem_graph, prev_node);
    memo[id] = std::pair<std::size_t, std::array<double, 2>>(
        end_node, {cost, cost}); // TODO: Correct to put cost for both???
    return cost;
  }
  // TODO: Recursive Call: Only
  auto v_new = sift(circuit, v[2], nQubits);
  auto args = get_possible_moments(circuit, v[1], v_new, check_final_cond);
  qc::fp temp_cost = 0;
  double min_cost = std::numeric_limits<double>::max();
  double min_weight = std::numeric_limits<double>::max();
  std::size_t min_node;
  for (const auto& val : args) {
    auto new_node = add_node_to_sub_prob_graph(v[0], val.first[0], val.second,
                                               subproblem_graph, prev_node);
    temp_cost = schedule_remaining({val.first[1], val.first[2], val.first[3]},
                                   circuit, subproblem_graph, new_node, nQubits,
                                   check_final_cond, memo) +
                val.second;
    if (temp_cost < min_cost) {
      min_cost = temp_cost;
      min_node = new_node;
      min_weight = val.second;
    }
  }
  memo[id] = std::pair<std::size_t, std::array<double, 2>>(
      min_node, {min_cost, min_weight});
  return min_cost;
}

auto NativeGateDecomposer::schedule_theta_opt(
    const std::pair<std::vector<std::vector<StructU3>>,
                    std::vector<TwoQubitGateLayer>>& schedule,
    std::size_t nQubits) const -> std::pair<std::vector<std::vector<StructU3>>,
                                            std::vector<TwoQubitGateLayer>> {

  // TODO: Convert Circuit to DAG: How to handle the unique Pointer situation???
  DiGraph<std::variant<StructU3, std::array<qc::Qubit, 2>>> circuit =
      convert_circ_to_dag(schedule, nQubits);
  // TODO: Get initial Moments( Not does MQB THEN SQB!! SOl to get SQB MQB??)
  std::vector<std::size_t> v_start{};
  v_start.reserve(circuit.size());
  for (size_t i = 0; i < circuit.size(); ++i) {
    v_start.push_back(i);
  }
  // v=(v_p,v_c,v_r)
  std::array<std::vector<size_t>, 3> v = sift(circuit, v_start, nQubits);
  // TODO: Create Subproblem Graph
  DiGraph<std::pair<std::vector<std::size_t>, std::vector<std::size_t>>>
      sub_prob_graph = DiGraph<
          std::pair<std::vector<std::size_t>, std::vector<std::size_t>>>();
  // TODO: First Call of Recursive Function to create Schedule
  auto base_node = sub_prob_graph.add_Node(
      std::pair<std::vector<std::size_t>, std::vector<std::size_t>>({}, {}));
  std::map<std::size_t, std::pair<std::size_t, std::array<double, 2>>> memo =
      {};
  auto cost = schedule_remaining(v, circuit, sub_prob_graph, base_node, nQubits,
                                 config_.check_final_cond, memo);
  // TODO: Create Schedule from Subproblem Graph
  std::pair<std::vector<std::vector<StructU3>>, std::vector<TwoQubitGateLayer>>
      final_circuit = build_schedule(circuit, sub_prob_graph);
  return final_circuit;
}
} // namespace na::zoned
