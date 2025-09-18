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
#include "Z3Logic.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <z3++.h>

namespace logicutil {
using namespace logicbase;

enum class ParamType : std::uint8_t {
  STR,
  BOOL,
  DOUBLE,
  UINT,
};

class Param {
public:
  ParamType type;
  std::string name;
  std::string strvalue;
  bool bvalue = false;
  double dvalue = 0.;
  uint32_t uivalue = 0;
  Param(std::string n, std::string value)
      : type(ParamType::STR), name(std::move(n)), strvalue(std::move(value)) {}

  Param(std::string n, bool value)
      : type(ParamType::BOOL), name(std::move(n)), bvalue(value) {}

  Param(std::string n, double value)
      : type(ParamType::DOUBLE), name(std::move(n)), dvalue(value) {}

  Param(std::string n, uint32_t value)
      : type(ParamType::UINT), name(std::move(n)), uivalue(value) {}
};
class Params {
  std::vector<Param> params;

public:
  void addParam(const std::string& n, const std::string& value) {
    params.emplace_back(n, value);
  }
  void addParam(const std::string& n, bool value) {
    params.emplace_back(n, value);
  }
  void addParam(const std::string& n, double value) {
    params.emplace_back(n, value);
  }
  void addParam(const std::string& n, uint32_t value) {
    params.emplace_back(n, value);
  }
  [[nodiscard]] std::vector<Param> getParams() const { return params; }
};

inline void setZ3Params(z3::params& p, const Params& params) {
  for (const auto& param : params.getParams()) {
    switch (param.type) {
    case ParamType::STR:
      p.set(param.name.c_str(), param.strvalue.c_str());
      break;
    case ParamType::BOOL:
      p.set(param.name.c_str(), param.bvalue);
      break;
    case ParamType::DOUBLE:
      p.set(param.name.c_str(), param.dvalue);
      break;
    case ParamType::UINT:
      p.set(param.name.c_str(), param.uivalue);
      break;
    default:
      break;
    }
  }
}

inline std::unique_ptr<LogicBlock>
getZ3LogicBlock(bool& success, bool convertWhenAssert,
                const Params& params = Params()) {
  auto c = std::make_shared<z3::context>();
  auto slv = std::make_shared<z3::solver>(*c);
  z3::params p(*c);
  setZ3Params(p, params);
  slv->set(p);
  success = true;
  return std::make_unique<z3logic::Z3LogicBlock>(c, slv, convertWhenAssert);
}

inline std::unique_ptr<LogicBlockOptimizer>
getZ3LogicOptimizer(bool& success, bool convertWhenAssert,
                    const Params& params = Params()) {
  auto c = std::make_shared<z3::context>();
  auto opt = std::make_shared<z3::optimize>(*c);
  z3::params p(*c);
  setZ3Params(p, params);
  opt->set(p);
  success = true;
  return std::make_unique<z3logic::Z3LogicOptimizer>(c, opt, convertWhenAssert);
}

} // namespace logicutil
