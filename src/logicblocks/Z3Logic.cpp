/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "Z3Logic.hpp"

#include "Logic.hpp"
#include "LogicTerm.hpp"
#include "Z3Model.hpp"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <plog/Log.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <z3++.h>

namespace z3logic {
namespace {
CType extractNumberType(
    const std::vector<LogicTerm>&
        terms) { // TODO check if all terms are numbers, handle BV
  CType res = CType::INT;
  for (const LogicTerm& it : terms) {
    if (it.getCType() == CType::REAL) {
      res = CType::REAL;
      break;
    }
    if (it.getCType() == CType::BITVECTOR) {
      res = CType::BITVECTOR;
      break;
    }
  }
  return res;
}
} // namespace

z3::expr Z3Base::getExprTerm(const uint64_t id, const CType type,
                             Z3Base* z3base) {
  if (!z3base->variables.contains(id) ||
      !z3base->variables.at(id)[static_cast<size_t>(type)].first) {
    const auto* const msg = "Variable not found";
    PLOG_FATAL << msg;
    throw std::runtime_error(msg);
  }
  return z3base->variables.at(id)[static_cast<size_t>(type)].second;
}

z3::expr Z3Base::convert(const LogicTerm& a, CType toType) {
  // Short-circuit for constants
  if (a.getOpType() == OpType::Constant) {
    return convertConstant(a, toType);
  }
  std::vector<std::pair<bool, z3::expr>> v;

  // First, try to find the expression in the cache
  if (cache.contains(a)) {
    v = cache.at(a);
    if (v[static_cast<size_t>(toType)].first) {
      return v[static_cast<size_t>(toType)].second;
    }
  } else {
    for (int32_t i = 0; i < 4; i++) {
      v.emplace_back(false, ctx->bool_val(false));
    }
  }

  // If the expression is not in the cache, convert it
  switch (a.getOpType()) {
  case OpType::Variable: {
    v[static_cast<size_t>(toType)].second = convertVariableTo(a, toType);
  } break;

  case OpType::AND: {
    z3::expr s = this->ctx->bool_val(true);
    bool alternate = false;
    for (const LogicTerm& lt : a.getNodes()) {
      if (alternate) {
        s = s && convert(lt, toType);
      } else {
        s = convert(lt, toType) && s;
      }
      alternate = !alternate;
    }
    v[static_cast<size_t>(toType)].second = s.simplify();
  } break;
  case OpType::OR: {
    z3::expr s = this->ctx->bool_val(false);
    bool alternate = false;
    for (const LogicTerm& lt : a.getNodes()) {
      if (alternate) {
        s = s || convert(lt, toType);
      } else {
        s = convert(lt, toType) || s;
      }
      alternate = !alternate;
    }
    v[static_cast<size_t>(toType)].second = s.simplify();
  } break;
  case OpType::EQ:
    v[static_cast<size_t>(toType)].second = convertOperator(
        a.getNodes()[0], a.getNodes()[1], z3::operator==, CType::ERRORTYPE);
    break;
  case OpType::XOR:
    v[static_cast<size_t>(toType)].second = convertOperator(
        a.getNodes()[0], a.getNodes()[1], z3::operator!=, CType::ERRORTYPE);
    break;
  case OpType::NEG:
    v[static_cast<size_t>(toType)].second =
        convertOperator(a.getNodes()[0], z3::operator!, CType::ERRORTYPE);
    break;
  case OpType::ITE:
    v[static_cast<size_t>(toType)].second =
        convertOperator(a.getNodes()[0], a.getNodes()[1], a.getNodes()[2],
                        z3::ite, CType::ERRORTYPE);
    break;
  case OpType::IMPL:
    v[static_cast<size_t>(toType)].second = convertOperator(
        a.getNodes()[0], a.getNodes()[1], z3::implies, CType::BOOL);
    break;
  case OpType::ADD: {
    v[static_cast<size_t>(toType)].second = convertOperator(
        a.getNodes(), z3::operator+, extractNumberType(a.getNodes()));
  } break;
  case OpType::SUB: {
    v[static_cast<size_t>(toType)].second = convertOperator(
        a.getNodes(), z3::operator-, extractNumberType(a.getNodes()));
  } break;
  case OpType::MUL: {
    v[static_cast<size_t>(toType)].second = convertOperator(
        a.getNodes(), z3::operator*, extractNumberType(a.getNodes()));
  } break;
  case OpType::DIV: {
    v[static_cast<size_t>(toType)].second =
        convertOperator(a.getNodes()[0], a.getNodes()[1], z3::operator/,
                        extractNumberType(a.getNodes()));
  } break;
  case OpType::GT: {
    v[static_cast<size_t>(toType)].second =
        convertOperator(a.getNodes()[0], a.getNodes()[1], z3::operator>,
                        extractNumberType(a.getNodes()));
  } break;
  case OpType::LT: {
    v[static_cast<size_t>(toType)].second =
        convertOperator(a.getNodes()[0], a.getNodes()[1], z3::operator<,
                        extractNumberType(a.getNodes()));
  } break;
  case OpType::GTE: {
    v[static_cast<size_t>(toType)].second =
        convertOperator(a.getNodes()[0], a.getNodes()[1], z3::operator>=,
                        extractNumberType(a.getNodes()));
  } break;
  case OpType::LTE: {
    v[static_cast<size_t>(toType)].second =
        convertOperator(a.getNodes()[0], a.getNodes()[1], z3::operator<=,
                        extractNumberType(a.getNodes()));
  } break;
  case OpType::BitAnd: {
    v[static_cast<size_t>(toType)].second = convertOperator(
        a.getNodes(), z3::operator&, extractNumberType(a.getNodes()));
  } break;
  case OpType::BitOr: {
    v[static_cast<size_t>(toType)].second = convertOperator(
        a.getNodes(), z3::operator|, extractNumberType(a.getNodes()));
  } break;
  case OpType::BitXor: {
    v[static_cast<size_t>(toType)].second = convertOperator(
        a.getNodes(), z3::operator^, extractNumberType(a.getNodes()));
  } break;
  case OpType::BitEq: {
    v[static_cast<size_t>(toType)].second =
        convertOperator(a.getNodes()[0], a.getNodes()[1], z3::operator==,
                        extractNumberType(a.getNodes()));
  } break;
  default:
    const auto* const msg = "Unsupported operation";
    PLOG_FATAL << msg;
    throw std::runtime_error(msg);
  }

  // Cache the result and return it
  v[static_cast<size_t>(toType)].first = true;
  cache.insert_or_assign(a, v);
  return cache.at(a)[static_cast<size_t>(toType)].second;
}

void Z3LogicBlock::assertFormula(const LogicTerm& a) {
  if (a.getOpType() == OpType::AND) {
    for (const auto& clause : a.getNodes()) {
      clauses.insert(clause);
      if (convertWhenAssert) {
        this->solver->add(convert(clause, CType::BOOL).simplify());
      }
    }
  } else {
    clauses.insert(a);
    if (convertWhenAssert) {
      this->solver->add(convert(a, CType::BOOL).simplify());
    }
  }
}

void Z3LogicBlock::produceInstance() {
  for (const auto& clause : clauses) {
    solver->add(convert(clause, CType::BOOL).simplify());
  }
}

Result Z3LogicBlock::solve() {
  produceInstance();
  const auto res = solver->check();
  if (res == z3::sat) {
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
    model = new Z3Model(ctx, std::make_shared<z3::model>(solver->get_model()));
    return Result::SAT;
  }
  return Result::UNSAT;
}

void Z3LogicBlock::internalReset() {
  variables.clear();
  cache.clear();
  solver->reset();
  Z3Base::variables.clear();
}

z3::expr Z3Base::convertVariableTo(const LogicTerm& a, CType toType) {
  std::vector<std::pair<bool, z3::expr>> v;
  if (variables.contains(a.getID())) {
    v = variables.at(a.getID());
    if (v[static_cast<size_t>(toType)].first) {
      return v[static_cast<size_t>(toType)].second;
    }
  } else {
    for (int32_t i = 0; i < 4; i++) {
      v.emplace_back(false, ctx->bool_val(false));
    }
  }
  v[static_cast<size_t>(toType)].first = true;
  switch (a.getCType()) {
  case CType::BOOL:
    v[static_cast<size_t>(toType)].second =
        convertVariableFromBoolTo(a, toType);
    break;
  case CType::INT:
    v[static_cast<size_t>(toType)].second = convertVariableFromIntTo(a, toType);
    break;
  case CType::REAL:
    v[static_cast<size_t>(toType)].second =
        convertVariableFromRealTo(a, toType);
    break;
  case CType::BITVECTOR:
    v[static_cast<size_t>(toType)].second =
        convertVariableFromBitvectorTo(a, toType);
    break;
  default:
    const auto* const msg = "Unsupported type";
    PLOG_FATAL << msg;
    throw std::runtime_error(msg);
  }
  variables.insert_or_assign(a.getID(), v);
  return v[static_cast<size_t>(toType)].second;
}
z3::expr Z3Base::convertVariableFromBoolTo(const LogicTerm& a, CType toType) {
  std::stringstream ss;
  ss << a.getName() << "_" << a.getID();
  switch (toType) {
  case CType::BOOL:
    return ctx->bool_const(ss.str().c_str());
  case CType::INT:
    return z3::ite(ctx->bool_const(ss.str().c_str()), ctx->int_val(1),
                   ctx->int_val(0));
  case CType::REAL:
    return z3::ite(ctx->bool_const(ss.str().c_str()), ctx->real_val(1),
                   ctx->real_val(0));
  case CType::BITVECTOR:
    return ite(ctx->bool_const(ss.str().c_str()), ctx->bv_val(1, 32),
               ctx->bv_val(0, 32));
  default:
    const auto* const msg = "Unsupported type";
    PLOG_FATAL << msg;
    throw std::runtime_error(msg);
  }
}
z3::expr Z3Base::convertVariableFromIntTo(const LogicTerm& a, CType toType) {
  std::stringstream ss;
  ss << a.getName() << "_" << a.getID();
  switch (toType) {
  case CType::BOOL:
    return ctx->int_const(ss.str().c_str()) != 0;
  case CType::INT:
  case CType::REAL:
    return ctx->int_const(ss.str().c_str());
  case CType::BITVECTOR:
    return z3::int2bv(32U, ctx->int_const(ss.str().c_str()));
  default:
    const auto* const msg = "Unsupported type";
    PLOG_FATAL << msg;
    throw std::runtime_error(msg);
  }
}
z3::expr Z3Base::convertVariableFromRealTo(const LogicTerm& a, CType toType) {
  std::stringstream ss;
  ss << a.getName() << "_" << a.getID();
  switch (toType) {
  case CType::BOOL:
    return ctx->real_const(ss.str().c_str()) != 0;
  case CType::INT:
  case CType::REAL:
    return ctx->real_const(ss.str().c_str());
  case CType::BITVECTOR:
    return z3::int2bv(32U, z3::round_fpa_to_closest_integer(
                               ctx->real_const(ss.str().c_str())));
  default:
    const auto* const msg = "Unsupported type";
    PLOG_FATAL << msg;
    throw std::runtime_error(msg);
  }
}
z3::expr Z3Base::convertVariableFromBitvectorTo(const LogicTerm& a,
                                                CType toType) {
  std::stringstream ss;
  ss << a.getName() << "_" << a.getID();
  switch (toType) {
  case CType::BOOL:
    return ctx->bv_const(ss.str().c_str(), 32U) != 0;
  case CType::INT:
  case CType::REAL:
    return z3::bv2int(ctx->bv_const(ss.str().c_str(), 32U), false);
  case CType::BITVECTOR:
    return ctx->bv_const(ss.str().c_str(), a.getBitVectorSize());
  default:
    const auto* const msg = "Unsupported type";
    PLOG_FATAL << msg;
    throw std::runtime_error(msg);
  }
}

z3::expr Z3Base::convertOperator(const LogicTerm& a,
                                 z3::expr (*op)(const z3::expr&),
                                 CType toType) {
  if (toType == CType::ERRORTYPE) {
    toType = a.getCType();
  }
  return op(convert(a, toType));
}

z3::expr Z3Base::convertOperator(const LogicTerm& a, const LogicTerm& b,
                                 z3::expr (*op)(const z3::expr&,
                                                const z3::expr&),
                                 CType toType) {
  if (toType == CType::ERRORTYPE) {
    toType = LogicTerm::getTargetCType(a, b, OpType::None);
  }
  return op(convert(a, toType), convert(b, toType));
}
z3::expr Z3Base::convertOperator(
    const LogicTerm& a, const LogicTerm& b, const LogicTerm& c,
    z3::expr (*op)(const z3::expr&, const z3::expr&, const z3::expr&),
    CType toType) {
  toType = LogicTerm::getTargetCType(a, b, OpType::None);
  toType = LogicTerm::getTargetCType(toType, c);
  return op(convert(a, CType::BOOL), convert(b, toType), convert(c, toType));
}

z3::expr Z3Base::convertOperator(const std::vector<LogicTerm>& terms,
                                 z3::expr (*op)(const z3::expr&,
                                                const z3::expr&),
                                 CType toType) {
  z3::expr res = convert(static_cast<LogicTerm>(*terms.begin()), toType);
  for (auto it = (terms.begin() + 1); it != terms.end(); ++it) {
    res = op(res, convert(static_cast<LogicTerm>(*it), toType));
  }
  return res;
}

z3::expr Z3Base::convertConstant(const LogicTerm& a, CType toType) {
  switch (toType) {
  case logicbase::CType::BOOL:
    return ctx->bool_val(a.getBoolValue());
  case logicbase::CType::INT:
    return ctx->int_val(a.getIntValue());
  case logicbase::CType::REAL:
    return ctx->real_val(std::to_string(a.getFloatValue()).c_str());
  case logicbase::CType::BITVECTOR:
    return ctx->bv_val(a.getBitVectorValue(), a.getBitVectorSize());
  default:
    const auto* const msg = "Unsupported type";
    PLOG_FATAL << msg;
    throw std::runtime_error(msg);
  }
}

bool Z3LogicOptimizer::makeMinimize() {
  for (const auto& [term, weight] : weightedTerms) {
    optimizer->add(convert(LogicTerm::neg(term), CType::BOOL).simplify(),
                   static_cast<unsigned>(weight));
  }
  return false;
}

bool Z3LogicOptimizer::makeMaximize() {
  for (const auto& [term, weight] : weightedTerms) {
    optimizer->add(convert(term, CType::BOOL).simplify(),
                   static_cast<unsigned>(weight));
  }
  return false;
}

bool Z3LogicOptimizer::maximize(const LogicTerm& term) {
  optimizer->maximize(convert(term, CType::REAL).simplify());
  return true;
}
bool Z3LogicOptimizer::minimize(const LogicTerm& term) {
  optimizer->minimize(convert(term, CType::REAL).simplify());
  return true;
}

void Z3LogicOptimizer::assertFormula(const LogicTerm& a) {
  if (a.getOpType() == OpType::AND) {
    for (const auto& clause : a.getNodes()) {
      clauses.insert(clause);
      if (convertWhenAssert) {
        optimizer->add(convert(clause, CType::BOOL).simplify());
      }
    }
  } else {
    clauses.insert(a);
    if (convertWhenAssert) {
      optimizer->add(convert(a, CType::BOOL).simplify());
    }
  }
}

void Z3LogicOptimizer::produceInstance() {
  for (const auto& clause : clauses) {
    optimizer->add(convert(clause, CType::BOOL).simplify());
  }
}

Result Z3LogicOptimizer::solve() {
  produceInstance();
  const auto res = optimizer->check();
  if (res == z3::sat) {
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
    model =
        new Z3Model(ctx, std::make_shared<z3::model>(optimizer->get_model()));
    return Result::SAT;
  }
  return Result::UNSAT;
}

void Z3LogicOptimizer::internalReset() {
  weightedTerms.clear();
  variables.clear();
  cache.clear();
  Z3Base::variables.clear();
  optimizer = std::make_shared<z3::optimize>(*ctx);
}

} // namespace z3logic
