/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "Encodings.hpp"

#include "Logic.hpp"
#include "LogicBlock.hpp"
#include "LogicTerm.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string>
#include <vector>

namespace encodings {

LogicTerm naiveExactlyOne(const std::vector<LogicTerm>& clauseVars) {
  return naiveAtLeastOne(clauseVars) && naiveAtMostOne(clauseVars);
}

LogicTerm naiveAtLeastOne(const std::vector<LogicTerm>& clauseVars) {
  LogicTerm naiveAtLeastOne = LogicTerm(false);
  for (const LogicTerm& x : clauseVars) {
    naiveAtLeastOne = naiveAtLeastOne || x;
  }
  return naiveAtLeastOne;
}

LogicTerm naiveAtMostOne(const std::vector<LogicTerm>& clauseVars) {
  LogicTerm naiveAtMostOne = LogicTerm(true);
  for (std::size_t i = 0U; i < clauseVars.size() - 1; i++) {
    for (std::size_t j = i + 1U; j < clauseVars.size(); j++) {
      naiveAtMostOne = naiveAtMostOne && (!clauseVars[i] || !clauseVars[j]);
    }
  }
  return naiveAtMostOne;
}

LogicTerm atMostOneBiMander(const std::vector<LogicTerm>& vars,
                            LogicBlock* logic) {
  auto subords = groupVarsBimander(vars, vars.size() / 2);
  LogicTerm ret = LogicTerm(true);
  std::vector<LogicTerm> binaryVars{};
  auto m = subords.size();
  binaryVars.reserve(static_cast<size_t>(std::ceil(std::log2(m))));
  for (int32_t j = 0; j < std::ceil(std::log2(m)); j++) {
    binaryVars.emplace_back(
        logic->makeVariable("binary_var_" + std::to_string(j)));
  }
  for (std::size_t i = 0U; i < m; i++) {
    LogicTerm binary = LogicTerm(true);
    for (std::size_t h = 0U; h < subords[i].size(); h++) {
      LogicTerm b2 = LogicTerm(true);
      for (std::size_t j = 0U;
           j < static_cast<std::size_t>(std::ceil(std::log2(m))); j++) {
        if ((i & 1U << j) != 0U) {
          b2 = b2 && (!subords[i][h] || binaryVars[j]);
        } else {
          b2 = b2 && (!subords[i][h] || !binaryVars[j]);
        }
      }
      binary = binary && b2;
    }
    ret = ret && binary && naiveAtMostOne(vars);
  }
  return ret;
}

LogicTerm exactlyOneCmdr(const std::vector<NestedVar>& subords,
                         const LogicTerm& cmdrVar, LogicBlock* logic) {
  auto ret = LogicTerm(true);
  std::vector<LogicTerm> clauseVars{};
  clauseVars.reserve(subords.size());
  for (const auto& it : subords) {
    if (it.var.getOpType() != OpType::None) {
      clauseVars.emplace_back(it.var);
    } else {
      LogicTerm const localCdr = logic->makeVariable("cdr_var");
      clauseVars.emplace_back(localCdr);
      ret = ret && exactlyOneCmdr(it.list, localCdr, logic);
    }
  }
  if (cmdrVar.getOpType() == OpType::Variable) {
    clauseVars.emplace_back(!cmdrVar);
  }
  return ret && naiveExactlyOne(clauseVars);
}

LogicTerm atMostOneCmdr(const std::vector<NestedVar>& subords,
                        const LogicTerm& cmdrVar, LogicBlock* logic) {
  auto ret = LogicTerm(true);
  std::vector<LogicTerm> clauseVars;
  clauseVars.reserve(subords.size());
  for (const auto& it : subords) {
    if (it.var.getOpType() != OpType::None) {
      clauseVars.emplace_back(it.var);
    } else {
      LogicTerm const localCdr = logic->makeVariable("cdr_var");
      clauseVars.emplace_back(localCdr);
      ret = ret && atMostOneCmdr(it.list, localCdr, logic);
    }
  }
  if (cmdrVar.getOpType() == OpType::Variable) {
    clauseVars.emplace_back(!cmdrVar);
  }
  return ret && naiveAtMostOne(clauseVars);
}

std::vector<NestedVar> groupVars(const std::vector<LogicTerm>& vars,
                                 std::size_t maxSize) {
  std::vector<NestedVar> vVars;
  vVars.reserve(vars.size());
  for (const auto& var : vars) {
    vVars.emplace_back(var);
  }
  if (vVars.size() < 6U) {
    return vVars;
  }
  return groupVarsAux(vVars, maxSize);
}

std::vector<NestedVar> groupVarsAux(const std::vector<NestedVar>& vars,
                                    std::size_t maxSize) {
  auto numVars = vars.size();
  if (numVars <= maxSize) {
    return vars;
  }
  std::vector<NestedVar> ret{};
  const std::size_t numGr = numVars / maxSize;
  ret.reserve(numGr);
  auto to = vars.begin();
  for (std::size_t i = 0U; i < numGr; i++) {
    const auto from = to;
    if (i == numGr - 1U) {
      to = vars.end();
    } else {
      std::advance(to, maxSize);
    }
    ret.emplace_back(LogicTerm::noneTerm(), std::vector<NestedVar>(from, to));
  }
  return groupVarsAux(ret, maxSize);
}

std::vector<std::vector<LogicTerm>>
groupVarsBimander(const std::vector<LogicTerm>& vars, std::size_t groupCount) {
  std::vector<std::vector<LogicTerm>> result{};
  auto chunkSize = vars.size() / groupCount;

  for (size_t i = 0U; i < vars.size(); i += chunkSize) {
    auto from = vars.begin();
    std::advance(from, static_cast<int64_t>(i));
    auto to = vars.begin();
    auto end = std::min(vars.size(), i + chunkSize);
    std::advance(to, static_cast<int64_t>(end));
    result.emplace_back(from, to);
  }

  return result;
}
} // namespace encodings
