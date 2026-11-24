/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "sc/Mapper.hpp"

#include "datastructures/CircuitOptimizations.hpp"
#include "ir/Definitions.hpp"
#include "ir/Permutation.hpp"
#include "ir/operations/CompoundOperation.hpp"
#include "ir/operations/Control.hpp"
#include "ir/operations/Expression.hpp"
#include "ir/operations/OpType.hpp"
#include "ir/operations/Operation.hpp"
#include "ir/operations/SymbolicOperation.hpp"
#include "sc/Architecture.hpp"
#include "sc/configuration/Layering.hpp"
#include "sc/utils.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <ranges>
#include <set>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {
/**
 * @brief Append measurements matching the circuit's output permutation.
 */
void appendMeasurementsAccordingToOutputPermutation(
    qc::QuantumComputation& circuit) {
  constexpr auto registerName = "c";
  const auto numOutputs = circuit.outputPermutation.size();
  circuit.addClassicalRegister(numOutputs, registerName);

  circuit.barrier();
  for (const auto& [qubit, clbit] : circuit.outputPermutation) {
    circuit.measure(qubit, clbit);
  }
}

void negateXXPlusYYPhase(qc::Operation& op) {
  if (auto* symbolic = dynamic_cast<qc::SymbolicOperation*>(&op)) {
    const auto beta = symbolic->getParameter(1);
    if (const auto* expr = std::get_if<qc::Symbolic>(&beta)) {
      symbolic->setSymbolicParameter(-*expr, 1);
      return;
    }
  }
  op.getParameter().at(1) *= -1.;
}
} // namespace

void Mapper::initResults() {
  if (qc.getNqubits() > architecture->getNqubits()) {
    throw QMAPException(
        "The circuit has more active qubits than the architecture.");
  }
  assumeIdealSingleQubits = architecture->hasCompleteCnotCalibration();
  countGates(qc, results.input, false);
  results.input.name = qc.getName();
  results.input.qubits = static_cast<std::uint16_t>(qc.getNqubits());
  results.architecture = architecture->getName();
  results.output.name = qc.getName() + "_mapped";
  results.output.qubits = architecture->getNqubits();
  results.output.gates = std::numeric_limits<std::size_t>::max();
  qcMapped.setName(qc.getName());
  for (const auto& variable : qc.getVariables()) {
    qcMapped.addVariable(qc::Symbolic{variable});
  }
  qcMapped.addQubitRegister(architecture->getNqubits());
}

Mapper::Mapper(qc::QuantumComputation quantumComputation, Architecture& arch)
    : qc(std::move(quantumComputation)), architecture(&arch) {
  // Compact idle wires only when the input exceeds the device width.
  if (qc.getNqubits() > arch.getNqubits()) {
    qc.stripIdleQubits(true);
    // stripIdleQubits keeps the physical indices on operations and registers.
    // Use the compact logical numbering for the remaining wires.
    for (const auto& op : qc) {
      op->apply(qc.initialLayout);
    }
    qc::Permutation outputs;
    for (const auto& [physical, output] : qc.outputPermutation) {
      outputs.emplace(qc.initialLayout.at(physical), output);
    }
    qc.outputPermutation.swap(outputs);
    qc.initialLayout.clear();
    for (qc::Qubit q = 0; q < qc.getNqubits(); ++q) {
      qc.initialLayout.emplace(q, q);
    }
    qc.unifyQuantumRegisters();
  }
  qc.removeFinalMeasurements();
  qubits.resize(architecture->getNqubits(), DEFAULT_POSITION);
  locations.resize(architecture->getNqubits(), DEFAULT_POSITION);
}

Mapper::Reversal Mapper::reversalFor(const qc::Operation& op) {
  if (op.isControlled()) {
    if (op.getType() == qc::I) {
      return Reversal::Exchange;
    }
    if (op.getControls().begin()->type == qc::Control::Type::Neg) {
      return Reversal::None;
    }
    switch (op.getType()) {
    case qc::X:
      return Reversal::CNOT;
    case qc::Z:
    case qc::S:
    case qc::Sdg:
    case qc::T:
    case qc::Tdg:
    case qc::P:
      return Reversal::Exchange;
    default:
      return Reversal::None;
    }
  }
  switch (op.getType()) {
  case qc::SWAP:
  case qc::iSWAP:
  case qc::iSWAPdg:
  case qc::RXX:
  case qc::RYY:
  case qc::RZZ:
  case qc::XXminusYY:
    return Reversal::Exchange;
  case qc::XXplusYY:
    return Reversal::NegateBeta;
  default:
    return Reversal::None;
  }
}

Architecture::GateFidelity Mapper::gateFidelity(const Gate& gate,
                                                const Edge& edge) const {
  const auto& op = *gate.op;
  if (op.getType() == qc::SWAP) {
    return architecture->swapFidelity(edge, assumeIdealSingleQubits);
  }
  if (op.isControlled() &&
      op.getControls().begin()->type == qc::Control::Type::Neg) {
    return {.cost = std::numeric_limits<double>::infinity(), .reversed = false};
  }
  const auto direct = architecture->nativeTwoQubitCost(op.getType(), edge);
  if (gate.reversal == Reversal::None) {
    return {.cost = direct, .reversed = false};
  }
  const auto [first, second] = edge;
  auto reverse =
      architecture->nativeTwoQubitCost(op.getType(), {second, first});
  if (gate.reversal == Reversal::CNOT) {
    reverse += architecture->reversalCost(edge, assumeIdealSingleQubits);
  }
  return {.cost = std::min(direct, reverse), .reversed = reverse < direct};
}

void Mapper::appendMappedGate(const Gate& gate, std::uint16_t first) {
  auto op = gate.op->clone();
  if (gate.singleQubit()) {
    op->setTargets({first});
    qcMapped.emplace_back(std::move(op));
    return;
  }

  auto second = static_cast<std::uint16_t>(locations.at(*gate.second));
  const bool reversed = fidelityAwareHeur
                            ? gateFidelity(gate, {first, second}).reversed
                            : !architecture->isEdgeConnected({first, second});
  if (reversed) {
    if (gate.reversal == Reversal::None ||
        !architecture->isEdgeConnected({second, first})) {
      throw QMAPException("Cannot place " + op->getName() + " on qubits " +
                          std::to_string(first) + " and " +
                          std::to_string(second));
    }
    std::swap(first, second);
    if (gate.reversal == Reversal::CNOT) {
      qcMapped.h(first);
      qcMapped.h(second);
      qcMapped.cx(first, second);
      qcMapped.h(second);
      qcMapped.h(first);
      ++results.output.directionReverse;
      return;
    }
    if (gate.reversal == Reversal::NegateBeta) {
      negateXXPlusYYPhase(*op);
    }
  }
  if (op->isControlled()) {
    op->setTargets({second});
    op->setControls({qc::Control{first, gate.op->getControls().begin()->type}});
  } else {
    op->setTargets({first, second});
  }
  qcMapped.emplace_back(std::move(op));
}

bool Mapper::Gate::conflictsWith(const Edge& edge,
                                 const TwoQubitGateCounts& counts) const {
  if (pair() == edge) {
    const auto& opposite =
        first == edge.first ? counts.backward : counts.forward;
    return reversal == Reversal::None && opposite.requiresOrder;
  }
  // Distinct pairs with a common qubit can require incompatible edge
  // directions.
  const bool ordered = reversal == Reversal::None ||
                       counts.forward.requiresOrder ||
                       counts.backward.requiresOrder;
  return ordered && (first == edge.first || first == edge.second ||
                     second == edge.first || second == edge.second);
}

void Mapper::createLayers() {
  const auto layering = results.config.layering;
  const bool directed = !architecture->bidirectional();
  std::vector<std::optional<std::size_t>> lastLayer(architecture->getNqubits());

  for (const auto& op : qc) {
    if (op->getType() == qc::Barrier || op->getType() == qc::Measure) {
      continue;
    }
    if (!op->isUnitary()) {
      throw QMAPException(
          "Mapping not possible: circuit contains non-unitary operation: " +
          std::string(op->getName()));
    }
    if ((!op->isStandardOperation() && !op->isSymbolicOperation()) ||
        op->getNtargets() == 0 || op->getNqubits() > 2) {
      throw QMAPException("Decompose " + op->getName() +
                          " into one- or two-qubit gates with at least one "
                          "target before mapping.");
    }

    const auto& targets = op->getTargets();
    const auto first =
        op->isControlled() ? op->getControls().begin()->qubit : targets.front();
    Gate gate{
        .first = static_cast<std::uint16_t>(qc.initialLayout.at(first)),
        .second = std::nullopt,
        .op = op.get(),
        .reversal = Reversal::None,
    };
    if (op->getNqubits() == 2) {
      gate.second =
          static_cast<std::uint16_t>(qc.initialLayout.at(targets.back()));
      gate.reversal = reversalFor(*op);
    }

    std::size_t layer = layers.size();
    switch (layering) {
    case Layering::IndividualGates:
      break;
    case Layering::DisjointQubits:
    case Layering::Disjoint2qBlocks: {
      const auto blocks = layering == Layering::Disjoint2qBlocks;
      auto& lastFirst = lastLayer.at(gate.first);
      if (gate.singleQubit()) {
        if (blocks) {
          // A single-qubit gate can join the preceding or following block.
          layer = lastFirst.value_or(0);
        } else {
          layer = lastFirst ? *lastFirst + 1 : 0;
          lastFirst = layer;
        }
        break;
      }

      auto& lastSecond = lastLayer.at(*gate.second);
      const auto last = std::max(lastFirst, lastSecond);
      layer = last ? *last + 1 : 0;
      if (blocks && last && lastFirst == lastSecond) {
        const auto& pairs = layers.at(*last).interactions;
        if (const auto it = pairs.find(gate.pair());
            it != pairs.end() &&
            (!directed || !gate.conflictsWith(it->first, it->second))) {
          layer = *last;
        }
      }
      lastFirst = lastSecond = layer;
      break;
    }
    case Layering::OddGates:
    case Layering::QubitTriangle: {
      if (layers.empty()) {
        break;
      }
      if (layering == Layering::OddGates && layers.back().gates.size() == 2) {
        break;
      }
      if (gate.second) {
        if (layering == Layering::QubitTriangle) {
          const auto& used = layers.back().activeQubits2QGates;
          const auto added =
              static_cast<std::size_t>(!used.contains(gate.first)) +
              static_cast<std::size_t>(!used.contains(*gate.second));
          if (used.size() + added > 3) {
            break;
          }
        }
        if (directed && std::ranges::any_of(layers.back().interactions,
                                            [&](const auto& entry) {
                                              return gate.conflictsWith(
                                                  entry.first, entry.second);
                                            })) {
          break;
        }
      }
      --layer;
      break;
    }
    }

    if (layer == layers.size()) {
      layers.emplace_back();
    }
    layers[layer].add(gate);
  }
  results.input.layers = layers.size();
}

void Mapper::Layer::add(const Gate& gate) {
  gates.push_back(gate);
  activeQubits.insert(gate.first);
  if (!gate.second) {
    return;
  }
  activeQubits.insert(*gate.second);
  activeQubits2QGates.insert(gate.first);
  activeQubits2QGates.insert(*gate.second);
  auto& counts = interactions[gate.pair()];
  auto& direction =
      gate.first < *gate.second ? counts.forward : counts.backward;
  if (gate.reversal == Reversal::CNOT) {
    ++direction.cnots;
  } else if (gate.reversal == Reversal::None) {
    direction.requiresOrder = true;
  }
}

std::vector<Edge> Mapper::Layer::sortedPairs() const {
  const auto keys = std::views::keys(interactions);
  std::vector<Edge> pairs(keys.begin(), keys.end());
  std::ranges::sort(pairs);
  return pairs;
}

bool Mapper::isLayerSplittable(std::size_t index) {
  return layers.at(index).activeQubits.size() > 2;
}

void Mapper::splitLayer(std::size_t index) {
  auto& layer = layers.at(index);
  std::set<std::uint16_t> firstHalf;
  bool take = false;
  for (const auto& pair : layer.sortedPairs()) {
    if (take) {
      firstHalf.insert(pair.first);
      firstHalf.insert(pair.second);
    }
    take = !take;
  }
  take = true;
  for (const auto qubit : layer.activeQubits) {
    if (!layer.activeQubits2QGates.contains(qubit)) {
      if (take) {
        firstHalf.insert(qubit);
      }
      take = !take;
    }
  }
  const auto second =
      std::ranges::stable_partition(layer.gates, [&](const Gate& gate) {
        return firstHalf.contains(gate.first);
      });
  splitLayerAt(index,
               static_cast<std::size_t>(second.begin() - layer.gates.begin()));
}

void Mapper::splitLayerAt(std::size_t index, std::size_t offset) {
  auto gates = std::move(layers.at(index).gates);
  layers.at(index) = Layer{};
  layers.emplace(layers.begin() + static_cast<std::ptrdiff_t>(index + 1));
  for (std::size_t i = 0; i < gates.size(); ++i) {
    auto& layer = i < offset ? layers[index] : layers[index + 1];
    layer.add(gates[i]);
  }
  results.input.layers = layers.size();
}

std::size_t Mapper::getNextLayer(std::size_t idx) {
  for (auto next = idx + 1; next < layers.size(); ++next) {
    if (!layers[next].interactions.empty()) {
      return next;
    }
  }
  return std::numeric_limits<std::size_t>::max();
}

void Mapper::finalizeMappedCircuit() {
  // add additional qubits if the architecture contains more qubits than the
  // circuit
  if (architecture->getNqubits() > qcMapped.getNqubits()) {
    for (auto logicalQubit = qcMapped.getNqubits();
         logicalQubit < architecture->getNqubits(); ++logicalQubit) {
      auto physicalQubit = static_cast<qc::Qubit>(logicalQubit);

      // check if the corresponding physical qubit is already in use
      if (qcMapped.initialLayout.find(static_cast<qc::Qubit>(logicalQubit)) !=
          qcMapped.initialLayout.end()) {
        // get the next unused physical qubit
        for (physicalQubit = 0; physicalQubit < architecture->getNqubits();
             ++physicalQubit) {
          if (qcMapped.initialLayout.find(physicalQubit) ==
              qcMapped.initialLayout.end()) {
            break;
          }
        }
      }

      // the added logical qubits are not used in the circuit itself, so they
      // are regarded garbage
      qcMapped.addAncillaryQubit(physicalQubit, std::nullopt);
    }
  }
  // unify quantum registers
  qcMapped.unifyQuantumRegisters();

  // append measurements according to output permutation
  if (results.config.addMeasurementsToMappedCircuit &&
      !qcMapped.outputPermutation.empty()) {
    appendMeasurementsAccordingToOutputPermutation(qcMapped);
  }
}

void Mapper::placeRemainingArchitectureQubits() {
  qc::Qubit nextLogical = 0;
  for (qc::Qubit physical = 0; physical < qubits.size(); ++physical) {
    if (qubits.at(physical) == DEFAULT_POSITION) {
      while (locations.at(nextLogical) != DEFAULT_POSITION) {
        ++nextLogical;
      }
      qubits.at(physical) = static_cast<std::int16_t>(nextLogical);
      locations.at(nextLogical) = static_cast<std::int16_t>(physical);
      qcMapped.initialLayout[physical] = nextLogical;
      if (nextLogical >= qc.getNqubits()) {
        qcMapped.setLogicalQubitAncillary(nextLogical);
        qcMapped.setLogicalQubitGarbage(nextLogical);
      }
    }
  }
}

void Mapper::preMappingOptimizations(const Configuration& config
                                     [[maybe_unused]]) {
  if (!config.preMappingOptimizations) {
    return;
  }

  // at the moment there are no pre-mapping optimizations
}

void Mapper::postMappingOptimizations(const Configuration& config) {
  if (!config.postMappingOptimizations) {
    return;
  }

  // try to cancel adjacent CNOT gates
  qmap::cancelCNOTs(qcMapped, [this](const qc::Qubit q0, const qc::Qubit q1) {
    return !fidelityAwareHeur && architecture->isEdgeBidirectional({q0, q1});
  });
}

void Mapper::countGates(decltype(qcMapped.cbegin()) it,
                        const decltype(qcMapped.cend())& end,
                        MappingResults::CircuitInfo& info, const bool mapped) {
  const auto& properties = std::as_const(*architecture).getProperties();
  const auto cnotCalibrated = architecture->isFidelityAvailable();
  const auto calibrated = !properties.twoQubitErrorRate.empty() ||
                          !properties.singleQubitErrorRate.empty();
  constexpr auto unavailable = std::numeric_limits<double>::quiet_NaN();
  const auto addCost = [&](const double cost) {
    info.totalLogFidelity += std::isfinite(cost) ? cost : unavailable;
  };
  for (; it != end; ++it) {
    const auto& g = *it;
    if (g->getType() == qc::Barrier) {
      continue;
    }
    if (g->isStandardOperation() || g->isSymbolicOperation()) {
      if (g->getNtargets() == 1 && !g->isControlled()) {
        ++info.singleQubitGates;
        ++info.gates;
        const auto q1 = static_cast<std::uint16_t>(g->getTargets()[0]);
        if (mapped && calibrated) {
          addCost(architecture->singleQubitCost(g->getType(), q1,
                                                assumeIdealSingleQubits));
        } else if (!mapped && cnotCalibrated) {
          addCost(architecture->getSingleQubitFidelityCost(q1));
        }
        continue;
      }
      if (g->getNqubits() != 2 || g->getNtargets() == 0) {
        continue;
      }

      const auto controlled = g->isControlled();
      const auto q1 = static_cast<std::uint16_t>(
          controlled ? g->getControls().begin()->qubit : g->getTargets()[0]);
      const auto q2 = static_cast<std::uint16_t>(
          controlled ? g->getTargets()[0] : g->getTargets()[1]);
      const auto cnot = qmap::isCNOT(*g);
      const auto swap = !controlled && g->getType() == qc::SWAP;
      if (swap) {
        info.cnots += GATES_OF_BIDIRECTIONAL_SWAP;
        info.gates += GATES_OF_BIDIRECTIONAL_SWAP;
        if (mapped && !architecture->isEdgeBidirectional({q1, q2})) {
          info.gates += GATES_OF_DIRECTION_REVERSE;
          info.singleQubitGates += GATES_OF_DIRECTION_REVERSE;
        }
      } else {
        ++info.gates;
        if (cnot) {
          ++info.cnots;
        }
      }

      if (mapped && calibrated) {
        if (controlled &&
            g->getControls().begin()->type == qc::Control::Type::Neg) {
          info.totalLogFidelity = unavailable;
        } else {
          const auto cost =
              architecture->nativeTwoQubitCost(g->getType(), {q1, q2});
          addCost(swap && !std::isfinite(cost)
                      ? architecture
                            ->swapFidelity({q1, q2}, assumeIdealSingleQubits)
                            .cost
                      : cost);
        }
      } else if (!mapped && cnot && cnotCalibrated) {
        addCost(architecture->getTwoQubitFidelityCost(q1, q2));
      } else if (calibrated && !cnot) {
        info.totalLogFidelity = unavailable;
      }
      continue;
    }

    if (const auto& cg = dynamic_cast<const qc::CompoundOperation*>(g.get());
        cg != nullptr) {
      countGates(cg->cbegin(), cg->cend(), info, mapped);
    }
  }
  info.totalFidelity = std::pow(2, -info.totalLogFidelity);
}
