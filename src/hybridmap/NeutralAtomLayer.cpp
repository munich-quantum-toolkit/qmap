/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "hybridmap/NeutralAtomLayer.hpp"

#include "hybridmap/NeutralAtomDefinitions.hpp"
#include "ir/Definitions.hpp"
#include "ir/operations/OpType.hpp"
#include "ir/operations/Operation.hpp"

#include <algorithm>
#include <cstddef>
#include <set>
#include <vector>

namespace na {

void NeutralAtomLayer::updateByQubits(
    const std::set<qc::Qubit>& qubitsToUpdate) {
  updateCandidatesByQubits(qubitsToUpdate);
  candidatesToGates(qubitsToUpdate);
}

void NeutralAtomLayer::initAllQubits() {
  std::set<qc::Qubit> allQubits;
  for (std::size_t i = 0; i < dag.size(); ++i) {
    allQubits.emplace(static_cast<qc::Qubit>(i));
  }
  updateByQubits(allQubits);
}

void NeutralAtomLayer::updateCandidatesByQubits(
    const std::set<qc::Qubit>& qubitsToUpdate) {
  for (const auto& qubit : qubitsToUpdate) {
    if (isFrontLayer) {
      while (iterators[qubit] != ends[qubit]) {
        auto* op = (*iterators[qubit])->get();
        // check if operation commutes with gates and candidates
        const auto commutes = commutesWithAtQubit(gates, op, qubit) &&
                              commutesWithAtQubit(candidates[qubit], op, qubit);
        if (commutes) {
          candidates[qubit].emplace_back(op);
          ++iterators[qubit];
        } else {
          break;
        }
      }
    }
    // for lookahead layer, take the next k multi-qubit gates
    else {
      std::size_t multiQubitGatesFound = 0;
      while (iterators[qubit] != ends[qubit] &&
             multiQubitGatesFound < lookaheadDepth) {
        auto* op = (*iterators[qubit])->get();
        candidates[qubit].emplace_back(op);
        ++iterators[qubit];
        if (op->getUsedQubits().size() > 1) {
          multiQubitGatesFound++;
        }
      }
    }
  }
}

void NeutralAtomLayer::candidatesToGates(
    const std::set<qc::Qubit>& qubitsToUpdate) {
  newGates.clear();
  for (const auto& qubit : qubitsToUpdate) {
    // operations moved from candidates to gates have to be removed afterward
    std::vector<const qc::Operation*> toRemove;
    for (const auto* opPointer : candidates[qubit]) {
      // check if gate is candidate for all qubits it uses
      bool inFrontLayer = true;
      for (const auto& opQubit : opPointer->getUsedQubits()) {
        if (qubit == opQubit) {
          continue;
        }
        if (std::ranges::find(candidates[opQubit], opPointer) ==
            candidates[opQubit].end()) {
          inFrontLayer = false;
          break;
        }
      }
      if (inFrontLayer) {
        gates.emplace_back(opPointer);
        newGates.emplace_back(opPointer);
        // remove from candidacy of other qubits
        for (const auto& opQubit : opPointer->getUsedQubits()) {
          if (qubit == opQubit) {
            continue;
          }
          candidates[opQubit].erase(
              std::ranges::find(candidates[opQubit], opPointer));
        }

        toRemove.emplace_back(opPointer);
      }
    }
    // remove from candidacy of this qubit
    // has to be done now to not change iterating list
    for (const auto* opPointer : toRemove) {
      candidates[qubit].erase(std::ranges::find(candidates[qubit], opPointer));
    }
  }
}

void NeutralAtomLayer::removeGatesAndUpdate(const GateList& gatesToRemove) {
  std::set<qc::Qubit> qubitsToUpdate;
  for (const auto& gate : gatesToRemove) {
    const auto it = std::ranges::find(gates, gate);
    if (it != gates.end()) {
      gates.erase(it);
      auto usedQubits = gate->getUsedQubits();
      qubitsToUpdate.insert(usedQubits.begin(), usedQubits.end());
    }
  }
  updateByQubits(qubitsToUpdate);
}

// Commutation

bool commutesWithAtQubit(const GateList& layer, const qc::Operation* opPointer,
                         const qc::Qubit& qubit) {
  return std::ranges::all_of(
      layer, [&opPointer, &qubit](const auto& frontOpPointer) {
        return commuteAtQubit(opPointer, frontOpPointer, qubit);
      });
}

bool commuteAtQubit(const qc::Operation* op1, const qc::Operation* op2,
                    const qc::Qubit& qubit) {
  if (op1->isNonUnitaryOperation() || op2->isNonUnitaryOperation()) {
    return false;
  }
  // single qubit gates commute
  if (op1->getUsedQubits().size() == 1 && op2->getUsedQubits().size() == 1) {
    return true;
  }

  if (op1->getType() == qc::OpType::I || op2->getType() == qc::OpType::I) {
    return true;
  }

  // commutes at qubit if at least one of the two gates does not use qubit
  const auto usedQubits1 = op1->getUsedQubits();
  const auto usedQubits2 = op2->getUsedQubits();
  if (!usedQubits1.contains(qubit) || !usedQubits2.contains(qubit)) {
    return true;
  }

  // for two-qubit gates, check if they commute at qubit
  // commute if both are controlled at qubit or const Operation* on qubit is
  // same check controls
  if (op1->getControls().contains(qubit) &&
      op2->getControls().contains(qubit)) {
    return true;
  }
  // control and Z also commute
  if ((op1->getControls().contains(qubit) && op2->getType() == qc::OpType::Z) ||
      (op2->getControls().contains(qubit) && op1->getType() == qc::OpType::Z)) {
    return true;
  }

  // check targets
  if (std::ranges::find(op1->getTargets(), qubit) != op1->getTargets().end() &&
      std::ranges::find(op2->getTargets(), qubit) != op2->getTargets().end() &&
      op1->getType() == op2->getType()) {
    return true;
  }
  return false;
}

} // namespace na
