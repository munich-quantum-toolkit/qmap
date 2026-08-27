/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

/** @file NeutralAtomOpType.hpp
 * @brief Neutral-atom operation types embedded in quantum computations.
 */

#pragma once

#include "ir/operations/Operation.hpp"

#include <cstdint>
#include <optional>
#include <string_view>

namespace na {

/// Neutral-atom operation types embedded in a quantum computation.
enum class NeutralAtomOpType : std::uint8_t {
  None,
  Move,
  Bridge,
  AodActivate,
  AodDeactivate,
  AodMove,
};

/// Converts a neutral-atom operation type to its textual name.
[[nodiscard]] std::string_view toString(NeutralAtomOpType type);

/// Converts a textual name to a neutral-atom operation type.
[[nodiscard]] NeutralAtomOpType
neutralAtomOpTypeFromString(std::string_view name);

/// Returns the neutral-atom type of an operation, if it has one.
[[nodiscard]] std::optional<NeutralAtomOpType>
getNeutralAtomOpType(const qc::Operation& operation);

/// Checks whether an operation has a particular neutral-atom type.
[[nodiscard]] bool hasNeutralAtomOpType(const qc::Operation& operation,
                                        NeutralAtomOpType type);

/// Checks whether an operation is an AOD operation.
[[nodiscard]] bool isAodOperation(const qc::Operation& operation);

} // namespace na
