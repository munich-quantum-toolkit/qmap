#include "na/zoned/decomposer/AxialDecomposer.hpp"

#include "ir/operations/CompoundOperation.hpp"
#include "ir/operations/Operation.hpp"
#include "ir/operations/StandardOperation.hpp"

namespace na::zoned {
auto AxialDecomposer::convertGateToQuaternion(
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

auto AxialDecomposer::combineQuaternions(const Quaternion& q1,
                                         const Quaternion& q2) -> Quaternion {
  Quaternion new_quat{};
  new_quat[0] = q1[0] * q2[0] - q1[1] * q2[1] - q1[2] * q2[2] - q1[3] * q2[3];
  new_quat[1] = q1[0] * q2[1] + q1[1] * q2[0] + q1[2] * q2[3] - q1[3] * q2[2];
  new_quat[2] = q1[0] * q2[2] - q1[1] * q2[3] + q1[2] * q2[0] + q1[3] * q2[1];
  new_quat[3] = q1[0] * q2[3] + q1[1] * q2[2] - q1[2] * q2[1] + q1[3] * q2[0];
  return new_quat;
}

auto AxialDecomposer::getU3AnglesFromQuaternion(const Quaternion& quat)
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

auto AxialDecomposer::transformToU3(
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

auto AxialDecomposer::decompose(
    size_t nQubits, const std::pair<std::vector<SingleQubitGateRefLayer>,
                                    std::vector<TwoQubitGateLayer>>& schedule)
    -> std::pair<std::vector<SingleQubitGateLayer>,
                 std::vector<TwoQubitGateLayer>> {

  std::vector<std::vector<StructU3>> U3Layers =
      transformToU3(schedule.first, nQubits);
  std::vector<TwoQubitGateLayer> NewTwoQubitGateLayers = schedule.second;
  std::vector<SingleQubitGateLayer> NewSingleQubitLayers =
      std::vector<SingleQubitGateLayer>{};

  for (const auto& layer : U3Layers) {
    SingleQubitGateLayer FrontLayer;
    SingleQubitGateLayer MidLayer;
    SingleQubitGateLayer BackLayer;
    SingleQubitGateLayer NewLayer = {};

    for (auto gate : layer) {

      // Global RX here instead of RY
      FrontLayer.emplace_back(std::make_unique<const qc::StandardOperation>(
          qc::StandardOperation(gate.qubit, qc::RZ, {gate.angles[1]})));

      MidLayer.emplace_back(std::make_unique<const qc::StandardOperation>(
          qc::StandardOperation(gate.qubit, qc::RZ, {gate.angles[0]})));

      BackLayer.emplace_back(std::make_unique<const qc::StandardOperation>(
          qc::StandardOperation(gate.qubit, qc::RZ, {gate.angles[2]})));
    } // gate::layer
    if (!layer.empty()) {
      std::vector<std::unique_ptr<qc::Operation>> GR_plus;
      std::vector<std::unique_ptr<qc::Operation>> GR_minus;

      for (size_t i = 0; i < nQubits; ++i) {
        GR_plus.emplace_back(std::make_unique<qc::StandardOperation>(
            i, qc::RX, std::initializer_list<qc::fp>{-qc::PI / 2}));
        GR_minus.emplace_back(std::make_unique<qc::StandardOperation>(
            i, qc::RX, std::initializer_list<qc::fp>{qc::PI / 2}));
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

} // namespace na::zoned