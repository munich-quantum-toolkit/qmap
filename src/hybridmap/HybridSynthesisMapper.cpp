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
#include <memory>
#include <span>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <utility>
#include <vector>

namespace na {

namespace {
void routeRange(
    NeutralAtomMapper& router, const qc::QuantumComputation& logical,
    const std::size_t begin, const std::size_t end,
    const std::span<const std::unique_ptr<qc::Operation>> candidate = {}) {
  if (begin == end) {
    return;
  }
  qc::QuantumComputation batch(logical.getNqubits());
  batch.reserve(end - begin);
  for (auto i = begin; i < end; ++i) {
    const auto& op =
        i < logical.size() ? logical.at(i) : candidate[i - logical.size()];
    batch.emplace_back(op->clone());
  }
  router.mapAppend(batch);
}
} // namespace

void HybridSynthesisMapper::initMapping(const std::size_t width) {
  if (mappedOps || !mappedQc.empty()) {
    NeutralAtomMapper::operator=(initialMapper(parameters, width));
  } else {
    setLogicalWidth(width);
  }
  synthesizedQc = qc::QuantumComputation(width);
  mappedOps = 0;
}

void HybridSynthesisMapper::mapPending(const std::size_t end,
                                       const MapperParameters* restartWith) {
  if (restartWith != nullptr) {
    auto fresh = initialMapper(*restartWith, synthesizedQc.getNqubits());
    routeRange(fresh, synthesizedQc, 0, end);
    NeutralAtomMapper::operator=(std::move(fresh));
  } else {
    routeRange(*this, synthesizedQc, mappedOps.value_or(0), end);
  }
  if (mappedOps) {
    mappedOps = end;
  }
}

std::size_t HybridSynthesisMapper::bufferStart(const std::size_t size) const {
  return size - std::min<std::size_t>(bufferSize, size - *mappedOps);
}

std::vector<qc::fp> HybridSynthesisMapper::evaluateSynthesisSteps(
    qcs& synthesisSteps, const bool completeRemap, const bool alsoMap) {
  if (synthesisSteps.empty()) {
    return {};
  }
  if (!mappedOps) {
    initMapping(synthesisSteps.front().getNqubits());
  }
  const auto start = completeRemap
                         ? initialMapper(parameters, synthesizedQc.getNqubits())
                         : continuationMapper();
  const auto begin = completeRemap ? 0U : *mappedOps;
  std::vector<qc::fp> fidelities;
  fidelities.reserve(synthesisSteps.size());
  for (const auto& candidate : synthesisSteps) {
    auto trial = start;
    const auto end = synthesizedQc.size() + candidate.size();
    const auto split = bufferStart(end);
    routeRange(trial, synthesizedQc, begin, split, candidate);
    routeRange(trial, synthesizedQc, split, end, candidate);
    fidelities.push_back(trial.schedule().totalFidelities);
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

void HybridSynthesisMapper::appendWithMapping(qc::QuantumComputation& qc,
                                              const bool completeRemap) {
  if (!mappedOps) {
    initMapping(qc.getNqubits());
  }
  for (const auto& op : qc) {
    synthesizedQc.emplace_back(op->clone());
  }
  mapPending(bufferStart(synthesizedQc.size()),
             completeRemap ? &parameters : nullptr);
}

AdjacencyMatrix HybridSynthesisMapper::getCircuitAdjacencyMatrix() const {
  if (!mappedOps) {
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
