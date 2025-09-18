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

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

namespace logicbase {
class LogicTerm {
private:
  Logic* lb = nullptr;
  uint64_t id = 0;
  uint64_t depth = 0U;
  std::string name;

  OpType opType = OpType::Variable;
  bool value = false;
  int iValue = 0;
  double fValue = 0.;
  uint64_t bvValue = 0U;
  uint16_t bvSize = 0;
  std::vector<LogicTerm> nodes;
  CType cType = CType::BOOL;

  // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
  static inline uint64_t gid = 1;

public:
  explicit LogicTerm(bool v) : opType(OpType::Constant), value(v) {}

  explicit LogicTerm(int32_t v)
      : opType(OpType::Constant), iValue(v), cType(CType::INT) {}

  explicit LogicTerm(double v)
      : opType(OpType::Constant), fValue(v), cType(CType::REAL) {}

  LogicTerm(uint64_t v, uint16_t bvs)
      : opType(OpType::Constant), bvValue(v), bvSize(bvs),
        cType(CType::BITVECTOR) {}

  explicit LogicTerm(Logic* logic = nullptr);
  explicit LogicTerm(std::string n, Logic* logic = nullptr);
  explicit LogicTerm(CType type, Logic* logic = nullptr);

  LogicTerm(OpType op, std::string n, CType type, Logic* logic = nullptr);

  LogicTerm(std::string n, uint64_t identifier, Logic* logic = nullptr);

  LogicTerm(std::string n, CType type, Logic* logic = nullptr,
            uint16_t bvs = 0);

  LogicTerm(std::string n, uint64_t identifier, CType type,
            Logic* logic = nullptr);

  LogicTerm(OpType op, const std::initializer_list<LogicTerm>& n,
            CType type = CType::BOOL, Logic* logic = nullptr);

  LogicTerm(OpType op, const std::vector<LogicTerm>& n,
            CType type = CType::BOOL, Logic* logic = nullptr);

  LogicTerm(OpType op, const LogicTerm& a, CType type, Logic* logic = nullptr)
      : LogicTerm(op, {a}, type, logic) {}

  LogicTerm(OpType op, const LogicTerm& a, const LogicTerm& b, CType type,
            Logic* logic)
      : LogicTerm(op, {a, b}, type, logic) {}

  LogicTerm(OpType op, const LogicTerm& a, const LogicTerm& b,
            const LogicTerm& c, CType type, Logic* logic)
      : LogicTerm(op, {a, b, c}, type, logic) {}

  LogicTerm(OpType op, const LogicTerm& a);
  LogicTerm(OpType op, const LogicTerm& a, const LogicTerm& b);
  LogicTerm(OpType op, const LogicTerm& a, const LogicTerm& b,
            const LogicTerm& c);

  ~LogicTerm() = default;

  static uint64_t getNextId(Logic* logic = nullptr);

  static LogicTerm noneTerm();
  static LogicTerm eq(const LogicTerm& a, const LogicTerm& b);
  static LogicTerm neq(const LogicTerm& a, const LogicTerm& b);
  static LogicTerm o(const LogicTerm& a, const LogicTerm& b);
  static LogicTerm a(const LogicTerm& a, const LogicTerm& b);
  static LogicTerm bvAnd(const LogicTerm& a, const LogicTerm& b);
  static LogicTerm bvOr(const LogicTerm& a, const LogicTerm& b);
  static LogicTerm bvXor(const LogicTerm& a, const LogicTerm& b);
  static LogicTerm implies(const LogicTerm& a, const LogicTerm& b);
  static LogicTerm ite(const LogicTerm& a, const LogicTerm& b,
                       const LogicTerm& c);
  static LogicTerm add(const LogicTerm& a, const LogicTerm& b);
  static LogicTerm sub(const LogicTerm& a, const LogicTerm& b);
  static LogicTerm mul(const LogicTerm& a, const LogicTerm& b);
  static LogicTerm div(const LogicTerm& a, const LogicTerm& b);
  static LogicTerm gt(const LogicTerm& a, const LogicTerm& b);
  static LogicTerm lt(const LogicTerm& a, const LogicTerm& b);
  static LogicTerm gte(const LogicTerm& a, const LogicTerm& b);
  static LogicTerm lte(const LogicTerm& a, const LogicTerm& b);
  static LogicTerm neg(const LogicTerm& a);

  LogicTerm operator&&(const LogicTerm& other) const;
  LogicTerm operator&(const LogicTerm& other) const;
  LogicTerm operator|(const LogicTerm& other) const;
  LogicTerm operator^(const LogicTerm& other) const;
  LogicTerm operator||(const LogicTerm& other) const;
  LogicTerm operator==(const LogicTerm& other) const;
  LogicTerm operator!=(const LogicTerm& other) const;
  LogicTerm operator+(const LogicTerm& other) const;
  LogicTerm operator-(const LogicTerm& other) const;
  LogicTerm operator*(const LogicTerm& other) const;
  LogicTerm operator/(const LogicTerm& other) const;
  LogicTerm operator>(const LogicTerm& other) const;
  LogicTerm operator<(const LogicTerm& other) const;
  LogicTerm operator>=(const LogicTerm& other) const;
  LogicTerm operator<=(const LogicTerm& other) const;
  LogicTerm operator!() const;

  [[nodiscard]] bool isConst() const;

  [[nodiscard]] uint64_t getID() const { return id; }
  [[nodiscard]] const std::vector<LogicTerm>& getNodes() const { return nodes; }
  [[nodiscard]] OpType getOpType() const { return opType; }
  [[nodiscard]] CType getCType() const { return cType; }
  [[nodiscard]] const std::string& getName() const { return name; }
  [[nodiscard]] Logic* getLogic() const { return lb; }
  [[nodiscard]] uint64_t getDepth() const { return depth; }

  [[nodiscard]] bool getBoolValue() const;
  [[nodiscard]] int getIntValue() const;
  [[nodiscard]] double getFloatValue() const;
  [[nodiscard]] uint64_t getBitVectorValue() const;
  [[nodiscard]] uint16_t getBitVectorSize() const;

  [[nodiscard]] bool deepEquals(const LogicTerm& other) const;

  [[nodiscard]] uint64_t getMaxChildrenDepth() const;

  [[nodiscard]] static std::string getStrRep(OpType op);

  [[nodiscard]] static Logic* getValidLogicPtr(const LogicTerm& a,
                                               const LogicTerm& b);

  [[nodiscard]] static Logic*
  getValidLogicPtr(const LogicTerm& a, const LogicTerm& b, const LogicTerm& c);

  [[nodiscard]] static LogicTerm
  combineTerms(const LogicTerm& a, const LogicTerm& b, OpType op, Logic* logic);

  [[nodiscard]] static LogicTerm
  combineConst(const LogicTerm& a, const LogicTerm& b, OpType op, Logic* logic);

  [[nodiscard]] static LogicTerm combineOneConst(const LogicTerm& constant,
                                                 const LogicTerm& other,
                                                 OpType op, Logic* logic);

  [[nodiscard]] static LogicTerm negateTerm(const LogicTerm& term,
                                            Logic* logic);

  [[nodiscard]] LogicTerm getBoolConversion() const;

  [[nodiscard]] static CType getTargetCType(const LogicTerm& a,
                                            const LogicTerm& b, OpType op);

  [[nodiscard]] static CType getTargetCType(CType targetType,
                                            const LogicTerm& b);

  [[nodiscard]] static std::vector<LogicTerm>
  getFlatTerms(const LogicTerm& t, OpType op = OpType::AND);

  [[nodiscard]] static uint64_t getMax(const std::vector<LogicTerm>& terms);

  [[nodiscard]] static uint16_t
  getMaxBVSize(const std::vector<LogicTerm>& terms);

  static void reset() { gid = 0; }
};

struct TermHash {
  std::size_t operator()(const LogicTerm& t) const;
  bool operator()(const LogicTerm& t1, const LogicTerm& t2) const;
};

struct TermDepthComparator {
  bool operator()(const LogicTerm& t1, const LogicTerm& t2) const;
};

} // namespace logicbase
