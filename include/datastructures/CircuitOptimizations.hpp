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

#include "ir/Definitions.hpp"

#include <functional>

namespace qc {
class Operation;
class QuantumComputation;
} // namespace qc

namespace qmap {

/// Return whether the operation is an X gate with one positive control.
[[nodiscard]] bool isCNOT(const qc::Operation& op);

/**
 * @brief Decompose uncontrolled SWAP gates into CNOT gates.
 * @details Controlled SWAP gates are preserved.
 * @param qc Quantum circuit to transform.
 * @param isDirectedArchitecture Whether all CNOTs must use the same direction.
 */
void decomposeSWAP(qc::QuantumComputation& qc, bool isDirectedArchitecture);

/**
 * @brief Cancel adjacent CNOT and SWAP patterns introduced during mapping.
 * @param qc Quantum circuit to transform.
 */
void cancelCNOTs(qc::QuantumComputation& qc);

/// Restrict rewrites between SWAP and CNOT gates to the given edges.
void cancelCNOTs(
    qc::QuantumComputation& qc,
    const std::function<bool(qc::Qubit, qc::Qubit)>& allowSwapRewrite);

/**
 * @brief Fuse adjacent single-qubit gates.
 * @details Preserves measurements, resets, and barriers.
 * @param qc Quantum circuit to transform.
 */
void singleQubitGateFusion(qc::QuantumComputation& qc);

/**
 * @brief Replace controlled X gates by controlled Z gates surrounded by H
 * gates on the target.
 * @param qc Quantum circuit to transform.
 */
void replaceMCXWithMCZ(qc::QuantumComputation& qc);

} // namespace qmap
