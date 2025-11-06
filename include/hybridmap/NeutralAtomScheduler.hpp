/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#ifndef HYBRIDMAP_NEUTRAL_ATOM_SCHEDULER_HPP
#define HYBRIDMAP_NEUTRAL_ATOM_SCHEDULER_HPP

#include "hybridmap/NeutralAtomArchitecture.hpp"
#include "hybridmap/NeutralAtomDefinitions.hpp"
#include "ir/Definitions.hpp"
#include "ir/QuantumComputation.hpp"

#include <cstdint>
#include <deque>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace na {
/**
 * @brief Struct to store the results of the scheduler
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
   * @brief Create a new results object.
   * @param executionTime The overall makespan (end time of the last operation).
   * @param idleTime The sum of all idling time across qubits: end_time *
   * n_qubits - total_gate_time.
   * @param gateFidelities Product of native gate fidelities (excludes
   * decoherence).
   * @param fidelities Overall fidelity including decoherence during idle time.
   * @param cZs The number of CZ operations encountered.
   * @param aodActivate The number of AOD activation operations encountered.
   * @param aodMove The number of AOD shuttling/move operations encountered.
   */
  SchedulerResults(const qc::fp executionTime, const qc::fp idleTime,
                   const qc::fp gateFidelities, const qc::fp fidelities,
                   const uint32_t cZs, const uint32_t aodActivate,
                   const uint32_t aodMove)
      : totalExecutionTime(executionTime), totalIdleTime(idleTime),
        totalGateFidelities(gateFidelities), totalFidelities(fidelities),
        nCZs(cZs), nAodActivate(aodActivate), nAodMove(aodMove) {}

  /**
   * @brief Export a compact CSV line with execution time, idle time, and
   * overall fidelity.
   * @return A string in the format "totalExecutionTime, totalIdleTime,
   * totalFidelities".
   */
  [[nodiscard]] std::string toCsv() const {
    std::stringstream ss;
    ss << totalExecutionTime << ", " << totalIdleTime << "," << totalFidelities;
    return ss.str();
  }

  /**
   * @brief Export selected metrics to a key-value map.
   * @details Currently includes totalExecutionTime, totalIdleTime,
   * totalGateFidelities, totalFidelities, and nCZs. Counts for
   * nAodActivate/nAodMove are not included.
   * @return An unordered_map from metric names to values.
   */
  [[maybe_unused]] [[nodiscard]] std::unordered_map<std::string, qc::fp>
  toMap() const {
    std::unordered_map<std::string, qc::fp> result;
    result["totalExecutionTime"] = totalExecutionTime;
    result["totalIdleTime"] = totalIdleTime;
    result["totalGateFidelities"] = totalGateFidelities;
    result["totalFidelities"] = totalFidelities;
    result["nCZs"] = nCZs;
    return result;
  }
};

/**
 * @brief Class to schedule a quantum circuit on a neutral atom architecture
 * @details For each gate/operation in the input circuit, the scheduler checks
 * the earliest possible time slot for execution. If the gate is a multi qubit
 * gate, also the blocking of other qubits is taken into consideration. The
 * execution times are read from the neutral atom architecture.
 */
class NeutralAtomScheduler {
protected:
  const NeutralAtomArchitecture* arch = nullptr;
  std::string animation;
  std::string animationMachine;
  std::string animationStyle;

public:
  // Constructor
  NeutralAtomScheduler() = default;
  explicit NeutralAtomScheduler(const NeutralAtomArchitecture& architecture)
      : arch(&architecture) {}

  /**
   * @brief Schedules the given quantum circuit on the neutral atom architecture
   * @details For each gate/operation in the input circuit, the scheduler checks
   * the earliest possible time slot for execution. If the gate is a multi qubit
   * gate, also the blocking of other qubits is taken into consideration. The
   * execution times are read from the neutral atom architecture.
   * Blocking windows for multi-qubit Rydberg interactions and AOD moves are
   * respected; optional animation traces can be produced.
   *
   * @param qc Quantum circuit to schedule.
   * @param initHwPos Initial positions of atoms on the hardware grid (by
   * hardware qubit).
   * @param initFaPos Initial positions of the addressing focus array (by
   * hardware qubit).
   * @param verbose If true, prints progress and a summary to std::cout.
   * @param createAnimationCsv If true, records animation artifacts for
   * visualization.
   * @param shuttlingSpeedFactor Scale factor applied to AOD
   * move/activate/deactivate durations (e.g., 0.5 for twice as fast shuttling).
   * @return Aggregated scheduling results including makespan, idle time, and
   * fidelities.
   */
  SchedulerResults schedule(const qc::QuantumComputation& qc,
                            const std::map<HwQubit, CoordIndex>& initHwPos,
                            const std::map<HwQubit, CoordIndex>& initFaPos,
                            bool verbose, bool createAnimationCsv = false,
                            qc::fp shuttlingSpeedFactor = 1.0);

  /**
   * @brief Get the machine description for the animation output.
   * @note Only populated when schedule(...) was run with
   * createAnimationCsv=true.
   */
  [[nodiscard]] std::string getAnimationMachine() const {
    return animationMachine;
  }
  /**
   * @brief Get the visualization event log in .naviz format.
   * @note Only populated when schedule(...) was run with
   * createAnimationCsv=true.
   */
  [[nodiscard]] std::string getAnimationViz() const { return animation; }
  /**
   * @brief Get the visualization style sheet for the animation.
   * @note Only populated when schedule(...) was run with
   * createAnimationCsv=true.
   */
  [[nodiscard]] std::string getAnimationStyle() const { return animationStyle; }

  /**
   * @brief Persist the generated animation artifacts to disk.
   * @details Creates three files next to the provided filename (without its
   * extension):
   *  - .naviz     (visualization event log)
   *  - .namachine (machine/layout description)
   *  - .nastyle   (visual style configuration)
   * The contents are derived from
   * getAnimationViz()/getAnimationMachine()/getAnimationStyle().
   * @param filename Base filename whose stem is reused for the three outputs.
   */
  void saveAnimationFiles(const std::string& filename) const {
    const auto filenameWithoutExtension =
        filename.substr(0, filename.find_last_of('.'));
    const auto filenameViz = filenameWithoutExtension + ".naviz";
    const auto filenameMachine = filenameWithoutExtension + ".namachine";
    const auto filenameStyle = filenameWithoutExtension + ".nastyle";

    // save animation
    auto file = std::ofstream(filenameViz);
    file << getAnimationViz();
    file.close();
    // save machine
    file.open(filenameMachine);
    file << getAnimationMachine();
    file.close();
    // save style
    file.open(filenameStyle);
    file << getAnimationStyle();
    file.close();
  }

  // Helper Print functions
  /**
   * @brief Print a human-readable summary of scheduling results.
   * @param totalExecutionTimes Per-qubit accumulated execution/makespan
   * timeline.
   * @param totalIdleTime Sum of idle time across all qubits.
   * @param totalGateFidelities Product of native gate fidelities.
   * @param totalFidelities Overall fidelity including decoherence during idle
   * time.
   * @param nCZs Number of CZ gates in the circuit.
   * @param nAodActivate Number of AOD activation operations.
   * @param nAodMove Number of AOD move operations.
   */
  static void printSchedulerResults(std::vector<qc::fp>& totalExecutionTimes,
                                    qc::fp totalIdleTime,
                                    qc::fp totalGateFidelities,
                                    qc::fp totalFidelities, uint32_t nCZs,
                                    uint32_t nAodActivate, uint32_t nAodMove);
};

} // namespace na

#endif // HYBRIDMAP_NEUTRAL_ATOM_SCHEDULER_HPP
