/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

/** @file NAStandardOperation.hpp
 * @brief Neutral-atom operations embedded in a quantum computation.
 */

#pragma once

#include "ir/Definitions.hpp"
#include "ir/operations/Operation.hpp"
#include "na/ir/operations/NAOpType.hpp"

#include <cstddef>
#include <memory>
#include <ostream>
#include <stdexcept>

namespace na {

/**
 * @brief Standard neutral-atom operation embedded in a quantum computation.
 * @details This class replaces the neutral-atom operation types that were
 * formerly represented by `qc::StandardOperation` in MQT Core.
 */
class NAStandardOperation final : public qc::Operation {
  NAOpType naOpType = NAOpType::None;

  static NAOpType validateType(NAOpType candidateNAOpType);

public:
  NAStandardOperation() = default;
  NAStandardOperation(NAOpType newNAOpType, qc::Targets operationTargets);

  /// Returns the neutral-atom operation type.
  [[nodiscard]] NAOpType getNAOpType() const { return naOpType; }

  [[nodiscard]] std::unique_ptr<qc::Operation> clone() const override {
    return std::make_unique<NAStandardOperation>(*this);
  }

  [[nodiscard]] bool equals(const qc::Operation& operation) const override;

  void setGate(qc::OpType operationType) override;

  /// Move and bridge operations do not support quantum controls.
  [[noreturn]] void addControl([[maybe_unused]] qc::Control control) override {
    throw std::invalid_argument(
        "Neutral-atom operations do not support controls.");
  }
  void clearControls() override { controls.clear(); }
  [[noreturn]] void
  removeControl([[maybe_unused]] qc::Control control) override {
    throw std::invalid_argument(
        "Neutral-atom operations do not have controls.");
  }
  qc::Controls::iterator
  removeControl(const qc::Controls::iterator it) override {
    return controls.erase(it);
  }

  [[nodiscard]] auto commutesAtQubit(const qc::Operation& other,
                                     const qc::Qubit& qubit) const
      -> bool override;

  std::ostream& print(std::ostream& os, const qc::Permutation& permutation,
                      std::size_t prefixWidth,
                      std::size_t nQubits) const override;

  void invert() override;
};

/// Creates a move operation between two locations.
[[nodiscard]] std::unique_ptr<qc::Operation>
makeMoveOperation(qc::Qubit origin, qc::Qubit target);

/// Creates a bridge operation over the given locations.
[[nodiscard]] std::unique_ptr<qc::Operation>
makeBridgeOperation(qc::Targets targets);

} // namespace na
