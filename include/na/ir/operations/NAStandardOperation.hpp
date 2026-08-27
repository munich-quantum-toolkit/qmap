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
#include "ir/Register.hpp"
#include "ir/operations/Operation.hpp"
#include "ir/operations/StandardOperation.hpp"
#include "na/ir/operations/NeutralAtomOpType.hpp"

#include <cstddef>
#include <memory>
#include <ostream>

namespace na {

/**
 * @brief Standard neutral-atom operation embedded in a quantum computation.
 * @details This class replaces the neutral-atom operation types that were
 * formerly represented by `qc::StandardOperation` in MQT Core.
 */
class NAStandardOperation final : public qc::StandardOperation {
  NeutralAtomOpType neutralAtomOpType = NeutralAtomOpType::None;

  static NeutralAtomOpType
  validateType(NeutralAtomOpType candidateNeutralAtomOpType);

public:
  NAStandardOperation() = default;
  NAStandardOperation(NeutralAtomOpType newNeutralAtomOpType,
                      qc::Targets operationTargets);

  /// Returns the neutral-atom operation type.
  [[nodiscard]] NeutralAtomOpType getNeutralAtomOpType() const {
    return neutralAtomOpType;
  }

  [[nodiscard]] std::unique_ptr<qc::Operation> clone() const override {
    return std::make_unique<NAStandardOperation>(*this);
  }

  [[nodiscard]] bool equals(const qc::Operation& operation,
                            const qc::Permutation& permutation1,
                            const qc::Permutation& permutation2) const override;

  [[nodiscard]] bool equals(const qc::Operation& operation) const override;

  void setGate(qc::OpType operationType) override;

  [[nodiscard]] auto commutesAtQubit(const qc::Operation& other,
                                     const qc::Qubit& qubit) const
      -> bool override;

  std::ostream& print(std::ostream& os, const qc::Permutation& permutation,
                      std::size_t prefixWidth,
                      std::size_t nQubits) const override;

  void dumpOpenQASM(std::ostream& of,
                    const qc::QubitIndexToRegisterMap& qubitMap,
                    const qc::BitIndexToRegisterMap& bitMap, std::size_t indent,
                    bool openQASM3) const override;

  void invert() override;
};

/// Creates a move operation between two locations.
[[nodiscard]] std::unique_ptr<qc::Operation>
makeMoveOperation(qc::Qubit origin, qc::Qubit target);

/// Creates a bridge operation over the given locations.
[[nodiscard]] std::unique_ptr<qc::Operation>
makeBridgeOperation(qc::Targets targets);

} // namespace na
