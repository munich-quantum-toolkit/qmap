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
#include "LogicTerm.hpp"
#include "Model.hpp"
#include "Z3Logic.hpp"

#include <cstddef>
#include <gtest/gtest.h>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include <z3++.h>

using namespace logicbase;

class TestZ3 : public testing::TestWithParam<logicbase::OpType> {
protected:
  void SetUp() override {}

  std::shared_ptr<z3::context> ctx = std::make_shared<z3::context>();
  std::shared_ptr<z3::solver> solver = std::make_shared<z3::solver>(*ctx);
};
TEST_F(TestZ3, ConstructDestruct) {
  auto const z3logic =
      std::make_unique<z3logic::Z3LogicBlock>(ctx, solver, false);

  auto const t = LogicTerm("x", CType::BOOL);
}

TEST_F(TestZ3, SimpleTrue) {
  z3logic::Z3LogicBlock z3logic(ctx, solver, true);

  LogicTerm a = z3logic.makeVariable("a", CType::BOOL);
  LogicTerm b = z3logic.makeVariable("b", CType::BOOL);
  LogicTerm c = z3logic.makeVariable("c", CType::BOOL);
  z3logic.assertFormula(a && b);
  z3logic.produceInstance();
  EXPECT_EQ(z3logic.solve(), Result::SAT);
  EXPECT_EQ(a.getMaxChildrenDepth(), 1);
  z3logic.reset();

  a = z3logic.makeVariable("a", CType::BOOL);
  b = z3logic.makeVariable("b", CType::BOOL);
  z3logic.assertFormula(a || b);
  z3logic.produceInstance();
  EXPECT_EQ(z3logic.solve(), Result::SAT);
  z3logic.reset();

  a = z3logic.makeVariable("a", CType::BOOL);
  b = z3logic.makeVariable("b", CType::BOOL);
  z3logic.assertFormula(a == b);
  z3logic.produceInstance();
  EXPECT_EQ(z3logic.solve(), Result::SAT);
  z3logic.reset();

  a = z3logic.makeVariable("a", CType::BOOL);
  b = z3logic.makeVariable("b", CType::BOOL);
  z3logic.assertFormula(a != b);
  z3logic.produceInstance();
  EXPECT_EQ(z3logic.solve(), Result::SAT);
  z3logic.reset();

  a = z3logic.makeVariable("a", CType::BOOL);
  b = z3logic.makeVariable("b", CType::BOOL);
  z3logic.assertFormula(a && !b);
  z3logic.produceInstance();
  EXPECT_EQ(z3logic.solve(), Result::SAT);
  z3logic.reset();

  a = z3logic.makeVariable("a", CType::BOOL);
  b = z3logic.makeVariable("b", CType::BOOL);
  z3logic.assertFormula(!a || !b);
  z3logic.produceInstance();
  EXPECT_EQ(z3logic.solve(), Result::SAT);
  z3logic.reset();

  a = z3logic.makeVariable("a", CType::BOOL);
  b = z3logic.makeVariable("b", CType::BOOL);
  z3logic.assertFormula(LogicTerm::implies(a, b));
  z3logic.produceInstance();
  EXPECT_EQ(z3logic.solve(), Result::SAT);
  z3logic.reset();

  a = z3logic.makeVariable("a", CType::BOOL);
  z3logic.assertFormula(a);
  z3logic.produceInstance();
  EXPECT_EQ(z3logic.solve(), Result::SAT);
  z3logic.reset();

  b = z3logic.makeVariable("b", CType::BOOL);
  z3logic.assertFormula(b);
  z3logic.produceInstance();

  EXPECT_EQ(z3logic.solve(), Result::SAT);
  z3logic.reset();

  a = z3logic.makeVariable("a", CType::BOOL);
  b = z3logic.makeVariable("b", CType::BOOL);
  c = z3logic.makeVariable("c", CType::BOOL);
  z3logic.assertFormula(a && b && c);
  z3logic.produceInstance();
  EXPECT_EQ(z3logic.solve(), Result::SAT);
  z3logic.reset();

  a = z3logic.makeVariable("a", CType::BOOL);
  b = z3logic.makeVariable("b", CType::BOOL);
  c = z3logic.makeVariable("c", CType::BOOL);
  LogicTerm const d = z3logic.makeVariable("d", CType::BOOL);
  z3logic.assertFormula((a && b) || (c && d));
  z3logic.produceInstance();
  EXPECT_EQ(z3logic.solve(), Result::SAT);
  z3logic.reset();
}

TEST_F(TestZ3, SimpleFalse) {
  z3logic::Z3LogicBlock z3logic(ctx, solver, false);

  LogicTerm a = z3logic.makeVariable("a", CType::BOOL);
  LogicTerm b = z3logic.makeVariable("b", CType::BOOL);
  z3logic.assertFormula(!a);
  z3logic.assertFormula(a);
  z3logic.produceInstance();
  EXPECT_EQ(z3logic.solve(), Result::UNSAT);
  z3logic.reset();

  b = z3logic.makeVariable("b", CType::BOOL);
  z3logic.assertFormula(!b);
  z3logic.assertFormula(b);
  z3logic.produceInstance();
  EXPECT_EQ(z3logic.solve(), Result::UNSAT);
  z3logic.reset();

  a = z3logic.makeVariable("a", CType::BOOL);
  b = z3logic.makeVariable("b", CType::BOOL);
  z3logic.assertFormula(!a);
  z3logic.assertFormula(b);
  z3logic.assertFormula(a == b);
  z3logic.produceInstance();
  EXPECT_EQ(z3logic.solve(), Result::UNSAT);
  z3logic.reset();

  a = z3logic.makeVariable("a", CType::BOOL);
  b = z3logic.makeVariable("b", CType::BOOL);
  z3logic.assertFormula(a);
  z3logic.assertFormula(!b);
  z3logic.assertFormula(a == b);
  z3logic.produceInstance();
  EXPECT_EQ(z3logic.solve(), Result::UNSAT);
  z3logic.reset();

  a = z3logic.makeVariable("a", CType::BOOL);
  b = z3logic.makeVariable("b", CType::BOOL);
  z3logic.assertFormula(!a);
  z3logic.assertFormula(b);
  z3logic.assertFormula(a == b);
  z3logic.produceInstance();
  EXPECT_EQ(z3logic.solve(), Result::UNSAT);
  z3logic.reset();

  a = z3logic.makeVariable("a", CType::BOOL);
  b = z3logic.makeVariable("b", CType::BOOL);
  z3logic.assertFormula(a);
  z3logic.assertFormula(b);
  z3logic.assertFormula(a != b);
  z3logic.produceInstance();
  EXPECT_EQ(z3logic.solve(), Result::UNSAT);
  z3logic.reset();

  a = z3logic.makeVariable("a", CType::BOOL);
  b = z3logic.makeVariable("b", CType::BOOL);
  z3logic.assertFormula(!a);
  z3logic.assertFormula(!b);
  z3logic.assertFormula(a != b);
  z3logic.produceInstance();
  EXPECT_EQ(z3logic.solve(), Result::UNSAT);
  z3logic.reset();

  a = z3logic.makeVariable("a", CType::BOOL);
  b = z3logic.makeVariable("b", CType::BOOL);
  z3logic.assertFormula(!a);
  z3logic.assertFormula(!b);
  z3logic.assertFormula(a && !b);
  z3logic.produceInstance();
  EXPECT_EQ(z3logic.solve(), Result::UNSAT);
  z3logic.reset();

  a = z3logic.makeVariable("a", CType::BOOL);
  b = z3logic.makeVariable("b", CType::BOOL);
  z3logic.assertFormula(a);
  z3logic.assertFormula(b);
  z3logic.assertFormula(a && !b);
  z3logic.produceInstance();
  EXPECT_EQ(z3logic.solve(), Result::UNSAT);
  z3logic.reset();

  a = z3logic.makeVariable("a", CType::BOOL);
  b = z3logic.makeVariable("b", CType::BOOL);
  z3logic.assertFormula(a);
  z3logic.assertFormula(!b);
  z3logic.assertFormula(LogicTerm::implies(a, b));
  z3logic.produceInstance();
  EXPECT_EQ(z3logic.solve(), Result::UNSAT);
  z3logic.reset();
}

TEST_F(TestZ3, IntBase) {
  z3logic::Z3LogicBlock z3logic(ctx, solver, false);

  LogicTerm a = z3logic.makeVariable("a", CType::INT);
  LogicTerm b = z3logic.makeVariable("b", CType::INT);
  LogicTerm c = z3logic.makeVariable("c", CType::INT);
  z3logic.assertFormula(a + b == c);
  z3logic.produceInstance();
  EXPECT_EQ(z3logic.solve(), Result::SAT);
  z3logic.reset();

  a = z3logic.makeVariable("a", CType::INT);
  b = z3logic.makeVariable("b", CType::INT);
  c = z3logic.makeVariable("c", CType::INT);
  z3logic.assertFormula(a - b == c);
  z3logic.produceInstance();
  EXPECT_EQ(z3logic.solve(), Result::SAT);
  z3logic.reset();

  a = z3logic.makeVariable("a", CType::INT);
  b = z3logic.makeVariable("b", CType::INT);
  c = z3logic.makeVariable("c", CType::INT);
  z3logic.assertFormula(a * b == c);
  z3logic.produceInstance();
  EXPECT_EQ(z3logic.solve(), Result::SAT);
  z3logic.reset();

  a = z3logic.makeVariable("a", CType::INT);
  b = z3logic.makeVariable("b", CType::INT);
  c = z3logic.makeVariable("c", CType::INT);
  z3logic.assertFormula(a / b == c);
  z3logic.produceInstance();
  EXPECT_EQ(z3logic.solve(), Result::SAT);
  z3logic.reset();

  a = z3logic.makeVariable("a", CType::INT);
  b = z3logic.makeVariable("b", CType::INT);
  z3logic.assertFormula(a > b);
  z3logic.produceInstance();
  EXPECT_EQ(z3logic.solve(), Result::SAT);
  z3logic.reset();

  a = z3logic.makeVariable("a", CType::INT);
  c = z3logic.makeVariable("c", CType::INT);
  z3logic.assertFormula(a < c);
  z3logic.produceInstance();
  EXPECT_EQ(z3logic.solve(), Result::SAT);
  z3logic.reset();

  a = z3logic.makeVariable("a", CType::INT);
  b = z3logic.makeVariable("b", CType::INT);
  z3logic.assertFormula(a >= b);
  z3logic.produceInstance();
  EXPECT_EQ(z3logic.solve(), Result::SAT);
  z3logic.reset();

  a = z3logic.makeVariable("a", CType::INT);
  c = z3logic.makeVariable("c", CType::INT);
  z3logic.assertFormula(a <= c);
  z3logic.produceInstance();
  EXPECT_EQ(z3logic.solve(), Result::SAT);
  z3logic.reset();
}
TEST_F(TestZ3, IntNumbers) {
  z3logic::Z3LogicBlock z3logic(ctx, solver, false);

  LogicTerm a = z3logic.makeVariable("a", CType::INT);
  LogicTerm b = z3logic.makeVariable("b", CType::INT);
  LogicTerm c = z3logic.makeVariable("c", CType::INT);
  z3logic.assertFormula(a == LogicTerm(3));
  z3logic.assertFormula(b == LogicTerm(2));
  z3logic.assertFormula(c == LogicTerm(1));
  z3logic.assertFormula(a - b == c);
  z3logic.produceInstance();
  EXPECT_EQ(z3logic.solve(), Result::SAT);
  z3logic.reset();

  a = z3logic.makeVariable("a", CType::INT);
  b = z3logic.makeVariable("b", CType::INT);
  c = z3logic.makeVariable("c", CType::INT);
  z3logic.assertFormula(a == LogicTerm(3));
  z3logic.assertFormula(b == LogicTerm(2));
  z3logic.assertFormula(c == LogicTerm(1));
  z3logic.assertFormula(c + b == a);
  z3logic.produceInstance();
  EXPECT_EQ(z3logic.solve(), Result::SAT);
  z3logic.reset();

  a = z3logic.makeVariable("a", CType::INT);
  b = z3logic.makeVariable("b", CType::INT);
  c = z3logic.makeVariable("c", CType::INT);
  z3logic.assertFormula(a == LogicTerm(3));
  z3logic.assertFormula(b == LogicTerm(2));
  z3logic.assertFormula(c == LogicTerm(1));
  z3logic.assertFormula((a > b) == LogicTerm(true));
  z3logic.assertFormula((b > c) == LogicTerm(true));
  z3logic.produceInstance();
  EXPECT_EQ(z3logic.solve(), Result::SAT);
  z3logic.reset();

  a = z3logic.makeVariable("a", CType::INT);
  b = z3logic.makeVariable("b", CType::INT);
  c = z3logic.makeVariable("c", CType::INT);
  z3logic.assertFormula(a == LogicTerm(3));
  z3logic.assertFormula(b == LogicTerm(2));
  z3logic.assertFormula(c == LogicTerm(1));
  z3logic.assertFormula((c < a) == LogicTerm(true));
  z3logic.assertFormula((a < LogicTerm(4)) == LogicTerm(true));
  z3logic.produceInstance();
  EXPECT_EQ(z3logic.solve(), Result::SAT);
  z3logic.reset();

  LogicTerm boolA = z3logic.makeVariable("bool_a", CType::BOOL);
  a = z3logic.makeVariable("a", CType::INT);
  b = z3logic.makeVariable("b", CType::INT);
  c = z3logic.makeVariable("c", CType::INT);
  z3logic.assertFormula(a == LogicTerm(3));
  z3logic.assertFormula(b == LogicTerm(2));
  z3logic.assertFormula(c == LogicTerm(1));
  z3logic.assertFormula(LogicTerm::ite(boolA, a, b) == a);
  z3logic.produceInstance();
  EXPECT_EQ(z3logic.solve(), Result::SAT);
  z3logic.reset();

  boolA = z3logic.makeVariable("bool_a", CType::BOOL);
  a = z3logic.makeVariable("a", CType::INT);
  b = z3logic.makeVariable("b", CType::INT);
  c = z3logic.makeVariable("c", CType::INT);
  z3logic.assertFormula(a == LogicTerm(3));
  z3logic.assertFormula(b == LogicTerm(2));
  z3logic.assertFormula(c == LogicTerm(1));
  z3logic.assertFormula(LogicTerm::ite(boolA, a, b) == b);
  z3logic.produceInstance();
  EXPECT_EQ(z3logic.solve(), Result::SAT);
  z3logic.reset();
}

TEST_F(TestZ3, AMOAndExactlyOneNaive) {
  std::unique_ptr<z3logic::Z3LogicBlock> z3logic =
      std::make_unique<z3logic::Z3LogicBlock>(ctx, solver, false);

  std::vector<std::vector<LogicTerm>> aNodes;

  for (int i = 0; i < 4; ++i) {
    aNodes.emplace_back();
    for (int j = 0; j < 4; ++j) {
      aNodes.back().emplace_back(z3logic->makeVariable(
          "a_" + std::to_string(i) + "_" + std::to_string(j), CType::BOOL));
    }
  }

  for (size_t i = 0; i < 4; ++i) {
    LogicTerm a = LogicTerm(0);
    for (size_t j = 0; j < 4; ++j) {
      a = a + LogicTerm::ite(aNodes[i][j], LogicTerm(1), LogicTerm(0));
    }
    LogicTerm const aa = (a <= LogicTerm(1));
    z3logic->assertFormula(aa);
  }
  for (size_t i = 0; i < 4; ++i) {
    LogicTerm a = LogicTerm(0);
    for (size_t j = 0; j < 4; ++j) {
      a = a + LogicTerm::ite(aNodes[j][i], LogicTerm(1), LogicTerm(0));
    }
    LogicTerm const aa = (a == LogicTerm(1));
    z3logic->assertFormula(aa);
  }
  z3logic->produceInstance();
  EXPECT_EQ(z3logic->solve(), Result::SAT);
  z3logic.reset();
}

TEST_F(TestZ3, AMOAndExactlyOneCMDR) {
  using namespace encodings;

  constexpr size_t n = 22;

  auto z3logic = std::make_unique<z3logic::Z3LogicBlock>(ctx, solver, false);

  std::vector<std::vector<LogicTerm>> aNodes;

  for (size_t i = 0; i < n; ++i) {
    aNodes.emplace_back();
    for (size_t j = 0; j < n; ++j) {
      aNodes.back().emplace_back(z3logic->makeVariable(
          "a_" + std::to_string(i) + "_" + std::to_string(j), CType::BOOL));
    }
  }

  for (size_t i = 0; i < n; ++i) {
    std::vector<LogicTerm> a;
    a.reserve(n);
    for (size_t j = 0; j < n; ++j) {
      a.emplace_back(aNodes[i][j]);
    }
    LogicTerm const aa = encodings::exactlyOneCmdr(
        groupVars(a, n / 2), LogicTerm::noneTerm(), z3logic.get());
    z3logic->assertFormula(aa);
  }
  for (size_t i = 0; i < n; ++i) {
    std::vector<LogicTerm> a;
    a.reserve(n);
    for (size_t j = 0; j < n; ++j) {
      a.emplace_back(aNodes[i][j]);
    }
    LogicTerm const aa =
        atMostOneCmdr(groupVars(a, 3), LogicTerm::noneTerm(), z3logic.get());
    z3logic->assertFormula(aa);
  }
  z3logic->produceInstance();
  EXPECT_EQ(z3logic->solve(), Result::SAT);
  z3logic.reset();
}

TEST_F(TestZ3, AMOAndExactlyOneBimander) {
  using namespace encodings;

  constexpr size_t n = 11;

  auto z3logic = std::make_unique<z3logic::Z3LogicBlock>(ctx, solver, false);

  std::vector<std::vector<LogicTerm>> aNodes;
  aNodes.reserve(n);
  for (size_t i = 0; i < n; ++i) {
    aNodes.emplace_back();
    for (size_t j = 0; j < n; ++j) {
      aNodes.back().emplace_back(z3logic->makeVariable(
          "a_" + std::to_string(i) + "_" + std::to_string(j), CType::BOOL));
    }
  }

  for (size_t i = 0; i < n; ++i) {
    std::vector<LogicTerm> a;
    a.reserve(n);
    for (size_t j = 0; j < n; ++j) {
      a.emplace_back(aNodes[i][j]);
    }
    LogicTerm const aa =
        exactlyOneCmdr(groupVars(a, 3), LogicTerm::noneTerm(), z3logic.get());
    z3logic->assertFormula(aa);
  }
  for (size_t i = 0; i < n; ++i) {
    std::vector<LogicTerm> a;
    a.reserve(n);
    for (size_t j = 0; j < n; ++j) {
      a.emplace_back(aNodes[i][j]);
    }
    LogicTerm const aa = atMostOneBiMander(a, z3logic.get());
    z3logic->assertFormula(aa);
  }
  z3logic->produceInstance();
  EXPECT_EQ(z3logic->solve(), Result::SAT);
  z3logic.reset();
}

TEST_F(TestZ3, TestBasicModel) {
  auto z3logic = std::make_unique<z3logic::Z3LogicBlock>(ctx, solver, false);

  LogicTerm const a = z3logic->makeVariable("a", CType::BOOL);
  LogicTerm const b = z3logic->makeVariable("b", CType::INT);
  LogicTerm const c = z3logic->makeVariable("c", CType::REAL);
  LogicTerm const d = z3logic->makeVariable("d", CType::BITVECTOR, 8);

  z3logic->assertFormula(a);
  z3logic->assertFormula(b == LogicTerm(1));
  z3logic->assertFormula(c == LogicTerm(1.0));
  z3logic->assertFormula(d == LogicTerm(1, 8));
  z3logic->produceInstance();
  EXPECT_EQ(z3logic->solve(), Result::SAT);

  auto* model = z3logic->getModel();

  EXPECT_EQ(model->getBoolValue(a, z3logic.get()), true);
  EXPECT_EQ(model->getIntValue(b, z3logic.get()), 1);
  EXPECT_EQ(model->getRealValue(c, z3logic.get()), 1.0);
  EXPECT_EQ(model->getBitvectorValue(d, z3logic.get()), 1);
  z3logic.reset();
}

TEST_F(TestZ3, TestVariableConversionsToBool) {
  auto z3logic = std::make_unique<z3logic::Z3LogicBlock>(ctx, solver, true);

  LogicTerm const a = z3logic->makeVariable("a", CType::BOOL);
  LogicTerm const b = z3logic->makeVariable("b", CType::INT);
  LogicTerm const c = z3logic->makeVariable("c", CType::REAL);
  LogicTerm const d = z3logic->makeVariable("d", CType::BITVECTOR, 32);

  z3logic->assertFormula(a);
  z3logic->assertFormula(b);
  z3logic->assertFormula(c);
  z3logic->assertFormula(d);

  EXPECT_EQ(z3logic->solve(), Result::SAT);
  z3logic.reset();
}

TEST_F(TestZ3, TestVariableConversionsToBV) {
  auto z3logic = std::make_unique<z3logic::Z3LogicBlock>(ctx, solver, true);

  LogicTerm const a = z3logic->makeVariable("a", CType::BOOL);
  LogicTerm const b = z3logic->makeVariable("b", CType::INT);
  LogicTerm const c = z3logic->makeVariable("c", CType::REAL);
  LogicTerm const d = z3logic->makeVariable("d", CType::BITVECTOR, 32);

  z3logic->assertFormula(LogicTerm::bvAnd(d, a) == d);
  z3logic->assertFormula(LogicTerm::eq(d, a) == d);
  z3logic->assertFormula(LogicTerm::bvOr(d, b) == d);
  z3logic->assertFormula(LogicTerm::bvXor(d, b) == d);

  EXPECT_EQ(z3logic->solve(), Result::SAT);
  z3logic.reset();
}

TEST_F(TestZ3, TestVariableConversionsToInt) {
  auto z3logic = std::make_unique<z3logic::Z3LogicBlock>(ctx, solver, true);

  LogicTerm const a = z3logic->makeVariable("a", CType::BOOL);
  LogicTerm const b = z3logic->makeVariable("b", CType::INT);
  LogicTerm const c = z3logic->makeVariable("c", CType::REAL);
  LogicTerm const d = z3logic->makeVariable("d", CType::BITVECTOR, 32);

  z3logic->assertFormula(LogicTerm::bvAnd(d, a) == d);
  z3logic->assertFormula(LogicTerm::bvOr(d, b) == d);
  z3logic->assertFormula(LogicTerm::bvXor(d, b) == d);

  EXPECT_EQ(z3logic->solve(), Result::SAT);
  z3logic.reset();
}

TEST_F(TestZ3, TestVariableConversionsToReal) {
  auto z3logic = std::make_unique<z3logic::Z3LogicBlock>(ctx, solver, true);

  LogicTerm const a = z3logic->makeVariable("a", CType::BOOL);
  LogicTerm const b = z3logic->makeVariable("b", CType::INT);
  LogicTerm const c = z3logic->makeVariable("c", CType::REAL);
  LogicTerm const d = z3logic->makeVariable("d", CType::BITVECTOR, 32);

  z3logic->assertFormula(LogicTerm::bvAnd(d, a) == d);
  z3logic->assertFormula(LogicTerm::bvOr(d, b) == d);
  z3logic->assertFormula(LogicTerm::bvXor(d, b) == d);

  EXPECT_EQ(z3logic->solve(), Result::SAT);
  z3logic.reset();
}

class TestZ3Opt : public testing::TestWithParam<logicbase::OpType> {
protected:
  void SetUp() override {}

  std::shared_ptr<z3::context> ctx = std::make_shared<z3::context>();
  std::shared_ptr<z3::optimize> opt = std::make_shared<z3::optimize>(*ctx);
};

TEST_F(TestZ3Opt, ConstructDestruct) {
  auto z3logic = std::make_unique<z3logic::Z3LogicOptimizer>(ctx, opt, false);

  LogicTerm const a = z3logic->makeVariable("a", CType::BOOL);
  LogicTerm const b = z3logic->makeVariable("b", CType::BOOL);
  LogicTerm const c = z3logic->makeVariable("c", CType::BOOL);

  z3logic->assertFormula(a && b);
  z3logic->produceInstance();
  EXPECT_EQ(z3logic->solve(), Result::SAT);
  std::stringstream ss{};
  ss << opt;

  z3logic->reset();
}

TEST_F(TestZ3Opt, SimpleTrue) {
  auto z3logic = std::make_unique<z3logic::Z3LogicOptimizer>(ctx, opt, false);

  LogicTerm a = z3logic->makeVariable("a", CType::BOOL);
  LogicTerm b = z3logic->makeVariable("b", CType::BOOL);
  LogicTerm c = z3logic->makeVariable("c", CType::BOOL);
  z3logic->assertFormula(a && b);
  z3logic->produceInstance();
  EXPECT_EQ(z3logic->solve(), Result::SAT);
  z3logic->reset();

  a = z3logic->makeVariable("a", CType::BOOL);
  b = z3logic->makeVariable("b", CType::BOOL);
  z3logic->assertFormula(a || b);
  z3logic->produceInstance();
  EXPECT_EQ(z3logic->solve(), Result::SAT);
  z3logic->reset();

  a = z3logic->makeVariable("a", CType::BOOL);
  b = z3logic->makeVariable("b", CType::BOOL);
  z3logic->assertFormula(a == b);
  z3logic->produceInstance();
  EXPECT_EQ(z3logic->solve(), Result::SAT);
  z3logic->reset();

  a = z3logic->makeVariable("a", CType::BOOL);
  b = z3logic->makeVariable("b", CType::BOOL);
  z3logic->assertFormula(a != b);
  z3logic->produceInstance();
  EXPECT_EQ(z3logic->solve(), Result::SAT);
  z3logic->reset();

  a = z3logic->makeVariable("a", CType::BOOL);
  b = z3logic->makeVariable("b", CType::BOOL);
  z3logic->assertFormula(a && !b);
  z3logic->produceInstance();
  EXPECT_EQ(z3logic->solve(), Result::SAT);
  z3logic->reset();

  a = z3logic->makeVariable("a", CType::BOOL);
  b = z3logic->makeVariable("b", CType::BOOL);
  z3logic->assertFormula(!a || !b);
  z3logic->produceInstance();
  EXPECT_EQ(z3logic->solve(), Result::SAT);
  z3logic->reset();

  a = z3logic->makeVariable("a", CType::BOOL);
  b = z3logic->makeVariable("b", CType::BOOL);
  z3logic->assertFormula(LogicTerm::implies(a, b));
  z3logic->produceInstance();
  EXPECT_EQ(z3logic->solve(), Result::SAT);
  z3logic->reset();

  a = z3logic->makeVariable("a", CType::BOOL);
  z3logic->assertFormula(a);
  z3logic->produceInstance();
  EXPECT_EQ(z3logic->solve(), Result::SAT);
  z3logic->reset();

  b = z3logic->makeVariable("b", CType::BOOL);
  z3logic->assertFormula(b);
  z3logic->produceInstance();
  EXPECT_EQ(z3logic->solve(), Result::SAT);
  z3logic->reset();

  a = z3logic->makeVariable("a", CType::BOOL);
  b = z3logic->makeVariable("b", CType::BOOL);
  c = z3logic->makeVariable("c", CType::BOOL);
  z3logic->assertFormula(a && b && c);
  z3logic->produceInstance();
  EXPECT_EQ(z3logic->solve(), Result::SAT);
  z3logic->reset();
}

TEST_F(TestZ3Opt, SimpleFalse) {
  auto z3logic = std::make_unique<z3logic::Z3LogicOptimizer>(ctx, opt, false);

  LogicTerm a = z3logic->makeVariable("a", CType::BOOL);
  LogicTerm b = z3logic->makeVariable("b", CType::BOOL);
  z3logic->assertFormula(!a);
  z3logic->assertFormula(a);
  z3logic->produceInstance();
  EXPECT_EQ(z3logic->solve(), Result::UNSAT);
  z3logic = std::make_unique<z3logic::Z3LogicOptimizer>(ctx, opt, false);
  z3logic->reset();

  b = z3logic->makeVariable("b", CType::BOOL);
  z3logic->assertFormula(!b);
  z3logic->assertFormula(b);
  z3logic->produceInstance();
  EXPECT_EQ(z3logic->solve(), Result::UNSAT);
  z3logic = std::make_unique<z3logic::Z3LogicOptimizer>(ctx, opt, false);
  z3logic->reset();

  a = z3logic->makeVariable("a", CType::BOOL);
  b = z3logic->makeVariable("b", CType::BOOL);
  z3logic->assertFormula(!a);
  z3logic->assertFormula(b);
  z3logic->assertFormula(a == b);
  z3logic->produceInstance();
  EXPECT_EQ(z3logic->solve(), Result::UNSAT);
  z3logic = std::make_unique<z3logic::Z3LogicOptimizer>(ctx, opt, false);
  z3logic->reset();

  a = z3logic->makeVariable("a", CType::BOOL);
  b = z3logic->makeVariable("b", CType::BOOL);
  z3logic->assertFormula(a);
  z3logic->assertFormula(!b);
  z3logic->assertFormula(a == b);
  z3logic->produceInstance();
  EXPECT_EQ(z3logic->solve(), Result::UNSAT);
  z3logic = std::make_unique<z3logic::Z3LogicOptimizer>(ctx, opt, false);
  z3logic->reset();

  a = z3logic->makeVariable("a", CType::BOOL);
  b = z3logic->makeVariable("b", CType::BOOL);
  z3logic->assertFormula(!a);
  z3logic->assertFormula(b);
  z3logic->assertFormula(a == b);
  z3logic->produceInstance();
  EXPECT_EQ(z3logic->solve(), Result::UNSAT);
  z3logic = std::make_unique<z3logic::Z3LogicOptimizer>(ctx, opt, false);
  z3logic->reset();

  a = z3logic->makeVariable("a", CType::BOOL);
  b = z3logic->makeVariable("b", CType::BOOL);
  z3logic->assertFormula(a);
  z3logic->assertFormula(b);
  z3logic->assertFormula(a != b);
  z3logic->produceInstance();
  EXPECT_EQ(z3logic->solve(), Result::UNSAT);
  z3logic = std::make_unique<z3logic::Z3LogicOptimizer>(ctx, opt, false);
  z3logic->reset();

  a = z3logic->makeVariable("a", CType::BOOL);
  b = z3logic->makeVariable("b", CType::BOOL);
  z3logic->assertFormula(!a);
  z3logic->assertFormula(!b);
  z3logic->assertFormula(a != b);
  z3logic->produceInstance();
  EXPECT_EQ(z3logic->solve(), Result::UNSAT);
  z3logic = std::make_unique<z3logic::Z3LogicOptimizer>(ctx, opt, false);
  z3logic->reset();

  a = z3logic->makeVariable("a", CType::BOOL);
  b = z3logic->makeVariable("b", CType::BOOL);
  z3logic->assertFormula(!a);
  z3logic->assertFormula(!b);
  z3logic->assertFormula(a && !b);
  z3logic->produceInstance();
  EXPECT_EQ(z3logic->solve(), Result::UNSAT);
  z3logic = std::make_unique<z3logic::Z3LogicOptimizer>(ctx, opt, false);
  z3logic->reset();

  a = z3logic->makeVariable("a", CType::BOOL);
  b = z3logic->makeVariable("b", CType::BOOL);
  z3logic->assertFormula(a);
  z3logic->assertFormula(b);
  z3logic->assertFormula(a && !b);
  z3logic->produceInstance();
  EXPECT_EQ(z3logic->solve(), Result::UNSAT);
  z3logic = std::make_unique<z3logic::Z3LogicOptimizer>(ctx, opt, false);
  z3logic->reset();

  a = z3logic->makeVariable("a", CType::BOOL);
  b = z3logic->makeVariable("b", CType::BOOL);
  z3logic->assertFormula(a);
  z3logic->assertFormula(!b);
  z3logic->assertFormula(LogicTerm::implies(a, b));
  z3logic->produceInstance();
  EXPECT_EQ(z3logic->solve(), Result::UNSAT);
}

TEST_F(TestZ3Opt, IntBase) {
  auto z3logic = std::make_unique<z3logic::Z3LogicOptimizer>(ctx, opt, false);
  z3logic->reset();

  LogicTerm a = z3logic->makeVariable("a", CType::INT);
  LogicTerm b = z3logic->makeVariable("b", CType::INT);
  LogicTerm c = z3logic->makeVariable("c", CType::INT);
  z3logic->assertFormula(a + b == c);
  z3logic->produceInstance();
  EXPECT_EQ(z3logic->solve(), Result::SAT);
  z3logic = std::make_unique<z3logic::Z3LogicOptimizer>(ctx, opt, false);
  z3logic->reset();

  a = z3logic->makeVariable("a", CType::INT);
  b = z3logic->makeVariable("b", CType::INT);
  c = z3logic->makeVariable("c", CType::INT);
  z3logic->assertFormula(a - b == c);
  z3logic->produceInstance();
  EXPECT_EQ(z3logic->solve(), Result::SAT);
  z3logic = std::make_unique<z3logic::Z3LogicOptimizer>(ctx, opt, false);
  z3logic->reset();

  a = z3logic->makeVariable("a", CType::INT);
  b = z3logic->makeVariable("b", CType::INT);
  c = z3logic->makeVariable("c", CType::INT);
  z3logic->assertFormula(a * b == c);
  z3logic->produceInstance();
  EXPECT_EQ(z3logic->solve(), Result::SAT);
  z3logic = std::make_unique<z3logic::Z3LogicOptimizer>(ctx, opt, false);
  z3logic->reset();

  a = z3logic->makeVariable("a", CType::INT);
  b = z3logic->makeVariable("b", CType::INT);
  c = z3logic->makeVariable("c", CType::INT);
  z3logic->assertFormula(a / b == c);
  z3logic->produceInstance();
  EXPECT_EQ(z3logic->solve(), Result::SAT);
  z3logic = std::make_unique<z3logic::Z3LogicOptimizer>(ctx, opt, false);
  z3logic->reset();

  a = z3logic->makeVariable("a", CType::INT);
  b = z3logic->makeVariable("b", CType::INT);
  z3logic->assertFormula(a > b);
  z3logic->produceInstance();
  EXPECT_EQ(z3logic->solve(), Result::SAT);
  z3logic = std::make_unique<z3logic::Z3LogicOptimizer>(ctx, opt, false);
  z3logic->reset();

  a = z3logic->makeVariable("a", CType::INT);
  c = z3logic->makeVariable("c", CType::INT);
  z3logic->assertFormula(a < c);
  z3logic->produceInstance();
  EXPECT_EQ(z3logic->solve(), Result::SAT);
  z3logic = std::make_unique<z3logic::Z3LogicOptimizer>(ctx, opt, false);
  z3logic->reset();

  a = z3logic->makeVariable("a", CType::INT);
  b = z3logic->makeVariable("b", CType::INT);
  z3logic->assertFormula(a >= b);
  z3logic->produceInstance();
  EXPECT_EQ(z3logic->solve(), Result::SAT);
  z3logic = std::make_unique<z3logic::Z3LogicOptimizer>(ctx, opt, false);
  z3logic->reset();

  a = z3logic->makeVariable("a", CType::INT);
  c = z3logic->makeVariable("c", CType::INT);
  z3logic->assertFormula(a <= c);
  z3logic->produceInstance();
  EXPECT_EQ(z3logic->solve(), Result::SAT);
}
TEST_F(TestZ3Opt, IntNumbers) {
  auto z3logic = std::make_unique<z3logic::Z3LogicOptimizer>(ctx, opt, false);
  z3logic->reset();

  LogicTerm a = z3logic->makeVariable("a", CType::INT);
  LogicTerm b = z3logic->makeVariable("b", CType::INT);
  LogicTerm c = z3logic->makeVariable("c", CType::INT);
  z3logic->assertFormula(a == LogicTerm(3));
  z3logic->assertFormula(b == LogicTerm(2));
  z3logic->assertFormula(c == LogicTerm(1));
  z3logic->assertFormula(a - b == c);
  z3logic->produceInstance();
  EXPECT_EQ(z3logic->solve(), Result::SAT);
  z3logic = std::make_unique<z3logic::Z3LogicOptimizer>(ctx, opt, false);
  z3logic->reset();

  a = z3logic->makeVariable("a", CType::INT);
  b = z3logic->makeVariable("b", CType::INT);
  c = z3logic->makeVariable("c", CType::INT);
  z3logic->assertFormula(a == LogicTerm(3));
  z3logic->assertFormula(b == LogicTerm(2));
  z3logic->assertFormula(c == LogicTerm(1));
  z3logic->assertFormula(c + b == a);
  z3logic->produceInstance();
  EXPECT_EQ(z3logic->solve(), Result::SAT);
  z3logic = std::make_unique<z3logic::Z3LogicOptimizer>(ctx, opt, false);
  z3logic->reset();

  a = z3logic->makeVariable("a", CType::INT);
  b = z3logic->makeVariable("b", CType::INT);
  c = z3logic->makeVariable("c", CType::INT);
  z3logic->assertFormula(a == LogicTerm(3));
  z3logic->assertFormula(b == LogicTerm(2));
  z3logic->assertFormula(c == LogicTerm(1));
  z3logic->assertFormula((a > b) == LogicTerm(true));
  z3logic->assertFormula((b > c) == LogicTerm(true));
  z3logic->produceInstance();
  EXPECT_EQ(z3logic->solve(), Result::SAT);
  z3logic = std::make_unique<z3logic::Z3LogicOptimizer>(ctx, opt, false);
  z3logic->reset();

  a = z3logic->makeVariable("a", CType::INT);
  b = z3logic->makeVariable("b", CType::INT);
  c = z3logic->makeVariable("c", CType::INT);
  z3logic->assertFormula(a == LogicTerm(3));
  z3logic->assertFormula(b == LogicTerm(2));
  z3logic->assertFormula(c == LogicTerm(1));
  z3logic->assertFormula((c < a) == LogicTerm(true));
  z3logic->assertFormula((a < LogicTerm(4)) == LogicTerm(true));
  z3logic->produceInstance();
  EXPECT_EQ(z3logic->solve(), Result::SAT);
  z3logic = std::make_unique<z3logic::Z3LogicOptimizer>(ctx, opt, false);
  z3logic->reset();

  LogicTerm boolA = z3logic->makeVariable("bool_a", CType::BOOL);
  a = z3logic->makeVariable("a", CType::INT);
  b = z3logic->makeVariable("b", CType::INT);
  c = z3logic->makeVariable("c", CType::INT);
  z3logic->assertFormula(a == LogicTerm(3));
  z3logic->assertFormula(b == LogicTerm(2));
  z3logic->assertFormula(c == LogicTerm(1));
  z3logic->assertFormula(LogicTerm::ite(boolA, a, b) == a);
  z3logic->produceInstance();
  EXPECT_EQ(z3logic->solve(), Result::SAT);
  z3logic = std::make_unique<z3logic::Z3LogicOptimizer>(ctx, opt, false);
  z3logic->reset();

  boolA = z3logic->makeVariable("bool_a", CType::BOOL);
  a = z3logic->makeVariable("a", CType::INT);
  b = z3logic->makeVariable("b", CType::INT);
  c = z3logic->makeVariable("c", CType::INT);
  z3logic->assertFormula(a == LogicTerm(3));
  z3logic->assertFormula(b == LogicTerm(2));
  z3logic->assertFormula(c == LogicTerm(1));
  z3logic->assertFormula(LogicTerm::ite(boolA, a, b) == b);
  z3logic->produceInstance();
  EXPECT_EQ(z3logic->solve(), Result::SAT);
}

TEST_F(TestZ3Opt, AMOAndExactlyOneNaive) {
  auto z3logic = std::make_unique<z3logic::Z3LogicOptimizer>(ctx, opt, false);
  z3logic->reset();

  std::vector<std::vector<LogicTerm>> aNodes;
  aNodes.reserve(4);
  for (int i = 0; i < 4; ++i) {
    aNodes.emplace_back();
    for (int j = 0; j < 4; ++j) {
      aNodes.back().emplace_back(z3logic->makeVariable(
          "a_" + std::to_string(i) + "_" + std::to_string(j), CType::BOOL));
    }
  }

  for (size_t i = 0; i < 4; ++i) {
    auto a = LogicTerm(0);
    for (size_t j = 0; j < 4; ++j) {
      a = a + LogicTerm::ite(aNodes[i][j], LogicTerm(1), LogicTerm(0));
    }
    LogicTerm const aa = (a <= LogicTerm(1));
    z3logic->assertFormula(aa);
  }
  for (size_t i = 0; i < 4; ++i) {
    auto a = LogicTerm(0);
    for (size_t j = 0; j < 4; ++j) {
      a = a + LogicTerm::ite(aNodes[j][i], LogicTerm(1), LogicTerm(0));
    }
    LogicTerm const aa = (a == LogicTerm(1));
    z3logic->assertFormula(aa);
  }
  z3logic->produceInstance();
  EXPECT_EQ(z3logic->solve(), Result::SAT);
}
