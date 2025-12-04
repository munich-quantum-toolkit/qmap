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
 * @brief Helper for constructing executable or look-ahead layers from a circuit
 * DAG.
 * @details Consumes a per-qubit DAG representation and maintains frontier
 * iterators to build either (a) the front layer of mutually commuting gates
 * (ready to execute) or (b) look-ahead layers containing a bounded depth of
 * forthcoming multi-qubit gates per qubit for heuristic evaluation.
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
   * @brief Advance frontier and refresh candidates/gates for specified qubits.
   * @details Moves DAG iterators forward for each qubit, replenishes candidate
   * queues and promotes ready operations into the current layer (all involved
   * qubits agree). Front-layer mode restricts to commuting operations;
   * look-ahead mode gathers up to lookaheadDepth multi-qubit gates.
   * @param qubitsToUpdate Logical qubits whose DAG frontier and candidate sets
   * are updated.
   */
  void updateByQubits(const std::set<qc::Qubit>& qubitsToUpdate);

  /**
   * @brief Extend per-qubit candidate queues from DAG columns.
   * @details Front-layer: continue pulling while new ops commute with existing
   * layer and per-qubit candidates. Look-ahead: pull until lookaheadDepth
   * multi-qubit ops encountered (including intervening single-qubit ops).
   * @param qubitsToUpdate Logical qubits whose candidate queues should be
   * extended.
   */
  void updateCandidatesByQubits(const std::set<qc::Qubit>& qubitsToUpdate);
  /**
   * @brief Promote multi-qubit candidates that are ready on all their qubits.
   * @details Checks for each qubit whether the head candidate appears across
   * candidate lists of all its operand qubits; if so, moves it to the layer and
   * records it in newGates, removing from all candidate lists.
   * @param qubitsToUpdate Logical qubits whose candidate lists are evaluated
   * for promotion.
   */
  void candidatesToGates(const std::set<qc::Qubit>& qubitsToUpdate);

public:
  /**
   * @brief Construct a layer builder over a per-qubit DAG.
   * @param graph Per-qubit DAG columns (each deque owns operation pointers).
   * @param isFrontLayer True for executable front layer mode; false for
   * look-ahead mode.
   * @param lookaheadDepth Max number of multi-qubit gates to look ahead per
   * qubit.
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
   * @brief Get the current executable/look-ahead layer gate list.
   * @return Copy of current layer gates.
   */
  [[nodiscard]] GateList getGates() const { return gates; }
  /**
   * @brief Get gates newly added during the latest update.
   * @details Populated by the most recent update invocation then refreshed each
   * subsequent update.
   * @return Copy of gates added since prior update.
   */
  [[nodiscard]] GateList getNewGates() const { return newGates; }
  /**
   * @brief Initialize internal frontiers and populate initial candidates/gates.
   * @details Advances all qubit DAG iterators, builds candidate queues and
   * promotes ready operations.
   */
  void initAllQubits();
  /**
   * @brief Remove specified gates then advance affected qubit frontiers.
   * @details After erasing gates, updates candidates/gates for all qubits they
   * touched, potentially adding new ready operations.
   * @param gatesToRemove Gates to erase from current layer.
   */
  void removeGatesAndUpdate(const GateList& gatesToRemove);
};

// Commutation checks
/**
 * @brief Determine if an operation commutes with all layer operations at a
 * qubit.
 * @param layer Current layer gate list.
 * @param opPointer Operation to test.
 * @param qubit Qubit index for commutation assessment.
 * @return True if opPointer commutes with every gate in layer at qubit; false
 * otherwise.
 */
bool commutesWithAtQubit(const GateList& layer, const qc::Operation* opPointer,
                         const qc::Qubit& qubit);
/**
 * @brief Check pairwise commutation of two operations at a qubit.
 * @details Simple syntactic rules: non-unitaries never commute; identities and
 * single-qubit ops commute; ops not acting on the qubit commute; specific
 * two-qubit control/target patterns are considered commuting.
 * @param op1 First operation.
 * @param op2 Second operation.
 * @param qubit Qubit index for commutation assessment.
 * @return True if operations commute at qubit; false otherwise.
 */
bool commuteAtQubit(const qc::Operation* op1, const qc::Operation* op2,
                    const qc::Qubit& qubit);
} // namespace na
