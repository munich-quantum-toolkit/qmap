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

#include <complex>
#include <vector>

namespace na::zoned {

auto NativeGateDecomposer::convertGateToQuaternion(
    const std::reference_wrapper<const qc::Operation> op) -> Quaternion {
  assert(op.get().getNqubits() == 1);
  Quaternion quat;
  switch (op.get().getType()) {
  case qc::RZ:
  case qc::P:
    quat = {cos(op.get().getParameter().front() / 2), 0, 0,
            sin(op.get().getParameter().front() / 2)};
    break;
  case qc::Z:
    quat = {0, 0, 0, 1};
    break;
  case qc::S:
    quat = {cos(qc::PI_4), 0, 0, sin(qc::PI_4)};
    break;
  case qc::Sdg:
    quat = {cos(-qc::PI_4), 0, 0, sin(-qc::PI_4)};
    break;
  case qc::T:
    quat = {cos(qc::PI_4 / 2), 0, 0, sin(qc::PI_4 / 2)};
    break;
  case qc::Tdg:
    quat = {cos(-qc::PI_4 / 2), 0, 0, sin(-qc::PI_4 / 2)};
    break;
  case qc::U:
    quat = combineQuaternions(
        combineQuaternions({cos(op.get().getParameter().at(1) / 2), 0, 0,
                            sin(op.get().getParameter().at(1) / 2)},
                           {cos(op.get().getParameter().front() / 2), 0,
                            sin(op.get().getParameter().front() / 2), 0}),
        {cos(op.get().getParameter().at(2) / 2), 0, 0,
         sin(op.get().getParameter().at(2) / 2)});
    break;
  case qc::U2:
    quat = combineQuaternions(
        combineQuaternions({cos(op.get().getParameter().front() / 2), 0, 0,
                            sin(op.get().getParameter().front() / 2)},
                           {cos(qc::PI_4), 0, sin(qc::PI_4), 0}),
        {cos(op.get().getParameter().at(1) / 2), 0, 0,
         sin(op.get().getParameter().at(1) / 2)});
    break;
  case qc::RX:
    quat = {cos(op.get().getParameter().front() / 2),
            sin(op.get().getParameter().front() / 2), 0, 0};
    break;
  case qc::RY:
    quat = {cos(op.get().getParameter().front() / 2), 0,
            sin(op.get().getParameter().front() / 2), 0};
    break;
  case qc::H:
    quat = combineQuaternions(
        combineQuaternions({1, 0, 0, 0}, {cos(qc::PI_4), 0, sin(qc::PI_4), 0}),
        {cos(qc::PI_2), 0, 0, sin(qc::PI_2)});
    break;
  case qc::X:
    quat = {0, 1, 0, 0};
    break;
  case qc::Y:
    quat = {0, 0, 1, 0};
    break;
  case qc::Vdg:
    quat = combineQuaternions(
        combineQuaternions({cos(qc::PI_4), 0, 0, sin(qc::PI_4)},
                           {cos(-qc::PI_4), 0, sin(-qc::PI_4), 0}),
        {cos(-qc::PI_4), 0, 0, sin(-qc::PI_4)});
    break;
  case qc::SX:
    quat = combineQuaternions(
        combineQuaternions({cos(-qc::PI_4), 0, 0, sin(-qc::PI_4)},
                           {cos(qc::PI_4), 0, sin(qc::PI_4), 0}),
        {cos(qc::PI_4), 0, 0, sin(qc::PI_4)});
    break;
  case qc::SXdg:
  case qc::V:
    quat = combineQuaternions(
        combineQuaternions({cos(-qc::PI_4), 0, 0, sin(-qc::PI_4)},
                           {cos(-qc::PI_4), 0, sin(-qc::PI_4), 0}),
        {cos(qc::PI_4), 0, 0, sin(qc::PI_4)});
    break;
  default:
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
  return {q1[0] * q2[0] - q1[1] * q2[1] - q1[2] * q2[2] - q1[3] * q2[3],
          q1[0] * q2[1] + q1[1] * q2[0] + q1[2] * q2[3] - q1[3] * q2[2],
          q1[0] * q2[2] - q1[1] * q2[3] + q1[2] * q2[0] + q1[3] * q2[1],
          q1[0] * q2[3] + q1[1] * q2[2] - q1[2] * q2[1] + q1[3] * q2[0]};
}

auto NativeGateDecomposer::getU3AnglesFromQuaternion(const Quaternion& quat)
    -> std::array<qc::fp, 3> {
  qc::fp theta;
  qc::fp phi;
  qc::fp lambda;
  if (std::fabs(quat[0]) > epsilon || std::fabs(quat[3]) > epsilon) {
    theta = 2. * std::atan2(std::sqrt(quat[2] * quat[2] + quat[1] * quat[1]),
                            std::sqrt(quat[0] * quat[0] + quat[3] * quat[3]));
    qc::fp alpha_1 = std::atan2(quat[3], quat[0]); // phi+ lambda
    if (std::fabs(quat[1]) > epsilon || std::fabs(quat[2]) > epsilon) {
      qc::fp alpha_2 = -1 * std::atan2(quat[1], quat[2]);
      phi = alpha_1 + alpha_2; // phi
      lambda = alpha_1 - alpha_2;
    } else {
      phi = 0;
      lambda = 2 * alpha_1;
    }
  } else {
    theta = qc::PI; // or § PI if sin(theta/2)=-1... Relevant? Or is the -1=
                    // exp(i*pi) just global phase
    if (std::fabs(quat[1]) > epsilon || std::fabs(quat[2]) > epsilon) {
      phi = 0;
      lambda = 2 * std::atan2(quat[1], quat[2]);
      // atan can give PI instead of 0 Problem?
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
  for (auto [angles, qubit] : layer) {
    if (const auto a = std::fabs(angles[0]); a > theta_max) {
      theta_max = a;
    }
  }
  return theta_max;
}
auto NativeGateDecomposer::transformToU3(
    const std::vector<SingleQubitGateRefLayer>& layers) const
    -> std::vector<std::vector<StructU3>> {
  std::vector<std::vector<StructU3>> new_layers;
  for (const auto& layer : layers) {
    std::vector<std::vector<std::reference_wrapper<const qc::Operation>>> gates(
        nQubits_);
    auto& new_layer = new_layers.emplace_back();
    for (const auto gate : layer) {
      if (!gate.get().getTargets().empty()) {
        gates[gate.get().getTargets().front()].emplace_back(gate);
      }
    }

    for (const auto& qubit_gates : gates) {
      if (!qubit_gates.empty()) {
        auto quat = convertGateToQuaternion(qubit_gates.front());
        for (const auto& gate : qubit_gates | std::views::drop(1)) {
          quat = combineQuaternions(quat, convertGateToQuaternion(gate));
        }
        const auto angles = getU3AnglesFromQuaternion(quat);
        new_layer.emplace_back(angles,
                               qubit_gates.front().get().getTargets().front());
      }
    }
  }
  return new_layers;
}
auto NativeGateDecomposer::getDecompositionAngles(
    const std::array<qc::fp, 3>& angles, const qc::fp theta_max)
    -> std::array<qc::fp, 3> {
  qc::fp alpha;
  qc::fp chi;
  // U3(theta,phi_min(phi),phi_plus(lambda)->Rz(gamma_minus)GR(theta_max/2,
  // PI_2)Rz(chi)GR(-theta_max/2,PI_2)RZ(gamma_plus)
  const auto sin_sq_diff = (sin(theta_max / 2) * sin(theta_max / 2) -
                            (sin(angles[0] / 2) * sin(angles[0] / 2)));
  if (std::fabs(sin_sq_diff) < epsilon) {
    chi = qc::PI;
    if (std::fabs(cos(theta_max / 2)) < epsilon) { // Periodicity covered?
      alpha = 0;
    } else {
      alpha = qc::PI_2;
    }
  } else {
    const auto kappa =
        std::sqrt((sin(angles[0] / 2) * sin(angles[0] / 2)) / sin_sq_diff);
    alpha = atan(cos(theta_max / 2) * kappa);
    chi = fmod(2 * atan(kappa), qc::TAU);
  }
  const auto beta = angles[0] < 0 ? -qc::PI_2 : qc::PI_2;
  const auto gamma_plus = std::fmod(angles[2] - (alpha + beta), qc::TAU);
  const auto gamma_minus = std::fmod(angles[1] - (alpha - beta), qc::TAU);

  return {chi, gamma_minus, gamma_plus};
}

auto NativeGateDecomposer::decompose(
    const size_t nQubits,
    const std::vector<SingleQubitGateRefLayer>& singleQubitGateLayers)
    -> std::vector<SingleQubitGateLayer> {
  nQubits_ = nQubits;

  const auto U3Layers = transformToU3(singleQubitGateLayers);
  std::vector<SingleQubitGateLayer> newSingleQubitLayers;

  for (const auto& layer : U3Layers) {
    const auto theta_max = calcThetaMax(layer);
    SingleQubitGateLayer frontLayer;
    SingleQubitGateLayer midLayer;
    SingleQubitGateLayer backLayer;
    auto& newLayer = newSingleQubitLayers.emplace_back();

    for (const auto& [angles, qubit] : layer) {
      const auto& decomp_angles = getDecompositionAngles(angles, theta_max);

      // GR(theta_max/2, PI_2)==Global Y due to PI_2
      frontLayer.emplace_back(std::make_unique<const qc::StandardOperation>(
          qc::StandardOperation(qubit, qc::RZ, {decomp_angles[1]})));

      midLayer.emplace_back(std::make_unique<const qc::StandardOperation>(
          qc::StandardOperation(qubit, qc::RZ, {decomp_angles[0]})));

      backLayer.emplace_back(std::make_unique<const qc::StandardOperation>(
          qc::StandardOperation(qubit, qc::RZ, {decomp_angles[2]})));
    } // gate::layer

    std::vector<std::unique_ptr<qc::Operation>> grPlus;
    std::vector<std::unique_ptr<qc::Operation>> grMinus;

    for (size_t i = 0; i < nQubits_; ++i) {
      grPlus.emplace_back(std::make_unique<qc::StandardOperation>(
          i, qc::RY, std::vector{theta_max / 2}));
      grMinus.emplace_back(std::make_unique<qc::StandardOperation>(
          i, qc::RY, std::vector{-1 * theta_max / 2}));
    }

    for (auto&& gate : frontLayer) {
      newLayer.emplace_back(std::move(gate));
    }

    newLayer.emplace_back(
        std::make_unique<const qc::CompoundOperation>(std::move(grPlus), true));

    for (auto&& gate : midLayer) {
      newLayer.emplace_back(std::move(gate));
    }
    newLayer.emplace_back(std::make_unique<const qc::CompoundOperation>(
        std::move(grMinus), true));

    for (auto&& gate : backLayer) {
      newLayer.emplace_back(std::move(gate));
    }
  }
  return newSingleQubitLayers;
}
} // namespace na::zoned
