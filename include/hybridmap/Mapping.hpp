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

#include "HardwareQubits.hpp"
#include "NeutralAtomArchitecture.hpp"
#include "circuit_optimizer/CircuitOptimizer.hpp"
#include "hybridmap/NeutralAtomDefinitions.hpp"
#include "hybridmap/NeutralAtomUtils.hpp"
#include "ir/Definitions.hpp"
#include "ir/Permutation.hpp"
#include "ir/QuantumComputation.hpp"
#include "ir/operations/Operation.hpp"

#include <algorithm>
#include <cstddef>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>

namespace na {

/**
 * @brief Maintains a bijective mapping between circuit (logical) and hardware
 * qubits.
 * @details Supports different initialization strategies, queries in both
 * directions, and in-place rewriting of operation qubit indices. The mapping is
 * kept one-to-one; swaps can update the mapping as the circuit is transformed.
 */
class Mapping {
protected:
  using DAG = std::vector<std::deque<std::unique_ptr<qc::Operation>*>>;

  qc::Permutation circToHw;
  HardwareQubits hwQubits;
  DAG dag;

  /**
   * @brief Compute an initial mapping via (heuristic) graph matching.
   * @details Matches circuit interaction structure to device structure to
   * reduce expected routing overhead.
   * @return Vector mapping logical qubit index i -> chosen hardware index for
   * qubit index i.
   */
  [[nodiscard]]
  std::vector<HwQubit> graphMatching();

public:
  /**
   * @brief Default-construct an empty mapping.
   */
  Mapping() = default;
  /**
   * @brief Initialize with identity mapping for the first nQubits.
   * @param nQubits Number of logical qubits; maps i -> i for i in [0,nQubits).
   */
  explicit Mapping(const size_t nQubits) {
    for (size_t i = 0; i < nQubits; ++i) {
      circToHw.emplace(i, i);
    }
  }
  /**
   * @brief Construct a mapping using a chosen initialization strategy.
   * @param nQubits Number of logical qubits to map.
   * @param initialMapping Initialization strategy (Identity or Graph).
   * @param qc Circuit used to derive structure for graph-based initialization.
   * @param hwQubitsArg Target hardware description (capacity/topology
   * constraints).
   * @throw std::runtime_error If the circuit has more qubits than available
   * hardware qubits.
   */
  Mapping(const size_t nQubits, const InitialMapping initialMapping,
          qc::QuantumComputation& qc, HardwareQubits hwQubitsArg)
      : hwQubits(std::move(hwQubitsArg)),
        dag(qc::CircuitOptimizer::constructDAG(qc)) {

    if (qc.getNqubits() > hwQubits.getNumQubits()) {
      throw std::runtime_error("Not enough qubits in architecture for circuit");
    }
    if (nQubits > qc.getNqubits()) {
      throw std::runtime_error(
          "nQubits exceeds number of qubits in provided circuit");
    }

    switch (initialMapping) {
    case Identity:
      for (size_t i = 0; i < nQubits; ++i) {
        circToHw.emplace(i, i);
      }
      break;
    case Graph:
      const auto qubitIndices = graphMatching();
      for (size_t i = 0; i < nQubits; i++) {
        circToHw.emplace(i, qubitIndices[i]);
      }
      break;
    }
  }
  /**
   * @brief Assigns a circuit qubit to a hardware qubit.
   * @param qubit Circuit qubit to assign.
   * @param hwQubit Hardware qubit index.
   */
  void setCircuitQubit(const qc::Qubit qubit, const HwQubit hwQubit) {
    circToHw[qubit] = hwQubit;
  }

  /**
   * @brief Returns the hardware qubit assigned to the given circuit qubit.
   * @param qubit Circuit qubit to query.
   * @return Hardware qubit assigned to the given circuit qubit.
   * @throw std::out_of_range If the circuit qubit is not present in the
   * mapping.
   */
  [[nodiscard]] HwQubit getHwQubit(const qc::Qubit qubit) const {
    return circToHw.at(qubit);
  }

  /**
   * @brief Returns the hardware qubits assigned to the given circuit qubits.
   * @param qubits Set of circuit qubits to query.
   * @return Set of corresponding hardware qubits.
   * @throw std::out_of_range If any circuit qubit is not present in the
   * mapping.
   */
  [[nodiscard]] std::set<HwQubit>
  getHwQubits(const std::set<qc::Qubit>& qubits) const {
    std::set<HwQubit> hw;
    for (const auto& qubit : qubits) {
      hw.emplace(getHwQubit(qubit));
    }
    return hw;
  }

  /**
   * @brief Returns the hardware qubits assigned to the given circuit qubits.
   * @param qubits Ordered list of circuit qubits to query.
   * @return Vector of corresponding hardware qubits (same order as input).
   * @throw std::out_of_range If any circuit qubit is not present in the
   * mapping.
   */
  [[nodiscard]] std::vector<HwQubit>
  getHwQubits(const std::vector<qc::Qubit>& qubits) const {
    std::vector<HwQubit> hw;
    hw.reserve(qubits.size());
    for (const auto& qubit : qubits) {
      hw.emplace_back(getHwQubit(qubit));
    }
    return hw;
  }

  /**
   * @brief Returns the circuit qubit assigned to the given hardware qubit.
   * @details Throws an exception if the hardware qubit is not assigned to any
   * circuit qubit.
   * @param qubit Hardware qubit to query.
   * @return Circuit qubit assigned to the given hardware qubit.
   * @throw std::runtime_error If the hardware qubit is not found in the
   * mapping.
   */
  [[nodiscard]] qc::Qubit getCircQubit(const HwQubit qubit) const {
    for (const auto& [circQubit, hwQubit] : circToHw) {
      if (hwQubit == qubit) {
        return circQubit;
      }
    }
    throw std::runtime_error("Hardware qubit: " + std::to_string(qubit) +
                             " not found in mapping");
  }

  /**
   * @brief Indicates if any circuit qubit is assigned to the given hardware
   * qubit.
   * @param qubit Hardware qubit to query.
   * @return True if any circuit qubit currently maps to this hardware qubit;
   * false otherwise.
   */
  [[nodiscard]] bool isMapped(HwQubit qubit) const {
    return std::ranges::any_of(
        circToHw, [qubit](const auto& pair) { return pair.second == qubit; });
  }

  /**
   * @brief Converts the qubits of an operation from circuit qubits to hardware
   * qubits.
   * @details Rewrites targets (and controls, if present) in-place using the
   * current mapping.
   * @param op Operation to be converted (modified in place).
   */
  void mapToHwQubits(qc::Operation* op) const {
    op->setTargets(circToHw.apply(op->getTargets()));
    if (op->isControlled()) {
      op->setControls(circToHw.apply(op->getControls()));
    }
  }

  /**
   * @brief Interchanges the mapping of two hardware qubits. At least one of it
   * must be mapped to a circuit qubit.
   * @param swap Pair of hardware qubits whose mapped circuit qubits shall be
   * swapped.
   * @throw std::runtime_error If neither hardware qubit is currently mapped.
   */
  void applySwap(const Swap& swap);
};

} // namespace na
