/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "datastructures/CircuitOptimizations.hpp"

#include "ir/Definitions.hpp"
#include "ir/QuantumComputation.hpp"
#include "ir/operations/CompoundOperation.hpp"
#include "ir/operations/Control.hpp"
#include "ir/operations/OpType.hpp"
#include "ir/operations/Operation.hpp"
#include "ir/operations/StandardOperation.hpp"

#include <cassert>
#include <deque>
#include <iterator>
#include <memory>
#include <set>
#include <utility>
#include <vector>

namespace qmap {
namespace {
using DAG = std::vector<std::deque<std::unique_ptr<qc::Operation>*>>;

void addToDAG(DAG& dag, std::unique_ptr<qc::Operation>* op) {
  for (const auto qubit : (*op)->getUsedQubits()) {
    dag.at(qubit).push_back(op);
  }
}

void removeIdentities(qc::QuantumComputation& qc) {
  auto it = qc.begin();
  while (it != qc.end()) {
    if ((*it)->getType() == qc::I) {
      it = qc.erase(it);
    } else if ((*it)->isCompoundOperation()) {
      auto& compound = dynamic_cast<qc::CompoundOperation&>(**it);
      auto compoundIt = compound.cbegin();
      while (compoundIt != compound.cend()) {
        if ((*compoundIt)->getType() == qc::I) {
          compoundIt = compound.erase(compoundIt);
        } else {
          ++compoundIt;
        }
      }
      if (compound.empty()) {
        it = qc.erase(it);
      } else {
        if (compound.size() == 1U) {
          *it = std::move(*compound.begin());
        }
        ++it;
      }
    } else {
      ++it;
    }
  }
}

template <class Container> void replaceMCXWithMCZIn(Container& operations) {
  for (auto it = operations.begin(); it != operations.end(); ++it) {
    auto& op = *it;
    if (op->getType() == qc::X && op->getNcontrols() > 0) {
      const auto& controls = op->getControls();
      assert(op->getNtargets() == 1U);
      const auto target = op->getTargets()[0];

      it = operations.insert(
          it, std::make_unique<qc::StandardOperation>(target, qc::H));
      it = operations.insert(
          it, std::make_unique<qc::StandardOperation>(controls, target, qc::Z));
      it = operations.insert(
          it, std::make_unique<qc::StandardOperation>(target, qc::H));
      std::advance(it, 3);
      it = operations.erase(it);
      --it;
    } else if (op->isCompoundOperation()) {
      auto* compound = dynamic_cast<qc::CompoundOperation*>(op.get());
      replaceMCXWithMCZIn(*compound);
    }
  }
}

template <class Container>
auto decomposeSWAPAt(Container& operations, typename Container::iterator it,
                     const bool isDirectedArchitecture) {
  const auto targets = (*it)->getTargets();
  it = operations.erase(it);
  it = operations.insert(it, std::make_unique<qc::StandardOperation>(
                                 qc::Control{targets[0]}, targets[1], qc::X));
  if (isDirectedArchitecture) {
    it = operations.insert(
        it, std::make_unique<qc::StandardOperation>(targets[0], qc::H));
    it = operations.insert(
        it, std::make_unique<qc::StandardOperation>(targets[1], qc::H));
    it = operations.insert(it, std::make_unique<qc::StandardOperation>(
                                   qc::Control{targets[0]}, targets[1], qc::X));
    it = operations.insert(
        it, std::make_unique<qc::StandardOperation>(targets[0], qc::H));
    it = operations.insert(
        it, std::make_unique<qc::StandardOperation>(targets[1], qc::H));
  } else {
    it = operations.insert(it, std::make_unique<qc::StandardOperation>(
                                   qc::Control{targets[1]}, targets[0], qc::X));
  }
  return operations.insert(it, std::make_unique<qc::StandardOperation>(
                                   qc::Control{targets[0]}, targets[1], qc::X));
}
} // namespace

void decomposeSWAP(qc::QuantumComputation& qc,
                   const bool isDirectedArchitecture) {
  auto it = qc.begin();
  while (it != qc.end()) {
    if ((*it)->isStandardOperation()) {
      if ((*it)->getType() == qc::SWAP) {
        it = decomposeSWAPAt(qc, it, isDirectedArchitecture);
      } else {
        ++it;
      }
    } else if ((*it)->isCompoundOperation()) {
      auto& compound = dynamic_cast<qc::CompoundOperation&>(**it);
      auto compoundIt = compound.begin();
      while (compoundIt != compound.end()) {
        if ((*compoundIt)->isStandardOperation() &&
            (*compoundIt)->getType() == qc::SWAP) {
          compoundIt =
              decomposeSWAPAt(compound, compoundIt, isDirectedArchitecture);
        } else {
          ++compoundIt;
        }
      }
      ++it;
    } else {
      ++it;
    }
  }
}

void cancelCNOTs(qc::QuantumComputation& qc) {
  auto dag = DAG(qc.getHighestPhysicalQubitIndex() + 1U);

  for (auto& operation : qc) {
    if (!operation->isStandardOperation()) {
      addToDAG(dag, &operation);
      continue;
    }

    const auto isCNOT =
        operation->getType() == qc::X && operation->getNcontrols() == 1U &&
        operation->getControls().begin()->type == qc::Control::Type::Pos;
    const auto isSWAP =
        operation->getType() == qc::SWAP && operation->getNcontrols() == 0U;
    if (!isCNOT && !isSWAP) {
      addToDAG(dag, &operation);
      continue;
    }

    const auto q0 = operation->getTargets().at(0);
    const auto q1 = isSWAP ? operation->getTargets().at(1)
                           : operation->getControls().begin()->qubit;
    if (dag.at(q0).empty() || dag.at(q1).empty()) {
      addToDAG(dag, &operation);
      continue;
    }

    auto* previous0 = dag.at(q0).back()->get();
    auto* previous1 = dag.at(q1).back()->get();
    if (previous0 != previous1) {
      addToDAG(dag, &operation);
      continue;
    }

    const auto previousIsCNOT =
        previous0->getType() == qc::X && previous0->getNcontrols() == 1U &&
        previous0->getControls().begin()->type == qc::Control::Type::Pos;
    const auto previousIsSWAP =
        previous0->getType() == qc::SWAP && previous0->getNcontrols() == 0U;
    if (!previousIsCNOT && !previousIsSWAP) {
      addToDAG(dag, &operation);
      continue;
    }

    const auto previousQ0 = previous0->getTargets().at(0);
    const auto previousQ1 = previousIsSWAP
                                ? previous0->getTargets().at(1)
                                : previous0->getControls().begin()->qubit;

    if (isCNOT && previousIsCNOT) {
      if (q0 == previousQ0 && q1 == previousQ1) {
        dag.at(q0).pop_back();
        dag.at(q1).pop_back();
        previous0->setGate(qc::I);
        previous0->clearControls();
        operation->setGate(qc::I);
        operation->clearControls();
      } else {
        auto beforePrevious0 = ++dag.at(q0).rbegin();
        auto beforePrevious1 = ++dag.at(q1).rbegin();
        if (beforePrevious0 == dag.at(q0).rend() ||
            beforePrevious1 == dag.at(q1).rend()) {
          addToDAG(dag, &operation);
          continue;
        }

        auto* earlier0 = (*beforePrevious0)->get();
        auto* earlier1 = (*beforePrevious1)->get();
        if (earlier0 != earlier1) {
          addToDAG(dag, &operation);
          continue;
        }

        const auto earlierIsCNOT =
            earlier0->getType() == qc::X && earlier0->getNcontrols() == 1U &&
            earlier0->getControls().begin()->type == qc::Control::Type::Pos;
        if (!earlierIsCNOT) {
          addToDAG(dag, &operation);
          continue;
        }

        const auto earlierQ0 = earlier0->getTargets().at(0);
        const auto earlierQ1 = earlier0->getControls().begin()->qubit;
        if (q0 == earlierQ0 && q1 == earlierQ1) {
          earlier0->setGate(qc::SWAP);
          earlier0->clearControls();
          if (previousQ0 > previousQ1) {
            earlier0->setTargets({previousQ1, previousQ0});
          } else {
            earlier0->setTargets({previousQ0, previousQ1});
          }
          previous0->setGate(qc::I);
          previous0->clearControls();
          operation->setGate(qc::I);
          operation->clearControls();
          dag.at(q0).pop_back();
          dag.at(q1).pop_back();
        } else {
          addToDAG(dag, &operation);
          continue;
        }
      }
      continue;
    }

    if (isSWAP && previousIsSWAP) {
      if (std::set{q0, q1} == std::set{previousQ0, previousQ1}) {
        dag.at(q0).pop_back();
        dag.at(q1).pop_back();
        previous0->setGate(qc::I);
        previous0->clearControls();
        operation->setGate(qc::I);
        operation->clearControls();
      } else {
        addToDAG(dag, &operation);
      }
      continue;
    }

    if (isCNOT && previousIsSWAP) {
      previous0->setGate(qc::X);
      previous0->setTargets({q0});
      previous0->setControls({qc::Control{q1}});
      operation->setTargets({q1});
      operation->setControls({qc::Control{q0}});
      addToDAG(dag, &operation);
      continue;
    }

    if (isSWAP && previousIsCNOT) {
      previous0->setTargets({previousQ1});
      previous0->setControls({qc::Control{previousQ0}});
      operation->setGate(qc::X);
      operation->setTargets({previousQ0});
      operation->setControls({qc::Control{previousQ1}});
      addToDAG(dag, &operation);
      continue;
    }
  }

  removeIdentities(qc);
}

void replaceMCXWithMCZ(qc::QuantumComputation& qc) { replaceMCXWithMCZIn(qc); }

} // namespace qmap
