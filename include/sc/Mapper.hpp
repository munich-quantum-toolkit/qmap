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

#include "Architecture.hpp"
#include "MappingResults.hpp"
#include "configuration/Configuration.hpp"
#include "ir/Definitions.hpp"
#include "ir/QuantumComputation.hpp"
#include "ir/operations/Operation.hpp"
#include "utils.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <map>
#include <nlohmann/json.hpp>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

/// Number of gates in each operand order for each logical qubit pair.
using TwoQubitMultiplicity =
    std::map<Edge, std::pair<std::uint16_t, std::uint16_t>>;

/// Gate counts for a pair of logical qubits in ascending order.
struct TwoQubitGateCounts {
  struct Direction {
    std::uint32_t cnots = 0;
    bool requiresOrder = false;
  };
  Direction forward;
  Direction backward;
};

/**
 * number of single-qubit gates acting on each logical qubit in some
 * layer.
 *
 * e.g. with multiplicity {1,0,2} there is 1 1Q-gate acting on q0, no 1Q-gates
 * acting on q1, and 2 1Q-gates acting on q2
 */
using SingleQubitMultiplicity = std::vector<std::uint16_t>;

constexpr std::int16_t DEFAULT_POSITION = -1;

class Mapper {
protected:
  enum class Reversal { None, Exchange, NegateBeta, CNOT };

  /// Logical operands in the order used by the source operation.
  struct Gate {
    std::uint16_t first;
    std::optional<std::uint16_t> second;
    const qc::Operation* op;
    Reversal reversal;

    [[nodiscard]] bool singleQubit() const { return !second.has_value(); }
    [[nodiscard]] Edge pair() const {
      return std::minmax(first, second.value());
    }
    [[nodiscard]] bool conflictsWith(const Edge& edge,
                                     const TwoQubitGateCounts& counts) const;
  };

  static Reversal reversalFor(const qc::Operation& op);

  /**
   * @brief The quantum circuit to be mapped
   */
  qc::QuantumComputation qc;
  /**
   * @brief The quantum architecture on which to map the circuit
   */
  Architecture* architecture;

  /**
   * @brief The resulting quantum circuit after mapping
   */
  qc::QuantumComputation qcMapped;
  struct Layer {
    void add(const Gate& gate);

    std::vector<Gate> gates;
    std::map<Edge, TwoQubitGateCounts> interactions;
    std::set<std::uint16_t> activeQubits;
    std::set<std::uint16_t> activeQubits2QGates;
  };
  std::vector<Layer> layers;
  bool fidelityAwareHeur = false;
  bool assumeIdealSingleQubits = false;

  /**
   * @brief containing the logical qubit currently mapped to each physical
   * qubit. `qubits[physical_qubit] = logical_qubit`
   *
   * The inverse of `locations`
   */
  std::vector<std::int16_t> qubits;
  /**
   * @brief containing the logical qubit currently mapped to each physical
   * qubit. `locations[logical_qubit] = physical_qubit`
   *
   * The inverse of `qubits`
   */
  std::vector<std::int16_t> locations;

  MappingResults results;

  /**
   * @brief Initialize the results structure with circuit names, registers in
   * the output circuit, gate counts, etc.
   */
  virtual void initResults();

  [[nodiscard]] Architecture::GateFidelity gateFidelity(const Gate& gate,
                                                        const Edge& edge) const;

  /// Emit the gate on its physical operands.
  void appendMappedGate(const Gate& gate, std::uint16_t first);

  /**
   * @brief Splits the circuit into layers according to the method set in
   * `config.layering` and saves the result in `layers`
   *
   * methods of layering described in
   * https://iic.jku.at/files/eda/2019_dac_mapping_quantum_circuits_ibm_architectures_using_minimal_number_swap_h_gates.pdf
   *
   * Layering::IndividualGates -> each gate on separate layer
   * Layering::DisjointQubits -> each layer contains gates only acting on a
   * disjoint set of qubits
   * Layering::OddGates -> always 2 gates per layer
   * (assigned by order of original gate index in the circuit)
   * Layering::QubitTriangle -> intended for architectures which contain
   * triangles of physical qubits, each layer only contains gates acting on 3
   * distinct qubits
   * Layering::Disjoint2qBlocks -> each layer contains 2Q-Blocks only acting on
   * a disjoint set of qubits
   */
  virtual void createLayers();

  /**
   * @brief Returns true if the layer at the given index can be split into two
   * without resulting in an empty layer (assuming the original layer only has
   * disjoint 2Q-gate-blocks)
   *
   * @param index the index of the layer to be split
   * @return true if the layer is splittable
   * @return false if splitting the layer will result in an empty layer
   */
  virtual bool isLayerSplittable(std::size_t index);

  /**
   * @brief Splits the layer at the given index into two layers with half as
   * many qubits acted on by gates in each layer
   *
   * @param index the index of the layer to be split
   */
  virtual void splitLayer(std::size_t index);
  void splitLayerAt(std::size_t index, std::size_t offset);

  /**
   * @brief Get the index of the next layer after the given index containing a
   * gate acting on more than one qubit
   */
  [[gnu::pure]] virtual std::size_t getNextLayer(std::size_t idx);

  /// Assign free physical positions to idle inputs and spare device qubits.
  virtual void placeRemainingArchitectureQubits();

  /**
   * @brief finalizes the circuit after mapping
   * (e.g. adding unused qubits if architecture has more physical qubits than
   * mapped circuit has logical qubits)
   */
  virtual void finalizeMappedCircuit();

  /**
   * @brief count number of elementary gates and cnots in circuit and save the
   * results in `info.gates` and `info.cnots`
   * @param mapped Whether circuit operands identify physical qubits.
   */
  virtual void countGates(const qc::QuantumComputation& circuit,
                          MappingResults::CircuitInfo& info,
                          bool mapped = true) {
    countGates(circuit.cbegin(), circuit.cend(), info, mapped);
  }
  /**
   * @brief count number of elementary gates and cnots in circuit and save the
   * results in `info.gates` and `info.cnots`
   */
  virtual void countGates(decltype(qcMapped.cbegin()) it,
                          const decltype(qcMapped.cend())& end,
                          MappingResults::CircuitInfo& info, bool mapped);

  /**
   * @brief performs optimizations on the circuit before mapping
   *
   * @param config contains settings of the current mapping run (e.g.
   * `config.preMappingOptimizations` controls if pre-mapping optimizations are
   * performed)
   */
  virtual void preMappingOptimizations(const Configuration& config);

  /**
   * @brief performs optimizations on the circuit before mapping
   *
   * @param config contains settings of the current mapping run (e.g.
   * `config.postMappingOptimizations` controls if post-mapping optimizations
   * are performed)
   */
  virtual void postMappingOptimizations(const Configuration& config);

public:
  Mapper(qc::QuantumComputation quantumComputation, Architecture& architecture);
  virtual ~Mapper() = default;

  /**
   * @brief map the circuit passed at initialization to the architecture
   *
   * @param config the settings for this mapping run (controls e.g. layering
   * methods, pre- and post-optimizations, etc.)
   */
  virtual void map(const Configuration& config) = 0;

  virtual void dumpResult(const std::string& outputFilename) {
    if (qcMapped.empty()) {
      std::cerr << "Mapped circuit is empty.\n";
      return;
    }
    const std::size_t slash = outputFilename.find_last_of('/');
    const std::size_t dot = outputFilename.find_last_of('.');
    results.output.name = outputFilename.substr(slash + 1, dot - slash - 1);
    qcMapped.dump(outputFilename, qc::Format::OpenQASM3);
  }

  virtual void dumpResult(std::ostream& os) { qcMapped.dumpOpenQASM(os); }

  virtual std::ostream& printResult(std::ostream& out) {
    out << results.toString();
    return out;
  }

  virtual MappingResults& getResults() { return results; }

  /// get an rvalue reference to the mapped circuit to move it out of the mapper
  qc::QuantumComputation&& moveMappedCircuit() { return std::move(qcMapped); }

  virtual nlohmann::basic_json<> json() { return results.json(); }

  std::ostream& printLayering(std::ostream& out) {
    out << "---------------- Layering -------------------\n";
    for (const auto& layer : layers) {
      for (const auto& gate : layer.gates) {
        out << "(" << gate.first;
        if (gate.second) {
          out << " " << *gate.second;
        }
        out << ") ";
      }
      out << "\n";
    }
    out << "---------------------------------------------\n";
    return out;
  }

  std::ostream& printLocations(std::ostream& out) {
    out << "---------------- Locations -------------------\n";
    for (std::size_t i = 0; i < qc.getNqubits(); ++i) {
      out << locations.at(i) << " ";
    }
    out << "\n---------------------------------------------\n";
    return out;
  }
  std::ostream& printQubits(std::ostream& out) {
    out << "---------------- Qubits -------------------\n";
    for (std::size_t i = 0; i < architecture->getNqubits(); ++i) {
      out << qubits.at(i) << " ";
    }
    out << "\n---------------------------------------------\n";
    return out;
  }

  virtual void reset() {
    architecture->reset();
    qc.reset();
    layers.clear();
    qubits.clear();
    locations.clear();

    results = MappingResults();
  }
};
