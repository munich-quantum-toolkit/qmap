/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
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
 * @brief Class to manage the mapping between circuit qubits and hardware qubits
 * in a bijective manner.
 */
class Mapping {
protected:
  // std::map<qc::Qubit, HwQubit>
  qc::Permutation circToHw;
  HardwareQubits hwQubits;
  qc::DAG dag;

  /**
   * @brief GraphMatching for initCoordMapping
   */
  [[nodiscard]]
  std::vector<CoordIndex> graphMatching();

public:
  Mapping() = default;
  explicit Mapping(const size_t nQubits) {
    for (size_t i = 0; i < nQubits; ++i) {
      circToHw.emplace(i, i);
    }
  }
  Mapping(const size_t nQubits, const InitialMapping initialMapping,
          qc::QuantumComputation qc, HardwareQubits hwQubits)
      : hwQubits(std::move(hwQubits)),
        dag(qc::CircuitOptimizer::constructDAG(qc)) {

    if (qc.getNqubits() > hwQubits.getNumQubits()) {
      throw std::runtime_error("Not enough qubits in architecture for circuit");
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
   * @param qubit The circuit qubit to be assigned
   * @param hwQubit The hardware qubit to be assigned
   */
  void setCircuitQubit(const qc::Qubit qubit, const HwQubit hwQubit) {
    circToHw[qubit] = hwQubit;
  }

  /**
   * @brief Returns the hardware qubit assigned to the given circuit qubit.
   * @param qubit The circuit qubit to be queried
   * @return The hardware qubit assigned to the given circuit qubit
   */
  [[nodiscard]] HwQubit getHwQubit(const qc::Qubit qubit) const {
    return circToHw.at(qubit);
  }

  /**
   * @brief Returns the hardware qubits assigned to the given circuit qubits.
   * @param qubits The circuit qubits to be queried
   * @return The hardware qubits assigned to the given circuit qubits
   */
  [[nodiscard]] std::set<HwQubit>
  getHwQubits(const std::set<qc::Qubit>& qubits) const {
    std::set<HwQubit> hwQubits;
    for (const auto& qubit : qubits) {
      hwQubits.emplace(this->getHwQubit(qubit));
    }
    return hwQubits;
  }

  /**
   * @brief Returns the hardware qubits assigned to the given circuit qubits.
   * @param qubits The circuit qubits to be queried
   * @return The hardware qubits assigned to the given circuit qubits
   */
  [[nodiscard]] std::vector<HwQubit>
  getHwQubits(const std::vector<qc::Qubit>& qubits) const {
    std::vector<HwQubit> hwQubits;
    for (const auto& qubit : qubits) {
      hwQubits.emplace_back(this->getHwQubit(qubit));
    }
    return hwQubits;
  }

  /**
   * @brief Returns the circuit qubit assigned to the given hardware qubit.
   * @details Throws an exception if the hardware qubit is not assigned to any
   * circuit qubit.
   * @param qubit The hardware qubit to be queried
   * @return The circuit qubit assigned to the given hardware qubit
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
   * @param qubit The hardware qubit to be queried
   * @return True if any circuit qubit is assigned to the given hardware qubit,
   * false otherwise
   */
  [[nodiscard]] bool isMapped(HwQubit qubit) const {
    return std::any_of(
        circToHw.begin(), circToHw.end(),
        [qubit](const auto& pair) { return pair.second == qubit; });
  }

  /**
   * @brief Converts the qubits of an operation from circuit qubits to hardware
   * qubits.
   * @param op The operation to be converted
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
   * @param swap The two circuit qubits to be swapped
   * @throws std::runtime_error if hardware qubits are not mapped
   */
  void applySwap(const Swap& swap);
};

} // namespace na
