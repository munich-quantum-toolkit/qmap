/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "cliffordsynthesis/encoding/ObjectiveEncoder.hpp"

#include "cliffordsynthesis/TargetMetric.hpp"
#include "cliffordsynthesis/encoding/GateEncoder.hpp"
#include "ir/operations/OpType.hpp"
#include "logicblocks/LogicTerm.hpp"

#include <cstddef>
#include <functional>
#include <plog/Log.h>
#include <stdexcept>

namespace cs::encoding {

using namespace logicbase;

LogicTerm
ObjectiveEncoder::collectGateCount(const bool includeSingleQubitGates) const {
  auto cost = LogicTerm(0);
  for (std::size_t t = 0U; t < T; ++t) {
    if (includeSingleQubitGates) {
      collectSingleQubitGateTerms(t, cost, std::plus{});
    }
    collectTwoQubitGateTerms(t, cost, std::plus{});
  }
  return cost;
}

void ObjectiveEncoder::optimizeGateCount(
    const bool includeSingleQubitGates) const {
  PLOG_DEBUG << "Optimizing " << (includeSingleQubitGates ? "" : "two-qubit ")
             << "gate count";
  const auto cost = collectGateCount(includeSingleQubitGates);
  dynamic_cast<LogicBlockOptimizer*>(lb.get())->minimize(cost);
}

void ObjectiveEncoder::optimizeDepth() const {
  PLOG_DEBUG << "Optimizing depth";
  auto* optimizer = dynamic_cast<LogicBlockOptimizer*>(lb.get());

  constexpr auto noGateIndex = GateEncoder::gateToIndex(qc::OpType::None);
  for (std::size_t t = 0U; t < T; ++t) {
    const auto& gS = gvars->gS[t];
    auto noGate = LogicTerm(true);
    for (std::size_t q = 0U; q < N; ++q) {
      noGate = noGate && gS[noGateIndex][q];
    }
    optimizer->weightedTerm(!noGate, 1);
  }
  optimizer->makeMinimize();
}

void ObjectiveEncoder::optimizeMetric(TargetMetric targetMetric) const {
  switch (targetMetric) {
  case TargetMetric::Gates:
    optimizeGateCount();
    break;
  case TargetMetric::TwoQubitGates:
    optimizeGateCount(false);
    break;
  case TargetMetric::Depth:
    optimizeDepth();
    break;
  default:
    const auto msg = "Unknown target metric: " + toString(targetMetric);
    PLOG_FATAL << msg;
    throw std::runtime_error(msg);
  }
}
} // namespace cs::encoding
