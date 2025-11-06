/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#ifndef HYBRIDMAP_NEUTRAL_ATOM_LAYER_HPP
#define HYBRIDMAP_NEUTRAL_ATOM_LAYER_HPP

#include "hybridmap/NeutralAtomDefinitions.hpp"
#include "ir/Definitions.hpp"

#include <cstdint>
#include <deque>
#include <memory>
#include <set>
#include <utility>
#include <vector>

namespace na {
/**
 * @brief Class to manage the creation of layers when traversing a quantum
 * circuit.
 * @details The class uses the qc::DAG of the circuit to create layers of gates
 * that can be executed at the same time. It can be used to create the front or
 * look ahead layer.
 */

class NeutralAtomLayer {
protected:
  using DAG = std::vector<std::deque<std::unique_ptr<qc::Operation>*>>;
  using DAGIterator = std::deque<std::unique_ptr<qc::Operation>*>::iterator;
  using DAGIterators = std::vector<DAGIterator>;

  DAG dag;
  DAGIterators iterators;
  DAGIterators ends;
  GateList gates;
  GateList newGates;
  GateLists candidates;
  uint32_t lookaheadDepth;
  bool isFrontLayer;

  /**
   * @brief Update layer state for a set of qubits.
   * @details Advances the internal iterators of the qc::DAG for the provided
   * qubits, updates the per-qubit candidate lists, and moves operations that
   * are ready across all their used qubits into the current layer.
   * In front-layer mode, only commuting operations are considered; in
   * look-ahead mode, up to @ref lookaheadDepth multi-qubit gates ahead are
   * considered for each qubit.
   * @param qubitsToUpdate Logical qubits whose frontier should be advanced and
   * whose candidates/gates should be refreshed.
   */
  void updateByQubits(const std::set<qc::Qubit>& qubitsToUpdate);

  /**
   * @brief Update the per-qubit candidate queues.
   * @details For front-layer construction, keep pulling operations from each
   * qubit's DAG column while they commute with both the already selected
   * gates and the existing candidates at that qubit. For look-ahead
   * construction, pull forward operations until @ref lookaheadDepth
   * multi-qubit operations have been encountered (single-qubit operations in
   * between are included as well).
   * @param qubitsToUpdate Logical qubits whose candidate queues should be
   * extended.
   */
  void updateCandidatesByQubits(const std::set<qc::Qubit>& qubitsToUpdate);
  /**
   * @brief Promote eligible candidates to the current layer.
   * @details For each provided qubit, move an operation from the candidate
   * list to the current layer if and only if it appears as a candidate on
   * all qubits it uses. Newly added operations are also tracked in
   * @ref newGates, and removed from the candidate lists of all their
   * constituent qubits.
   * @param qubitsToUpdate Logical qubits whose candidate lists should be
   * evaluated.
   */
  void candidatesToGates(const std::set<qc::Qubit>& qubitsToUpdate);

public:
  /**
   * @brief Construct a NeutralAtomLayer helper.
   * @param graph The per-qubit DAG representation of the circuit (each entry
   * corresponds to one qubit line).
   * @param isFrontLayer If true, build the executable front layer (only
   * commuting operations are pulled); if false, build a look-ahead layer.
   * @param lookaheadDepth For look-ahead mode, the number of multi-qubit
   * operations to consider ahead for each qubit (defaults to 1). Ignored in
   * front-layer mode.
   */
  explicit NeutralAtomLayer(DAG graph, const bool isFrontLayer,
                            const uint32_t lookaheadDepth = 1)
      : dag(std::move(graph)), lookaheadDepth(lookaheadDepth),
        isFrontLayer(isFrontLayer) {
    iterators.reserve(dag.size());
    candidates.reserve(dag.size());
    for (auto& i : dag) {
      auto it = i.begin();
      iterators.emplace_back(it);
      ends.emplace_back(i.end());
      candidates.emplace_back();
    }
  }

  /**
   * @brief Returns the current layer of gates
   * @return The current layer of gates
   */
  [[nodiscard]] GateList getGates() const { return gates; }
  /**
   * @brief Return the gates that were added the last time the layer was
   * updated.
   * @details This is populated during the most recent call to
   * updateByQubits()/candidatesToGates() and is cleared and repopulated on
   * subsequent updates.
   * @return The subset of gates newly added to the layer during the last
   * update.
   */
  [[nodiscard]] GateList getNewGates() const { return newGates; }
  /**
   * @brief Initialize the layer by updating all qubits from their current
   * DAG-frontier.
   */
  void initAllQubits();
  /**
   * @brief Remove gates from the current layer and advance affected qubits.
   * @details Erases the provided gates from the layer, then advances the DAG
   * frontier for all qubits touched by those gates, updating candidates and
   * possibly pulling in new gates.
   * @param gatesToRemove Gates to remove from the current layer.
   */
  void removeGatesAndUpdate(const GateList& gatesToRemove);
};

// Commutation checks
/**
 * @brief Check whether an operation commutes at a specific qubit with all
 * operations already present in a layer.
 * @param layer The current layer (list of operations) to check against.
 * @param opPointer The operation to be tested for commutation.
 * @param qubit The qubit at which commutation is assessed.
 * @return true if @p opPointer commutes with every operation in @p layer at
 * @p qubit; false otherwise.
 */
bool commutesWithAtQubit(const GateList& layer, const qc::Operation* opPointer,
                         const qc::Qubit& qubit);
/**
 * @brief Check whether two operations commute at a specific qubit.
 * @details Applies simple syntactic rules: non-unitaries never commute;
 * single-qubit gates commute; identities commute; gates that do not act on the
 * qubit commute; for two-qubit gates, certain control/target patterns commute
 * (both controlled on the qubit, control with Z on the qubit, or equal target
 * types on the qubit).
 * @param op1 First operation.
 * @param op2 Second operation.
 * @param qubit The qubit at which commutation is assessed.
 * @return true if the operations commute at @p qubit; false otherwise.
 */
bool commuteAtQubit(const qc::Operation* op1, const qc::Operation* op2,
                    const qc::Qubit& qubit);
} // namespace na

#endif // HYBRIDMAP_NEUTRAL_ATOM_LAYER_HPP
