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

#include <cassert>
#include <cstdarg>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace logicbase {

enum class Result : std::uint8_t { SAT, UNSAT, NDEF };
enum class OpType : std::uint8_t {
  None,
  Constant,
  Variable,
  EQ,
  XOR,
  AND,
  OR,
  ITE,
  NEG,
  IMPL,
  ADD,
  SUB,
  MUL,
  DIV,
  GT,
  LT,
  GTE,
  LTE,
  CALL,
  GET,
  SET,
  BitAnd,
  BitOr,
  BitEq,
  BitXor
};

enum class CType : std::uint8_t {
  BOOL,
  INT,
  REAL,
  BITVECTOR,
  FUNCTION,
  ARRAY,
  SET,
  ERRORTYPE
};

[[maybe_unused]] static Result resultFromString(const std::string& result) {
  if (result == "sat") {
    return Result::SAT;
  }
  if (result == "unsat") {
    return Result::UNSAT;
  }
  return Result::NDEF;
}

inline std::string toString(Result result) {
  switch (result) {
  case Result::SAT:
    return "SAT";
  case Result::UNSAT:
    return "UNSAT";
  case Result::NDEF:
  default:
    return "NDEF";
  }
}

inline std::string toString(OpType opType) {
  switch (opType) {
  case OpType::Variable:
    return "Variable";
  case OpType::Constant:
    return "Constant";
  case OpType::EQ:
    return "EQ";
  case OpType::XOR:
    return "XOR";
  case OpType::AND:
    return "AND";
  case OpType::OR:
    return "OR";
  case OpType::ITE:
    return "ITE";
  case OpType::NEG:
    return "NEG";
  case OpType::IMPL:
    return "IMPL";
  case OpType::ADD:
    return "ADD";
  case OpType::SUB:
    return "SUB";
  case OpType::MUL:
    return "MUL";
  case OpType::DIV:
    return "DIV";
  case OpType::GT:
    return "GT";
  case OpType::LT:
    return "LT";
  case OpType::GTE:
    return "GTE";
  case OpType::LTE:
    return "LTE";
  case OpType::BitAnd:
    return "BIT_AND";
  case OpType::BitOr:
    return "BIT_OR";
  case OpType::BitEq:
    return "BIT_EQ";
  case OpType::BitXor:
    return "BIT_XOR";
  case OpType::CALL:
    return "CALL";
  case OpType::GET:
    return "GET";
  case OpType::SET:
    return "SET";
  default:
    return "Unknown";
  }
}

inline std::string toString(CType ctype) {
  switch (ctype) {
  case CType::BOOL:
    return "B";
  case CType::BITVECTOR:
    return "BV";
  case CType::INT:
    return "I";
  case CType::REAL:
    return "F";
  case CType::FUNCTION:
    return "F(...)";
  case CType::ARRAY:
    return "A[...]";
  case CType::SET:
    return "S{...}";
  case CType::ERRORTYPE:
    throw std::runtime_error("Error: Unknown CType");
  }
  return "Error";
}
inline CType cTypeFromString(const std::string& ctype) {
  if (ctype == "B") {
    return CType::BOOL;
  }
  if (ctype == "BV") {
    return CType::BITVECTOR;
  }
  if (ctype == "I") {
    return CType::INT;
  }
  if (ctype == "F") {
    return CType::REAL;
  }
  if (ctype == "F(...)") {
    return CType::FUNCTION;
  }
  if (ctype == "A[...]") {
    return CType::ARRAY;
  }
  if (ctype == "S{...}") {
    return CType::SET;
  }
  return CType::BOOL;
}

inline bool isArith(OpType optype) {
  switch (optype) {
  case OpType::ADD:
  case OpType::SUB:
  case OpType::MUL:
  case OpType::DIV:
  case OpType::GT:
  case OpType::LT:
    return true;
  default:
    return false;
  }
}

inline bool isNumber(CType ctype) {
  switch (ctype) {
  case CType::INT:
  case CType::REAL:
  case CType::BITVECTOR:
    return true;
  default:
    return false;
  }
}

inline bool isCommutative(OpType op) {
  switch (op) {
  case OpType::ADD:
  case OpType::MUL:
  case OpType::EQ:
  case OpType::XOR:
  case OpType::AND:
  case OpType::OR:
    return true;
  default:
    return false;
  }
}

inline bool isAssociative(OpType op) {
  switch (op) {
  case OpType::ADD:
  case OpType::MUL:
  case OpType::EQ:
  case OpType::XOR:
  case OpType::AND:
  case OpType::OR:
    return true;
  default:
    return false;
  }
}
inline bool hasNeutralElement(OpType op) {
  switch (op) {
  case OpType::ADD:
  case OpType::MUL:
  case OpType::AND:
  case OpType::OR:
    return true;
  default:
    return false;
  }
}

inline CType getResultCType(OpType op) {
  switch (op) {
  case OpType::NEG:
  case OpType::IMPL:
  case OpType::AND:
  case OpType::OR:
  case OpType::GT:
  case OpType::LT:
  case OpType::GTE:
  case OpType::LTE:
    return CType::BOOL;
  case OpType::ADD:
  case OpType::SUB:
  case OpType::MUL:
  case OpType::DIV:
    return CType::INT;
  case OpType::ITE:
    return CType::BOOL;
  case OpType::BitAnd:
  case OpType::BitOr:
  case OpType::BitEq:
  case OpType::BitXor:
    return CType::BITVECTOR;
  default:
    return CType::BOOL;
  }
}

class LogicTerm;

using LogicVector = std::vector<LogicTerm>;
using LogicMatrix = std::vector<LogicVector>;
using LogicMatrix3D = std::vector<LogicMatrix>;
using LogicMatrix4D = std::vector<LogicMatrix3D>;

class Logic {
public:
  virtual ~Logic() = default;
  virtual uint64_t getNextId() = 0;
  virtual uint64_t getId() = 0;
};

} // namespace logicbase
