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

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace encodings {

using namespace logicbase;

struct NestedVar {
  explicit NestedVar(const LogicTerm& v) : var(v) {};
  NestedVar(const LogicTerm& v, std::vector<NestedVar> l)
      : var(v), list(std::move(l)) {}
  LogicTerm var = LogicTerm::noneTerm();
  std::vector<NestedVar> list;
};

struct WeightedVar {
  WeightedVar(const LogicTerm& v, const int w) : var(v), weight(w) {}
  LogicTerm var = LogicTerm::noneTerm();
  int weight = 0;
};
inline bool operator<(const WeightedVar& rhs, const WeightedVar& lhs) {
  return rhs.weight < lhs.weight;
}
inline bool operator==(const WeightedVar& rhs, const WeightedVar& lhs) {
  return rhs.weight == lhs.weight && rhs.var.getID() == lhs.var.getID();
}

enum class Type : uint8_t { Uninitialized, AuxVar, ProgramVar };
struct SavedLit {
  SavedLit() : var(LogicTerm::noneTerm()) {}
  SavedLit(Type t, const LogicTerm& v) : type(t), var(v) {}
  Type type = Type::Uninitialized;
  LogicTerm var = LogicTerm::noneTerm();
};

LogicTerm atMostOneCmdr(const std::vector<NestedVar>& subords,
                        const LogicTerm& cmdrVar, LogicBlock* logic);

LogicTerm exactlyOneCmdr(const std::vector<NestedVar>& subords,
                         const LogicTerm& cmdrVar, LogicBlock* logic);

LogicTerm naiveExactlyOne(const std::vector<LogicTerm>& clauseVars);

LogicTerm naiveAtMostOne(const std::vector<LogicTerm>& clauseVars);

LogicTerm naiveAtLeastOne(const std::vector<LogicTerm>& clauseVars);

LogicTerm atMostOneBiMander(const std::vector<LogicTerm>& vars,
                            LogicBlock* logic);

std::vector<NestedVar> groupVars(const std::vector<LogicTerm>& vars,
                                 std::size_t maxSize);
std::vector<NestedVar> groupVarsAux(const std::vector<NestedVar>& vars,
                                    std::size_t maxSize);

std::vector<std::vector<LogicTerm>>
groupVarsBimander(const std::vector<LogicTerm>& vars, std::size_t groupCount);
} // namespace encodings
