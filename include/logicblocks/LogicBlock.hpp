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

#include "Logic.hpp"
#include "LogicTerm.hpp"

#include <cstdint>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace logicbase {
class Model;

class LogicBlock : public Logic {
protected:
  std::set<LogicTerm, TermDepthComparator> clauses;
  Model* model{};
  bool convertWhenAssert;
  virtual void internalReset() = 0;
  uint64_t gid = 0U;

public:
  explicit LogicBlock(bool convert = false) : convertWhenAssert(convert) {}

  uint64_t getNextId() override { return gid++; };
  uint64_t getId() override { return gid; };

  Model* getModel() { return model; }

  virtual void assertFormula(const LogicTerm& a);

  LogicTerm makeVariable(const std::string& name, CType type = CType::BOOL,
                         uint16_t bvSize = 32U);

  virtual void produceInstance() = 0;
  virtual Result solve() = 0;
  virtual void reset();

  virtual std::string dumpInternalSolver() { return ""; }
};

class LogicBlockOptimizer : public LogicBlock {
protected:
  std::vector<std::pair<LogicTerm, double>> weightedTerms;

public:
  explicit LogicBlockOptimizer(bool convert) : LogicBlock(convert) {}
  void weightedTerm(const LogicTerm& a, double weight);
  virtual bool makeMinimize() = 0;
  virtual bool makeMaximize() = 0;
  virtual bool maximize(const LogicTerm& term) = 0;
  virtual bool minimize(const LogicTerm& term) = 0;
  void reset() override;
};
} // namespace logicbase
