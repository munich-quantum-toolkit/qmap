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
#include "hybridmap/Mapping.hpp"
#include "hybridmap/MoveToAodConverter.hpp"
#include "hybridmap/NeutralAtomDefinitions.hpp"
#include "hybridmap/NeutralAtomLayer.hpp"
#include "hybridmap/NeutralAtomUtils.hpp"
#include "ir/Definitions.hpp"
#include "ir/QuantumComputation.hpp"
#include "ir/operations/Control.hpp"
#include "ir/operations/OpType.hpp"
#include "ir/operations/Operation.hpp"
#include "ir/operations/StandardOperation.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <limits>
#include <memory>
#include <queue>
#include <ranges>
#include <set>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace na {
void NeutralAtomMapper::mapAppend(qc::QuantumComputation& qc,
                                  const Mapping& initialMapping) {
  // remove barriers and measurements
  qc::CircuitOptimizer::removeFinalMeasurements(qc);
  // check if multi-qubit gates are present
  multiQubitGates = false;
  for (const auto& op : qc) {
    if (op->getUsedQubits().size() > 2) {
      // deactivate static mapping
      spdlog::warn(
          "The circuit contains multi-qubit gates (more than 2 qubits). "
          "Bridge gates will NOT be used for mapping.");

      multiQubitGates = true;
      break;
    }
  }
  // only add flying ancillas if not already present
  if (mappedQc.getNancillae() == 0) {
    // ancilla register has indices [npositions, 2*npositions-1]
    mappedQc.addAncillaryRegister(this->arch->getNpositions());
    // flying ancilla register has indices [2*npositions, 3*npositions-1]
    mappedQc.addAncillaryRegister(this->arch->getNpositions(), "fa");
  }

  qc::CircuitOptimizer::replaceMCXWithMCZ(qc);
  qc::CircuitOptimizer::singleQubitGateFusion(qc);
  qc::CircuitOptimizer::flattenOperations(qc);
  qc::CircuitOptimizer::removeFinalMeasurements(qc);

  const auto dag = qc::CircuitOptimizer::constructDAG(qc);

  mapping = initialMapping;

  if (this->parameters.verbose) {
    spdlog::info("* Init Coord Mapping w/ [row:{} X col:{}] hardware",
                 arch->getNrows(), arch->getNcolumns());
    for (uint32_t q = 0; q < qc.getNqubits(); q++) {
      const auto hwQubit = this->mapping.getHwQubit(q);
      spdlog::info("q {:3} -> h {:3} -> c {:3}", q, hwQubit,
                   hardwareQubits.getCoordIndex(hwQubit));
    }
  }

  // init layers
  NeutralAtomLayer lookaheadLayer(dag, false, this->parameters.lookaheadDepth);
  lookaheadLayer.initAllQubits();
  NeutralAtomLayer frontLayer(dag, true, this->parameters.lookaheadDepth);
  frontLayer.initAllQubits();
  lookaheadLayer.removeGatesAndUpdate(frontLayer.getGates());
  mapAllPossibleGates(frontLayer, lookaheadLayer);

  // Checks
  if (dag.size() > arch->getNqubits()) {
    throw std::runtime_error("More qubits in circuit than in architecture");
  }

  // Mapping Loop
  size_t i = 0;
  while (!frontLayer.getGates().empty()) {
    // assign gates to layers
    reassignGatesToLayers(frontLayer.getGates(), lookaheadLayer.getGates());
    if (this->parameters.verbose) {
      spdlog::info("Iteration {}", i);
      printLayers();
    }

    i = gateBasedMapping(frontLayer, lookaheadLayer, i);
    i = shuttlingBasedMapping(frontLayer, lookaheadLayer, i);
  }

  if (this->parameters.verbose) {
    spdlog::info("nSwaps: {}", stats.nSwaps);
    spdlog::info("nBridges: {}", stats.nBridges);
    spdlog::info("nFAncillas: {}", stats.nFAncillas);
    spdlog::info("nMoves: {}", stats.nMoves);
    spdlog::info("nPassBy: {}", stats.nPassBy);
  }
}

void NeutralAtomMapper::mapAllPossibleGates(NeutralAtomLayer& frontLayer,
                                            NeutralAtomLayer& lookaheadLayer) {
  auto executableGates = getExecutableGates(frontLayer.getGates());
  while (!executableGates.empty()) {
    for (const auto* opPointer : executableGates) {
      mapGate(opPointer);
    }
    frontLayer.removeGatesAndUpdate(executableGates);
    lookaheadLayer.removeGatesAndUpdate(frontLayer.getNewGates());
    executableGates = getExecutableGates(frontLayer.getGates());
  }
}

void NeutralAtomMapper::decomposeBridgeGates(qc::QuantumComputation& qc) const {
  auto it = qc.begin();
  while (it != qc.end()) {
    if ((*it)->isStandardOperation() && (*it)->getType() == qc::Bridge) {
      const auto targets = (*it)->getTargets();
      it = qc.erase(it);
      size_t nInserted = 0;
      for (const auto& bridgeOp :
           this->arch->getBridgeCircuit(targets.size())) {
        const auto bridgeQubits = bridgeOp->getUsedQubits();
        if (bridgeOp->getType() == qc::OpType::H) {
          it = qc.insert(it, std::make_unique<qc::StandardOperation>(
                                 targets[*bridgeQubits.begin()], qc::H));
        } else {
          it = qc.insert(it, std::make_unique<qc::StandardOperation>(
                                 qc::Control{targets[*bridgeQubits.begin()]},
                                 targets[*bridgeQubits.rbegin()], qc::Z));
        }
        ++nInserted;
      }
      // Advance past all inserted operations
      for (size_t i = 0; i < nInserted && it != qc.end(); ++i) {
        ++it;
      }
    } else {
      ++it;
    }
  }
}

qc::QuantumComputation NeutralAtomMapper::convertToAod() {
  // decompose SWAP gates
  qc::CircuitOptimizer::decomposeSWAP(mappedQc, false);
  // decompose bridge gates
  decomposeBridgeGates(mappedQc);
  qc::CircuitOptimizer::replaceMCXWithMCZ(mappedQc);
  qc::CircuitOptimizer::singleQubitGateFusion(mappedQc);
  qc::CircuitOptimizer::flattenOperations(mappedQc);
  // decompose AOD moves
  MoveToAodConverter aodScheduler(*arch, hardwareQubits, flyingAncillas);
  mappedQcAOD = aodScheduler.schedule(mappedQc);
  if (this->parameters.verbose) {
    spdlog::info("nMoveGroups: {}", aodScheduler.getNMoveGroups());
  }
  return mappedQcAOD;
}

void NeutralAtomMapper::applyPassBy(NeutralAtomLayer& frontLayer,
                                    const PassByComb& pbComb) {
  const auto opTargets = pbComb.op->getTargets();
  const auto targetHwQubits = mapping.getHwQubits(opTargets);
  auto targetCoords = hardwareQubits.getCoordIndices(targetHwQubits);
  const auto opControls = pbComb.op->getControls();
  HwQubitsVector controlQubits;
  for (const auto& control : opControls) {
    controlQubits.emplace_back(control.qubit);
  }
  const auto controlHwQubits = mapping.getHwQubits(controlQubits);
  auto controlCoords = hardwareQubits.getCoordIndices(controlHwQubits);

  for (const auto& passBy : pbComb.moves) {
    mappedQc.move(passBy.c1, passBy.c2 + arch->getNpositions());
    if (this->parameters.verbose) {
      spdlog::info("passby {} {}", passBy.c1, passBy.c2);
    }
    auto itT = std::ranges::find(targetCoords, passBy.c1);
    if (itT != targetCoords.end()) {
      *itT = passBy.c2 + arch->getNpositions();
    }
    auto itC = std::ranges::find(controlCoords, passBy.c1);
    if (itC != controlCoords.end()) {
      *itC = passBy.c2 + arch->getNpositions();
    }
  }
  const auto opCopy = pbComb.op->clone();
  opCopy->setTargets(targetCoords);
  qc::Controls controls;
  for (const auto& control : controlCoords) {
    controls.emplace(control);
  }
  opCopy->setControls(controls);
  mappedQc.emplace_back(opCopy->clone());

  // mapGate(faComb.op);
  for (const auto& passBy : pbComb.moves) {
    mappedQc.move(passBy.c2 + arch->getNpositions(), passBy.c1);
    if (this->parameters.verbose) {
      spdlog::info("passby {} {}", passBy.c2, passBy.c1);
    }
  }

  frontLayer.removeGatesAndUpdate({pbComb.op});
  this->frontLayerShuttling.erase(
      std::ranges::find(this->frontLayerShuttling, pbComb.op));
  stats.nPassBy += pbComb.moves.size();
}

void NeutralAtomMapper::reassignGatesToLayers(const GateList& frontGates,
                                              const GateList& lookaheadGates) {
  // assign gates to gates or shuttling
  this->frontLayerGate.clear();
  this->frontLayerShuttling.clear();
  for (const auto& gate : frontGates) {
    if (gate->getNqubits() == 1) {
      continue;
    }
    if (swapGateBetter(gate)) {
      this->frontLayerGate.emplace_back(gate);
    } else {
      this->frontLayerShuttling.emplace_back(gate);
    }
  }

  this->lookaheadLayerGate.clear();
  this->lookaheadLayerShuttling.clear();
  for (const auto& gate : lookaheadGates) {
    if (gate->getNqubits() == 1) {
      continue;
    }
    if (swapGateBetter(gate)) {
      this->lookaheadLayerGate.emplace_back(gate);
    } else {
      this->lookaheadLayerShuttling.emplace_back(gate);
    }
  }
}

void NeutralAtomMapper::mapGate(const qc::Operation* op) {
  if (this->parameters.verbose) {
    spdlog::info("mapped {}", op->getName());
    for (const auto qubit : op->getUsedQubits()) {
      spdlog::info("{}", qubit);
    }
  }
  // convert circuit qubits to CoordIndex and append to mappedQc
  const auto opCopyUnique = op->clone();
  auto* opCopy = opCopyUnique.get();
  this->mapping.mapToHwQubits(opCopy);
  this->hardwareQubits.mapToCoordIdx(opCopy);
  this->mappedQc.emplace_back(opCopy->clone());
}

bool NeutralAtomMapper::isExecutable(const qc::Operation* opPointer) {
  const auto usedQubits = opPointer->getUsedQubits();
  std::set<qc::Qubit> usedHwQubits;
  for (const auto qubit : usedQubits) {
    usedHwQubits.emplace(this->mapping.getHwQubit(qubit));
  }
  return this->hardwareQubits.getAllToAllSwapDistance(usedHwQubits) == 0;
}

void NeutralAtomMapper::printLayers() const {
  spdlog::info("f,g:");
  for (const auto* op : this->frontLayerGate) {
    spdlog::info("{}", op->getName());
    for (const auto qubit : op->getUsedQubits()) {
      spdlog::info("{}", qubit);
    }
  }
  spdlog::info("");
  spdlog::info("f,s:");
  for (const auto* op : this->frontLayerShuttling) {
    spdlog::info("{}", op->getName());
    for (const auto qubit : op->getUsedQubits()) {
      spdlog::info("{}", qubit);
    }
  }
  spdlog::info("");
  spdlog::info("l,g:");
  for (const auto* op : this->lookaheadLayerGate) {
    spdlog::info("{}", op->getName());
    for (const auto qubit : op->getUsedQubits()) {
      spdlog::info("{}", qubit);
    }
  }
  spdlog::info("");
  spdlog::info("l,s:");
  for (const auto* op : this->lookaheadLayerShuttling) {
    spdlog::info("{}", op->getName());
    for (const auto qubit : op->getUsedQubits()) {
      spdlog::info("{}", qubit);
    }
  }
  spdlog::info("");
}

GateList NeutralAtomMapper::getExecutableGates(const GateList& gates) {
  GateList executableGates;
  for (const auto* opPointer : gates) {
    if (opPointer->getNqubits() == 1 || isExecutable(opPointer)) {
      executableGates.emplace_back(opPointer);
    }
  }
  return executableGates;
}

void NeutralAtomMapper::updateBlockedQubits(const HwQubits& qubits) {
  // save to lastSwaps
  this->lastBlockedQubits.emplace_back(
      this->hardwareQubits.getBlockedQubits(qubits));
  if (this->lastBlockedQubits.size() > this->arch->getNcolumns()) {
    this->lastBlockedQubits.pop_front();
  }
}

void NeutralAtomMapper::applySwap(const Swap& swap) {
  stats.nSwaps++;

  this->mapping.applySwap(swap);
  // convert circuit qubits to CoordIndex and append to mappedQc
  const auto idxFirst = this->hardwareQubits.getCoordIndex(swap.first);
  const auto idxSecond = this->hardwareQubits.getCoordIndex(swap.second);
  this->mappedQc.swap(idxFirst, idxSecond);
  if (this->parameters.verbose) {
    spdlog::info("swapped {} {}", swap.first, swap.second);
    spdlog::info("  logical qubits:");
    if (this->mapping.isMapped(swap.first)) {
      spdlog::info("{}", this->mapping.getCircQubit(swap.first));
    } else {
      spdlog::info("not mapped");
    }
    if (this->mapping.isMapped(swap.second)) {
      spdlog::info(" {}", this->mapping.getCircQubit(swap.second));
    } else {
      spdlog::info(" not mapped");
    }
  }
}

void NeutralAtomMapper::applyMove(AtomMove move) {
  this->lastMoves.emplace_back(move);
  if (this->lastMoves.size() > 4) {
    this->lastMoves.pop_front();
  }
  mappedQc.move(move.c1, move.c2);
  const auto toMoveHwQubit = this->hardwareQubits.getHwQubit(move.c1);
  this->hardwareQubits.move(toMoveHwQubit, move.c2);
  if (this->parameters.verbose) {
    spdlog::info("moved {} to {}", move.c1, move.c2);
    if (this->mapping.isMapped(toMoveHwQubit)) {
      spdlog::info("  logical qubit: {}",
                   this->mapping.getCircQubit(toMoveHwQubit));
    } else {
      spdlog::info("  not mapped");
    }
  }
  stats.nMoves++;
}
void NeutralAtomMapper::applyBridge(NeutralAtomLayer& frontLayer,
                                    const Bridge& bridge) {
  const auto coordIndices = this->hardwareQubits.getCoordIndices(bridge.second);
  mappedQc.bridge(coordIndices);

  if (this->parameters.verbose) {
    spdlog::info("bridged {}", bridge.first->getName());
    for (const auto qubit : bridge.second) {
      spdlog::info("{}", qubit);
    }
  }

  // // remove gate from frontLayer
  const auto* op = bridge.first;
  frontLayer.removeGatesAndUpdate({op});
  this->frontLayerGate.erase(std::ranges::find(this->frontLayerGate, op));

  stats.nBridges++;
}
void NeutralAtomMapper::applyFlyingAncilla(NeutralAtomLayer& frontLayer,
                                           const FlyingAncillaComb& faComb) {
  auto targetCoords = hardwareQubits.getCoordIndices(
      mapping.getHwQubits(faComb.op->getTargets()));
  // get control vector
  const auto opControls = faComb.op->getControls();
  HwQubitsVector controlQubits;
  for (const auto& control : opControls) {
    controlQubits.emplace_back(control.qubit);
  }
  auto controlCoords =
      hardwareQubits.getCoordIndices(mapping.getHwQubits(controlQubits));
  // merge target and control coords
  auto allCoords = targetCoords;
  allCoords.insert(allCoords.end(), controlCoords.begin(), controlCoords.end());

  uint32_t i = 0;
  const auto nPos = this->arch->getNpositions();
  for (const auto& passBy : faComb.moves) {
    const auto ancQ1 = passBy.q1 + (nPos * 2);
    const auto ancQ2 = passBy.q2 + (nPos * 2);
    mappedQc.move(passBy.origin + 2 * nPos, ancQ1);
    mappedQc.h(ancQ1);
    mappedQc.cz(allCoords[i], ancQ1);
    mappedQc.h(ancQ1);
    mappedQc.move(ancQ1, ancQ2);

    auto itT = std::ranges::find(targetCoords, allCoords[i]);
    if (itT != targetCoords.end()) {
      *itT = ancQ2;
    }
    auto itC = std::ranges::find(controlCoords, allCoords[i]);
    if (itC != controlCoords.end()) {
      *itC = ancQ2;
    }
    i += 2;

    if (this->parameters.verbose) {
      spdlog::info("passby (flying ancilla) {} {} {}", passBy.origin, passBy.q1,
                   passBy.q2);
    }
  }
  const auto opCopy = faComb.op->clone();
  opCopy->setTargets(targetCoords);
  qc::Controls controls;
  for (const auto& control : controlCoords) {
    controls.emplace(control);
  }
  opCopy->setControls(controls);
  mappedQc.emplace_back(opCopy->clone());

  i = 0;
  for (const auto& passBy : faComb.moves) {
    const auto ancQ1 = passBy.q1 + (nPos * 2);
    const auto ancQ2 = passBy.q2 + (nPos * 2);
    mappedQc.move(ancQ2, ancQ1);
    mappedQc.h(ancQ1);
    mappedQc.cz(allCoords[i], ancQ1);
    i += 2;
    mappedQc.h(ancQ1);

    // update position of flying ancillas
    if (passBy.q1 != passBy.origin) {
      this->flyingAncillas.move(static_cast<uint32_t>(passBy.index), passBy.q1);
    }

    if (this->parameters.verbose) {
      spdlog::info("passby (flying ancilla) {} {}", passBy.q2, passBy.q1);
    }
  }

  frontLayer.removeGatesAndUpdate({faComb.op});
  this->frontLayerShuttling.erase(
      std::ranges::find(this->frontLayerShuttling, faComb.op));
  stats.nFAncillas += faComb.moves.size();
}

Swap NeutralAtomMapper::findBestSwap(const Swap& lastSwapUsed) {
  // compute necessary movements
  const auto swapsFront = initSwaps(this->frontLayerGate);
  const auto swapsLookahead = initSwaps(this->lookaheadLayerGate);
  setTwoQubitSwapWeight(swapsFront.second);

  // evaluate swaps based on cost function
  auto swaps = getAllPossibleSwaps(swapsFront);
  // remove last swap to prevent immediate swap back
  swaps.erase(lastSwapUsed);
  swaps.erase({lastSwapUsed.second, lastSwapUsed.first});

  // no swap possible
  if (swaps.empty()) {
    return {};
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
  const auto bestSwap = std::ranges::min_element(
      swapCosts, [](const auto& swap1, const auto& swap2) {
        return swap1.second < swap2.second;
      });
  return bestSwap->first;
}

void NeutralAtomMapper::setTwoQubitSwapWeight(const WeightedSwaps& swapExact) {
  for (const auto& weight : swapExact | std::views::values) {
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
  for (const auto& swap : swapExactFront | std::views::keys) {
    const auto nearbySwapsFirst =
        this->hardwareQubits.getNearbySwaps(swap.first);
    for (const auto& swapFirst : nearbySwapsFirst) {
      swaps.emplace(swapFirst);
    }
  }
  return swaps;
}
Bridge NeutralAtomMapper::findBestBridge(const Swap& bestSwap) {
  auto allBridges = getShortestBridges(bestSwap);
  if (allBridges.empty()) {
    return {};
  }
  if (allBridges.size() == 1) {
    return allBridges.front();
  }
  // use bridge along less used qubits
  const auto qubitUsages = computeCurrentCoordUsages();
  size_t bestBridgeIdx = 0;
  size_t minUsage = std::numeric_limits<size_t>::max();
  for (size_t i = 0; i < allBridges.size(); ++i) {
    size_t usage = 0;
    for (const auto qubit : allBridges[i].second) {
      usage += qubitUsages[hardwareQubits.getCoordIndex(qubit)];
    }
    if (usage < minUsage) {
      minUsage = usage;
      bestBridgeIdx = i;
    }
  }
  return allBridges[bestBridgeIdx];
}

Bridges NeutralAtomMapper::getShortestBridges(const Swap& bestSwap) {
  Bridges allBridges;
  size_t minBridgeLength = std::numeric_limits<size_t>::max();
  for (const auto* const op : this->frontLayerGate) {
    if (op->getUsedQubits().size() == 2) {
      // only consider gates which involve at least one of the swapped qubits
      auto usedQuBits = op->getUsedQubits();
      auto usedHwQubits = this->mapping.getHwQubits(usedQuBits);
      if (!usedHwQubits.contains(bestSwap.first) &&
          !usedHwQubits.contains(bestSwap.second) && bestSwap != Swap()) {
        continue;
      }
      // shortcut if distance already larger than minBridgeLength
      const auto dist =
          this->hardwareQubits.getAllToAllSwapDistance(usedHwQubits);
      if (dist > this->parameters.maxBridgeDistance ||
          dist > static_cast<qc::fp>(minBridgeLength)) {
        continue;
      }
      const auto bridges = this->hardwareQubits.computeAllShortestPaths(
          *usedHwQubits.begin(), *usedHwQubits.rbegin());
      if (bridges.empty()) {
        continue;
      }
      if (bridges.front().size() < minBridgeLength) {
        minBridgeLength = bridges.front().size();
        allBridges.clear();
      }
      for (const auto& bridge : bridges) {
        if (bridge.size() == minBridgeLength) {
          allBridges.emplace_back(op, bridge);
        }
      }
    }
  }
  return allBridges;
}
CoordIndices NeutralAtomMapper::computeCurrentCoordUsages() const {
  // Size to cover all register spaces: logical + ancilla + flying ancilla
  CoordIndices coordUsages(static_cast<CoordIndex>(arch->getNpositions() * 3U),
                           0);
  // in front layer
  for (const auto* const op : this->frontLayerGate) {
    for (const auto qubit : op->getUsedQubits()) {
      coordUsages[hardwareQubits.getCoordIndex(mapping.getHwQubit(qubit))]++;
    }
  }
  // in mapped qc, go backwards same length as front layer
  auto nFrontLayerGates = this->frontLayerGate.size();
  auto it = this->mappedQc.rbegin();
  while (it != this->mappedQc.rend() && nFrontLayerGates > 0) {
    for (const auto coordIdx : (*it)->getUsedQubits()) {
      coordUsages[coordIdx]++;
    }
    ++it;
    nFrontLayerGates--;
  }
  // add last blocked qubits
  if (this->lastBlockedQubits.empty()) {
    return coordUsages;
  }
  const auto lastBlockedQubitSet = this->lastBlockedQubits.back();
  for (const auto qubit : lastBlockedQubitSet) {
    coordUsages[hardwareQubits.getCoordIndex(qubit)]++;
  }
  return coordUsages;
}
FlyingAncillaComb NeutralAtomMapper::convertMoveCombToFlyingAncillaComb(
    const MoveComb& moveComb) const {
  if (this->flyingAncillas.getNumQubits() == 0) {
    return {};
  }
  const auto usedQubits = moveComb.op->getUsedQubits();
  const auto hwQubits = this->mapping.getHwQubits(usedQubits);
  const auto usedCoords = this->hardwareQubits.getCoordIndices(hwQubits);

  // multi-qubit gate -> only one direction
  std::vector<FlyingAncilla> bestFAs;
  FlyingAncilla bestFA{};
  HwQubits usedFA;
  for (const auto move : moveComb.moves) {
    if (usedCoords.contains(move.c1)) {
      const auto nearFirstIdx =
          this->flyingAncillas.getClosestQubit(move.c1, usedFA);
      const auto nearFirst = this->flyingAncillas.getCoordIndex(nearFirstIdx);
      const auto nearSecondIdx =
          this->flyingAncillas.getClosestQubit(move.c2, usedFA);
      const auto nearSecond = this->flyingAncillas.getCoordIndex(nearSecondIdx);
      if (usedQubits.size() == 2) {
        // both directions possible, check if reversed is better
        if (this->arch->getEuclideanDistance(nearFirst, move.c1) <
            this->arch->getEuclideanDistance(nearSecond, move.c2)) {
          bestFA.q1 = move.c2;
          bestFA.q2 = move.c1;
          bestFA.origin = nearSecond;
          bestFA.index = nearSecondIdx;

          usedFA.emplace(bestFA.index);
          bestFAs.emplace_back(bestFA);
          continue;
        }
      }
      bestFA.q1 = move.c1;
      bestFA.q2 = move.c2;
      bestFA.origin = nearFirst;
      bestFA.index = nearFirstIdx;

      usedFA.emplace(bestFA.index);
      bestFAs.emplace_back(bestFA);
    }
  }
  return {.moves = bestFAs, .op = moveComb.op};
}

PassByComb
NeutralAtomMapper::convertMoveCombToPassByComb(const MoveComb& moveComb) const {
  if (!this->parameters.usePassBy) {
    return {};
  }
  const auto usedQubits = moveComb.op->getUsedQubits();
  const auto hwQubits = this->mapping.getHwQubits(usedQubits);
  const auto usedCoords = this->hardwareQubits.getCoordIndices(hwQubits);

  std::vector<AtomMove> bestPbs;
  for (const auto move : moveComb.moves) {
    if (usedCoords.contains(move.c1)) {
      bestPbs.emplace_back(AtomMove{
          .c1 = move.c1, .c2 = move.c2, .load1 = true, .load2 = false});
    }
  }
  return PassByComb{.moves = bestPbs, .op = moveComb.op};
}

qc::fp NeutralAtomMapper::swapCost(
    const Swap& swap, const std::pair<Swaps, WeightedSwaps>& swapsFront,
    const std::pair<Swaps, WeightedSwaps>& swapsLookahead) {
  auto [swapCloseByFront, swapExactFront] = swapsFront;
  auto [swapCloseByLookahead, swapExactLookahead] = swapsLookahead;
  // compute the change in total distance
  const auto distanceChangeFront =
      swapCostPerLayer(swap, swapCloseByFront, swapExactFront) /
      static_cast<qc::fp>(this->frontLayerGate.size());
  qc::fp distanceChangeLookahead = 0;
  if (!this->lookaheadLayerGate.empty()) {
    distanceChangeLookahead =
        swapCostPerLayer(swap, swapCloseByLookahead, swapExactLookahead) /
        static_cast<qc::fp>(this->lookaheadLayerGate.size());
  }
  auto cost = (parameters.lookaheadWeightSwaps * distanceChangeLookahead /
               this->parameters.lookaheadDepth) +
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
qc::fp NeutralAtomMapper::swapDistanceReduction(const Swap& swap,
                                                const GateList& layer) {
  qc::fp swapDistReduction = 0;
  for (const auto& op : layer) {
    auto usedQubits = op->getUsedQubits();
    auto hwQubits = this->mapping.getHwQubits(usedQubits);
    const auto& distBefore =
        this->hardwareQubits.getAllToAllSwapDistance(hwQubits);
    const auto firstPos = hwQubits.find(swap.first);
    const auto secondPos = hwQubits.find(swap.second);
    if (firstPos != hwQubits.end() && secondPos != hwQubits.end()) {
      continue;
    }
    if (firstPos != hwQubits.end()) {
      hwQubits.erase(firstPos);
      hwQubits.insert(swap.second);
    }
    if (secondPos != hwQubits.end()) {
      hwQubits.erase(secondPos);
      hwQubits.insert(swap.first);
    }
    const auto& distAfter =
        this->hardwareQubits.getAllToAllSwapDistance(hwQubits);
    swapDistReduction += distBefore - distAfter;
  }
  return swapDistReduction;
}

qc::fp
NeutralAtomMapper::moveCombDistanceReduction(const MoveComb& moveComb,
                                             const GateList& layer) const {
  qc::fp moveDistReduction = 0;
  for (const auto& op : layer) {
    auto usedQubits = op->getUsedQubits();
    auto hwQubits = this->mapping.getHwQubits(usedQubits);
    auto coordIndices = this->hardwareQubits.getCoordIndices(hwQubits);
    for (const auto& move : moveComb.moves) {
      if (coordIndices.contains(move.c1)) {
        const auto& distBefore =
            this->arch->getAllToAllEuclideanDistance(coordIndices);
        coordIndices.erase(move.c1);
        coordIndices.insert(move.c2);
        const auto& distAfter =
            this->arch->getAllToAllEuclideanDistance(coordIndices);
        moveDistReduction += distBefore - distAfter;
      }
    }
  }
  return moveDistReduction;
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
        spdlog::info("bestPos:");
        for (const auto qubit : bestPos) {
          spdlog::info("{}", qubit);
        }
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
    const auto origin = exactSwap.first;
    const auto destination = exactSwap.second;
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

HwQubits
NeutralAtomMapper::getBestMultiQubitPosition(const qc::Operation* opPointer) {
  // try to find position around gate Qubits recursively
  // if not, search through coupling graph until found according to a
  // priority queue based on the distance to the other qubits

  std::priority_queue<std::pair<qc::fp, HwQubit>,
                      std::vector<std::pair<qc::fp, HwQubit>>, std::greater<>>
      qubitQueue;
  // add the gate qubits to the priority queue
  const auto gateQubits = opPointer->getUsedQubits();
  const auto gateHwQubits = this->mapping.getHwQubits(gateQubits);
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
  const auto idxFrontGate = std::ranges::find(this->frontLayerGate, opPointer);
  if (idxFrontGate != this->frontLayerGate.end()) {
    this->frontLayerGate.erase(idxFrontGate);
    this->frontLayerShuttling.emplace_back(opPointer);
  }
  // remove from lookahead layer if there
  const auto idxLookaheadGate =
      std::ranges::find(this->lookaheadLayerGate, opPointer);
  if (idxLookaheadGate != this->lookaheadLayerGate.end()) {
    this->lookaheadLayerGate.erase(idxLookaheadGate);
    this->lookaheadLayerShuttling.emplace_back(opPointer);
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
  const auto newQubit = *selectedQubits.rbegin();
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
  const auto nextQubitDist = std::ranges::min_element(
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
    const auto distance =
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
  const auto gateQubits = op->getUsedQubits();
  auto gateHwQubits = this->mapping.getHwQubits(gateQubits);
  WeightedSwaps swapsExact;
  while (!position.empty() && !gateHwQubits.empty()) {
    std::vector<std::tuple<HwQubit, std::set<HwQubit>, SwapDistance>>
        minimalDistances;
    std::set<HwQubit> minimalDistancePosQubit;
    for (const auto& gateQubit : gateHwQubits) {
      SwapDistance minimalDistance = std::numeric_limits<SwapDistance>::max();
      for (const auto& posQubit : position) {
        const auto distance =
            this->hardwareQubits.getSwapDistance(gateQubit, posQubit, false);
        if (distance < minimalDistance) {
          minimalDistance = distance;
          minimalDistancePosQubit.clear();
          minimalDistancePosQubit.emplace(posQubit);
        } else if (distance == minimalDistance) {
          minimalDistancePosQubit.emplace(posQubit);
        }
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
  for (const auto& swap : swapsExact | std::views::keys) {
    auto [q1, q2] = swap;
    totalDistance += this->hardwareQubits.getSwapDistance(q1, q2, false);
  }
  // add cost to the moves -> move first qubit corresponding to almost finished
  // positions
  const auto nQubits = op->getUsedQubits().size();
  const auto multiQubitFactor =
      (static_cast<qc::fp>(nQubits) * static_cast<qc::fp>(nQubits - 1)) / 2;
  for (auto& val : swapsExact | std::views::values) {
    val = multiQubitFactor / static_cast<qc::fp>(totalDistance);
  }

  return swapsExact;
}

MoveComb NeutralAtomMapper::findBestAtomMove() {
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
  const auto bestMove = std::ranges::min_element(
      moveCosts, [](const auto& move1, const auto& move2) {
        return move1.second < move2.second;
      });
  return bestMove->first;
}

qc::fp NeutralAtomMapper::moveCostComb(const MoveComb& moveComb) const {
  qc::fp costComb = 0;
  const auto frontDistReduction =
      moveCombDistanceReduction(moveComb, this->frontLayerShuttling) /
      static_cast<qc::fp>(this->frontLayerShuttling.size());
  costComb -= frontDistReduction;
  if (!lookaheadLayerShuttling.empty()) {
    const auto lookaheadDistReduction =
        moveCombDistanceReduction(moveComb, this->lookaheadLayerShuttling) /
        static_cast<qc::fp>(this->lookaheadLayerShuttling.size());
    costComb -= parameters.lookaheadWeightMoves * lookaheadDistReduction /
                static_cast<qc::fp>(this->parameters.lookaheadDepth);
  }
  if (!this->lastMoves.empty()) {
    const auto parallelMovecCost =
        parameters.shuttlingTimeWeight * parallelMoveCost(moveComb) /
        static_cast<qc::fp>(this->frontLayerShuttling.size());
    costComb += parallelMovecCost;
  }
  return costComb;
}

qc::fp NeutralAtomMapper::parallelMoveCost(const MoveComb& moveComb) const {
  qc::fp parallelCost = 0;
  // only first move matters for parallelization
  const auto move = moveComb.moves.front();
  const auto moveVector = this->arch->getVector(move.c1, move.c2);
  parallelCost += arch->getVectorShuttlingTime(moveVector);
  bool canBeDoneInParallel = true;
  for (const auto& lastMove : this->lastMoves) {
    // decide of shuttling can be done in parallel
    auto lastMoveVector = this->arch->getVector(lastMove.c1, lastMove.c2);
    if (moveVector.overlap(lastMoveVector)) {
      if (moveVector.direction != lastMoveVector.direction) {
        canBeDoneInParallel = false;
        break;
      } // check if move can be done in parallel
      if (moveVector.include(lastMoveVector)) {
        canBeDoneInParallel = false;
        break;
      }
    }
  }
  if (canBeDoneInParallel) {
    parallelCost -= arch->getVectorShuttlingTime(moveVector);
  }
  // check if in same row/column like last moves
  // then can may be loaded in parallel
  const auto moveCoordInit = this->arch->getCoordinate(move.c1);
  const auto moveCoordEnd = this->arch->getCoordinate(move.c2);
  parallelCost += arch->getShuttlingTime(qc::OpType::AodActivate) +
                  arch->getShuttlingTime(qc::OpType::AodDeactivate);
  for (const auto& lastMove : this->lastMoves) {
    const auto lastMoveCoordInit = this->arch->getCoordinate(lastMove.c1);
    const auto lastMoveCoordEnd = this->arch->getCoordinate(lastMove.c2);
    if (moveCoordInit.x == lastMoveCoordInit.x ||
        moveCoordInit.y == lastMoveCoordInit.y) {
      parallelCost -= arch->getShuttlingTime(qc::OpType::AodActivate);
    }
    if (moveCoordEnd.x == lastMoveCoordEnd.x ||
        moveCoordEnd.y == lastMoveCoordEnd.y) {
      parallelCost -= arch->getShuttlingTime(qc::OpType::AodDeactivate);
    }
  }
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

  const auto nearbyCoords =
      this->arch->getNearbyCoordinates(currentPos.coords.back());
  // filter out coords that have a SWAP distance unequal to 0 to any of the
  // current qubits. Also sort out coords that are already in the vector
  std::vector<CoordIndex> filteredNearbyCoords;
  for (const auto& coord : nearbyCoords) {
    bool valid = true;
    for (const auto& qubit : currentPos.coords) {
      if (this->arch->getSwapDistance(qubit, coord) != 0 || coord == qubit) {
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
    auto nextPos = MultiQubitMovePos(currentPos);
    nextPos.coords.emplace_back(gateCoord);
    if (auto bestPos = getMovePositionRec(nextPos, gateCoords, maxNMoves);
        bestPos.coords.size() == gateCoords.size()) {
      return bestPos;
    }
  }

  for (const auto& freeCoord : freeNearbyCoords) {
    auto nextPos = MultiQubitMovePos(currentPos);
    nextPos.coords.emplace_back(freeCoord);
    nextPos.nMoves += 1;
    if (auto bestPos = getMovePositionRec(nextPos, gateCoords, maxNMoves);
        bestPos.coords.size() == gateCoords.size()) {
      return bestPos;
    }
  }

  for (const auto& occCoord : occupiedNearbyCoords) {
    auto nextPos = MultiQubitMovePos(currentPos);
    nextPos.coords.emplace_back(occCoord);
    nextPos.nMoves += 2;
    if (auto bestPos = getMovePositionRec(nextPos, gateCoords, maxNMoves);
        bestPos.coords.size() == gateCoords.size()) {
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
    auto usedCoords = std::vector(usedCoordsSet.begin(), usedCoordsSet.end());
    std::set<CoordIndices> bestPositions;
    // iterate over all possible permutations of the usedCoords
    // to find different best positions
    std::ranges::sort(usedCoords);
    do {
      bestPositions.insert(getBestMovePos(usedCoords));
    } while (std::ranges::next_permutation(usedCoords).found);
    for (const auto& bestPos : bestPositions) {
      auto moves = getMoveCombinationsToPosition(usedHwQubits, bestPos);
      moves.setOperation(op, bestPos);
      if (allMoves.size() > this->parameters.limitShuttlingLayer) {
        break;
      }
      allMoves.addMoveCombs(moves);
    }
  }
  allMoves.removeLongerMoveCombs();
  return allMoves;
}

CoordIndices NeutralAtomMapper::getBestMovePos(const CoordIndices& gateCoords) {
  size_t const maxMoves = gateCoords.size() * 2;
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
    if (!bestPos.coords.empty() && bestPos.nMoves < nMovesGate) {
      nMovesGate = bestPos.nMoves;
      finalBestPos = bestPos;
    }

    // min not yet reached, check nearby
    if (!bestPos.coords.empty()) {
      nMovesGate = std::min(nMovesGate, bestPos.nMoves);
    }
  }
  if (finalBestPos.coords.empty()) {
    throw std::runtime_error(
        "No move position found (check if enough free coords are available)");
  }
  return finalBestPos.coords;
}

MoveCombs NeutralAtomMapper::getMoveCombinationsToPosition(
    const HwQubits& gateQubits, const CoordIndices& position) const {
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

  // save coords where atoms have been moved away to
  CoordIndices movedAwayCoords = remainingCoords;
  while (!remainingGateCoords.empty()) {
    auto currentGateQubit = *remainingGateCoords.begin();
    // compute costs and find best coord
    std::vector<std::pair<CoordIndex, qc::fp>> costs;
    for (const auto& remainingCoord : remainingCoords) {
      if (this->hardwareQubits.isMapped(remainingCoord)) {
        const auto moveAwayComb = getMoveAwayCombinations(
            currentGateQubit, remainingCoord, movedAwayCoords);
        for (const auto& moveAway : moveAwayComb) {
          auto cost = moveCostComb(moveAway);
          costs.emplace_back(remainingCoord, cost);
        }
      } else {
        MoveComb const moveCombNew(
            {AtomMove{.c1 = currentGateQubit, .c2 = remainingCoord}});
        auto cost = moveCostComb(moveCombNew);
        costs.emplace_back(remainingCoord, cost);
      }
    }
    // find minimal cost
    const auto bestCost = std::ranges::min_element(
        costs, [](const auto& cost1, const auto& cost2) {
          return cost1.second < cost2.second;
        });
    auto targetCoord = bestCost->first;
    if (this->hardwareQubits.isMapped(targetCoord)) {
      auto moveAwayComb = getMoveAwayCombinations(currentGateQubit, targetCoord,
                                                  movedAwayCoords);
      moveComb.append(moveAwayComb.moveCombs[0]);
      movedAwayCoords.emplace_back(moveAwayComb.moveCombs[0].moves[0].c2);
    } else {
      moveComb.append(AtomMove{.c1 = currentGateQubit,
                               .c2 = targetCoord,
                               .load1 = true,
                               .load2 = true});
    }
    remainingGateCoords.erase(currentGateQubit);
    remainingCoords.erase(std::ranges::find(remainingCoords, targetCoord));
  }
  return MoveCombs({moveComb});
}

MoveCombs NeutralAtomMapper::getMoveAwayCombinations(
    const CoordIndex startCoord, const CoordIndex targetCoord,
    const CoordIndices& excludedCoords) const {
  MoveCombs moveCombinations;
  auto const originalVector = this->arch->getVector(startCoord, targetCoord);
  auto const originalDirection = originalVector.direction;
  // Find move away target in the same direction as the original move
  const auto moveAwayTargets = this->hardwareQubits.findClosestFreeCoord(
      targetCoord, originalDirection, excludedCoords);
  for (const auto& moveAwayTarget : moveAwayTargets) {
    const AtomMove move = {
        .c1 = startCoord, .c2 = targetCoord, .load1 = true, .load2 = true};
    const AtomMove moveAway = {
        .c1 = targetCoord, .c2 = moveAwayTarget, .load1 = true, .load2 = true};
    moveCombinations.addMoveComb(MoveComb({moveAway, move}));
  }
  if (moveCombinations.empty()) {
    throw std::runtime_error("No move away target found");
  }
  return moveCombinations;
}

size_t NeutralAtomMapper::shuttlingBasedMapping(
    NeutralAtomLayer& frontLayer, NeutralAtomLayer& lookaheadLayer, size_t i) {
  while (!this->frontLayerShuttling.empty()) {
    ++i;
    if (this->parameters.verbose) {
      spdlog::info("iteration {}", i);
    }
    auto bestComb = findBestAtomMove();
    MappingMethod bestMethod = MoveMethod;
    if (!multiQubitGates) {
      auto bestFaComb = convertMoveCombToFlyingAncillaComb(bestComb);
      auto bestPbComb = convertMoveCombToPassByComb(bestComb);
      bestMethod =
          compareShuttlingAndFlyingAncilla(bestComb, bestFaComb, bestPbComb);

      switch (bestMethod) {
      case MoveMethod:
        // apply whole move combination at once
        for (const auto& move : bestComb.moves) {
          applyMove(move);
        }
        // applyMove(bestComb.moves[0]);
        break;
      case FlyingAncillaMethod:
        applyFlyingAncilla(frontLayer, bestFaComb);
        break;
      case PassByMethod:
        applyPassBy(frontLayer, bestPbComb);
        break;
      default:
        break;
      }
    } else {
      for (const auto& move : bestComb.moves) {
        applyMove(move);
      }
    }

    mapAllPossibleGates(frontLayer, lookaheadLayer);
    reassignGatesToLayers(frontLayer.getGates(), lookaheadLayer.getGates());
    if (this->parameters.verbose) {
      printLayers();
    }
  }
  return i;
}

std::pair<uint32_t, qc::fp>
NeutralAtomMapper::estimateNumSwapGates(const qc::Operation* opPointer) {
  const auto usedQubits = opPointer->getUsedQubits();
  const auto usedHwQubits = this->mapping.getHwQubits(usedQubits);
  qc::fp minNumSwaps = 0;
  if (usedHwQubits.size() == 2) {
    minNumSwaps = this->hardwareQubits.getSwapDistance(
        *usedHwQubits.begin(), *(usedHwQubits.rbegin()), true);
  } else { // multi-qubit gates
    const auto bestPos = getBestMultiQubitPosition(opPointer);
    if (bestPos.empty()) {
      return {std::numeric_limits<SwapDistance>::max(),
              std::numeric_limits<qc::fp>::max()};
    }
    auto exactSwaps = getExactSwapsToPosition(opPointer, bestPos);
    if (exactSwaps.empty()) {
      return {std::numeric_limits<SwapDistance>::max(),
              std::numeric_limits<qc::fp>::max()};
    }
    for (const auto& swap : exactSwaps | std::views::keys) {
      auto [q1, q2] = swap;
      minNumSwaps += this->hardwareQubits.getSwapDistance(q1, q2, false);
    }
  }
  const qc::fp minTime = minNumSwaps * this->arch->getGateTime("swap");
  return {minNumSwaps, minTime};
}

std::pair<uint32_t, qc::fp>
NeutralAtomMapper::estimateNumMove(const qc::Operation* opPointer) const {
  const auto usedQubits = opPointer->getUsedQubits();
  const auto usedHwQubits = this->mapping.getHwQubits(usedQubits);
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
      const auto otherCoord = *otherQubitsIt;
      if (otherCoord == coord) {
        ++otherQubitsIt;
        continue;
      }
      if (nearbyFreeIt != nearbyFreeCoords.end()) {
        totalTime += this->arch->getVectorShuttlingTime(
            this->arch->getVector(otherCoord, *nearbyFreeIt));
        totalTime += this->arch->getShuttlingTime(qc::OpType::AodActivate) +
                     this->arch->getShuttlingTime(qc::OpType::AodDeactivate);
        ++nearbyFreeIt;
        totalMoves++;
      } else if (nearbyOccIt != nearbyOccupiedCoords.end()) {
        totalTime += 2 * this->arch->getVectorShuttlingTime(
                             this->arch->getVector(otherCoord, *nearbyOccIt));
        totalTime +=
            2 * (this->arch->getShuttlingTime(qc::OpType::AodActivate) +
                 this->arch->getShuttlingTime(qc::OpType::AodDeactivate));
        ++nearbyOccIt;
        totalMoves += 2;
      } else {
        throw std::runtime_error("No space to "
                                 "execute a multi-qubit gate. "
                                 "Check int radius. Op:" +
                                 opPointer->getName() + " nQubit: " +
                                 std::to_string(usedQubits.size()));
      }

      ++otherQubitsIt;
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
  const auto fidSwaps =
      std::exp(-minTimeSwaps * this->arch->getNqubits() /
               this->arch->getDecoherenceTime()) *
      std::pow(this->arch->getGateAverageFidelity("swap"), minNumSwaps);
  const auto fidMoves =
      std::exp(-minTimeMoves * this->arch->getNqubits() /
               this->arch->getDecoherenceTime()) *
      std::pow(
          this->arch->getShuttlingAverageFidelity(qc::OpType::AodMove) *
              this->arch->getShuttlingAverageFidelity(qc::OpType::AodActivate) *
              this->arch->getShuttlingAverageFidelity(
                  qc::OpType::AodDeactivate),
          minMoves);

  return fidSwaps * parameters.gateWeight >
         fidMoves * parameters.shuttlingWeight;
}

size_t NeutralAtomMapper::gateBasedMapping(NeutralAtomLayer& frontLayer,
                                           NeutralAtomLayer& lookaheadLayer,
                                           size_t i) {
  // first do all gate based mapping gates
  while (!this->frontLayerGate.empty()) {
    GateList gatesToExecute;
    while (gatesToExecute.empty() && !this->frontLayerGate.empty()) {
      ++i;
      if (this->parameters.verbose) {
        spdlog::info("iteration {}", i);
      }

      auto bestSwap = findBestSwap(lastSwap);
      MappingMethod bestMethod = SwapMethod;
      if (parameters.maxBridgeDistance > 0 && !multiQubitGates) {
        auto bestBridge = findBestBridge(bestSwap);
        bestMethod = compareSwapAndBridge(bestSwap, bestBridge);
        if (bestMethod == BridgeMethod) {
          updateBlockedQubits(
              HwQubits(bestBridge.second.begin(), bestBridge.second.end()));
          applyBridge(frontLayer, bestBridge);
        }
      }
      if (bestMethod == SwapMethod) {
        if (bestSwap == Swap() || bestSwap.first == bestSwap.second) {
          throw std::runtime_error(
              "No possible SWAP found to execute gates in front layer.");
        }
        lastSwap = bestSwap;
        updateBlockedQubits(HwQubits{bestSwap.first, bestSwap.second});
        applySwap(bestSwap);
      }

      gatesToExecute = getExecutableGates(frontLayer.getGates());
    }
    mapAllPossibleGates(frontLayer, lookaheadLayer);
    reassignGatesToLayers(frontLayer.getGates(), lookaheadLayer.getGates());
    if (this->parameters.verbose) {
      printLayers();
    }
  }
  return i;
}

MappingMethod
NeutralAtomMapper::compareSwapAndBridge(const Swap& bestSwap,
                                        const Bridge& bestBridge) {
  if (bestBridge == Bridge()) {
    return SwapMethod;
  }
  if (this->parameters.dynamicMappingWeight == 0) {
    return BridgeMethod;
  }
  const auto swapFrontDistReduction =
      swapDistanceReduction(bestSwap, this->frontLayerGate);
  const auto swapLookaheadDistReduction =
      swapDistanceReduction(bestSwap, this->lookaheadLayerGate);
  const auto swapDistReduction =
      swapFrontDistReduction +
      (this->parameters.lookaheadWeightSwaps * swapLookaheadDistReduction /
       this->parameters.lookaheadDepth);

  // bridge distance reduction
  qc::fp const bridgeDistReduction =
      static_cast<qc::fp>(bestBridge.second.size()) - 2;

  // fidelity comparison
  qc::fp const swapTime =
      this->arch->getGateTime("swap") + this->arch->getGateTime("cz");
  qc::fp const swapFidelity =
      this->arch->getGateAverageFidelity("swap") *
      this->arch->getGateAverageFidelity("cz") *
      std::exp(-swapTime / this->arch->getDecoherenceTime());
  const std::string bridgeName =
      "bridge" + std::to_string(bestBridge.second.size());
  qc::fp const bridgeFidelity = this->arch->getGateAverageFidelity(bridgeName) *
                                std::exp(-this->arch->getGateTime(bridgeName) /
                                         this->arch->getDecoherenceTime());
  const auto swap = std::log(swapFidelity) / swapDistReduction /
                    parameters.dynamicMappingWeight;
  const auto bridge = std::log(bridgeFidelity) / bridgeDistReduction;
  if (swap >= bridge) {
    return SwapMethod;
  }
  return BridgeMethod;
}

MappingMethod NeutralAtomMapper::compareShuttlingAndFlyingAncilla(
    const MoveComb& bestMoveComb, const FlyingAncillaComb& bestFaComb,
    const PassByComb& bestPbComb) const {
  if (flyingAncillas.getNumQubits() == 0 && !parameters.usePassBy) {
    return MoveMethod;
  }
  if (multiQubitGates) {
    return MoveMethod;
  }

  // move distance reduction
  const auto moveDistReductionFront =
      moveCombDistanceReduction(bestMoveComb, this->frontLayerShuttling);

  auto moveDistReductionLookAhead = 0.0;
  if (!this->lookaheadLayerShuttling.empty()) {
    moveDistReductionLookAhead =
        this->parameters.lookaheadWeightMoves *
        moveCombDistanceReduction(bestMoveComb, this->lookaheadLayerShuttling) /
        static_cast<double>(this->lookaheadLayerShuttling.size());
  }
  auto moveDistReduction = moveDistReductionFront + moveDistReductionLookAhead;
  // move
  auto const moveDist = this->arch->getMoveCombEuclideanDistance(bestMoveComb);
  auto const moveCombSize = bestMoveComb.size();
  auto const moveOpFidelity = std::pow(
      this->arch->getShuttlingAverageFidelity(qc::OpType::AodMove) *
          this->arch->getShuttlingAverageFidelity(qc::OpType::AodActivate) *
          this->arch->getShuttlingAverageFidelity(qc::OpType::AodDeactivate),
      moveCombSize);
  auto const moveTime =
      (moveDist / this->arch->getShuttlingTime(qc::OpType::AodMove)) +
      (this->arch->getShuttlingTime(qc::OpType::AodActivate) *
       static_cast<qc::fp>(moveCombSize)) +
      (this->arch->getShuttlingTime(qc::OpType::AodDeactivate) *
       static_cast<qc::fp>(moveCombSize));
  auto const moveDecoherence = std::exp(-moveTime * this->arch->getNqubits() /
                                        this->arch->getDecoherenceTime());
  auto const moveFidelity = moveOpFidelity * moveDecoherence;

  // fa
  auto faDistReduction = 0.0;
  auto faFidelity = 0.0;
  if (flyingAncillas.getNumQubits() != 0) {
    // flying ancilla distance reduction
    auto const faCoords = this->hardwareQubits.getCoordIndices(
        this->mapping.getHwQubits(bestFaComb.op->getUsedQubits()));
    faDistReduction = this->arch->getAllToAllEuclideanDistance(faCoords);

    // flying ancilla
    auto const faDist = this->arch->getFaEuclideanDistance(bestFaComb);
    auto const faCombSize = bestFaComb.moves.size();
    auto const faOpFidelity = std::pow(
        std::pow(this->arch->getShuttlingAverageFidelity(qc::OpType::AodMove),
                 3) *
            std::pow(this->arch->getGateAverageFidelity("cz"), 2) *
            std::pow(this->arch->getGateAverageFidelity("h"), 4),
        faCombSize);
    auto const faDecoherence = std::exp(
        -faDist / this->arch->getShuttlingTime(qc::OpType::AodMove) *
        this->arch->getNqubits() * 2 / this->arch->getDecoherenceTime());
    faFidelity = faOpFidelity * faDecoherence;
  }

  // passby
  auto pbDistReduction = 0.0;
  auto passByFidelity = 0.0;
  if (parameters.usePassBy) {
    auto const pbCoords = this->hardwareQubits.getCoordIndices(
        this->mapping.getHwQubits(bestPbComb.op->getUsedQubits()));
    pbDistReduction = this->arch->getAllToAllEuclideanDistance(pbCoords);
    const auto pbCombSize = bestPbComb.moves.size();

    auto const passByDist = this->arch->getPassByEuclideanDistance(bestPbComb);
    auto const passByTime =
        (passByDist / this->arch->getShuttlingTime(qc::OpType::AodMove)) * 2 +
        (this->arch->getShuttlingTime(qc::OpType::AodActivate) *
         static_cast<qc::fp>(pbCombSize)) +
        (this->arch->getShuttlingTime(qc::OpType::AodDeactivate) *
         static_cast<qc::fp>(pbCombSize));
    passByFidelity = std::pow(std::pow(this->arch->getShuttlingAverageFidelity(
                                           qc::OpType::AodMove),
                                       2) *
                                  this->arch->getShuttlingAverageFidelity(
                                      qc::OpType::AodActivate) *
                                  this->arch->getShuttlingAverageFidelity(
                                      qc::OpType::AodDeactivate),
                              pbCombSize) *
                     std::exp(-passByTime * this->arch->getNqubits() /
                              this->arch->getDecoherenceTime());
  }

  auto const minDistanceReduction =
      std::min({moveDistReduction, faDistReduction, pbDistReduction});
  constexpr qc::fp constant = 1;
  if (minDistanceReduction < 0) {
    moveDistReduction -= minDistanceReduction - constant;
    faDistReduction -= minDistanceReduction - constant;
    pbDistReduction -= minDistanceReduction - constant;
  } else {
    moveDistReduction += constant;
    faDistReduction += constant;
    pbDistReduction += constant;
  }

  // higher is better
  const auto move = std::log(moveFidelity) / moveDistReduction /
                    parameters.dynamicMappingWeight;

  const auto fa = std::log(faFidelity) / faDistReduction;
  const auto passBy = std::log(passByFidelity) / pbDistReduction;

  if (move > fa && move > passBy) {
    return MoveMethod;
  }
  if (fa > move && fa > passBy) {
    return FlyingAncillaMethod;
  }
  return PassByMethod;
}
} // namespace na
