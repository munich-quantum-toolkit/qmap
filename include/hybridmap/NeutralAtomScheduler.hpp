/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#pragma once

#include "hybridmap/NeutralAtomArchitecture.hpp"
#include "hybridmap/NeutralAtomDefinitions.hpp"
#include "ir/Definitions.hpp"
#include "ir/QuantumComputation.hpp"

#include <cstdint>
#include <deque>
#include <fstream>
#include <map>
#include <spdlog/spdlog.h>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace na {
/**
 * @brief Aggregated metrics produced by the neutral atom scheduler.
 * @details Captures timing and fidelity information over the entire scheduled
 * circuit: total execution (makespan), accumulated idle time, raw gate fidelity
 * product (excluding decoherence), overall fidelity (including idle
 * decoherence), and counts of selected operation types.
 */
struct SchedulerResults {
  qc::fp totalExecutionTime;
  qc::fp totalIdleTime;
  qc::fp totalGateFidelities;
  qc::fp totalFidelities;
  uint32_t nCZs = 0;
  uint32_t nAodActivate = 0;
  uint32_t nAodMove = 0;

  /**
   * @brief Construct and initialize scheduler result metrics.
   * @param executionTime Overall makespan (end time of last operation).
   * @param idleTime Sum of idle time across qubits: end_time * n_qubits -
   * total_gate_time.
   * @param gateFidelities Product of native gate fidelities (excluding
   * decoherence).
   * @param fidelities Overall fidelity including idle-time decoherence.
   * @param cZs Number of CZ operations.
   * @param aodActivate Number of AOD activation operations.
   * @param aodMove Number of AOD shuttling/move operations.
   */
  SchedulerResults(const qc::fp executionTime, const qc::fp idleTime,
                   const qc::fp gateFidelities, const qc::fp fidelities,
                   const uint32_t cZs, const uint32_t aodActivate,
                   const uint32_t aodMove)
      : totalExecutionTime(executionTime), totalIdleTime(idleTime),
        totalGateFidelities(gateFidelities), totalFidelities(fidelities),
        nCZs(cZs), nAodActivate(aodActivate), nAodMove(aodMove) {}

  /**
   * @brief Export a compact CSV line with execution time, idle time, fidelity.
   * @return String formatted as: totalExecutionTime, totalIdleTime,
   * totalFidelities
   */
  [[nodiscard]] std::string toCsv() const {
    std::stringstream ss;
    ss << totalExecutionTime << ", " << totalIdleTime << "," << totalFidelities;
    return ss.str();
  }

  /**
   * @brief Export selected metrics to a key-value map.
   * @details Includes totalExecutionTime, totalIdleTime, totalGateFidelities,
   * totalFidelities, nCZs, nAodActivate, nAodMove.
   * @return Unordered map from metric names to numeric values.
   */
  [[maybe_unused]] [[nodiscard]] std::unordered_map<std::string, qc::fp>
  toMap() const {
    std::unordered_map<std::string, qc::fp> result;
    result["totalExecutionTime"] = totalExecutionTime;
    result["totalIdleTime"] = totalIdleTime;
    result["totalGateFidelities"] = totalGateFidelities;
    result["totalFidelities"] = totalFidelities;
    result["nCZs"] = nCZs;
    result["nAodActivate"] = static_cast<qc::fp>(nAodActivate);
    result["nAodMove"] = static_cast<qc::fp>(nAodMove);
    return result;
  }
};

/**
 * @brief Schedules quantum circuits on a neutral atom architecture.
 * @details Iterates operations chronologically assigning earliest feasible
 * start times respecting per-gate durations, multi-qubit blocking windows (e.g.
 * Rydberg interaction zones), and AOD move/activation timing. Optionally
 * records visualization artifacts for animation.
 */
class NeutralAtomScheduler {
protected:
  const NeutralAtomArchitecture* arch;
  std::string animation;
  std::string animationMachine;

public:
  NeutralAtomScheduler() = delete;
  /**
   * @brief Construct with a given neutral atom architecture.
   * @param architecture Architecture reference whose timing data is used.
   */
  explicit NeutralAtomScheduler(const NeutralAtomArchitecture& architecture)
      : arch(&architecture) {}

  /**
   * @brief Schedule a quantum circuit on the architecture.
   * @details Greedily assigns earliest feasible start times to each operation
   * while tracking per-qubit availability and multi-qubit blocking intervals.
   * Generates optional animation traces.
   * @param qc Quantum circuit to schedule.
   * @param initHwPos Initial atom positions indexed by hardware qubit.
   * @param initFaPos Initial AOD focus array positions indexed by hardware
   * qubit.
   * @param verbose If true, prints progress and summary to stdout.
   * @param createAnimationCsv If true, records animation artifacts
   * (.naviz/.namachine).
   * @param shuttlingSpeedFactor Factor scaling AOD move/activation durations
   * (1.0 = unchanged).
   * @return SchedulerResults containing makespan, idle time, fidelity metrics,
   * and operation counts.
   */
  SchedulerResults schedule(const qc::QuantumComputation& qc,
                            const std::map<HwQubit, CoordIndex>& initHwPos,
                            const std::map<HwQubit, CoordIndex>& initFaPos,
                            bool verbose, bool createAnimationCsv = false,
                            qc::fp shuttlingSpeedFactor = 1.0);

  /**
   * @brief Retrieve machine/layout description (.namachine content).
   * @return Machine description string.
   * @note Populated only if schedule() ran with createAnimationCsv=true.
   */
  [[nodiscard]] std::string getAnimationMachine() const {
    return animationMachine;
  }
  /**
   * @brief Retrieve visualization event log (.naviz content).
   * @return Event log string.
   * @note Populated only if schedule() ran with createAnimationCsv=true.
   */
  [[nodiscard]] std::string getAnimationViz() const { return animation; }

  /**
   * @brief Write animation artifacts (.naviz/.namachine) to disk.
   * @details Uses the stem of the provided filename to derive target paths for
   * each artifact.
   * @param filename Base filename (its extension is stripped before appending
   * artifact extensions).
   */
  void saveAnimationFiles(const std::string& filename) const {
    if (animation.empty() || animationMachine.empty()) {
      SPDLOG_WARN("No animation data to save; did you run schedule() with "
                  "createAnimationCsv=true?");
      return;
    }
    const auto filenameWithoutExtension =
        filename.substr(0, filename.find_last_of('.'));
    const auto filenameViz = filenameWithoutExtension + ".naviz";
    const auto filenameMachine = filenameWithoutExtension + ".namachine";

    // save animation
    auto file = std::ofstream(filenameViz);
    file << getAnimationViz();
    file.close();
    // save machine
    file.open(filenameMachine);
    file << getAnimationMachine();
    file.close();
  }

  // Helper Print functions
  /**
   * @brief Print a human-readable summary of scheduling results.
   * @param totalExecutionTimes Per-qubit cumulative execution times.
   * @param totalIdleTime Sum of idle time across all qubits.
   * @param totalGateFidelities Product of native gate fidelities.
   * @param totalFidelities Overall fidelity including idle-time decoherence.
   * @param nCZs Count of CZ gates.
   * @param nAodActivate Count of AOD activation operations.
   * @param nAodMove Count of AOD move operations.
   */
  static void printSchedulerResults(std::vector<qc::fp>& totalExecutionTimes,
                                    qc::fp totalIdleTime,
                                    qc::fp totalGateFidelities,
                                    qc::fp totalFidelities, uint32_t nCZs,
                                    uint32_t nAodActivate, uint32_t nAodMove);
};

} // namespace na
