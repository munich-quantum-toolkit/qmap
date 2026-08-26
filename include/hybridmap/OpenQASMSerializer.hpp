/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

/** @file OpenQASMSerializer.hpp
 * @brief Serialization of hybrid neutral-atom circuits.
 */

#pragma once

#include "ir/QuantumComputation.hpp"

#include <iosfwd>

namespace na {

/**
 * @brief Serialize a hybrid neutral-atom circuit to extended OpenQASM 2.
 * @param computation Circuit containing Core and neutral-atom operations.
 * @param output Stream receiving the serialized circuit.
 */
void serializeOpenQASM(const qc::QuantumComputation& computation,
                       std::ostream& output);

} // namespace na
