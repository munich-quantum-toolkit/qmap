//
// This file is part of the MQT QMAP library released under the MIT license.
// See README.md or go to https://github.com/cda-tum/qmap for more information.
//

#pragma once

#include "Definitions.hpp"
#include "hybridmap/NeutralAtomArchitecture.hpp"
#include "hybridmap/NeutralAtomDefinitions.hpp"
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

  SchedulerResults(const qc::fp executionTime, const qc::fp idleTime,
                   const qc::fp gateFidelities, const qc::fp fidelities,
                   const uint32_t cZs, const uint32_t aodActivate,
                   const uint32_t aodMove)
      : totalExecutionTime(executionTime), totalIdleTime(idleTime),
        totalGateFidelities(gateFidelities), totalFidelities(fidelities),
        nCZs(cZs), nAodActivate(aodActivate), nAodMove(aodMove) {}

  [[nodiscard]] std::string toString() const {
    std::stringstream ss;
    ss << "Total execution time: " << totalExecutionTime;
    ss << "\nTotal idle time: " << totalIdleTime
       << "\nTotal fidelities: " << totalFidelities;
    return ss.str();
  }
  [[nodiscard]] std::string toCsv() const {
    std::stringstream ss;
    ss << totalExecutionTime << ", " << totalIdleTime << "," << totalFidelities;
    return ss.str();
  }

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
   * @param qc Quantum circuit to schedule
   * @param verbose If true, prints additional information
   * @return SchedulerResults
   */
  SchedulerResults schedule(const qc::QuantumComputation& qc,
                            const std::map<HwQubit, CoordIndex>& initHwPos,
                            const std::map<HwQubit, CoordIndex>& initFaPos,
                            bool verbose, bool createAnimationCsv = false,
                            qc::fp shuttlingSpeedFactor = 1.0);

  std::string getAnimationMachine() const { return animationMachine; }
  std::string getAnimationViz() const { return animation; }
  std::string getAnimationStyle() const { return animationStyle; }

  void saveAnimationFiles(const std::string& filename) const {
    const auto filenameWithoutExtension =
        filename.substr(0, filename.find_last_of('.'));
    const auto filenameViz = filenameWithoutExtension + ".naviz";
    const auto filenameMachine = filenameWithoutExtension + ".namachine";
    const auto filenameStyle = filenameWithoutExtension + ".nastyle";

    // save animation
    std::ofstream file = std::ofstream(filenameViz);
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
  static void printSchedulerResults(std::vector<qc::fp>& totalExecutionTimes,
                                    qc::fp totalIdleTime,
                                    qc::fp totalGateFidelities,
                                    qc::fp totalFidelities, uint32_t nCZs,
                                    uint32_t nAodActivate, uint32_t nAodMove);
  static void printTotalExecutionTimes(
      const std::vector<qc::fp>& totalExecutionTimes,
      const std::vector<std::deque<std::pair<qc::fp, qc::fp>>>&
          blockedQubitsTimes);
};

} // namespace na
