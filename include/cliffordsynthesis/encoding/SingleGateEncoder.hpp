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

#include "cliffordsynthesis/encoding/GateEncoder.hpp"
#include "logicblocks/LogicTerm.hpp"

#include <cstddef>

namespace cs::encoding {

class SingleGateEncoder : public GateEncoder {
public:
  using GateEncoder::GateEncoder;

protected:
  void assertConsistency() const override;
  void assertGateConstraints() override;
  void assertSingleQubitGateConstraints(std::size_t pos) override;
  void assertTwoQubitGateConstraints(std::size_t pos) override;
  void assertNoGateNoChangeConstraint(std::size_t pos);
  [[nodiscard]] logicbase::LogicTerm
  createTwoQubitGateConstraint(std::size_t pos, std::size_t ctrl,
                               std::size_t trgt) override;

  [[nodiscard]] logicbase::LogicTerm createNoChangeOnQubit(std::size_t pos,
                                                           std::size_t q);

  [[nodiscard]] logicbase::LogicTerm createNoGateOnQubit(std::size_t pos,
                                                         std::size_t q);

  void assertSingleQubitGateOrderConstraints(std::size_t pos,
                                             std::size_t qubit) override;
  void assertTwoQubitGateOrderConstraints(std::size_t pos, std::size_t ctrl,
                                          std::size_t trgt) override;
};

} // namespace cs::encoding
