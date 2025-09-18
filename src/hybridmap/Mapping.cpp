/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "hybridmap/Mapping.hpp"

#include "Definitions.hpp"
#include "hybridmap/NeutralAtomDefinitions.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <queue>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace na {
void Mapping::applySwap(const Swap& swap) {
  const auto q1 = swap.first;
  const auto q2 = swap.second;
  if (this->isMapped(q1) && this->isMapped(q2)) {
    const auto circQ1 = this->getCircQubit(q1);
    const auto circQ2 = this->getCircQubit(q2);
    this->setCircuitQubit(circQ2, q1);
    this->setCircuitQubit(circQ1, q2);
  } else if (this->isMapped(q1) && !this->isMapped(q2)) {
    this->setCircuitQubit(this->getCircQubit(q1), q2);
  } else if (this->isMapped(q2) && !this->isMapped(q1)) {
    this->setCircuitQubit(this->getCircQubit(q2), q1);
  } else {
    throw std::runtime_error("Cannot swap unmapped qubits");
  }
}

std::vector<CoordIndex> Mapping::graphMatching() {

  std::vector qubitIndices(dag.size(),
                           std::numeric_limits<unsigned int>::max());
  std::vector hwIndices(hwQubits.getNumQubits(),
                        std::numeric_limits<unsigned int>::max());

  // make hardware graph
  std::unordered_map<uint32_t, std::vector<uint32_t>> hwGraph;
  for (qc::Qubit i = 0; i < hwQubits.getNumQubits(); ++i) {
    auto neighbors = hwQubits.getNearbyQubits(i);
    hwGraph[i] = std::vector(neighbors.begin(), neighbors.end());
  }
  for (auto& [qubit, neighbors] : hwGraph) {
    std::sort(neighbors.begin(), neighbors.end(),
              [this](const uint32_t a, const uint32_t b) {
                return hwQubits.getNearbyQubits(a).size() >
                       hwQubits.getNearbyQubits(b).size();
              });
  }

  uint32_t hwCenter = std::numeric_limits<unsigned int>::max();
  size_t maxHwConnections = 0;
  for (const auto& [qubit, neighbors] : hwGraph) {
    if (neighbors.size() > maxHwConnections) {
      maxHwConnections = neighbors.size();
      hwCenter = qubit;
    }
  }

  // make circuit graph
  std::vector<std::vector<std::pair<qc::Qubit, double>>> circGraph(dag.size());
  for (qc::Qubit qubit = 0; qubit < dag.size(); ++qubit) {
    std::unordered_map<qc::Qubit, double> weightMap;
    for (const auto& opPtr : dag[qubit]) {
      const auto* op = opPtr->get();
      auto usedQubits = op->getUsedQubits();
      if (usedQubits.size() > 1) {
        for (auto i : usedQubits) {
          if (i != qubit) {
            weightMap[i] += 1.0;
          }
        }
      }
    }
    std::vector<std::pair<qc::Qubit, double>> neighbors(weightMap.begin(),
                                                        weightMap.end());
    std::sort(neighbors.begin(), neighbors.end(),
              [](const std::pair<qc::Qubit, double>& a,
                 const std::pair<qc::Qubit, double>& b) {
                return a.second > b.second;
              });
    circGraph[qubit] = std::move(neighbors);
  }

  // circuit queue for graph matching
  std::vector<std::pair<int, std::pair<int, double>>> nodes;
  for (size_t i = 0; i < circGraph.size(); ++i) {
    const auto degree = circGraph[i].size();
    double weightSum = 0;
    for (const auto& neighbor : circGraph[i]) {
      weightSum += neighbor.second;
    }
    nodes.emplace_back(i, std::make_pair(degree, weightSum));
  }
  std::sort(nodes.begin(), nodes.end(),
            [](const std::pair<int, std::pair<int, double>>& a,
               const std::pair<int, std::pair<int, double>>& b) {
              if (a.second.first == b.second.first) {
                return a.second.second > b.second.second;
              }
              return a.second.first > b.second.first;
            });
  std::queue<int> circGraphQueue;
  for (const auto& node : nodes) {
    circGraphQueue.push(node.first);
  }

  // graph matching -> return qubit Indices
  uint32_t nMapped = 0;
  bool firstCenter = true;
  while (!circGraphQueue.empty() && nMapped != dag.size()) {
    auto qi = circGraphQueue.front();
    HwQubit qI = std::numeric_limits<unsigned int>::max();
    //  center mapping
    if (qubitIndices[qi] == std::numeric_limits<unsigned int>::max()) {
      // first center
      if (firstCenter) {
        qI = hwCenter;
        firstCenter = false;
      }
      // next..
      else {
        auto minDistance = std::numeric_limits<qc::fp>::max();
        for (HwQubit qCandi = 0; qCandi < hwQubits.getNumQubits(); ++qCandi) {
          if (hwIndices[qCandi] != std::numeric_limits<unsigned int>::max()) {
            continue;
          }
          auto weightDistance = 0.0;
          for (auto qnPair : circGraph[qi]) {
            auto qn = qnPair.first;
            auto qnWeight = qnPair.second;
            HwQubit const qN = qubitIndices[qn];
            if (qN == std::numeric_limits<unsigned int>::max()) {
              continue;
            }
            weightDistance +=
                qnWeight * hwQubits.getSwapDistance(qN, qCandi, true);
          }
          if (weightDistance < minDistance) {
            minDistance = weightDistance;
            qI = qCandi;
          }
        }
      }
      qubitIndices[qi] = qI;
      hwIndices[qI] = qi;
      nMapped++;
    } else {
      qI = qubitIndices[qi];
    }
    // neighbor mapping
    for (auto& qnPair : circGraph[qi]) {
      auto const qn = qnPair.first;
      if (qubitIndices[qn] != std::numeric_limits<unsigned int>::max()) {
        continue;
      }
      HwQubit qN = std::numeric_limits<unsigned int>::max();
      for (const auto& qCandi : hwGraph[qI]) {
        if (hwIndices[qCandi] == std::numeric_limits<unsigned int>::max()) {
          qN = qCandi;
          break;
        }
      }
      if (qN != std::numeric_limits<unsigned int>::max()) {
        qubitIndices[qn] = qN;
        hwIndices[qN] = qn;
        nMapped++;
      }
    }
    circGraphQueue.pop();
  }

  // for debug
  for (size_t i = 0; i < dag.size(); ++i) {
    if (qubitIndices[i] == std::numeric_limits<unsigned int>::max()) {
      for (HwQubit hw = 0; hw < hwQubits.getNumQubits(); ++hw) {
        if (hwIndices[hw] == std::numeric_limits<unsigned int>::max()) {
          qubitIndices[i] = hw;
          hwIndices[hw] = i;
          break;
        }
      }
    }
  }

  return qubitIndices;
}

} // namespace na
