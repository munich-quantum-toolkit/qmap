/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

/** @file AodOperation.hpp
 * @brief Atom-array operation representation.
 */

#pragma once

#include "hybridmap/NeutralAtomOperation.hpp"
#include "ir/Definitions.hpp"
#include "ir/operations/Control.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

namespace na {

enum class Dimension : std::uint8_t { X = 0, Y = 1 };
struct SingleOperation {
  Dimension dir;
  qc::fp start;
  qc::fp end;

  SingleOperation(const Dimension d, const qc::fp s, const qc::fp e)
      : dir(d), start(s), end(e) {}

  bool operator==(const SingleOperation&) const = default;

  [[nodiscard]] std::string toQASMString() const;
};
class AodOperation final : public NeutralAtomOperation {
  std::vector<SingleOperation> operations;

  static NeutralAtomOperationKind validateKind(NeutralAtomOperationKind kind);

  static std::vector<Dimension>
  convertToDimension(const std::vector<uint32_t>& dirs);

public:
  AodOperation()
      : NeutralAtomOperation(NeutralAtomOperationKind::AodMove, {},
                             std::in_place) {}
  AodOperation(NeutralAtomOperationKind kind, std::vector<qc::Qubit> qubits,
               const std::vector<Dimension>& dirs,
               const std::vector<qc::fp>& starts,
               const std::vector<qc::fp>& ends);
  AodOperation(NeutralAtomOperationKind kind, std::vector<qc::Qubit> qubits,
               const std::vector<uint32_t>& dirs,
               const std::vector<qc::fp>& starts,
               const std::vector<qc::fp>& ends);
  AodOperation(const std::string& typeName, std::vector<qc::Qubit> qubits,
               const std::vector<uint32_t>& dirs,
               const std::vector<qc::fp>& starts,
               const std::vector<qc::fp>& ends);
  AodOperation(NeutralAtomOperationKind kind, std::vector<qc::Qubit> qubits,
               const std::vector<std::tuple<Dimension, qc::fp, qc::fp>>& ops);
  AodOperation(NeutralAtomOperationKind kind, std::vector<qc::Qubit> targets,
               std::vector<SingleOperation> operations);

  [[nodiscard]] std::unique_ptr<Operation> clone() const override {
    return std::make_unique<AodOperation>(*this);
  }

  void addControl([[maybe_unused]] qc::Control c) override {}
  void clearControls() override {}
  void removeControl([[maybe_unused]] qc::Control c) override {}
  qc::Controls::iterator
  removeControl(const qc::Controls::iterator it) override {
    return controls.erase(it);
  }

  [[nodiscard]] std::vector<qc::fp> getEnds(Dimension dir) const;

  [[nodiscard]] std::vector<qc::fp> getStarts(Dimension dir) const;

  [[nodiscard]] qc::fp getMaxDistance(Dimension dir) const;

  [[nodiscard]] std::vector<qc::fp> getDistances(Dimension dir) const;

  [[nodiscard]] bool equals(const qc::Operation& operation) const override;

  /// @brief Get the elementary AOD movements.
  [[nodiscard]] const std::vector<SingleOperation>& getOperations() const {
    return operations;
  }

  void invert() override;
};
} // namespace na
