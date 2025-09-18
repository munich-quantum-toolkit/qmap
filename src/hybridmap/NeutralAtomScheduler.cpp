//
// This file is part of the MQT QMAP library released under the MIT license.
// See README.md or go to https://github.com/cda-tum/qmap for more information.
//

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

na::SchedulerResults na::NeutralAtomScheduler::schedule(
    const qc::QuantumComputation& qc,
    const std::map<HwQubit, CoordIndex>& initHwPos,
    const std::map<HwQubit, CoordIndex>& initFaPos, const bool verbose,
    const bool createAnimationCsv, const qc::fp shuttlingSpeedFactor) {
  if (verbose) {
    std::cout << "\n* schedule start!\n";
  }

  std::vector<qc::fp> totalExecutionTimes(3 * arch->getNpositions(), 0);
  // saves for each coord the time slots that are blocked by a multi qubit gate
  std::vector rydbergBlockedQubitsTimes(
      3 * arch->getNpositions(), std::deque<std::pair<qc::fp, qc::fp>>());
  qc::fp aodLastBlockedTime = 0;
  qc::fp totalGateTime = 0;
  qc::fp totalGateFidelities = 1;

  AnimationAtoms animationAtoms(initHwPos, initFaPos, *arch);
  if (createAnimationCsv) {
    animation += animationAtoms.placeInitAtoms();
    animationMachine = arch->getAnimationMachine(shuttlingSpeedFactor);
    animationStyle = arch->getAnimationStyle("");
  }

  int index = 0;
  uint32_t nAodActivate = 0;
  uint32_t nAodMove = 0;
  uint32_t nCZs = 0;
  for (const auto& op : qc) {
    index++;
    if (verbose) {
      std::cout << "\n" << index << "\n";
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
      std::cout << op->getName() << "  ";
      // print control qubits
      for (const auto& c : op->getControls()) {
        std::cout << "c" << c.qubit << " ";
      }
      // print target qubits
      for (const auto& t : op->getTargets()) {
        std::cout << "q" << t << " ";
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
      animation += animationAtoms.opToNaViz(op, maxTime);
    }
  }
  if (verbose) {
    std::cout << "\n* schedule end!\n";
  }

  const auto maxExecutionTime =
      *std::max_element(totalExecutionTimes.begin(), totalExecutionTimes.end());
  const auto totalIdleTime =
      (maxExecutionTime * arch->getNqubits()) - totalGateTime;
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
  const auto totalExecutionTime =
      *std::max_element(totalExecutionTimes.begin(), totalExecutionTimes.end());
  std::cout << "\ntotalExecutionTimes: " << totalExecutionTime << "\n";
  std::cout << "totalIdleTime: " << totalIdleTime << "\n";
  std::cout << "totalGateFidelities: " << totalGateFidelities << "\n";
  std::cout << "totalFidelities: " << totalFidelities << "\n";
  std::cout << "totalNumCZs: " << nCZs << "\n";
  std::cout << "nAodActivate: " << nAodActivate << "\n";
  std::cout << "nAodMove: " << nAodMove << "\n";
}

void na::NeutralAtomScheduler::printTotalExecutionTimes(
    const std::vector<qc::fp>& totalExecutionTimes,
    const std::vector<std::deque<std::pair<qc::fp, qc::fp>>>&
        blockedQubitsTimes) {
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
