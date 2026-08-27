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

#include "ir/Permutation.hpp"
#include "ir/operations/Operation.hpp"
#include "na/ir/operations/NeutralAtomOpType.hpp"

#include <cstddef>
#include <ostream>

namespace na::detail {

/// Prints a neutral-atom operation in MQT Core's circuit-diagram format.
std::ostream& printNeutralAtomOperation(const qc::Operation& operation,
                                        NeutralAtomOpType operationType,
                                        std::ostream& os,
                                        const qc::Permutation& permutation,
                                        std::size_t prefixWidth,
                                        std::size_t nQubits);

} // namespace na::detail
