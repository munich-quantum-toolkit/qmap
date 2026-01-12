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
// See README.md or go to https://github.com/cda-tum/qmap for more information.
//

#pragma once

#include "HybridNeutralAtomMapper.hpp"
#include "NeutralAtomArchitecture.hpp"
#include "hybridmap/Mapping.hpp"
#include "hybridmap/NeutralAtomDefinitions.hpp"
#include "hybridmap/NeutralAtomScheduler.hpp"
#include "ir/Definitions.hpp"
#include "ir/QuantumComputation.hpp"

#include <cstdint>
#include <fstream>
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
  qc::QuantumComputation bufferedQc;
  uint32_t bufferSize;
  Mapping originalMapping;
  bool initialized = false;

  /**
   * @brief Evaluate a single proposed synthesis step.
   * @details Effort considers swaps/shuttling and execution time estimated by
   * the mapper.
   * @param qc Proposed synthesis subcircuit.
   * @param completeRemap If `true`, evaluate by completely remapping the whole
   * circuit; if `false`, preserve the current mapping state.
   * @return Scalar cost/effort score for mapping qc.
   */
  qc::fp evaluateSynthesisStep(qc::QuantumComputation& qc,
                               bool completeRemap = false) const;

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

  // Functions

  /**
   * @brief Initialize internal circuits and mapping state for a given number of
   * logical qubits.
   *
   * Sets up the synthesized and buffered quantum computations, the mapped
   * circuit storage, the logical-to-physical mapping, and records the original
   * mapping; marks the mapper as initialized.
   *
   * @param nQubits Number of logical qubits to prepare.
   * @throws std::runtime_error if `nQubits` exceeds the architecture's
   * available positions.
   */
  void initMapping(const size_t nQubits) {
    if (nQubits > arch->getNpositions()) {
      throw std::runtime_error("Not enough qubits in architecture.");
    }
    mappedQc = qc::QuantumComputation(arch->getNpositions());
    synthesizedQc = qc::QuantumComputation(nQubits);
    bufferedQc = qc::QuantumComputation(nQubits);
    mapping = Mapping(nQubits);
    initialized = true;
    originalMapping = mapping;
  }

  /**
   * @brief Remap the synthesized circuit onto the stored original hardware
   * mapping.
   *
   * If `includeBuffer` is true, the remap uses the combined
   * synthesized-plus-buffered circuit returned by getSynthesizedQc(); otherwise
   * it remaps only the internal synthesizedQc (excluding buffered operations).
   * The result is mapped into originalMapping.
   *
   * @param includeBuffer When true, include buffered operations in the remap.
   */
  void completeRemap(const bool includeBuffer = true) {
    if (includeBuffer) {
      auto copyQC = getSynthesizedQc();
      map(copyQC, originalMapping);
    } else {
      auto temp = synthesizedQc;
      map(temp, originalMapping);
    }
  }

  /**
   * @brief Appends buffered operations into the synthesized circuit, maps them,
   * and schedules the mapped circuit.
   *
   * Appends all operations currently held in the internal buffer to the
   * synthesized circuit, applies mapping for those appended operations, clears
   * the buffer, and then performs the scheduling pass using the base mapper.
   *
   * @param verboseArg Enable verbose scheduling output when true.
   * @param createAnimationCsv Emit scheduling animation CSV when true.
   * @param shuttlingSpeedFactor Factor to scale shuttling durations during
   * scheduling (1.0 = nominal speed).
   * @return SchedulerResults Results produced by the scheduling pass (timing
   * and placement outcomes).
   */
  [[nodiscard]] SchedulerResults
  schedule(const bool verboseArg = false, const bool createAnimationCsv = false,
           const qc::fp shuttlingSpeedFactor = 1.0) {
    for (const auto& op : bufferedQc) {
      synthesizedQc.emplace_back(op->clone());
    }
    mapAppend(bufferedQc, this->mapping);
    bufferedQc.clear();
    return NeutralAtomMapper::schedule(verboseArg, createAnimationCsv,
                                       shuttlingSpeedFactor);
  }

  /**
   * @brief Return a combined view of the synthesized and buffered (unmapped)
   * circuit.
   *
   * The returned QuantumComputation contains all operations from the mapper's
   * synthesized circuit followed by any buffered operations, using the mapper's
   * current qubit count.
   *
   * @return qc::QuantumComputation A new QuantumComputation with synthesized
   * operations then buffered operations.
   */
  [[nodiscard]] qc::QuantumComputation getSynthesizedQc() const {
    qc::QuantumComputation qc(synthesizedQc.getNqubits());
    qc.reserve(synthesizedQc.size() + bufferedQc.size());
    for (const auto& op : synthesizedQc) {
      qc.emplace_back(op->clone());
    }
    for (const auto& op : bufferedQc) {
      qc.emplace_back(op->clone());
    }
    return qc;
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
    const auto copyQC = getSynthesizedQc();
    copyQC.dumpOpenQASM(ss, false);
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
    const auto copyQC = getSynthesizedQc();
    copyQC.dumpOpenQASM(ofs, false);
    ofs.close();
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
