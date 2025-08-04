/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
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
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

na::SchedulerResults
na::NeutralAtomScheduler::schedule(const qc::QuantumComputation& qc,
                                   const std::map<HwQubit, HwQubit>& initHwPos,
                                   bool verbose, bool createAnimationCsv,
                                   qc::fp shuttlingSpeedFactor) {
  if (verbose) {
    std::cout << "\n* schedule start!\n";
  }

  std::vector<qc::fp> totalExecutionTimes(arch.getNpositions(), 0);
  // saves for each coord the time slots that are blocked by a multi qubit gate
  std::vector<std::deque<std::pair<qc::fp, qc::fp>>> rydbergBlockedQubitsTimes(
      arch.getNpositions(), std::deque<std::pair<qc::fp, qc::fp>>());
  qc::fp aodLastBlockedTime = 0;
  qc::fp totalGateTime = 0;
  qc::fp totalGateFidelities = 1;

  AnimationAtoms animationAtoms(initHwPos, arch);
  if (createAnimationCsv) {
    animationCsv += animationAtoms.getInitString();
    animationArchitectureCsv = arch.getAnimationCsv();
  }

  int index = 0;
  int nAodActivate = 0;
  uint32_t nCZs = 0;
  for (const auto& op : qc) {
    index++;
    if (verbose) {
      std::cout << "\n" << index << "\n";
    }
    if (op->getType() == qc::AodActivate) {
      nAodActivate++;
    } else if (op->getType() == qc::OpType::Z && op->getNcontrols() == 1) {
      nCZs++;
    }

    auto qubits = op->getUsedQubits();
    auto opTime = arch.getOpTime(op.get());
    if (op->getType() == qc::AodMove || op->getType() == qc::AodActivate ||
        op->getType() == qc::AodDeactivate) {
      opTime *= shuttlingSpeedFactor;
    }
    auto opFidelity = arch.getOpFidelity(op.get());

    // DEBUG info
    if (verbose) {
      std::cout << op->getName() << "  ";
      for (const auto& qubit : qubits) {
        std::cout << "q" << qubit << " ";
      }
      std::cout << "-> time: " << opTime << ", fidelity: " << opFidelity
                << "\n";
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
      auto rydbergBlockedQubits = arch.getBlockedCoordIndices(op.get());
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
            auto start = startEnd.first;
            auto end = startEnd.second;
            if ((start <= maxTime && end > maxTime) ||
                (start <= maxTime + opTime && end > maxTime + opTime)) {
              rydbergBlocked = true;
              // update maxTime to the end of the blocking
              maxTime = end;
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
    if (verbose) {
      std::cout << "\n";
      printTotalExecutionTimes(totalExecutionTimes, rydbergBlockedQubitsTimes);
    }

    // update animation
    if (createAnimationCsv) {
      animationCsv +=
          animationAtoms.createCsvOp(op, maxTime, maxTime + opTime, arch);
    }
  }
  if (verbose) {
    std::cout << "\n* schedule end!\n";
    std::cout << "nAodActivate: " << nAodActivate << "\n";
  }

  const auto maxExecutionTime = *std::ranges::max_element(totalExecutionTimes);
  const auto totalIdleTime =
      maxExecutionTime * arch.getNqubits() - totalGateTime;
  const auto totalFidelities =
      totalGateFidelities *
      std::exp(-totalIdleTime / arch.getDecoherenceTime());

  if (createAnimationCsv) {
    animationCsv += animationAtoms.getEndString(maxExecutionTime);
  }
  if (verbose) {
    printSchedulerResults(totalExecutionTimes, totalIdleTime,
                          totalGateFidelities, totalFidelities, nCZs);
  }
  return {maxExecutionTime, totalIdleTime, totalGateFidelities, totalFidelities,
          nCZs};
}

void na::NeutralAtomScheduler::printSchedulerResults(
    std::vector<qc::fp>& totalExecutionTimes, qc::fp totalIdleTime,
    qc::fp totalGateFidelities, qc::fp totalFidelities, uint32_t nCZs) {
  auto totalExecutionTime = *std::ranges::max_element(totalExecutionTimes);
  std::cout << "\ntotalExecutionTimes: " << totalExecutionTime << "\n";
  std::cout << "totalIdleTime: " << totalIdleTime << "\n";
  std::cout << "totalGateFidelities: " << totalGateFidelities << "\n";
  std::cout << "totalFidelities: " << totalFidelities << "\n";
  std::cout << "totalNumCZs: " << nCZs << "\n";
}

void na::NeutralAtomScheduler::printTotalExecutionTimes(
    std::vector<qc::fp>& totalExecutionTimes,
    std::vector<std::deque<std::pair<qc::fp, qc::fp>>>& blockedQubitsTimes) {
  std::cout << "ExecutionTime: "
            << "\n";
  for (size_t qubit = 0; qubit < totalExecutionTimes.size(); qubit++) {
    std::cout << "[" << qubit << "] " << totalExecutionTimes[qubit] << " \t";
    for (const auto& blockedTime : blockedQubitsTimes[qubit]) {
      std::cout << blockedTime.first << "-" << blockedTime.second << " \t";
    }
    std::cout << "\n";
  }
}
