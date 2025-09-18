/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "cliffordsynthesis/encoding/TableauEncoder.hpp"

#include "Logic.hpp"
#include "cliffordsynthesis/Results.hpp"
#include "cliffordsynthesis/Tableau.hpp"
#include "ir/operations/OpType.hpp"
#include "logicblocks/Model.hpp"

#include <cstddef>
#include <cstdint>
#include <plog/Log.h>
#include <stdexcept>
#include <string>
#include <utility>

namespace cs::encoding {

using namespace logicbase;

void TableauEncoder::createTableauVariables() {
  const auto n = static_cast<std::uint16_t>(S);

  PLOG_DEBUG << "Creating tableau variables.";
  vars.x.reserve(T);
  vars.z.reserve(T);
  vars.r.reserve(T);
  for (std::size_t t = 0U; t <= T; ++t) {
    auto& x = vars.x.emplace_back();
    auto& z = vars.z.emplace_back();
    x.reserve(N);
    z.reserve(N);
    for (std::size_t i = 0U; i < N; ++i) {
      const std::string xName =
          "x_" + std::to_string(t) + "_" + std::to_string(i);
      PLOG_VERBOSE << "Creating variable " << xName;
      x.emplace_back(lb->makeVariable(xName, CType::BITVECTOR, n));
      const std::string zName =
          "z_" + std::to_string(t) + "_" + std::to_string(i);
      PLOG_VERBOSE << "Creating variable " << zName;
      z.emplace_back(lb->makeVariable(zName, CType::BITVECTOR, n));
    }
    const std::string rName = "r_" + std::to_string(t);
    PLOG_VERBOSE << "Creating variable " << rName;
    vars.r.emplace_back(lb->makeVariable(rName, CType::BITVECTOR, n));
  }
}

void TableauEncoder::assertTableau(const Tableau& tableau,
                                   const std::size_t t) {
  const auto n = static_cast<std::uint16_t>(S);

  PLOG_DEBUG << "Asserting tableau at time step " << t;
  PLOG_VERBOSE << "Tableau:\n" << tableau;
  for (auto a = 0U; a < N; ++a) {
    const auto targetX = tableau.getBVFrom(a);
    lb->assertFormula(vars.x[t][a] == LogicTerm(targetX, n));

    const auto targetZ = tableau.getBVFrom(a + N);
    lb->assertFormula(vars.z[t][a] == LogicTerm(targetZ, n));
  }

  const auto targetR = tableau.getBVFrom(2U * N);
  lb->assertFormula(vars.r[t] == LogicTerm(targetR, n));
}

void TableauEncoder::extractTableauFromModel(Results& results,
                                             const std::size_t t,
                                             Model& model) const {
  Tableau tableau(N, S > N);
  for (std::size_t i = 0; i < N; ++i) {
    const auto bvx = model.getBitvectorValue(vars.x[t][i], lb.get());
    tableau.populateTableauFrom(bvx, S, i);
    const auto bvz = model.getBitvectorValue(vars.z[t][i], lb.get());
    tableau.populateTableauFrom(bvz, S, i + N);
  }
  const auto bvr = model.getBitvectorValue(vars.r[t], lb.get());
  tableau.populateTableauFrom(bvr, S, 2 * N);

  results.setResultTableau(tableau);
}

LogicTerm
TableauEncoder::Variables::singleQubitXChange(const std::size_t pos,
                                              const std::size_t qubit,
                                              const qc::OpType gate) const {
  switch (gate) {
  case qc::OpType::None:
  case qc::OpType::X:
  case qc::OpType::Y:
  case qc::OpType::Z:
  case qc::OpType::S:
  case qc::OpType::Sdg:
    return x[pos][qubit];
  case qc::OpType::H:
    return z[pos][qubit];
  default:
    const auto msg = "Unsupported single-qubit gate: " + toString(gate);
    PLOG_FATAL << msg;
    throw std::runtime_error(msg);
  }
}

LogicTerm
TableauEncoder::Variables::singleQubitZChange(const std::size_t pos,
                                              const std::size_t qubit,
                                              const qc::OpType gate) const {
  switch (gate) {
  case qc::OpType::None:
  case qc::OpType::X:
  case qc::OpType::Y:
  case qc::OpType::Z:
    return z[pos][qubit];
  case qc::OpType::H:
    return x[pos][qubit];
  case qc::OpType::S:
  case qc::OpType::Sdg:
    return (z[pos][qubit] ^ x[pos][qubit]);
  default:
    const auto msg = "Unsupported single-qubit gate: " + toString(gate);
    PLOG_FATAL << msg;
    throw std::runtime_error(msg);
  }
}

LogicTerm
TableauEncoder::Variables::singleQubitRChange(const std::size_t pos,
                                              const std::size_t qubit,
                                              const qc::OpType gate) const {
  switch (gate) {
  case qc::OpType::None: {
    const auto bvs = r[pos].getBitVectorSize();
    return {0, bvs};
  }
  case qc::OpType::H:
  case qc::OpType::S:
    return x[pos][qubit] & z[pos][qubit];
  case qc::OpType::Sdg:
    return x[pos][qubit] & (x[pos][qubit] ^ z[pos][qubit]);
  case qc::OpType::X:
    return z[pos][qubit];
  case qc::OpType::Y:
    return x[pos][qubit] ^ z[pos][qubit];
  case qc::OpType::Z:
    return x[pos][qubit];
  default:
    const auto msg = "Unsupported single-qubit gate: " + toString(gate);
    PLOG_FATAL << msg;
    throw std::runtime_error(msg);
  }
}

std::pair<LogicTerm, LogicTerm>
TableauEncoder::Variables::twoQubitXChange(const std::size_t pos,
                                           const std::size_t ctrl,
                                           const std::size_t trgt) const {
  return {x[pos][ctrl], x[pos][ctrl] ^ x[pos][trgt]};
}

std::pair<LogicTerm, LogicTerm>
TableauEncoder::Variables::twoQubitZChange(const std::size_t pos,
                                           const std::size_t ctrl,
                                           const std::size_t trgt) const {
  return {z[pos][ctrl] ^ z[pos][trgt], z[pos][trgt]};
}

LogicTerm
TableauEncoder::Variables::twoQubitRChange(const std::size_t pos,
                                           const std::size_t ctrl,
                                           const std::size_t trgt) const {
  const auto bvs = r[pos].getBitVectorSize();
  const auto one = LogicTerm((1ULL << bvs) - 1, bvs);

  return (x[pos][ctrl] & z[pos][trgt]) & ((z[pos][ctrl] ^ x[pos][trgt]) ^ one);
}

} // namespace cs::encoding
