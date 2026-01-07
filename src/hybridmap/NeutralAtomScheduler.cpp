/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "hybridmap/NeutralAtomScheduler.hpp"

#include "hybridmap/HybridAnimation.hpp"
#include "hybridmap/NeutralAtomDefinitions.hpp"
#include "ir/Definitions.hpp"
#include "ir/QuantumComputation.hpp"
#include "ir/operations/OpType.hpp"
#include "ir/operations/Operation.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <spdlog/spdlog.h>
#include <string>
#include <utility>
#include <vector>

na::SchedulerResults na::NeutralAtomScheduler::schedule(
    const qc::QuantumComputation& qc,
    const std::map<HwQubit, CoordIndex>& initHwPos,
    const std::map<HwQubit, CoordIndex>& initFaPos, const bool verbose,
    const bool createAnimationCsv, const qc::fp shuttlingSpeedFactor) {
  animation.clear();
  animationMachine.clear();
  if (verbose) {
    spdlog::info("* schedule start!");
  }

  const auto nPositions = static_cast<std::size_t>(arch->getNpositions());
  const std::size_t numCoords = 3ULL * nPositions;
  std::vector totalExecutionTimes(numCoords, qc::fp{0});
  std::vector<std::deque<std::pair<qc::fp, qc::fp>>> rydbergBlockedQubitsTimes(
      numCoords);
  qc::fp aodLastBlockedTime = 0;
  qc::fp totalGateTime = 0;
  qc::fp totalGateFidelities = 1;

  std::optional<AnimationAtoms> animationAtoms;
  if (createAnimationCsv) {
    animationAtoms.emplace(initHwPos, initFaPos, *arch);
    animation += animationAtoms->placeInitAtoms();
    animationMachine = arch->getAnimationMachine(shuttlingSpeedFactor);
  }

  int index = 0;
  uint32_t nAodActivate = 0;
  uint32_t nAodMove = 0;
  uint32_t nCZs = 0;
  for (const auto& op : qc) {
    index++;
    if (verbose) {
      spdlog::info("{}", index);
    }
    if (op->getType() == qc::AodActivate) {
      nAodActivate++;
    } else if (op->getType() == qc::AodMove) {
      nAodMove++;
    } else if (op->getType() == qc::OpType::Z && op->getNcontrols() == 1) {
      nCZs++;
    }

    auto qubits = op->getUsedQubits();
    auto opTime = arch->getOpTime(op.get());
    if (op->getType() == qc::AodMove || op->getType() == qc::AodActivate ||
        op->getType() == qc::AodDeactivate) {
      opTime *= shuttlingSpeedFactor;
    }
    const auto opFidelity = arch->getOpFidelity(op.get());

    // DEBUG info
    if (verbose) {
      spdlog::info("{}", op->getName());
      // print control qubits
      for (const auto& c : op->getControls()) {
        spdlog::info("c{} ", c.qubit);
      }
      // print target qubits
      for (const auto& t : op->getTargets()) {
        spdlog::info("q{} ", t);
      }
      spdlog::info("-> time: {}, fidelity: {}", opTime, opFidelity);
    }

    qc::fp maxTime = 0;
    if (op->getType() == qc::AodMove || op->getType() == qc::AodActivate ||
        op->getType() == qc::AodDeactivate) {
      // AodBlocking
      maxTime = aodLastBlockedTime;
      for (const auto& qubit : qubits) {
        maxTime = std::max(maxTime, totalExecutionTimes[qubit]);
      }
      aodLastBlockedTime = maxTime + opTime;
    } else if (qubits.size() > 1) {
      // multi qubit gates -> take into consideration blocking
      auto rydbergBlockedQubits = arch->getBlockedCoordIndices(op.get());
      // get max execution time over all blocked qubits
      bool rydbergBlocked = true;
      while (rydbergBlocked) {
        // get regular max execution time
        for (const auto& qubit : qubits) {
          maxTime = std::max(maxTime, totalExecutionTimes[qubit]);
        }
        // check if all blocked qubits are free at maxTime
        rydbergBlocked = false;
        for (const auto& qubit : rydbergBlockedQubits) {
          // check if qubit is blocked at maxTime
          for (const auto& startEnd : rydbergBlockedQubitsTimes[qubit]) {
            const auto start = startEnd.first;
            const auto end = startEnd.second;
            if ((start <= maxTime && end > maxTime) ||
                (start <= maxTime + opTime && end > maxTime + opTime)) {
              rydbergBlocked = true;
              // update maxTime to the end of the blocking
              maxTime = std::max(maxTime, end);
              // remove the blocking
              break;
            }
            // skip rest of the times
            if (end > maxTime) {
              break;
            }
          }
        }
      }

      for (const auto& qubit : rydbergBlockedQubits) {
        rydbergBlockedQubitsTimes[qubit].emplace_back(maxTime,
                                                      maxTime + opTime);
      }

    } else {
      // other operations -> no blocking
      // get max execution time over all qubits
      for (const auto& qubit : qubits) {
        maxTime = std::max(maxTime, totalExecutionTimes[qubit]);
        // remove all blocked times that are smaller than maxTime
        while (!rydbergBlockedQubitsTimes[qubit].empty() &&
               rydbergBlockedQubitsTimes[qubit].front().second < maxTime) {
          rydbergBlockedQubitsTimes[qubit].pop_front();
        }
      }
    }
    // update total execution times
    for (const auto& qubit : qubits) {
      totalExecutionTimes[qubit] = maxTime + opTime;
    }

    totalGateFidelities *= opFidelity;
    totalGateTime += opTime;

    // update animation
    if (createAnimationCsv) {
      animation += animationAtoms->opToNaViz(op, maxTime);
    }
  }
  if (verbose) {
    spdlog::info("* schedule end!");
  }

  const auto maxExecutionTime = *std::ranges::max_element(totalExecutionTimes);
  const auto totalIdleTime =
      maxExecutionTime * arch->getNqubits() - totalGateTime;
  const auto totalFidelities =
      totalGateFidelities *
      std::exp(-totalIdleTime / arch->getDecoherenceTime());

  if (verbose) {
    printSchedulerResults(totalExecutionTimes, totalIdleTime,
                          totalGateFidelities, totalFidelities, nCZs,
                          nAodActivate, nAodMove);
  }
  return {maxExecutionTime, totalIdleTime, totalGateFidelities,
          totalFidelities,  nCZs,          nAodActivate,
          nAodMove};
}

void na::NeutralAtomScheduler::printSchedulerResults(
    std::vector<qc::fp>& totalExecutionTimes, const qc::fp totalIdleTime,
    const qc::fp totalGateFidelities, const qc::fp totalFidelities,
    const uint32_t nCZs, const uint32_t nAodActivate, const uint32_t nAodMove) {
  const auto totalExecutionTime = *std::ranges::max_element(
      totalExecutionTimes.begin(), totalExecutionTimes.end());
  spdlog::info("totalExecutionTimes: {}", totalExecutionTime);
  spdlog::info("totalIdleTime: {}", totalIdleTime);
  spdlog::info("totalGateFidelities: {}", totalGateFidelities);
  spdlog::info("totalFidelities: {}", totalFidelities);
  spdlog::info("totalNumCZs: {}", nCZs);
  spdlog::info("nAodActivate: {}", nAodActivate);
  spdlog::info("nAodMove: {}", nAodMove);
}
