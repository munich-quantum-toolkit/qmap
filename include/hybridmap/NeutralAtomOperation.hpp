/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

/** @file NeutralAtomOperation.hpp
 * @brief Neutral-atom-specific operation representation.
 */

#pragma once

#include "ir/Definitions.hpp"
#include "ir/operations/Control.hpp"
#include "ir/operations/Operation.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace na {

/** @brief Kinds of operations that are specific to neutral-atom devices. */
enum class NeutralAtomOperationKind : std::uint8_t {
  Move,
  Bridge,
  AodActivate,
  AodDeactivate,
  AodMove,
};

/** @brief Convert a neutral-atom operation kind to its textual name. */
[[nodiscard]] std::string_view toString(NeutralAtomOperationKind kind);

/** @brief Convert a textual name to a neutral-atom operation kind. */
[[nodiscard]] NeutralAtomOperationKind
neutralAtomOperationKindFromString(std::string_view name);

/**
 * @brief Base representation for neutral-atom-specific operations.
 * @details MQT Core intentionally treats these operations as generic custom
 * operations. Their concrete kind is owned and interpreted by QMAP.
 */
class NeutralAtomOperation : public qc::Operation {
  NeutralAtomOperationKind kind;

  static NeutralAtomOperationKind
  validateKind(NeutralAtomOperationKind operationKind);

protected:
  NeutralAtomOperation(NeutralAtomOperationKind operationKind,
                       qc::Targets operationTargets, std::in_place_t);
  void setKind(NeutralAtomOperationKind newKind);

public:
  NeutralAtomOperation(NeutralAtomOperationKind operationKind,
                       qc::Targets operationTargets);

  [[nodiscard]] NeutralAtomOperationKind getKind() const { return kind; }

  [[nodiscard]] std::unique_ptr<Operation> clone() const override {
    return std::make_unique<NeutralAtomOperation>(*this);
  }

  void addControl([[maybe_unused]] qc::Control control) override {}
  void clearControls() override {}
  void removeControl([[maybe_unused]] qc::Control control) override {}
  qc::Controls::iterator
  removeControl(const qc::Controls::iterator it) override {
    return controls.erase(it);
  }

  [[nodiscard]] bool equals(const qc::Operation& operation) const override;

  void invert() override;
};

/** @brief Check whether an operation has a particular neutral-atom kind. */
[[nodiscard]] bool hasNeutralAtomOperationKind(const qc::Operation& operation,
                                               NeutralAtomOperationKind kind);

/** @brief Check whether an operation is an AOD operation. */
[[nodiscard]] bool isAodOperation(const qc::Operation& operation);

/** @brief Create a move operation between two locations. */
[[nodiscard]] std::unique_ptr<qc::Operation>
makeMoveOperation(qc::Qubit origin, qc::Qubit target);

/** @brief Create a bridge operation over the given locations. */
[[nodiscard]] std::unique_ptr<qc::Operation>
makeBridgeOperation(qc::Targets targets);

} // namespace na
