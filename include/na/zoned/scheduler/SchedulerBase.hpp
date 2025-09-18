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
#include "na/zoned/Types.hpp"

#include <utility>
#include <vector>

namespace na::zoned {
/**
 * The  Abstract Base Class for the Scheduler of the MQT's Zoned Neutral Atom
 * Compiler.
 */
class SchedulerBase {
public:
  virtual ~SchedulerBase() = default;
  /**
   * This function defines the interface of the scheduler.
   * @param qc is the quantum computation
   * @return a pair of two vectors. The first vector contains the layers of
   * single-qubit operations. The second vector contains the layers of two-qubit
   * operations. A pair of qubits represents every two-qubit operation.
   */
  [[nodiscard]] virtual auto schedule(const qc::QuantumComputation& qc) const
      -> std::pair<std::vector<SingleQubitGateLayer>,
                   std::vector<TwoQubitGateLayer>> = 0;
};
} // namespace na::zoned
