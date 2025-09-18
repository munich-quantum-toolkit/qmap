/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "Z3Model.hpp"

#include "LogicBlock.hpp"
#include "LogicTerm.hpp"
#include "Z3Logic.hpp"

#include <cstdint>
#include <string>
#include <z3++.h>

namespace z3logic {

bool Z3Model::getBoolValue(const LogicTerm& a, LogicBlock* lb) {
  auto* llb = dynamic_cast<Z3Base*>(lb);
  return z3::eq(model->eval(Z3Base::getExprTerm(a.getID(), a.getCType(), llb)),
                ctx->bool_val(true));
}

int32_t Z3Model::getIntValue(const LogicTerm& a, LogicBlock* lb) {
  auto* llb = dynamic_cast<Z3Base*>(lb);
  return static_cast<int>(
      model->eval(Z3Base::getExprTerm(a.getID(), a.getCType(), llb))
          .as_int64());
}

double Z3Model::getRealValue(const LogicTerm& a, LogicBlock* lb) {
  auto* llb = dynamic_cast<Z3Base*>(lb);
  return std::stod(
      model->eval(Z3Base::getExprTerm(a.getID(), a.getCType(), llb))
          .get_decimal_string(20));
}

uint64_t Z3Model::getBitvectorValue(const LogicTerm& a, LogicBlock* lb) {
  auto* llb = dynamic_cast<Z3Base*>(lb);
  return static_cast<uint64_t>(
      model->eval(Z3Base::getExprTerm(a.getID(), a.getCType(), llb))
          .as_int64());
}
} // namespace z3logic
