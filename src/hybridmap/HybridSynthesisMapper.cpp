/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

//
// This file is part of the MQT QMAP library released under the MIT license.
// See README.md or go to https://github.com/munich-quantum-toolkit/qmap for
// more information.
//
#include "hybridmap/HybridSynthesisMapper.hpp"

#include "hybridmap/HybridNeutralAtomMapper.hpp"
#include "hybridmap/NeutralAtomDefinitions.hpp"
#include "ir/Definitions.hpp"
#include "ir/QuantumComputation.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <utility>
#include <vector>

namespace na {

void HybridSynthesisMapper::setParameters(const MapperParameters& p) {
  HybridSynthesisMapper next(*arch, p, bufferSize);
  if (initialized) {
    next.initMapping(synthesizedQc.getNqubits());
    next.synthesizedQc = synthesizedQc;
    next.mapPending(mappedOps);
  }
  *this = std::move(next);
}

void HybridSynthesisMapper::initMapping(const std::size_t nQubits) {
  if (nQubits > hardwareQubits.getNumQubits()) {
    throw std::runtime_error("Not enough qubits in architecture.");
  }
  if (initialized || !mappedQc.empty()) {
    NeutralAtomMapper::reset();
  }
  synthesizedQc = qc::QuantumComputation(nQubits);
  mapping = Mapping(nQubits);
  mappedOps = 0;
  initialized = true;
}

void HybridSynthesisMapper::mapPending(const std::size_t end) {
  if (end == mappedOps) {
    return;
  }
  qc::QuantumComputation pending(synthesizedQc.getNqubits());
  for (auto i = mappedOps; i < end; ++i) {
    pending.emplace_back(synthesizedQc.at(i)->clone());
  }
  mapAppend(pending);
  mappedOps = end;
}

void HybridSynthesisMapper::mapFromStart(const std::size_t end) {
  mapping = Mapping(synthesizedQc.getNqubits());
  mappedOps = 0;
  mapPending(end);
}

std::size_t HybridSynthesisMapper::bufferStart(const std::size_t size) const {
  return size - std::min<std::size_t>(bufferSize, size - mappedOps);
}

std::vector<qc::fp> HybridSynthesisMapper::evaluateSynthesisSteps(
    qcs& synthesisSteps, const bool completeRemap, const bool alsoMap) {
  if (synthesisSteps.empty()) {
    return {};
  }
  if (!initialized) {
    initMapping(synthesisSteps.front().getNqubits());
  }
  std::vector<qc::fp> fidelities;
  fidelities.reserve(synthesisSteps.size());
  for (auto& qc : synthesisSteps) {
    fidelities.push_back(evaluateSynthesisStep(qc, completeRemap));
    if (parameters.verbose) {
      spdlog::info("Synthesis step {} fidelity: {}", fidelities.size() - 1,
                   fidelities.back());
    }
  }
  if (alsoMap) {
    const auto best = std::ranges::max_element(fidelities);
    appendWithMapping(
        *(synthesisSteps.begin() + std::distance(fidelities.begin(), best)),
        completeRemap);
  }
  return fidelities;
}

qc::fp
HybridSynthesisMapper::evaluateSynthesisStep(qc::QuantumComputation& qc,
                                             const bool completeRemap) const {
  const auto width = synthesizedQc.getNqubits();
  NeutralAtomMapper trial(*arch, parameters);
  if (!completeRemap) {
    trial.copyStateFrom(*this);
  }
  const auto accepted = synthesizedQc.size();
  const auto size = accepted + qc.size();
  const auto split = bufferStart(size);
  // Use the same routing batches as appending the candidate and flushing it.
  const auto fragment = [&](std::size_t begin, std::size_t end) {
    qc::QuantumComputation batch(width);
    for (auto i = begin; i < end; ++i) {
      const auto& op = i < accepted ? synthesizedQc.at(i) : qc.at(i - accepted);
      batch.emplace_back(op->clone());
    }
    return batch;
  };
  auto prefix = fragment(completeRemap ? 0 : mappedOps, split);
  trial.mapAppend(prefix, completeRemap ? Mapping(width) : mapping);
  if (split < size) {
    auto pending = fragment(split, size);
    trial.mapAppend(pending);
  }
  return trial.schedule().totalFidelities;
}

void HybridSynthesisMapper::appendWithMapping(qc::QuantumComputation& qc,
                                              const bool completeRemap) {
  if (!initialized) {
    initMapping(qc.getNqubits());
  }
  for (const auto& op : qc) {
    synthesizedQc.emplace_back(op->clone());
  }
  const auto end = bufferStart(synthesizedQc.size());
  if (completeRemap) {
    NeutralAtomMapper::reset();
    mapFromStart(end);
  } else {
    mapPending(end);
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
    const auto neighbors =
        hardwareQubits.getNearbyQubits(mapping.getHwQubit(i));
    for (uint32_t j = 0; j < i; ++j) {
      adjMatrix(i, j) =
          static_cast<uint8_t>(neighbors.contains(mapping.getHwQubit(j)));
    }
  }
  return adjMatrix;
}

} // namespace na
