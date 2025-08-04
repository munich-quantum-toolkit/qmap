/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "hybridmap/HybridNeutralAtomMapper.hpp"

#include "circuit_optimizer/CircuitOptimizer.hpp"
#include "hybridmap/MoveToAodConverter.hpp"
#include "hybridmap/NeutralAtomDefinitions.hpp"
#include "hybridmap/NeutralAtomLayer.hpp"
#include "hybridmap/NeutralAtomUtils.hpp"
#include "ir/Definitions.hpp"
#include "ir/QuantumComputation.hpp"
#include "ir/operations/OpType.hpp"
#include "ir/operations/Operation.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <iterator>
#include <limits>
#include <queue>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace na {
qc::QuantumComputation NeutralAtomMapper::map(qc::QuantumComputation& qc,
                                              InitialMapping initialMapping) {
  mappedQc = qc::QuantumComputation(arch.getNpositions());
  nMoves = 0;
  nSwaps = 0;
  qc::CircuitOptimizer::replaceMCXWithMCZ(qc);
  qc::CircuitOptimizer::singleQubitGateFusion(qc);
  qc::CircuitOptimizer::flattenOperations(qc);
  qc::CircuitOptimizer::removeFinalMeasurements(qc);

  auto dag = qc::CircuitOptimizer::constructDAG(qc);

  // init mapping
  this->mapping = Mapping(qc.getNqubits(), initialMapping);

  // init layers
  NeutralAtomLayer frontLayer(dag);
  frontLayer.initLayerOffset();
  mapAllPossibleGates(frontLayer);
  NeutralAtomLayer lookaheadLayer(dag);
  lookaheadLayer.initLayerOffset(frontLayer.getIteratorOffset());

  // Checks
  if (dag.size() > arch.getNqubits()) {
    throw std::runtime_error("More qubits in circuit than in architecture");
  }

  //   precompute exponential decay weights
  this->decayWeights.reserve(this->arch.getNcolumns());
  for (uint32_t i = this->arch.getNcolumns(); i > 0; --i) {
    this->decayWeights.emplace_back(std::exp(-this->parameters.decay * i));
  }

  auto i = 0;
  while (!frontLayer.getGates().empty()) {
    // assign gates to layers
    reassignGatesToLayers(frontLayer.getGates(), lookaheadLayer.getGates());

    // save last swap to prevent immediate swap back
    Swap lastSwap = {0, 0};

    // first do all gate based mapping gates
    while (!this->frontLayerGate.empty()) {
      GateList gatesToExecute;
      while (gatesToExecute.empty()) {
        ++i;
        if (this->parameters.verbose) {
          std::cout << "iteration " << i << '\n';
        }
        auto bestSwap = findBestSwap(lastSwap);
        lastSwap = bestSwap;
        updateMappingSwap(bestSwap);
        gatesToExecute = getExecutableGates(frontLayer.getGates());
      }
      mapAllPossibleGates(frontLayer);
      lookaheadLayer.initLayerOffset(frontLayer.getIteratorOffset());
      reassignGatesToLayers(frontLayer.getGates(), lookaheadLayer.getGates());
      if (this->parameters.verbose) {
        printLayers();
      }
    }
    // then do all shuttling based mapping gates
    while (!this->frontLayerShuttling.empty()) {
      GateList gatesToExecute;
      while (gatesToExecute.empty()) {
        ++i;
        if (this->parameters.verbose) {
          std::cout << "iteration " << i << '\n';
        }
        auto bestMove = findBestAtomMove();
        updateMappingMove(bestMove);
        gatesToExecute = getExecutableGates(frontLayer.getGates());
      }
      mapAllPossibleGates(frontLayer);
      lookaheadLayer.initLayerOffset(frontLayer.getIteratorOffset());
      reassignGatesToLayers(frontLayer.getGates(), lookaheadLayer.getGates());
      if (this->parameters.verbose) {
        printLayers();
      }
    }
  }
  if (this->parameters.verbose) {
    std::cout << "nSwaps: " << nSwaps << '\n';
    std::cout << "nMoves: " << nMoves << '\n';
  }
  return mappedQc;
}

void NeutralAtomMapper::mapAllPossibleGates(NeutralAtomLayer& layer) {
  // map single qubit gates
  for (const auto* opPointer : layer.getMappedSingleQubitGates()) {
    mapGate(opPointer);
  }
  layer.removeGatesAndUpdate({});
  // check and map multi qubit gates
  auto executableGates = getExecutableGates(layer.getGates());
  while (!executableGates.empty()) {
    for (const auto* opPointer : layer.getMappedSingleQubitGates()) {
      mapGate(opPointer);
    }
    for (const auto* opPointer : executableGates) {
      mapGate(opPointer);
    }
    layer.removeGatesAndUpdate(executableGates);
    executableGates = getExecutableGates(layer.getGates());
  }
}

qc::QuantumComputation
NeutralAtomMapper::convertToAod(qc::QuantumComputation& qc) {
  // decompose SWAP gates
  qc::CircuitOptimizer::decomposeSWAP(qc, false);
  qc::CircuitOptimizer::replaceMCXWithMCZ(qc);
  qc::CircuitOptimizer::singleQubitGateFusion(qc);
  qc::CircuitOptimizer::flattenOperations(qc);
  // decompose AOD moves
  MoveToAodConverter aodScheduler(arch);
  mappedQcAOD = aodScheduler.schedule(qc);
  if (this->parameters.verbose) {
    std::cout << "nMoveGroups: " << aodScheduler.getNMoveGroups() << '\n';
  }
  return mappedQcAOD;
}

void NeutralAtomMapper::reassignGatesToLayers(const GateList& frontGates,
                                              const GateList& lookaheadGates) {
  // assign gates to gates or shuttling
  this->frontLayerGate.clear();
  this->frontLayerShuttling.clear();
  for (const auto& gate : frontGates) {
    if (swapGateBetter(gate)) {
      this->frontLayerGate.emplace_back(gate);
    } else {
      this->frontLayerShuttling.emplace_back(gate);
    }
  }

  this->lookaheadLayerGate.clear();
  this->lookaheadLayerShuttling.clear();
  for (const auto& gate : lookaheadGates) {
    if (swapGateBetter(gate)) {
      this->lookaheadLayerGate.emplace_back(gate);
    } else {
      this->lookaheadLayerShuttling.emplace_back(gate);
    }
  }
}

void NeutralAtomMapper::mapGate(const qc::Operation* op) {
  if (op->getType() == qc::OpType::I) {
    return;
  }
  // Safety check
  if (std::ranges::find(this->executedCommutingGates, op) !=
      this->executedCommutingGates.end()) {
    return;
  }
  this->executedCommutingGates.emplace_back(op);
  if (this->parameters.verbose) {
    std::cout << "mapped " << op->getName() << " ";
    for (auto qubit : op->getUsedQubits()) {
      std::cout << qubit << " ";
    }
    std::cout << "\n";
  }
  // convert circuit qubits to CoordIndex and append to mappedQc
  auto opCopyUnique = op->clone();
  auto* opCopy = opCopyUnique.get();
  this->mapping.mapToHwQubits(opCopy);
  this->hardwareQubits.mapToCoordIdx(opCopy);
  this->mappedQc.emplace_back(opCopy->clone());
}

bool NeutralAtomMapper::isExecutable(const qc::Operation* opPointer) {
  auto usedQubits = opPointer->getUsedQubits();
  auto nUsedQubits = usedQubits.size();
  if (nUsedQubits == 1) {
    return true;
  }
  std::set<qc::Qubit> usedHwQubits;
  for (auto qubit : usedQubits) {
    usedHwQubits.emplace(this->mapping.getHwQubit(qubit));
  }
  return this->hardwareQubits.getAllToAllSwapDistance(usedHwQubits) == 0;
}

void NeutralAtomMapper::printLayers() {
  std::cout << "f,g: ";
  for (const auto* op : this->frontLayerGate) {
    std::cout << op->getName() << " ";
    for (auto qubit : op->getUsedQubits()) {
      std::cout << qubit << " ";
    }
    std::cout << '\n';
  }
  std::cout << "f,s: ";
  for (const auto* op : this->frontLayerShuttling) {
    std::cout << op->getName() << " ";
    for (auto qubit : op->getUsedQubits()) {
      std::cout << qubit << " ";
    }
    std::cout << '\n';
  }
  std::cout << "l,g: ";
  for (const auto* op : this->lookaheadLayerGate) {
    std::cout << op->getName() << " ";
    for (auto qubit : op->getUsedQubits()) {
      std::cout << qubit << " ";
    }
    std::cout << '\n';
  }
  std::cout << '\n';
  std::cout << "l,g: ";
  for (const auto* op : this->lookaheadLayerShuttling) {
    std::cout << op->getName() << " ";
    for (auto qubit : op->getUsedQubits()) {
      std::cout << qubit << " ";
    }
    std::cout << '\n';
  }
  std::cout << '\n';
}

GateList NeutralAtomMapper::getExecutableGates(const GateList& gates) {
  GateList executableGates;
  for (const auto* opPointer : gates) {
    if (isExecutable(opPointer)) {
      executableGates.emplace_back(opPointer);
    }
  }
  return executableGates;
}

void NeutralAtomMapper::updateMappingSwap(Swap swap) {
  nSwaps++;
  // save to lastSwaps
  this->lastBlockedQubits.emplace_back(
      this->hardwareQubits.getBlockedQubits({swap.first, swap.second}));
  if (this->lastBlockedQubits.size() > this->arch.getNcolumns()) {
    this->lastBlockedQubits.pop_front();
  }
  this->mapping.applySwap(swap);
  // convert circuit qubits to CoordIndex and append to mappedQc
  auto idxFirst = this->hardwareQubits.getCoordIndex(swap.first);
  auto idxSecond = this->hardwareQubits.getCoordIndex(swap.second);
  this->mappedQc.swap(idxFirst, idxSecond);
  if (this->parameters.verbose) {
    std::cout << "swapped " << swap.first << " " << swap.second;
    std::cout << "  logical qubits: ";
    if (this->mapping.isMapped(swap.first)) {
      std::cout << this->mapping.getCircQubit(swap.first);
    } else {
      std::cout << "not mapped";
    }
    if (this->mapping.isMapped(swap.second)) {
      std::cout << " " << this->mapping.getCircQubit(swap.second);
    } else {
      std::cout << " not mapped";
    }
    std::cout << '\n';
  }
}

void NeutralAtomMapper::updateMappingMove(AtomMove move) {
  this->lastMoves.emplace_back(move);
  if (this->lastMoves.size() > 4) {
    this->lastMoves.pop_front();
  }
  mappedQc.move(move.first, move.second);
  auto toMoveHwQubit = this->hardwareQubits.getHwQubit(move.first);
  this->hardwareQubits.move(toMoveHwQubit, move.second);
  if (this->parameters.verbose) {
    std::cout << "moved " << move.first << " to " << move.second;
    if (this->mapping.isMapped(toMoveHwQubit)) {
      std::cout << "  logical qubit: "
                << this->mapping.getCircQubit(toMoveHwQubit) << '\n';
    } else {
      std::cout << "  not mapped" << '\n';
    }
  }
  nMoves++;
}

Swap NeutralAtomMapper::findBestSwap(const Swap& lastSwap) {
  // compute necessary movements
  auto swapsFront = initSwaps(this->frontLayerGate);
  auto swapsLookahead = initSwaps(this->lookaheadLayerGate);
  setTwoQubitSwapWeight(swapsFront.second);

  // evaluate swaps based on cost function
  auto swaps = getAllPossibleSwaps(swapsFront);
  // remove last swap to prevent immediate swap back
  swaps.erase(lastSwap);
  swaps.erase({lastSwap.second, lastSwap.first});

  // no swap possible
  if (swaps.empty()) {
    return {std::numeric_limits<qc::Qubit>::max(),
            std::numeric_limits<qc::Qubit>::max()};
  }
  std::vector<std::pair<Swap, qc::fp>> swapCosts;
  swapCosts.reserve(swaps.size());
  for (const auto& swap : swaps) {
    swapCosts.emplace_back(swap, swapCost(swap, swapsFront, swapsLookahead));
  }
  std::ranges::sort(swapCosts, [](const auto& swap1, const auto& swap2) {
    return swap1.second < swap2.second;
  });
  // get swap of minimal cost
  auto bestSwap = std::ranges::min_element(
      swapCosts, [](const auto& swap1, const auto& swap2) {
        return swap1.second < swap2.second;
      });
  return bestSwap->first;
}

void NeutralAtomMapper::setTwoQubitSwapWeight(const WeightedSwaps& swapExact) {
  for (const auto& [swap, weight] : swapExact) {
    this->twoQubitSwapWeight = std::min(weight, this->twoQubitSwapWeight);
  }
}

std::set<Swap> NeutralAtomMapper::getAllPossibleSwaps(
    const std::pair<Swaps, WeightedSwaps>& swapsFront) const {
  auto [swapCloseByFront, swapExactFront] = swapsFront;
  std::set<Swap> swaps;
  for (const auto& swapNearby : swapCloseByFront) {
    const auto nearbySwapsFirst =
        this->hardwareQubits.getNearbySwaps(swapNearby.first);
    for (const auto& swapFirst : nearbySwapsFirst) {
      swaps.emplace(swapFirst);
    }
    const auto nearbySwapsSecond =
        this->hardwareQubits.getNearbySwaps(swapNearby.second);
    for (const auto& swapSecond : nearbySwapsSecond) {
      swaps.emplace(swapSecond);
    }
  }
  for (const auto& [swap, weight] : swapExactFront) {
    const auto nearbySwapsFirst =
        this->hardwareQubits.getNearbySwaps(swap.first);
    for (const auto& swapFirst : nearbySwapsFirst) {
      swaps.emplace(swapFirst);
    }
  }
  return swaps;
}

qc::fp NeutralAtomMapper::swapCost(
    const Swap& swap, const std::pair<Swaps, WeightedSwaps>& swapsFront,
    const std::pair<Swaps, WeightedSwaps>& swapsLookahead) {
  auto [swapCloseByFront, swapExactFront] = swapsFront;
  auto [swapCloseByLookahead, swapExactLookahead] = swapsLookahead;
  // compute the change in total distance
  auto distanceChangeFront =
      swapCostPerLayer(swap, swapCloseByFront, swapExactFront) /
      static_cast<qc::fp>(this->frontLayerGate.size());
  qc::fp distanceChangeLookahead = 0;
  if (!this->lookaheadLayerGate.empty()) {
    distanceChangeLookahead =
        swapCostPerLayer(swap, swapCloseByLookahead, swapExactLookahead) /
        static_cast<qc::fp>(this->lookaheadLayerGate.size());
  }
  auto cost = parameters.lookaheadWeightSwaps * distanceChangeLookahead +
              distanceChangeFront;
  //  compute the last time one of the swap qubits was used
  if (this->parameters.decay != 0) {
    uint32_t idxLastUsed = 0;
    for (uint32_t i = 0; i < this->lastBlockedQubits.size(); ++i) {
      if (this->lastBlockedQubits[i].contains(swap.first) ||
          this->lastBlockedQubits[i].contains(swap.second)) {
        idxLastUsed = i;
        break;
      }
    }
    cost *= this->decayWeights[idxLastUsed];
  }
  return cost;
}

std::pair<Swaps, WeightedSwaps>
NeutralAtomMapper::initSwaps(const GateList& layer) {
  Swaps swapCloseBy = {};
  WeightedSwaps swapExact = {};
  // computes for each gate the necessary moves to execute it
  for (const auto& gate : layer) {
    auto usedQubits = gate->getUsedQubits();
    auto usedHwQubits = this->mapping.getHwQubits(usedQubits);
    if (usedQubits.size() == 2) {
      // swap close by for two qubit gates
      swapCloseBy.emplace_back(*usedHwQubits.begin(), *usedHwQubits.rbegin());
    } else {
      // for multi-qubit gates, find the best position around the gate qubits
      auto bestPos = getBestMultiQubitPosition(gate);
      if (this->parameters.verbose) {
        std::cout << "bestPos: ";
        for (auto qubit : bestPos) {
          std::cout << qubit << " ";
        }
        std::cout << '\n';
      }
      // then compute the exact moves to get to the best position
      auto exactSwapsToPos = getExactSwapsToPosition(gate, bestPos);
      swapExact.insert(swapExact.end(), exactSwapsToPos.begin(),
                       exactSwapsToPos.end());
    }
  }
  // sort and remove duplicates from moveExact
  std::ranges::sort(swapExact, [](const auto& a, const auto& b) {
    return a.first < b.first;
  });
  auto newEnd = std::ranges::unique(swapExact, {},
                                    &decltype(swapExact)::value_type::first);
  swapExact.erase(newEnd.begin(), newEnd.end());
  return {swapCloseBy, swapExact};
}

qc::fp NeutralAtomMapper::swapCostPerLayer(const Swap& swap,
                                           const Swaps& swapCloseBy,
                                           const WeightedSwaps& swapExact) {
  SwapDistance distBefore = 0;
  SwapDistance distAfter = 0;
  qc::fp distChange = 0;
  // bring close only until swap distance =0, bring exact to the exact position
  // bring qubits together to execute gate
  for (const auto& [q1, q2] : swapCloseBy) {
    // distance before
    distBefore = this->hardwareQubits.getSwapDistance(q1, q2);
    if (distBefore == std::numeric_limits<SwapDistance>::max()) {
      continue;
    }
    // do swap
    if (q1 == swap.first) {
      distAfter = this->hardwareQubits.getSwapDistance(swap.second, q2);
    } else if (q2 == swap.second) {
      distAfter = this->hardwareQubits.getSwapDistance(q1, swap.first);
    } else if (q1 == swap.second) {
      distAfter = this->hardwareQubits.getSwapDistance(swap.first, q2);
    } else if (q2 == swap.first) {
      distAfter = this->hardwareQubits.getSwapDistance(q1, swap.second);
    } else {
      continue;
    }
    distChange +=
        static_cast<qc::fp>(distAfter - distBefore) * this->twoQubitSwapWeight;
  }

  // move qubits to the exact position for multi-qubit gates
  for (const auto& [exactSwap, weight] : swapExact) {
    auto origin = exactSwap.first;
    auto destination = exactSwap.second;
    distBefore =
        this->hardwareQubits.getSwapDistance(origin, destination, false);
    if (distBefore == std::numeric_limits<SwapDistance>::max()) {
      continue;
    }
    if (origin == swap.first) {
      if (destination == swap.second) {
        distAfter = 0;
      } else {
        distAfter = this->hardwareQubits.getSwapDistance(swap.second,
                                                         destination, false);
      }
    } else if (origin == swap.second) {
      if (destination == swap.first) {
        distAfter = 0;
      } else {
        distAfter = this->hardwareQubits.getSwapDistance(swap.first,
                                                         destination, false);
      }
    } else {
      continue;
    }
    // multiply by multi-qubit weight
    // is larger for more qubits and if the qubits are closer together
    distChange += static_cast<qc::fp>(distAfter - distBefore) * weight;
  }

  return distChange;
}

HwQubits NeutralAtomMapper::getBestMultiQubitPosition(const qc::Operation* op) {
  // try to find position around gate Qubits recursively
  // if not, search through coupling graph until found according to a
  // priority queue based on the distance to the other qubits

  std::priority_queue<std::pair<qc::fp, HwQubit>,
                      std::vector<std::pair<qc::fp, HwQubit>>, std::greater<>>
      qubitQueue;
  // add the gate qubits to the priority queue
  auto gateQubits = op->getUsedQubits();
  auto gateHwQubits = this->mapping.getHwQubits(gateQubits);
  // add the gate qubits to the priority queue
  for (const auto& gateQubit : gateHwQubits) {
    qc::fp totalDist = 0;
    for (const auto& otherGateQubit : gateHwQubits) {
      if (gateQubit == otherGateQubit) {
        continue;
      }
      totalDist +=
          this->hardwareQubits.getSwapDistance(gateQubit, otherGateQubit, true);
    }
    qubitQueue.emplace(totalDist, gateQubit);
  }

  // run through the priority queue until a position is found
  std::set<HwQubit> visitedQubits;
  while (!qubitQueue.empty()) {
    auto qubit = qubitQueue.top().second;
    visitedQubits.emplace(qubit);
    qubitQueue.pop();

    // remove selected qubit from the gate qubits
    auto tempGateHwQubits = gateHwQubits;
    if (tempGateHwQubits.contains(qubit)) {
      tempGateHwQubits.erase(tempGateHwQubits.find(qubit));
    }
    auto bestPos = getBestMultiQubitPositionRec(
        tempGateHwQubits, {qubit}, this->hardwareQubits.getNearbyQubits(qubit));
    if (!bestPos.empty()) {
      return bestPos;
    }
    // add nearby qubits to the priority queue
    for (const auto& nearbyQubit :
         this->hardwareQubits.getNearbyQubits(qubit)) {
      if (visitedQubits.contains(nearbyQubit)) {
        continue;
      }
      // compute total distance to all other gate qubits
      qc::fp totalDist = 0;
      for (const auto& otherGateQubit : gateHwQubits) {
        if (nearbyQubit == otherGateQubit) {
          continue;
        }
        totalDist +=
            this->hardwareQubits.getSwapDistance(nearbyQubit, otherGateQubit);
      }
      qubitQueue.emplace(totalDist, nearbyQubit);
    }
  }
  // find gate and move it to the shuttling layer
  auto idxFrontGate = std::ranges::find(this->frontLayerGate, op);
  if (idxFrontGate != this->frontLayerGate.end()) {
    this->frontLayerGate.erase(idxFrontGate);
    this->frontLayerShuttling.emplace_back(op);
  }
  // remove from lookahead layer if there
  auto idxLookaheadGate = std::ranges::find(this->lookaheadLayerGate, op);
  if (idxLookaheadGate != this->lookaheadLayerGate.end()) {
    this->lookaheadLayerGate.erase(idxLookaheadGate);
    this->lookaheadLayerShuttling.emplace_back(op);
  }
  return {};
}

HwQubits NeutralAtomMapper::getBestMultiQubitPositionRec(
    HwQubits remainingGateQubits, std::vector<HwQubit> selectedQubits,
    HwQubits remainingNearbyQubits) {
  // check if done
  if (remainingGateQubits.empty()) {
    HwQubits bestPos(selectedQubits.begin(), selectedQubits.end());
    return bestPos;
  }
  // update remainingNearbyQubits
  auto newQubit = *selectedQubits.rbegin();
  auto nearbyNextQubit = this->hardwareQubits.getNearbyQubits(newQubit);
  // compute remaining qubits as the intersection with current
  Qubits newRemainingQubits;
  std::ranges::set_intersection(
      remainingNearbyQubits, nearbyNextQubit,
      std::inserter(newRemainingQubits, newRemainingQubits.begin()));
  for (const auto& qubit : selectedQubits) {
    if (newRemainingQubits.contains(qubit)) {
      newRemainingQubits.erase(newRemainingQubits.find(qubit));
    }
  }
  remainingNearbyQubits = newRemainingQubits;

  // if not enough space
  if (remainingNearbyQubits.size() < remainingGateQubits.size()) {
    return {};
  }

  std::vector<std::pair<HwQubit, qc::fp>> summedDistances;
  for (const auto& hwQubit : remainingNearbyQubits) {
    qc::fp distance = 0;
    for (const auto& gateHwQubit : remainingGateQubits) {
      if (hwQubit == gateHwQubit) {
        // gate qubit is already at one of the positions -> assign it
        selectedQubits.emplace_back(hwQubit);
        remainingGateQubits.erase(remainingGateQubits.find(gateHwQubit));
        return getBestMultiQubitPositionRec(remainingGateQubits, selectedQubits,
                                            remainingNearbyQubits);
      }
      distance +=
          this->hardwareQubits.getSwapDistance(hwQubit, gateHwQubit, true);
    }
    summedDistances.emplace_back(hwQubit, distance);
  }
  // select next qubit as the one with minimal distance
  auto nextQubitDist = std::ranges::min_element(
      summedDistances, [](const auto& qubit1, const auto& qubit2) {
        return qubit1.second < qubit2.second;
      });
  auto nextQubit = nextQubitDist->first;
  selectedQubits.emplace_back(nextQubit);
  // remove from remaining gate qubits the one that is closest to the next
  auto closesGateQubits = *remainingGateQubits.begin();
  auto closesDistance =
      this->hardwareQubits.getSwapDistance(closesGateQubits, nextQubit, true);
  for (const auto& gateQubit : remainingGateQubits) {
    auto distance =
        this->hardwareQubits.getSwapDistance(gateQubit, nextQubit, true);
    if (distance < closesDistance) {
      closesGateQubits = gateQubit;
      closesDistance = distance;
    }
  }
  remainingGateQubits.erase(remainingGateQubits.find(closesGateQubits));

  return getBestMultiQubitPositionRec(remainingGateQubits, selectedQubits,
                                      remainingNearbyQubits);
}

WeightedSwaps
NeutralAtomMapper::getExactSwapsToPosition(const qc::Operation* op,
                                           HwQubits position) {
  if (position.empty()) {
    return {};
  }
  auto gateQubits = op->getUsedQubits();
  auto gateHwQubits = this->mapping.getHwQubits(gateQubits);
  WeightedSwaps swapsExact;
  while (!position.empty() && !gateHwQubits.empty()) {
    std::vector<std::tuple<HwQubit, std::set<HwQubit>, SwapDistance>>
        minimalDistances;
    std::set<HwQubit> minimalDistancePosQubit;
    for (const auto& gateQubit : gateHwQubits) {
      SwapDistance minimalDistance = std::numeric_limits<SwapDistance>::max();
      for (const auto& posQubit : position) {
        auto distance =
            this->hardwareQubits.getSwapDistance(gateQubit, posQubit, false);
        if (distance < minimalDistance) {
          minimalDistance = distance;
          minimalDistancePosQubit.clear();
          minimalDistancePosQubit.emplace(posQubit);
        } else if (distance == minimalDistance) {
          minimalDistancePosQubit.emplace(posQubit);
        }
      }
      if (minimalDistance == std::numeric_limits<SwapDistance>::max()) {
        // not possible to move to position
        // move gate to shuttling layer
        auto idxFrontGate = std::ranges::find(this->frontLayerGate, op);
        if (idxFrontGate != this->frontLayerGate.end()) {
          this->frontLayerGate.erase(idxFrontGate);
          this->frontLayerShuttling.emplace_back(op);
        }
        // remove from lookahead layer if there
        auto idxLookaheadGate = std::ranges::find(this->lookaheadLayerGate, op);
        if (idxLookaheadGate != this->lookaheadLayerGate.end()) {
          this->lookaheadLayerGate.erase(idxLookaheadGate);
          this->lookaheadLayerShuttling.emplace_back(op);
        }
        return {};
      }
      minimalDistances.emplace_back(gateQubit, minimalDistancePosQubit,
                                    minimalDistance);
    }
    // find gate qubit with maximal minimal distance to assign first to a
    // position
    auto assignFirst = std::ranges::max_element(
        minimalDistances, [](const auto& qubit1, const auto& qubit2) {
          return std::get<2>(qubit1) < std::get<2>(qubit2);
        });

    auto assignedGateQubit = std::get<0>(*assignFirst);
    auto assignedPosQubits = std::get<1>(*assignFirst);
    // for multiple equal good positions, choose the one that
    // is not assigned to one of the other ones
    HwQubit assignedPosQubit = *assignedPosQubits.begin();
    if (assignedPosQubits.size() > 1) {
      for (const auto& posQubit : assignedPosQubits) {
        // as all places within the position can reach each other, it is
        // sufficient to check for a single unoccupied position
        // check if posQubit is assigned at its current position
        if (std::ranges::none_of(
                minimalDistances, [&posQubit](const auto& qubit) {
                  return std::get<0>(qubit) == posQubit &&
                         *(std::get<1>(qubit).begin()) == posQubit;
                })) {
          assignedPosQubit = posQubit;
          break;
        }
      }
    }

    // assign gateQubit to position by removing both from gateHwQubits and
    // position
    gateHwQubits.erase(gateHwQubits.find(assignedGateQubit));
    position.erase(position.find(assignedPosQubit));
    // and add to exactMove if not swap with one of the other qubits
    // only problem if their exact swap distance is 1
    if (std::ranges::none_of(gateHwQubits,
                             [&assignedGateQubit, this](const auto& qubit) {
                               return assignedGateQubit == qubit &&
                                      this->hardwareQubits.getSwapDistance(
                                          assignedGateQubit, qubit, false) == 1;
                             }) &&
        assignedGateQubit != assignedPosQubit) {
      swapsExact.emplace_back(
          std::make_pair(assignedGateQubit, assignedPosQubit), 0);
    }
  }

  // compute total distance of all moves
  SwapDistance totalDistance = 0;
  for (const auto& [swap, weight] : swapsExact) {
    auto [q1, q2] = swap;
    totalDistance += this->hardwareQubits.getSwapDistance(q1, q2, false);
  }
  // add cost to the moves -> move first qubit corresponding to almost finished
  // positions
  auto nQubits = op->getUsedQubits().size();
  auto multiQubitFactor =
      (static_cast<qc::fp>(nQubits) * static_cast<qc::fp>(nQubits - 1)) / 2;
  for (auto& move : swapsExact) {
    move.second = multiQubitFactor / static_cast<qc::fp>(totalDistance);
  }

  return swapsExact;
}

AtomMove NeutralAtomMapper::findBestAtomMove() {
  auto moveCombs = getAllMoveCombinations();

  // compute cost for each move combination
  std::vector<std::pair<MoveComb, qc::fp>> moveCosts;
  moveCosts.reserve(moveCombs.size());
  for (const auto& moveComb : moveCombs) {
    moveCosts.emplace_back(moveComb, moveCostComb(moveComb));
  }

  std::ranges::sort(moveCosts, [](const auto& move1, const auto& move2) {
    return move1.second < move2.second;
  });

  // get move of minimal cost
  auto bestMove = std::ranges::min_element(
      moveCosts, [](const auto& move1, const auto& move2) {
        return move1.second < move2.second;
      });
  return bestMove->first.getFirstMove();
}

qc::fp NeutralAtomMapper::moveCostComb(const MoveComb& moveComb) {
  qc::fp costComb = 0;
  for (const auto& move : moveComb.moves) {
    costComb += moveCost(move);
  }
  return costComb;
}

qc::fp NeutralAtomMapper::moveCost(const AtomMove& move) {
  qc::fp cost = 0;
  auto frontCost = moveCostPerLayer(move, this->frontLayerShuttling) /
                   static_cast<qc::fp>(this->frontLayerShuttling.size());
  cost += frontCost;
  if (!lookaheadLayerShuttling.empty()) {
    auto lookaheadCost =
        moveCostPerLayer(move, this->lookaheadLayerShuttling) /
        static_cast<qc::fp>(this->lookaheadLayerShuttling.size());
    cost += parameters.lookaheadWeightMoves * lookaheadCost;
  }
  if (!this->lastMoves.empty()) {
    auto parallelCost = parameters.shuttlingTimeWeight *
                        parallelMoveCost(move) /
                        static_cast<qc::fp>(this->lastMoves.size()) /
                        static_cast<qc::fp>(this->frontLayerShuttling.size());
    cost += parallelCost;
  }

  return cost;
}

qc::fp NeutralAtomMapper::moveCostPerLayer(const AtomMove& move,
                                           GateList& layer) {
  // compute cost assuming the move was applied
  qc::fp distChange = 0;
  auto toMoveHwQubit = this->hardwareQubits.getHwQubit(move.first);
  if (this->mapping.isMapped(toMoveHwQubit)) {
    auto toMoveCircuitQubit = this->mapping.getCircQubit(toMoveHwQubit);
    for (const auto& gate : layer) {
      auto usedQubits = gate->getUsedQubits();
      if (usedQubits.contains(toMoveCircuitQubit)) {
        // check distance reduction
        qc::fp distanceBefore = 0;
        for (const auto& qubit : usedQubits) {
          if (qubit == toMoveCircuitQubit) {
            continue;
          }
          auto hwQubit = this->mapping.getHwQubit(qubit);
          auto dist = this->arch.getEuclideanDistance(
              this->hardwareQubits.getCoordIndex(hwQubit),
              this->hardwareQubits.getCoordIndex(toMoveHwQubit));
          distanceBefore += dist;
        }
        qc::fp distanceAfter = 0;
        for (const auto& qubit : usedQubits) {
          if (qubit == toMoveCircuitQubit) {
            continue;
          }
          auto hwQubit = this->mapping.getHwQubit(qubit);
          auto dist = this->arch.getEuclideanDistance(
              this->hardwareQubits.getCoordIndex(hwQubit), move.second);
          distanceAfter += dist;
        }
        distChange += distanceAfter - distanceBefore;
      }
    }
  }
  return distChange;
}

qc::fp NeutralAtomMapper::parallelMoveCost(const AtomMove& move) {
  qc::fp parallelCost = 0;
  auto moveVector = this->arch.getVector(move.first, move.second);
  std::vector<CoordIndex> lastEndingCoords;
  if (this->lastMoves.empty()) {
    parallelCost += arch.getVectorShuttlingTime(moveVector);
  }
  for (const auto& lastMove : this->lastMoves) {
    lastEndingCoords.emplace_back(lastMove.second);
    // decide of shuttling can be done in parallel
    auto lastMoveVector = this->arch.getVector(lastMove.first, lastMove.second);
    if (moveVector.overlap(lastMoveVector)) {
      if (moveVector.direction != lastMoveVector.direction) {
        parallelCost += arch.getVectorShuttlingTime(moveVector);
      } else {
        // check if move can be done in parallel
        if (moveVector.include(lastMoveVector)) {
          parallelCost += arch.getVectorShuttlingTime(moveVector);
        }
      }
    }
  }
  // check if in same row/column like last moves
  // then can may be loaded in parallel
  auto moveCoordInit = this->arch.getCoordinate(move.first);
  auto moveCoordEnd = this->arch.getCoordinate(move.second);
  parallelCost += arch.getShuttlingTime(qc::OpType::AodActivate) +
                  arch.getShuttlingTime(qc::OpType::AodDeactivate);
  for (const auto& lastMove : this->lastMoves) {
    auto lastMoveCoordInit = this->arch.getCoordinate(lastMove.first);
    auto lastMoveCoordEnd = this->arch.getCoordinate(lastMove.second);
    if (moveCoordInit.x == lastMoveCoordInit.x ||
        moveCoordInit.y == lastMoveCoordInit.y) {
      parallelCost -= arch.getShuttlingTime(qc::OpType::AodActivate);
    }
    if (moveCoordEnd.x == lastMoveCoordEnd.x ||
        moveCoordEnd.y == lastMoveCoordEnd.y) {
      parallelCost -= arch.getShuttlingTime(qc::OpType::AodDeactivate);
    }
  }
  // check if move can use AOD atom from last moves
  //  if (std::find(lastEndingCoords.begin(), lastEndingCoords.end(),
  //  move.first) ==
  //      lastEndingCoords.end()) {
  //    parallelCost += arch.getShuttlingTime(qc::OpType::AodActivate) +
  //                    arch.getShuttlingTime(qc::OpType::AodDeactivate);
  //  }
  return parallelCost;
}

MultiQubitMovePos
NeutralAtomMapper::getMovePositionRec(MultiQubitMovePos currentPos,
                                      const CoordIndices& gateCoords,
                                      const size_t& maxNMoves) {
  if (currentPos.coords.size() == gateCoords.size()) {
    return currentPos;
  }
  if (currentPos.nMoves > maxNMoves) {
    return {};
  }

  auto nearbyCoords = this->arch.getNearbyCoordinates(currentPos.coords.back());
  // filter out coords that have a SWAP distance unequal to 0 to any of the
  // current qubits. Also sort out coords that are already in the vector
  std::vector<CoordIndex> filteredNearbyCoords;
  for (const auto& coord : nearbyCoords) {
    bool valid = true;
    for (const auto& qubit : currentPos.coords) {
      if (this->arch.getSwapDistance(qubit, coord) != 0 || coord == qubit) {
        valid = false;
        break;
      }
    }
    if (valid) {
      filteredNearbyCoords.emplace_back(coord);
    }
  }

  // differentiate between free and occupied coords
  CoordIndices freeNearbyCoords;
  CoordIndices occupiedNearbyCoords;
  CoordIndices occupiedGateCoords;
  for (const auto& coord : filteredNearbyCoords) {
    if (this->hardwareQubits.isMapped(coord)) {
      if (std::ranges::find(gateCoords, coord) != gateCoords.end()) {
        occupiedGateCoords.emplace_back(coord);
      } else {
        occupiedNearbyCoords.emplace_back(coord);
      }
    } else {
      freeNearbyCoords.emplace_back(coord);
    }
  }

  // compute minimal possible moves
  size_t minPossibleMoves = currentPos.nMoves;
  size_t const nMissingQubits = gateCoords.size() - currentPos.coords.size();
  auto itGate = occupiedGateCoords.begin();
  auto itFree = freeNearbyCoords.begin();
  auto itOcc = occupiedNearbyCoords.begin();
  for (size_t i = 0; i < nMissingQubits; ++i) {
    if (itGate != occupiedGateCoords.end()) {
      ++itGate;
    } else if (itFree != freeNearbyCoords.end()) {
      ++itFree;
      minPossibleMoves += 1;
    } else if (itOcc != occupiedNearbyCoords.end()) {
      ++itOcc;
      minPossibleMoves += 2;
    }
  }
  if (minPossibleMoves > maxNMoves) {
    return {};
  }

  for (const auto& gateCoord : occupiedGateCoords) {
    MultiQubitMovePos nextPos = MultiQubitMovePos(currentPos);
    nextPos.coords.emplace_back(gateCoord);
    auto bestPos = getMovePositionRec(nextPos, gateCoords, maxNMoves);
    if (bestPos.coords.size() == gateCoords.size()) {
      return bestPos;
    }
  }

  for (const auto& freeCoord : freeNearbyCoords) {
    MultiQubitMovePos nextPos = MultiQubitMovePos(currentPos);
    nextPos.coords.emplace_back(freeCoord);
    nextPos.nMoves += 1;
    auto bestPos = getMovePositionRec(nextPos, gateCoords, maxNMoves);
    if (bestPos.coords.size() == gateCoords.size()) {
      return bestPos;
    }
  }

  for (const auto& occCoord : occupiedNearbyCoords) {
    MultiQubitMovePos nextPos = MultiQubitMovePos(currentPos);
    nextPos.coords.emplace_back(occCoord);
    nextPos.nMoves += 2;
    auto bestPos = getMovePositionRec(nextPos, gateCoords, maxNMoves);
    if (bestPos.coords.size() == gateCoords.size()) {
      return bestPos;
    }
  }

  // if no position found, return empty
  return {};
}

MoveCombs NeutralAtomMapper::getAllMoveCombinations() {
  MoveCombs allMoves;
  for (const auto& op : this->frontLayerShuttling) {
    auto usedQubits = op->getUsedQubits();
    auto usedHwQubits = this->mapping.getHwQubits(usedQubits);
    auto usedCoordsSet = this->hardwareQubits.getCoordIndices(usedHwQubits);
    auto usedCoords =
        std::vector<CoordIndex>(usedCoordsSet.begin(), usedCoordsSet.end());
    auto bestPos = getBestMovePos(usedCoords);
    auto moves = getMoveCombinationsToPosition(usedHwQubits, bestPos);
    allMoves.addMoveCombs(moves);
  }
  allMoves.removeLongerMoveCombs();
  return allMoves;
}

CoordIndices NeutralAtomMapper::getBestMovePos(const CoordIndices& gateCoords) {
  size_t const maxMoves = gateCoords.size() * 2;
  size_t const minMoves = gateCoords.size();
  size_t nMovesGate = maxMoves;
  // do a breadth first search for the best position
  // start with the used coords
  std::queue<CoordIndex> q;
  for (const auto& coord : gateCoords) {
    q.push(coord);
  }
  std::vector<CoordIndex> visited;

  auto finalBestPos = MultiQubitMovePos();
  while (!q.empty()) {
    auto coord = q.front();
    q.pop();
    if (std::ranges::find(visited, coord) != visited.end()) {
      continue;
    }
    visited.emplace_back(coord);
    MultiQubitMovePos currentPos;
    currentPos.coords.emplace_back(coord);
    if (this->hardwareQubits.isMapped(coord)) {
      if (std::ranges::find(gateCoords, coord) != gateCoords.end()) {
        currentPos.nMoves = 0;
      } else {
        currentPos.nMoves = 2;
      }
    } else {
      currentPos.nMoves = 1;
    }
    auto bestPos = getMovePositionRec(currentPos, gateCoords, nMovesGate);
    if (!bestPos.coords.empty() && bestPos.nMoves <= minMoves) {
      return bestPos.coords;
    }

    // min not yet reached, check nearby
    if (!bestPos.coords.empty()) {
      nMovesGate = std::min(nMovesGate, bestPos.nMoves);
    }
    for (const auto& nearbyCoord : this->arch.getNearbyCoordinates(coord)) {
      if (std::ranges::find(visited, nearbyCoord) == visited.end()) {
        q.push(nearbyCoord);
      }
    }
  }
  throw std::runtime_error(
      "No move position found (check if enough free coords are available)");
}

MoveCombs
NeutralAtomMapper::getMoveCombinationsToPosition(HwQubits& gateQubits,
                                                 CoordIndices& position) {
  if (position.empty()) {
    throw std::invalid_argument("No position given");
  }
  // compute for each qubit the best position around it based on the cost of
  // the single move choose best one
  MoveCombs const moveCombinations;
  std::set<CoordIndex> gateQubitCoords;
  for (const auto& gateQubit : gateQubits) {
    gateQubitCoords.emplace(this->hardwareQubits.getCoordIndex(gateQubit));
  }

  auto remainingCoords = position;
  MoveComb moveComb;
  // compute cost for each candidate and each gateQubit
  auto remainingGateCoords = gateQubitCoords;
  // pre-filter away all gateQubitCoords which are already in the position
  for (auto it = remainingGateCoords.begin();
       it != remainingGateCoords.end();) {
    auto remainingCoordsIdx = std::ranges::find(remainingCoords, *it);
    if (remainingCoordsIdx != remainingCoords.end()) {
      remainingCoords.erase(remainingCoordsIdx);
      it = remainingGateCoords.erase(it);
    } else {
      ++it;
    }
  }

  while (!remainingGateCoords.empty()) {
    auto currentGateQubit = *remainingGateCoords.begin();
    // compute costs and find best coord
    std::vector<std::pair<CoordIndex, qc::fp>> costs;
    for (const auto& remainingCoord : remainingCoords) {
      if (this->hardwareQubits.isMapped(remainingCoord)) {
        const auto moveAwayComb = getMoveAwayCombinations(
            currentGateQubit, remainingCoord, remainingCoords);
        for (const auto& moveAway : moveAwayComb) {
          auto cost = moveCostComb(moveAway);
          costs.emplace_back(remainingCoord, cost);
        }
      } else {
        auto cost = moveCost({currentGateQubit, remainingCoord});
        costs.emplace_back(remainingCoord, cost);
      }
    }
    // find minimal cost
    auto bestCost = std::ranges::min_element(
        costs, [](const auto& cost1, const auto& cost2) {
          return cost1.second < cost2.second;
        });
    auto bestCoord = bestCost->first;
    if (this->hardwareQubits.isMapped(bestCoord)) {
      auto moveAwayComb =
          getMoveAwayCombinations(currentGateQubit, bestCoord, remainingCoords);
      for (const auto& moveAway : moveAwayComb) {
        moveComb.append(moveAway);
      }
    } else {
      moveComb.append(AtomMove{currentGateQubit, bestCoord});
    }
    remainingGateCoords.erase(currentGateQubit);
    remainingCoords.erase(std::ranges::find(remainingCoords, bestCoord));
  }
  return MoveCombs({moveComb});
}

MoveCombs
NeutralAtomMapper::getMoveAwayCombinations(CoordIndex startCoord,
                                           CoordIndex targetCoord,
                                           const CoordIndices& excludedCoords) {
  MoveCombs moveCombinations;
  auto const originalVector = this->arch.getVector(startCoord, targetCoord);
  auto const originalDirection = originalVector.direction;
  // Find move away target in the same direction as the original move
  auto moveAwayTargets = this->hardwareQubits.findClosestFreeCoord(
      targetCoord, originalDirection, excludedCoords);
  for (const auto& moveAwayTarget : moveAwayTargets) {
    const AtomMove move = {startCoord, targetCoord};
    const AtomMove moveAway = {targetCoord, moveAwayTarget};
    moveCombinations.addMoveComb(MoveComb({moveAway, move}));
  }
  if (moveCombinations.empty()) {
    throw std::runtime_error("No move away target found");
  }
  return moveCombinations;
}

std::pair<uint32_t, qc::fp>
NeutralAtomMapper::estimateNumSwapGates(const qc::Operation* opPointer) {
  auto usedQubits = opPointer->getUsedQubits();
  auto usedHwQubits = this->mapping.getHwQubits(usedQubits);
  qc::fp minNumSwaps = 0;
  if (usedHwQubits.size() == 2) {
    SwapDistance minDistance = std::numeric_limits<SwapDistance>::max();
    for (const auto& hwQubit : usedHwQubits) {
      for (const auto& otherHwQubit : usedHwQubits) {
        if (hwQubit == otherHwQubit) {
          continue;
        }
        auto distance =
            this->hardwareQubits.getSwapDistance(hwQubit, otherHwQubit);
        minDistance = std::min(distance, minDistance);
      }
    }
    minNumSwaps = minDistance;
  } else { // multi-qubit gates
    auto bestPos = getBestMultiQubitPosition(opPointer);
    if (bestPos.empty()) {
      return {std::numeric_limits<SwapDistance>::max(),
              std::numeric_limits<qc::fp>::max()};
    }
    auto exactSwaps = getExactSwapsToPosition(opPointer, bestPos);
    if (exactSwaps.empty()) {
      return {std::numeric_limits<SwapDistance>::max(),
              std::numeric_limits<qc::fp>::max()};
    }
    for (const auto& [swap, weight] : exactSwaps) {
      auto [q1, q2] = swap;
      minNumSwaps += this->hardwareQubits.getSwapDistance(q1, q2, false);
    }
  }
  const qc::fp minTime = minNumSwaps * this->arch.getGateTime("swap");
  return {minNumSwaps, minTime};
}

std::pair<uint32_t, qc::fp>
NeutralAtomMapper::estimateNumMove(const qc::Operation* opPointer) {
  auto usedQubits = opPointer->getUsedQubits();
  auto usedHwQubits = this->mapping.getHwQubits(usedQubits);
  auto usedCoords = this->hardwareQubits.getCoordIndices(usedHwQubits);
  // estimate the number of moves as:
  // compute distance between qubits
  // 1. for each free coord in the vicinity = 1 move with corresponding
  // distance
  // 2. for each occupied coord in the vicinity = 2 moves with corresponding
  // distance

  uint32_t minMoves = std::numeric_limits<uint32_t>::max();
  qc::fp minTime = std::numeric_limits<qc::fp>::max();
  for (const auto& coord : usedCoords) {
    qc::fp totalTime = 0;
    uint32_t totalMoves = 0;
    auto nearbyFreeCoords =
        this->hardwareQubits.getNearbyFreeCoordinatesByCoord(coord);
    auto nearbyOccupiedCoords =
        this->hardwareQubits.getNearbyOccupiedCoordinatesByCoord(coord);
    auto otherQubitsIt = usedCoords.begin();
    auto nearbyFreeIt = nearbyFreeCoords.begin();
    auto nearbyOccIt = nearbyOccupiedCoords.begin();
    while (otherQubitsIt != usedCoords.end()) {
      auto otherCoord = *otherQubitsIt;
      if (otherCoord == coord) {
        otherQubitsIt++;
        continue;
      }
      if (nearbyFreeIt != nearbyFreeCoords.end()) {
        totalTime += this->arch.getVectorShuttlingTime(
            this->arch.getVector(otherCoord, *nearbyFreeIt));
        totalTime += this->arch.getShuttlingTime(qc::OpType::AodActivate) +
                     this->arch.getShuttlingTime(qc::OpType::AodDeactivate);
        nearbyFreeIt++;
        totalMoves++;
      } else if (nearbyOccIt != nearbyOccupiedCoords.end()) {
        totalTime += 2 * this->arch.getVectorShuttlingTime(
                             this->arch.getVector(otherCoord, *nearbyOccIt));
        totalTime +=
            2 * (this->arch.getShuttlingTime(qc::OpType::AodActivate) +
                 this->arch.getShuttlingTime(qc::OpType::AodDeactivate));
        nearbyOccIt++;
        totalMoves += 2;
      } else {
        throw std::runtime_error("No space to "
                                 "execute a multi-qubit gate. "
                                 "Check int radius. Op:" +
                                 opPointer->getName() + " nQubit: " +
                                 std::to_string(usedQubits.size()));
      }

      otherQubitsIt++;
    }

    if (totalTime < minTime) {
      minTime = totalTime;
      minMoves = totalMoves;
    }
  }

  return {minMoves, minTime};
}

bool NeutralAtomMapper::swapGateBetter(const qc::Operation* opPointer) {
  auto [minNumSwaps, minTimeSwaps] = estimateNumSwapGates(opPointer);
  if (minNumSwaps == 0) {
    return true;
  }
  auto [minMoves, minTimeMoves] = estimateNumMove(opPointer);
  auto fidSwaps =
      std::exp(-minTimeSwaps * this->arch.getNqubits() /
               this->arch.getDecoherenceTime()) *
      std::pow(this->arch.getGateAverageFidelity("swap"), minNumSwaps);
  auto fidMoves =
      std::exp(-minTimeMoves * this->arch.getNqubits() /
               this->arch.getDecoherenceTime()) *
      std::pow(
          this->arch.getShuttlingAverageFidelity(qc::OpType::AodMove) *
              this->arch.getShuttlingAverageFidelity(qc::OpType::AodActivate) *
              this->arch.getShuttlingAverageFidelity(qc::OpType::AodDeactivate),
          minMoves);

  return fidSwaps * parameters.gateWeight >
         fidMoves * parameters.shuttlingWeight;
}

} // namespace na
