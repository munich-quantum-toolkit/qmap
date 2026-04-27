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

#include "DecomposerBase.hpp"
#include "ir/operations/StandardOperation.hpp"
#include "na/zoned/Types.hpp"

#include <vector>

namespace na::zoned {

class NativeGateDecomposer : public DecomposerBase {
  /**
   * A minimal struct to store the parameters of a U3 gate along with the qubit
   * it acts on.
   */
  struct StructU3 {
    std::array<qc::fp, 3> angles;
    qc::Qubit qubit;
  };
  /**
   * A quaternion is represented by an array of four `qc::fp` values `{q0, q1,
   * q2, q3}` denoting the components of the quaternion.
   */
  using Quaternion = std::array<qc::fp, 4>;
  size_t nQubits_ = 0;

  /// Tolerance for floating-point comparisons, scaled to account for
  /// accumulated rounding errors in quaternion arithmetic (~10 bits margin).
  constexpr static qc::fp epsilon =
      std::numeric_limits<qc::fp>::epsilon() * 1024;

public:
  /// The configuration of the NativeGateDecomposer
  struct Config {
    template <typename BasicJsonType>
    friend void to_json(BasicJsonType& /* unused */,
                        const Config& /* unused */) {}
    template <typename BasicJsonType>
    friend void from_json(const BasicJsonType& /* unused */,
                          Config& /* unused */) {}
  };

private:
  /**
   * @brief Takes a vector of SingleQubitGateLayers and, for each layer,
   * transforms all gates into U3 gates represented by `StructU3` objects.
   * @details It combines all gates acting on the same qubit into a single U3
   * gate.
   * @param layers is a std::vector of SingleQubitGateLayers of a scheduled
   * circuit.
   * @returns a vector of vectors of StructU3 objects representing the single
   * qubit gate layers.
   */
  [[nodiscard]] auto
  transformToU3(const std::vector<SingleQubitGateRefLayer>& layers) const
      -> std::vector<std::vector<StructU3>>;

public:
  /// Create a new NativeGateDecomposer.
  NativeGateDecomposer(const Architecture& /* unused */,
                       const Config& /* unused */) {}

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
   * @brief Takes a vector of `qc::fp` representing the U3-gate angles of a
   * single-qubit gate and the maximal value of theta for the single qubit gate
   * layer and calculates the transversal decomposition angles as in Nottingham
   * et. al. 2024.
   * @param angles is a `std::array` of `qc::fp` representing (theta, phi,
   * lambda).
   * @param theta_max the maximal theta value of the single-qubit gate layer.
   * @returns an array of `qc::fp` values giving the angles (chi, gamma_minus,
   * gamma_plus).
   */
  auto static getDecompositionAngles(const std::array<qc::fp, 3>& angles,
                                     qc::fp theta_max) -> std::array<qc::fp, 3>;

  [[nodiscard]] auto
  decompose(size_t nQubits,
            const std::vector<SingleQubitGateRefLayer>& singleQubitGateLayers)
      -> std::vector<SingleQubitGateLayer> override;
};

} // namespace na::zoned
