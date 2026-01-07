/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "hybridmap/Mapping.hpp"

#include "hybridmap/NeutralAtomDefinitions.hpp"
#include "ir/Definitions.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <queue>
#include <ranges>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace na {
void Mapping::applySwap(const Swap& swap) {
  const auto q1 = swap.first;
  const auto q2 = swap.second;
  if (isMapped(q1) && isMapped(q2)) {
    const auto circQ1 = getCircQubit(q1);
    const auto circQ2 = getCircQubit(q2);
    setCircuitQubit(circQ2, q1);
    setCircuitQubit(circQ1, q2);
  } else if (isMapped(q1) && !isMapped(q2)) {
    setCircuitQubit(getCircQubit(q1), q2);
  } else if (isMapped(q2) && !isMapped(q1)) {
    setCircuitQubit(getCircQubit(q2), q1);
  } else {
    throw std::runtime_error("Cannot swap unmapped qubits");
  }
}

std::vector<HwQubit> Mapping::graphMatching() {
  constexpr auto invalidHw = std::numeric_limits<HwQubit>::max();
  constexpr auto invalidCirc = std::numeric_limits<qc::Qubit>::max();
  if (dag.size() > hwQubits.getNumQubits()) {
    throw std::runtime_error(
        "graphMatching: more circuit qubits than hardware qubits");
  }
  std::vector<HwQubit> qubitIndices(dag.size(), invalidHw);
  std::vector<HwQubit> hwIndices(hwQubits.getNumQubits(), invalidCirc);
  // make hardware graph
  std::unordered_map<HwQubit, HwQubitsVector> hwGraph;
  for (HwQubit i = 0; i < hwQubits.getNumQubits(); ++i) {
    auto neighbors = hwQubits.getNearbyQubits(i);
    hwGraph[i] = std::vector(neighbors.begin(), neighbors.end());
  }
  for (auto& neighbors : hwGraph | std::views::values) {
    std::ranges::sort(neighbors, [this](const HwQubit a, const HwQubit b) {
      return hwQubits.getNearbyQubits(a).size() >
             hwQubits.getNearbyQubits(b).size();
    });
  }
  HwQubit hwCenter = invalidHw;
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
    std::ranges::sort(neighbors, [](const std::pair<qc::Qubit, double>& a,
                                    const std::pair<qc::Qubit, double>& b) {
      return a.second > b.second;
    });
    circGraph[qubit] = std::move(neighbors);
  }
  // circuit queue for graph matching
  std::vector<std::pair<qc::Qubit, std::pair<size_t, double>>> nodes;
  for (size_t i = 0; i < circGraph.size(); ++i) {
    const auto degree = circGraph[i].size();
    double weightSum = 0;
    for (const auto& val : circGraph[i] | std::views::values) {
      weightSum += val;
    }
    nodes.emplace_back(i, std::make_pair(degree, weightSum));
  }
  std::ranges::sort(nodes.begin(), nodes.end(),
                    [](const auto& a, const auto& b) {
                      if (a.second.first == b.second.first) {
                        return a.second.second > b.second.second;
                      }
                      return a.second.first > b.second.first;
                    });
  std::queue<qc::Qubit> circGraphQueue;
  for (const auto& key : nodes | std::views::keys) {
    circGraphQueue.push(key);
  }
  // graph matching -> return qubit Indices
  size_t nMapped = 0;
  bool firstCenter = true;
  while (!circGraphQueue.empty() && nMapped != dag.size()) {
    auto qi = circGraphQueue.front();
    HwQubit qI = invalidHw;
    //  center mapping
    if (qubitIndices[qi] == invalidHw) {
      // first center
      if (firstCenter) {
        if (hwCenter == invalidHw) {
          throw std::runtime_error("graphMatching: no hardware center qubit");
        }
        qI = hwCenter;
        firstCenter = false;
      }
      // next..
      else {
        auto minDistance = std::numeric_limits<qc::fp>::max();
        for (HwQubit qCandi = 0; qCandi < hwQubits.getNumQubits(); ++qCandi) {
          if (hwIndices[qCandi] != invalidCirc) {
            continue;
          }
          auto weightDistance = 0.0;
          for (const auto& qnPair : circGraph[qi]) {
            auto qn = qnPair.first;
            auto qnWeight = qnPair.second;
            HwQubit const qN = qubitIndices[qn];
            if (qN == invalidHw) {
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
        if (qI == invalidHw) {
          throw std::runtime_error(
              "graphMatching: no free hardware qubit for circuit qubit");
        }
      }
      qubitIndices[qi] = qI;
      hwIndices[qI] = qi;
      nMapped++;
    } else {
      qI = qubitIndices[qi];
    }
    // neighbor mapping
    for (auto& key : circGraph[qi] | std::views::keys) {
      auto const qn = key;
      if (qubitIndices[qn] != invalidHw) {
        continue;
      }
      HwQubit qN = invalidHw;
      for (const auto& qCandi : hwGraph[qI]) {
        if (hwIndices[qCandi] == invalidCirc) {
          qN = qCandi;
          break;
        }
      }
      if (qN != invalidHw) {
        qubitIndices[qn] = qN;
        hwIndices[qN] = qn;
        nMapped++;
      }
    }
    circGraphQueue.pop();
  }
  return qubitIndices;
}
} // namespace na
