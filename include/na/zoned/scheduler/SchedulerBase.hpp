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

#include "ir/QuantumComputation.hpp"
#include "na/zoned/Types.hpp"

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
   * @return a SchedulerResult whose @p singleQubitLayers contains the layers of
   * single-qubit operations and whose @p twoQubitLayers contains the layers of
   * two-qubit operations.
   */
  [[nodiscard]] virtual auto schedule(const qc::QuantumComputation& qc) const
      -> SchedulerResult = 0;
};
} // namespace na::zoned
