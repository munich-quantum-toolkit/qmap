/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
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
#include <utility>
#include <vector>

namespace na::zoned {
/**
 * The class ASAPScheduler implements the as-soon-as-possible scheduling
 * strategy for the zoned neutral atom compiler.
 */
class ASAPScheduler : public SchedulerBase {
  /// A reference to the zoned neutral atom architecture
  std::reference_wrapper<const Architecture> architecture_;
  /**
   * This value is calculated based on the architecture and indicates the
   * the entanglement zone.
   */
  size_t maxTwoQubitGateNumPerLayer_ = 0;

public:
  /**
   * The configuration of the ASAPScheduler
   * @note ASAPScheduler does not have any configuration parameters.
   */
  struct Config {
    template <typename BasicJsonType>
    friend void to_json(BasicJsonType& /* unused */,
                        const Config& /* unused */) {}
    template <typename BasicJsonType>
    friend void from_json(const BasicJsonType& /* unused */,
                          Config& /* unused */) {}
  };
  /**
   * Create a new ASAPScheduler.
   * @note The second parameter of the constructor is unused.
   * @param architecture is the architecture of the neutral atom system
   */
  ASAPScheduler(const Architecture& architecture, const Config& /* unused */);
  /**
   * This function schedules the operations of a quantum computation.
   * @details Every operation is scheduled as soon as possible. The function
   * splits the operations into layers. Every layer (except for the last one)
   * contains some single-qubit operations and two-qubit operations. The
   * single-qubit operations are executed before the two-qubit operations. For
   * every layer, all two-qubit operations can be executed in parallel, i.e.,
   * every qubit is involved in at most one two-qubit operation. The last layer
   * contains only the remaining single-qubit operations.
   * @param qc is the quantum computation
   * @return a pair of two vectors. The first vector contains the layers of
   * single-qubit operations. The second vector contains the layers of two-qubit
   * operations. A pair of qubits represents every two-qubit operation.
   */
  [[nodiscard]] auto schedule(const qc::QuantumComputation& qc) const
      -> std::pair<std::vector<SingleQubitGateLayer>,
                   std::vector<TwoQubitGateLayer>>;
};
} // namespace na::zoned
