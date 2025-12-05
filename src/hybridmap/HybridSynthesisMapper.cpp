/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

//
// This file is part of the MQT QMAP library released under the MIT license.
// See README.md or go to https://github.com/cda-tum/qmap for more information.
//
#include "hybridmap/HybridSynthesisMapper.hpp"

#include "hybridmap/HybridNeutralAtomMapper.hpp"
#include "hybridmap/NeutralAtomDefinitions.hpp"
#include "ir/Definitions.hpp"
#include "ir/QuantumComputation.hpp"

#include <cstddef>
#include <cstdint>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <utility>
#include <vector>

namespace na {

std::vector<qc::fp>
HybridSynthesisMapper::evaluateSynthesisSteps(qcs& synthesisSteps,
                                              const bool alsoMap) {
  if (synthesisSteps.empty()) {
    return {};
  }
  if (!initialized) {
    initMapping(synthesisSteps[0].getNqubits());
    initialized = true;
  }
  std::vector<std::pair<qc::QuantumComputation, qc::fp>> candidates;
  size_t qcIndex = 0;
  for (auto& qc : synthesisSteps) {
    if (parameters.verbose) {
      spdlog::info("Evaluating synthesis step number {}", qcIndex);
    }
    const auto fidelity = evaluateSynthesisStep(qc);
    candidates.emplace_back(qc, fidelity);
    if (parameters.verbose) {
      spdlog::info("Fidelity: {}", fidelity);
    }
    ++qcIndex;
  }
  std::vector<qc::fp> fidelities;
  size_t bestIndex = 0;
  qc::fp bestFidelity = 0;
  for (size_t i = 0; i < candidates.size(); ++i) {
    fidelities.push_back(candidates[i].second);
    if (candidates[i].second > bestFidelity) {
      bestFidelity = candidates[i].second;
      bestIndex = i;
    }
  }
  if (alsoMap && !candidates.empty()) {
    appendWithMapping(candidates[bestIndex].first);
  }
  return fidelities;
}

qc::fp
HybridSynthesisMapper::evaluateSynthesisStep(qc::QuantumComputation& qc) const {
  NeutralAtomMapper tempMapper(arch, parameters);
  tempMapper.copyStateFrom(*this);
  // Make a copy of qc to avoid modifying the original
  auto qcCopy = qc;
  auto mappedQc = tempMapper.map(qcCopy, mapping);
  tempMapper.convertToAod();
  const auto results = tempMapper.schedule();
  return results.totalFidelities;
}

void HybridSynthesisMapper::appendWithoutMapping(
    const qc::QuantumComputation& qc) {
  if (mappedQc.empty()) {
    initMapping(qc.getNqubits());
  }
  for (const auto& op : qc) {
    synthesizedQc.emplace_back(op->clone());
    mapGate(op.get());
  }
}

void HybridSynthesisMapper::appendWithMapping(qc::QuantumComputation& qc) {
  if (mappedQc.empty()) {
    initMapping(qc.getNqubits());
  }
  mapAppend(qc, mapping);
  for (const auto& op : qc) {
    synthesizedQc.emplace_back(op->clone());
  }
}

AdjacencyMatrix HybridSynthesisMapper::getCircuitAdjacencyMatrix() const {
  if (!initialized) {
    throw std::runtime_error(
        "Not yet initialized. Cannot get circuit adjacency matrix.");
  }
  const auto numCircQubits = synthesizedQc.getNqubits();
  AdjacencyMatrix adjMatrix(numCircQubits);

  for (uint32_t i = 0; i < numCircQubits; ++i) {
    for (uint32_t j = 0; j < numCircQubits; ++j) {
      if (i == j) {
        adjMatrix(i, j) = 0;
        continue;
      }
      const auto mappedI = mapping.getHwQubit(i);
      const auto mappedJ = mapping.getHwQubit(j);
      if (arch->getSwapDistance(mappedI, mappedJ) == 0) {
        adjMatrix(i, j) = 1;
        adjMatrix(j, i) = 1;
      } else {
        adjMatrix(i, j) = 0;
        adjMatrix(j, i) = 0;
      }
    }
  }
  return adjMatrix;
}

} // namespace na
