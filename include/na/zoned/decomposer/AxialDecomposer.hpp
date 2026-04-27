#pragma once

#include "na/zoned/Types.hpp"
#include "na/zoned/decomposer/DecomposerBase.hpp"

#include <vector>

namespace na::zoned {
class AxialDecomposer : public DecomposerBase {
  /**
   * A quaternion is represented by an array of four `qc::fp` values `{q0, q1,
   * q2, q3}` denoting the components of the quaternion.
   */
  using Quaternion = std::array<qc::fp, 4>;

  /// A value to use as a margin of error for float equality
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
  AxialDecomposer(const Architecture& /* unused */,
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
};
} // namespace na::zoned