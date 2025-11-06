/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "hybridmap/NeutralAtomUtils.hpp"

#include "circuit_optimizer/CircuitOptimizer.hpp"
#include "ir/Definitions.hpp"
#include "ir/QuantumComputation.hpp"
#include "ir/operations/OpType.hpp"
#include "ir/operations/StandardOperation.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

namespace na {

bool MoveVector::overlap(const MoveVector& other) const {
  // do not consider direction for overlap
  const auto firstStartX = std::min(xStart, xEnd);
  const auto firstEndX = std::max(xStart, xEnd);
  const auto secondStartX = std::min(other.xStart, other.xEnd);
  const auto secondEndX = std::max(other.xStart, other.xEnd);
  const auto firstStartY = std::min(yStart, yEnd);
  const auto firstEndY = std::max(yStart, yEnd);
  const auto secondStartY = std::min(other.yStart, other.yEnd);
  const auto secondEndY = std::max(other.yStart, other.yEnd);

  // need to compute all combinations, as sometimes the start and end x/y points
  // are the same
  const auto overlapXFirstStart =
      firstStartX >= secondStartX && firstStartX <= secondEndX;
  const auto overlapXFirstEnd =
      firstEndX >= secondStartX && firstEndX <= secondEndX;
  const auto overlapXSecondStart =
      secondStartX >= firstStartX && secondStartX <= firstEndX;
  const auto overlapXSecondEnd =
      secondEndX >= firstStartX && secondEndX <= firstEndX;
  const auto overlapYFirstStart =
      firstStartY >= secondStartY && firstStartY <= secondEndY;
  const auto overlapYFirstEnd =
      firstEndY >= secondStartY && firstEndY <= secondEndY;
  const auto overlapYSecondStart =
      secondStartY >= firstStartY && secondStartY <= firstEndY;
  const auto overlapYSecondEnd =
      secondEndY >= firstStartY && secondEndY <= firstEndY;

  return overlapXFirstStart || overlapXFirstEnd || overlapXSecondStart ||
         overlapXSecondEnd || overlapYFirstStart || overlapYFirstEnd ||
         overlapYSecondStart || overlapYSecondEnd;
}

bool MoveVector::include(const MoveVector& other) const {
  const auto firstStartX = std::min(xStart, xEnd);
  const auto firstEndX = std::max(xStart, xEnd);
  const auto secondStartX = std::min(other.xStart, other.xEnd);
  const auto secondEndX = std::max(other.xStart, other.xEnd);
  const auto firstStartY = std::min(yStart, yEnd);
  const auto firstEndY = std::max(yStart, yEnd);
  const auto secondStartY = std::min(other.yStart, other.yEnd);
  const auto secondEndY = std::max(other.yStart, other.yEnd);

  const auto includeX = secondStartX < firstStartX && firstEndX < secondEndX;
  const auto includeY = secondStartY < firstStartY && firstEndY < secondEndY;

  return includeX || includeY;
}

void MoveCombs::addMoveComb(const MoveComb& moveComb) {
  for (auto& comb : moveCombs) {
    if (comb == moveComb) {
      comb.cost = std::numeric_limits<qc::fp>::max();
      return;
    }
  }
  moveCombs.emplace_back(moveComb);
}

void MoveCombs::addMoveCombs(const MoveCombs& otherMoveCombs) {
  for (const auto& otherMove : otherMoveCombs.moveCombs) {
    addMoveComb(otherMove);
  }
}

void MoveCombs::removeLongerMoveCombs() {
  size_t minSize = std::numeric_limits<uint32_t>::max();
  for (const auto& comb : moveCombs) {
    minSize = std::min(minSize, comb.size());
  }
  for (auto it = moveCombs.begin(); it != moveCombs.end();) {
    if (it->size() > minSize) {
      it = moveCombs.erase(it);
    } else {
      ++it;
    }
  }
}

void BridgeCircuits::computeGates(const size_t length) {
  std::vector<std::pair<size_t, size_t>> hsCzsPerQubit(
      bridgeCircuits[length].getNqubits(), {0, 0});
  for (const auto& op : bridgeCircuits[length]) {
    if (op->getType() == qc::OpType::H) {
      hs[length]++;
      hsCzsPerQubit[*op->getTargets().begin()].first++;
    } else if (op->getType() == qc::OpType::Z) {
      czs[length]++;
      hsCzsPerQubit[*op->getUsedQubits().begin()].second++;
      hsCzsPerQubit[*op->getUsedQubits().rbegin()].second++;
    }
  }
  // find max depth
  const auto maxHcZ =
      std::ranges::max_element(hsCzsPerQubit, [](const auto& a, const auto& b) {
        return a.first + a.second < b.first + b.second;
      });
  hDepth[length] = maxHcZ->first;
  czDepth[length] = maxHcZ->second;
}

void BridgeCircuits::computeBridgeCircuit(const size_t length) {
  qc::QuantumComputation qcBridge(3);
  qcBridge.cx(0, 1);
  qcBridge.cx(1, 2);
  qcBridge.cx(0, 1);
  qcBridge.cx(1, 2);

  qcBridge = recursiveBridgeIncrease(qcBridge, length - 3);
  // convert to CZ on qubit 0
  qcBridge.h(static_cast<qc::Qubit>(qcBridge.getNqubits() - 1));
  qcBridge.insert(qcBridge.begin(), std::make_unique<qc::StandardOperation>(
                                        qcBridge.getNqubits() - 1, qc::H));

  qc::CircuitOptimizer::replaceMCXWithMCZ(qcBridge);
  qc::CircuitOptimizer::singleQubitGateFusion(qcBridge);
  bridgeCircuits[length] = qcBridge;
}

qc::QuantumComputation
BridgeCircuits::recursiveBridgeIncrease(qc::QuantumComputation qcBridge,
                                        const size_t length) {
  if (length == 0) {
    return qcBridge;
  }
  // determine qubit pair with the least amount of gates
  std::vector<size_t> gates(qcBridge.getNqubits() - 1, 0);
  for (const auto& gate : qcBridge) {
    gates[*gate->getUsedQubits().begin()]++;
  }
  const auto minIndex =
      static_cast<size_t>(std::ranges::min_element(gates) - gates.begin());

  qcBridge = bridgeExpand(qcBridge, minIndex);

  return recursiveBridgeIncrease(qcBridge, length - 1);
}
qc::QuantumComputation
BridgeCircuits::bridgeExpand(const qc::QuantumComputation& qcBridge,
                             const size_t qubit) {
  qc::QuantumComputation qcBridgeNew(qcBridge.getNqubits() + 1);
  for (const auto& gate : qcBridge) {
    const auto usedQubits = gate->getUsedQubits();
    const auto q1 = *usedQubits.begin();
    const auto q2 = *usedQubits.rbegin();
    if (q1 == qubit && q2 == qubit + 1) {
      qcBridgeNew.cx(q1, q2);
      qcBridgeNew.cx(q1 + 1, q2 + 1);
      qcBridgeNew.cx(q1, q2);
      qcBridgeNew.cx(q1 + 1, q2 + 1);
    } else if (*usedQubits.begin() > qubit) {
      // shift qubits by one
      qcBridgeNew.cx(q1 + 1, q2 + 1);
    } else {
      qcBridgeNew.cx(q1, q2);
    }
  }
  return qcBridgeNew;
}

} // namespace na
