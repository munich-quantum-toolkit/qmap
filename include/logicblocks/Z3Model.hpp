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

#include "LogicBlock.hpp"
#include "LogicTerm.hpp"
#include "Model.hpp"

#include <cstdint>
#include <memory>
#include <utility>
#include <z3++.h>

namespace z3logic {

using namespace logicbase;

class Z3Model : public Model {
protected:
  std::shared_ptr<z3::model> model;
  std::shared_ptr<z3::context> ctx;

public:
  Z3Model(std::shared_ptr<z3::context> context, std::shared_ptr<z3::model> mdl)
      : model(std::move(mdl)), ctx(std::move(context)) {}
  int getIntValue(const LogicTerm& a, LogicBlock* lb) override;
  bool getBoolValue(const LogicTerm& a, LogicBlock* lb) override;
  double getRealValue(const LogicTerm& a, LogicBlock* lb) override;
  uint64_t getBitvectorValue(const LogicTerm& a, LogicBlock* lb) override;
};
} // namespace z3logic
