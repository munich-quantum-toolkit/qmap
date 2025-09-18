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
#include <iostream>
#include <utility>
#include <vector>

namespace na {

std::vector<qc::fp>
HybridSynthesisMapper::evaluateSynthesisSteps(qcs& synthesisSteps,
                                              bool alsoMap) {
  std::vector<std::pair<qc::QuantumComputation, qc::fp>> candidates;
  size_t qcIndex = 0;
  for (auto& qc : synthesisSteps) {
    if (this->parameters->verbose) {
      std::cout << "Evaluating synthesis step number " << qcIndex++ << "\n";
    }
    candidates.emplace_back(qc, this->evaluateSynthesisStep(qc));
    if (this->parameters->verbose) {
      std::cout << "Fidelity: " << candidates.back().second << "\n";
    }
    qcIndex++;
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
  if (alsoMap) {
    this->appendWithMapping(candidates[bestIndex].first);
  }
  return fidelities;
}

qc::fp
HybridSynthesisMapper::evaluateSynthesisStep(qc::QuantumComputation& qc) {
  NeutralAtomMapper tempMapper;
  tempMapper.copyStateFrom(*this);
  auto mappedQc = tempMapper.map(qc, mapping);
  tempMapper.convertToAod();
  const auto results = tempMapper.schedule();
  return results.totalFidelities;
}

void HybridSynthesisMapper::appendWithoutMapping(
    const qc::QuantumComputation& qc) {
  for (const auto& op : qc) {
    this->synthesizedQc.emplace_back(op->clone());
    this->mapGate(op.get());
  }
}

void HybridSynthesisMapper::appendWithMapping(qc::QuantumComputation& qc) {
  if (mappedQc.empty()) {
    initMapping(qc.getNqubits());
  }
  mapAppend(qc, this->mapping);
  for (const auto& op : qc) {
    this->synthesizedQc.emplace_back(op->clone());
  }
}

AdjacencyMatrix HybridSynthesisMapper::getCircuitAdjacencyMatrix() const {
  auto numCircQubits = synthesizedQc.getNqubits();
  AdjacencyMatrix adjMatrix(numCircQubits);

  for (uint32_t i = 0; i < numCircQubits; ++i) {
    for (uint32_t j = 0; j < i; ++j) {
      auto mappedI = this->mapping.getHwQubit(i);
      auto mappedJ = this->mapping.getHwQubit(j);
      if (this->arch->getSwapDistance(mappedI, mappedJ) == 0) {
        adjMatrix(i, j) = 1;
      } else {
        adjMatrix(i, j) = 0;
      }
    }
  }
  return adjMatrix;
}

} // namespace na
