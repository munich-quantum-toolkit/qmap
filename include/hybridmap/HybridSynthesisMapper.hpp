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

#pragma once

#include "HybridNeutralAtomMapper.hpp"
#include "NeutralAtomArchitecture.hpp"
#include "hybridmap/Mapping.hpp"
#include "hybridmap/NeutralAtomDefinitions.hpp"
#include "hybridmap/NeutralAtomScheduler.hpp"
#include "ir/Definitions.hpp"
#include "ir/QuantumComputation.hpp"
#include "na/ir/OpenQASMSerializer.hpp"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace na {

/**
 * @brief Bridges circuit synthesis (e.g., ZX extraction) with neutral-atom
 * mapping.
 * @details Derived from NeutralAtomMapper, this class maintains device state
 * and current mapping while accepting proposed synthesis steps. It evaluates
 * steps by mapping effort (e.g., required swaps/shuttling and timing), can
 * append steps with or without mapping, and exposes utilities to exchange
 * information with the synthesis algorithm.
 */
class HybridSynthesisMapper : public NeutralAtomMapper {
  using qcs = std::vector<qc::QuantumComputation>;

  qc::QuantumComputation synthesizedQc;
  /// Length of the routed prefix in an initialized synthesis session.
  std::optional<std::size_t> mappedOps;
  uint32_t bufferSize;

  /// Route through end. Supplying parameters restarts at the initial placement.
  void mapPending(std::size_t end,
                  const MapperParameters* restartWith = nullptr);

  /// Index of the first operation retained in the buffer after an append.
  [[nodiscard]] std::size_t bufferStart(std::size_t size) const;

public:
  // Constructors
  HybridSynthesisMapper() = delete;
  /**
   * @brief Create a HybridSynthesisMapper configured for a neutral-atom device.
   *
   * @param arch Neutral-atom device architecture used for mapping and
   * scheduling.
   * @param params Optional mapper configuration parameters; defaults to
   * MapperParameters().
   * @param bufferSize Number of operations to hold in the internal buffer
   * before flushing to the synthesized circuit (0 disables buffering).
   */
  explicit HybridSynthesisMapper(
      const NeutralAtomArchitecture& arch,
      const MapperParameters& params = MapperParameters(),
      const uint32_t bufferSize = 0)
      : NeutralAtomMapper(arch, params), bufferSize(bufferSize) {}

  /// Remap the committed prefix with new parameters and retain pending
  /// operations.
  void setParameters(const MapperParameters& p) override {
    mapPending(mappedOps.value_or(0), &p);
  }

  /// Discard the logical and routed circuits.
  void reset() override {
    *this = HybridSynthesisMapper(*arch, parameters, bufferSize);
  }

  /// Start a new synthesis session with an identity logical mapping.
  void initMapping(std::size_t width);

  /// Remap from the initial placement. Include pending operations by default.
  void completeRemap(bool includeBuffer = true) {
    mapPending(includeBuffer ? synthesizedQc.size() : mappedOps.value_or(0),
               &parameters);
  }

  /// Map any pending operations and schedule the resulting native circuit.
  [[nodiscard]] SchedulerResults
  schedule(const bool verboseArg = false, const bool createAnimationCsv = false,
           const qc::fp shuttlingSpeedFactor = 1.0) {
    mapPending(synthesizedQc.size());
    return NeutralAtomMapper::schedule(verboseArg, createAnimationCsv,
                                       shuttlingSpeedFactor);
  }

  /// Return the logical circuit, including operations still in the buffer.
  [[nodiscard]] qc::QuantumComputation getSynthesizedQc() const {
    return synthesizedQc;
  }

  /**
   * @brief Produce the OpenQASM representation of the synthesized circuit.
   *
   * Returns an OpenQASM string for the current synthesized circuit view,
   * including any buffered operations not yet scheduled.
   *
   * @return std::string OpenQASM text describing the synthesized (and buffered)
   * circuit.
   */
  [[nodiscard]] [[maybe_unused]] std::string getSynthesizedQcQASM() const {
    std::stringstream ss;
    serializeOpenQASM(synthesizedQc, ss);
    return ss.str();
  }

  /**
   * @brief Write the current synthesized circuit (including buffered
   * operations) to a file in OpenQASM format.
   *
   * @param filename Path to the output file where the OpenQASM representation
   * will be written.
   */
  [[maybe_unused]] void saveSynthesizedQc(const std::string& filename) const {
    std::ofstream ofs(filename);
    serializeOpenQASM(synthesizedQc, ofs);
  }

  /**
   * @brief Evaluate candidate synthesis steps and optionally map the best.
   * @param synthesisSteps Vector of candidate subcircuits.
   * @param completeRemap If true, completely remap before evaluation.
   * @param alsoMap If true, append and map the best candidate.
   * @return List of fidelity scores for mapped steps (order matches input).
   */
  std::vector<qc::fp> evaluateSynthesisSteps(qcs& synthesisSteps,
                                             bool completeRemap = false,
                                             bool alsoMap = false);

  /**
   * @brief Append and map a subcircuit to hardware (may insert moves/SWAPs).
   * @param qc Subcircuit to append and map.
   * @param completeRemap If true, completely remap before appending.
   */
  void appendWithMapping(qc::QuantumComputation& qc,
                         bool completeRemap = false);

  /**
   * @brief Get the current device adjacency (connectivity) matrix.
   * @return Symmetric adjacency matrix for the neutral atom hardware.
   */
  [[nodiscard]] AdjacencyMatrix getCircuitAdjacencyMatrix() const;
};
} // namespace na
