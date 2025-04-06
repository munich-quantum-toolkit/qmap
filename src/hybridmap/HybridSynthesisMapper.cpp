//
// This file is part of the MQT QMAP library released under the MIT license.
// See README.md or go to https://github.com/cda-tum/qmap for more information.
//
#include "hybridmap/HybridSynthesisMapper.hpp"

#include "Definitions.hpp"
#include "hybridmap/HybridNeutralAtomMapper.hpp"
#include "hybridmap/NeutralAtomDefinitions.hpp"
#include "ir/QuantumComputation.hpp"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <utility>
#include <vector>

namespace na {

std::vector<qc::fp> HybridSynthesisMapper::evaluateSynthesisSteps(
    qcs& synthesisSteps, const bool completeRemap, const bool alsoMap) {
  std::vector<std::pair<qc::QuantumComputation, qc::fp>> candidates;
  size_t qcIndex = 0;
  for (auto& qc : synthesisSteps) {
    if (this->parameters->verbose) {
      std::cout << "Evaluating synthesis step number " << qcIndex++ << "\n";
    }
    candidates.emplace_back(qc, this->evaluateSynthesisStep(qc, completeRemap));
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
    this->appendWithMapping(candidates[bestIndex].first, completeRemap);
  }
  return fidelities;
}

qc::fp
HybridSynthesisMapper::evaluateSynthesisStep(qc::QuantumComputation& qc,
                                             const bool completeRemap) const {
  NeutralAtomMapper tempMapper(*this->arch, *this->parameters);
  if (!completeRemap) {
    // copy the current mapping to the temporary mapper
    tempMapper.copyStateFrom(*this);
  } else {
    qc.reverse();
    for (auto opPointer = synthesizedQc.rbegin();
         opPointer != synthesizedQc.rend(); ++opPointer) {
      qc.emplace_back((*opPointer)->clone());
    }
    qc.reverse();
  }
  auto mappedQc = tempMapper.map(qc, mapping);
  tempMapper.convertToAod();
  const auto results = tempMapper.schedule();
  return results.totalFidelities;
}

void HybridSynthesisMapper::appendWithMapping(qc::QuantumComputation& qc,
                                              const bool completeRemap) {
  if (mappedQc.empty()) {
    initMapping(qc.getNqubits());
  }
  for (const auto& op : qc) {
    this->synthesizedQc.emplace_back(op->clone());
  }
  if (completeRemap) {
    this->completeRemap();
  } else {
    mapAppend(qc, this->mapping);
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
