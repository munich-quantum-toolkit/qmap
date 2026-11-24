/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "sc/heuristic/HeuristicMapper.hpp"

#include "ir/Definitions.hpp"
#include "ir/operations/CompoundOperation.hpp"
#include "ir/operations/OpType.hpp"
#include "ir/operations/Operation.hpp"
#include "sc/Architecture.hpp"
#include "sc/DataLogger.hpp"
#include "sc/Mapper.hpp"
#include "sc/configuration/Configuration.hpp"
#include "sc/configuration/EarlyTermination.hpp"
#include "sc/configuration/Heuristic.hpp"
#include "sc/configuration/InitialLayout.hpp"
#include "sc/configuration/Layering.hpp"
#include "sc/configuration/LookaheadHeuristic.hpp"
#include "sc/utils.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <set>
#include <utility>
#include <vector>

void HeuristicMapper::map(const Configuration& configuration) {
  if (configuration.dataLoggingEnabled()) {
    dataLogger = std::make_unique<DataLogger>(configuration.dataLoggingPath,
                                              *architecture, qc);
  }

  tightHeur = isTight(configuration.heuristic);
  fidelityAwareHeur = isFidelityAware(configuration.heuristic);

  results = MappingResults{};
  results.config = configuration;
  const auto& config = results.config;
  checkParameters();
  const auto start = std::chrono::steady_clock::now();
  initResults();

  // perform pre-mapping optimizations
  preMappingOptimizations(config);

  createLayers();
  prepareRoutingSwaps();
  if (config.verbose) {
    printLayering(std::clog);
  }

  createInitialMapping();
  if (config.verbose) {
    printLocations(std::clog);
    printQubits(std::clog);
  }

  for (std::size_t i = 0; i < config.iterativeBidirectionalRoutingPasses; ++i) {
    if (config.verbose) {
      std::clog << "\nIterative bidirectional routing (forward pass " << i
                << "):\n";
    }
    pseudoRouteCircuit(false);
    if (config.verbose) {
      std::clog << "\nIterative bidirectional routing (backward pass " << i
                << "):\n";
    }
    pseudoRouteCircuit(true);

    if (config.verbose) {
      std::clog << "\nMain routing:\n";
    }
  }

  routeCircuit();

  postMappingOptimizations(config);
  countGates(qcMapped, results.output);
  finalizeMappedCircuit();

  const auto end = std::chrono::steady_clock::now();
  const std::chrono::duration<double> diff = end - start;
  results.time = diff.count();
  results.timeout = false;

  if (config.dataLoggingEnabled()) {
    dataLogger->logOutputCircuit(qcMapped);
    dataLogger->logMappingResult(results);
    dataLogger->close();
  }
}

namespace {
bool isPairMapped(const Architecture& architecture, const Edge& physical,
                  const TwoQubitGateCounts& counts) {
  const Edge reverse{physical.second, physical.first};
  return (!counts.forward.requiresOrder ||
          architecture.isEdgeConnected(physical)) &&
         (!counts.backward.requiresOrder ||
          architecture.isEdgeConnected(reverse)) &&
         architecture.isEdgeConnected(physical, false);
}

double reversalCost(const Architecture& architecture, const Edge& physical,
                    const TwoQubitGateCounts& counts) {
  if (architecture.bidirectional()) {
    return 0.;
  }
  double cost = 0.;
  if (counts.forward.cnots != 0 && !architecture.isEdgeConnected(physical)) {
    cost += counts.forward.cnots * COST_DIRECTION_REVERSE;
  }
  if (counts.backward.cnots != 0 &&
      !architecture.isEdgeConnected({physical.second, physical.first})) {
    cost += counts.backward.cnots * COST_DIRECTION_REVERSE;
  }
  return cost;
}

double pairDistance(const Architecture& architecture, const Edge& physical,
                    const TwoQubitGateCounts& counts) {
  const auto [first, second] = physical;
  const bool forward =
      counts.forward.cnots != 0 || counts.forward.requiresOrder;
  const bool backward =
      counts.backward.cnots != 0 || counts.backward.requiresOrder;
  if (!forward && !backward) {
    return architecture.distance(first, second, false);
  }
  // The CNOT distance is also a lower bound when the forward edge is required.
  return std::max(forward ? architecture.distance(first, second) : 0.,
                  backward ? architecture.distance(second, first) : 0.);
}

double lookaheadPairDistance(const Architecture& architecture,
                             const Edge& physical,
                             const TwoQubitGateCounts& counts) {
  const auto [first, second] = physical;
  if (counts.forward.requiresOrder || counts.backward.requiresOrder) {
    return std::max(
        counts.forward.requiresOrder ? architecture.distance(first, second)
                                     : 0.,
        counts.backward.requiresOrder ? architecture.distance(second, first)
                                      : 0.);
  }
  if (counts.forward.cnots != 0 && counts.backward.cnots != 0) {
    return std::min(architecture.distance(first, second),
                    architecture.distance(second, first));
  }
  return pairDistance(architecture, physical, counts);
}

double placementDistance(const Architecture& architecture, const Edge& physical,
                         const TwoQubitGateCounts& counts,
                         const bool fromSecond = false) {
  if (counts.forward.requiresOrder || counts.backward.requiresOrder) {
    return lookaheadPairDistance(architecture, physical, counts);
  }
  const bool includeReversal =
      counts.forward.cnots != 0 || counts.backward.cnots != 0;
  return fromSecond ? architecture.distance(physical.second, physical.first,
                                            includeReversal)
                    : architecture.distance(physical.first, physical.second,
                                            includeReversal);
}
} // namespace

void HeuristicMapper::staticInitialMapping() {
  for (const auto& gate : layers.front().gates) {
    if (gate.singleQubit()) {
      continue;
    }

    const auto logicalFirst = gate.first;
    const auto logicalSecond = *gate.second;
    if (locations.at(logicalFirst) != DEFAULT_POSITION ||
        locations.at(logicalSecond) != DEFAULT_POSITION) {
      continue;
    }

    const bool forward = logicalFirst < logicalSecond;
    const Edge edge = forward ? Edge{logicalFirst, logicalSecond}
                              : Edge{logicalSecond, logicalFirst};
    const auto& counts = layers.front().interactions.at(edge);
    const bool backwardRequired =
        forward ? counts.backward.requiresOrder : counts.forward.requiresOrder;
    for (const auto& [q0, q1] : architecture->getCouplingMap()) {
      if (qubits.at(q0) != DEFAULT_POSITION ||
          qubits.at(q1) != DEFAULT_POSITION) {
        continue;
      }

      const auto physicalFirst = backwardRequired ? q1 : q0;
      const auto physicalSecond = backwardRequired ? q0 : q1;
      const Edge physical = forward ? Edge{physicalFirst, physicalSecond}
                                    : Edge{physicalSecond, physicalFirst};
      if (!isPairMapped(*architecture, physical, counts)) {
        continue;
      }

      qubits.at(physicalFirst) = static_cast<std::int16_t>(logicalFirst);
      qubits.at(physicalSecond) = static_cast<std::int16_t>(logicalSecond);
      locations.at(logicalFirst) = static_cast<std::int16_t>(physicalFirst);
      locations.at(logicalSecond) = static_cast<std::int16_t>(physicalSecond);
      break;
    }
  }

  finishStaticInitialMapping();
}

void HeuristicMapper::finishStaticInitialMapping() {
  for (qc::Qubit logical = 0; logical < qc.getNqubits(); ++logical) {
    if (locations.at(logical) != DEFAULT_POSITION) {
      continue;
    }
    for (std::uint16_t physical = 0; physical < architecture->getNqubits();
         ++physical) {
      if (qubits.at(physical) == DEFAULT_POSITION) {
        locations.at(logical) = static_cast<std::int16_t>(physical);
        qubits.at(physical) = static_cast<std::int16_t>(logical);
        break;
      }
    }
  }
}

void HeuristicMapper::checkParameters() {
  const auto& config = results.config;
  if (config.layering == Layering::OddGates ||
      config.layering == Layering::QubitTriangle) {
    throw QMAPException("Layering strategy " + toString(config.layering) +
                        " not suitable for heuristic mapper!");
  }
  if (fidelityAwareHeur && !architecture->isCalibrationDataAvailable()) {
    throw QMAPException("Fidelity aware heuristic chosen, but no or "
                        "insufficient calibration data available for this "
                        "architecture!");
  }
  if (fidelityAwareHeur && !isFidelityAware(config.lookaheadHeuristic) &&
      config.lookaheadHeuristic != LookaheadHeuristic::None) {
    throw QMAPException("Fidelity-aware heuristics may only be used with "
                        "fidelity-aware lookahead heuristics (or no "
                        "lookahead)!");
  }
}

void HeuristicMapper::createInitialMapping() {
  const auto& config = results.config;

  if (layers.empty()) {
    return;
  }

  switch (config.initialLayout) {
  case InitialLayout::Identity:
    for (qc::Qubit i = 0; i < qc.getNqubits(); ++i) {
      locations.at(i) = static_cast<std::int16_t>(i);
      qubits.at(i) = static_cast<std::int16_t>(i);
    }
    break;
  case InitialLayout::Static:
    if (!fidelityAwareHeur) {
      staticInitialMapping();
    }
    break;
  case InitialLayout::Dynamic:
    // nothing to be done here
    break;

    // TODO: Design strategy that maps most used qubit to most connected qubits
    // on architecture
  }
}

void HeuristicMapper::mapUnmappedGates(std::size_t layer) {
  for (const auto& [logEdge, counts] : layers.at(layer).interactions) {
    const auto& [q1, q2] = logEdge;

    const auto q1Location = locations.at(q1);
    const auto q2Location = locations.at(q2);

    if (q1Location == DEFAULT_POSITION && q2Location == DEFAULT_POSITION) {
      std::optional<Edge> chosenEdge;
      for (const auto& edge : architecture->getCouplingMap()) {
        if (qubits.at(edge.first) != DEFAULT_POSITION ||
            qubits.at(edge.second) != DEFAULT_POSITION) {
          continue;
        }
        const Edge physical = counts.backward.requiresOrder
                                  ? Edge{edge.second, edge.first}
                                  : edge;
        if ((!chosenEdge || physical < *chosenEdge) &&
            isPairMapped(*architecture, physical, counts)) {
          chosenEdge = physical;
        }
      }

      if (!chosenEdge) {
        // Map to two free qubits with minimal distance.
        auto bestScore = std::numeric_limits<double>::max();
        for (std::uint16_t i = 0; i < architecture->getNqubits(); ++i) {
          for (std::uint16_t j = i + 1; j < architecture->getNqubits(); ++j) {
            if (qubits.at(i) != DEFAULT_POSITION ||
                qubits.at(j) != DEFAULT_POSITION) {
              continue;
            }
            const Edge physical =
                counts.backward.requiresOrder ? Edge{j, i} : Edge{i, j};
            const double distance =
                placementDistance(*architecture, physical, counts);
            if (distance < bestScore) {
              bestScore = distance;
              chosenEdge = physical;
            }
          }
        }
        if (!chosenEdge) {
          throw QMAPException("No available placement for a two-qubit gate.");
        }
      }
      locations.at(q1) = static_cast<std::int16_t>(chosenEdge->first);
      locations.at(q2) = static_cast<std::int16_t>(chosenEdge->second);
      qubits.at(chosenEdge->first) = static_cast<std::int16_t>(q1);
      qubits.at(chosenEdge->second) = static_cast<std::int16_t>(q2);

    } else if (q1Location == DEFAULT_POSITION) {
      mapToMinDistance(q2, q1, counts);
    } else if (q2Location == DEFAULT_POSITION) {
      mapToMinDistance(q1, q2, counts);
    }
  }
}

void HeuristicMapper::mapToMinDistance(const std::uint16_t mapped,
                                       const std::uint16_t unmapped,
                                       const TwoQubitGateCounts& counts) {
  auto bestDistance = std::numeric_limits<double>::max();
  std::optional<std::uint16_t> position;
  const auto mappedLocation = static_cast<std::uint16_t>(locations.at(mapped));
  for (std::uint16_t i = 0; i < architecture->getNqubits(); ++i) {
    if (qubits.at(i) == DEFAULT_POSITION) {
      const bool fromSecond = mapped > unmapped;
      const Edge physical =
          fromSecond ? Edge{i, mappedLocation} : Edge{mappedLocation, i};
      const double distance =
          placementDistance(*architecture, physical, counts, fromSecond);
      if (distance < bestDistance) {
        bestDistance = distance;
        position = i;
      }
    }
  }
  if (!position) {
    throw QMAPException("No available placement for a two-qubit gate.");
  }
  qubits.at(*position) = static_cast<std::int16_t>(unmapped);
  locations.at(unmapped) = static_cast<std::int16_t>(*position);
}

void HeuristicMapper::prepareRoutingSwaps() {
  routingSwaps.clear();
  const auto n = architecture->getNqubits();
  if (fidelityAwareHeur) {
    calibratedSwapDistances.assign(
        n, std::vector<double>(n, std::numeric_limits<double>::infinity()));
  }
  for (const auto& edge : architecture->getCouplingMap()) {
    const auto [first, second] = edge;
    if (first > second && architecture->isEdgeConnected({second, first})) {
      continue;
    }
    if (!fidelityAwareHeur) {
      const auto cost = architecture->isEdgeBidirectional(edge)
                            ? COST_BIDIRECTIONAL_SWAP
                            : COST_UNIDIRECTIONAL_SWAP;
      routingSwaps.emplace_back(edge, cost);
      continue;
    }
    const auto [cost, reversed] = architecture->swapFidelity(first, second);
    if (std::isfinite(cost)) {
      routingSwaps.emplace_back(reversed ? Edge{second, first} : edge, cost);
      calibratedSwapDistances[first][second] = cost;
      calibratedSwapDistances[second][first] = cost;
    }
  }
  if (!fidelityAwareHeur) {
    return;
  }
  for (std::size_t q = 0; q < n; ++q) {
    calibratedSwapDistances[q][q] = 0.;
  }
  for (std::size_t via = 0; via < n; ++via) {
    for (std::size_t from = 0; from < n; ++from) {
      const auto firstLeg = calibratedSwapDistances[from][via];
      if (!std::isfinite(firstLeg)) {
        continue;
      }
      for (std::size_t to = 0; to < n; ++to) {
        auto& distance = calibratedSwapDistances[from][to];
        distance =
            std::min(distance, firstLeg + calibratedSwapDistances[via][to]);
      }
    }
  }
}

bool HeuristicMapper::prepareCalibratedInteractions(const Layer& layer) {
  const auto n = architecture->getNqubits();
  const auto infinity = std::numeric_limits<double>::infinity();
  std::map<std::uint16_t, Edge> logicalPairs;
  for (const auto& [edge, _] : layer.interactions) {
    logicalPairs.emplace(edge.first, edge);
    logicalPairs.emplace(edge.second, edge);
  }
  std::map<Edge, std::vector<const Gate*>> groups;
  for (const auto& gate : layer.gates) {
    const auto pair = logicalPairs.find(gate.first);
    const auto logical = pair == logicalPairs.end()
                             ? Edge{gate.first, gate.first}
                             : pair->second;
    groups[logical].push_back(&gate);
  }

  calibratedInteractions.clear();
  minLayerExecutionCost = 0.;
  for (const auto& [logical, gates] : groups) {
    CalibratedInteraction interaction{logical, {}, infinity};
    const auto addPlacement = [&](const Edge& physical) {
      double cost = 0.;
      for (const auto* gate : gates) {
        const bool forward = gate->first == logical.first;
        const auto first = forward ? physical.first : physical.second;
        if (gate->singleQubit()) {
          cost += architecture->singleQubitCost(gate->op->getType(), first);
        } else {
          const auto second = forward ? physical.second : physical.first;
          cost += gateFidelity(*gate, first, second).cost;
        }
        if (!std::isfinite(cost)) {
          return;
        }
      }
      interaction.placements.emplace(physical, cost);
      interaction.minExecutionCost =
          std::min(interaction.minExecutionCost, cost);
    };

    if (logical.first == logical.second) {
      for (std::uint16_t q = 0; q < n; ++q) {
        addPlacement({q, q});
      }
    } else {
      for (const auto& edge : architecture->getCouplingMap()) {
        addPlacement(edge);
        const Edge reverse{edge.second, edge.first};
        if (!architecture->isEdgeConnected(reverse)) {
          addPlacement(reverse);
        }
      }
    }

    if (interaction.placements.empty()) {
      return false;
    }
    minLayerExecutionCost += interaction.minExecutionCost;
    calibratedInteractions.push_back(std::move(interaction));
  }
  // Assign the most constrained interactions first.
  std::ranges::stable_sort(
      calibratedInteractions, {},
      [](const auto& interaction) { return interaction.placements.size(); });
  return true;
}

double HeuristicMapper::calibratedInteractionBound(
    const CalibratedInteraction& interaction, const Edge& current) const {
  auto bound = std::numeric_limits<double>::infinity();
  for (const auto& [goal, execution] : interaction.placements) {
    const auto firstMove = calibratedSwapDistances[current.first][goal.first];
    const auto secondMove =
        calibratedSwapDistances[current.second][goal.second];
    // Both operands can move in one SWAP. Their maximum is a lower bound.
    bound = std::min(bound, execution + std::max(firstMove, secondMove));
  }
  return bound;
}

bool HeuristicMapper::mapUnmappedCalibratedGates(const std::size_t index) {
  if (index == calibratedInteractions.size()) {
    return true;
  }
  const auto& interaction = calibratedInteractions[index];
  const auto [q1, q2] = interaction.logical;
  const auto first = locations.at(q1);
  const auto second = locations.at(q2);
  if (first != DEFAULT_POSITION && second != DEFAULT_POSITION) {
    return mapUnmappedCalibratedGates(index + 1);
  }

  const auto available = [this](const std::int16_t assigned,
                                const std::uint16_t physical) {
    return assigned == DEFAULT_POSITION
               ? qubits.at(physical) == DEFAULT_POSITION
               : std::cmp_equal(assigned, physical);
  };
  const auto place = [this](const std::uint16_t logical,
                            const std::uint16_t physical) {
    qubits.at(physical) = static_cast<std::int16_t>(logical);
    locations.at(logical) = static_cast<std::int16_t>(physical);
  };
  const auto restore = [this](const std::uint16_t logical,
                              const std::uint16_t physical,
                              const std::int16_t original) {
    if (original == DEFAULT_POSITION) {
      qubits.at(physical) = DEFAULT_POSITION;
      locations.at(logical) = DEFAULT_POSITION;
    }
  };
  const auto tryPlacements = [&](auto& candidates) {
    std::ranges::sort(candidates);
    for (const auto& [cost, physical] : candidates) {
      place(q1, physical.first);
      place(q2, physical.second);
      if (mapUnmappedCalibratedGates(index + 1)) {
        return true;
      }
      restore(q1, physical.first, first);
      restore(q2, physical.second, second);
    }
    return false;
  };

  std::vector<std::pair<double, Edge>> candidates;
  for (const auto& [physical, cost] : interaction.placements) {
    if (available(first, physical.first) &&
        available(second, physical.second)) {
      candidates.emplace_back(cost, physical);
    }
  }
  if (tryPlacements(candidates)) {
    return true;
  }

  // If no direct assignment works, try free sites that can reach an eligible
  // placement through calibrated SWAPs.
  if (routingSwaps.empty()) {
    return false;
  }
  candidates.clear();
  const bool single = q1 == q2;
  const auto n = architecture->getNqubits();
  for (std::uint16_t i = 0; i < n; ++i) {
    if (!available(first, i)) {
      continue;
    }
    const auto start = single ? i : std::uint16_t{0};
    const auto end = single ? static_cast<std::size_t>(i) + 1 : n;
    for (auto j = start; j < end; ++j) {
      const Edge physical{i, j};
      if ((!single && i == j) || !available(second, j) ||
          interaction.placements.contains(physical)) {
        continue;
      }
      const auto cost = calibratedInteractionBound(interaction, physical);
      if (std::isfinite(cost)) {
        candidates.emplace_back(cost, physical);
      }
    }
  }
  return tryPlacements(candidates);
}

void HeuristicMapper::updateFidelityCosts(Node& node) const {
  node.validMappedTwoQubitGates.clear();
  node.validMapping = true;
  double executionCost = 0.;
  double extraCost = 0.;
  for (const auto& interaction : calibratedInteractions) {
    const auto [q1, q2] = interaction.logical;
    const Edge current{static_cast<std::uint16_t>(node.locations.at(q1)),
                       static_cast<std::uint16_t>(node.locations.at(q2))};
    const auto placement = interaction.placements.find(current);
    if (placement == interaction.placements.end()) {
      node.validMapping = false;
    } else {
      executionCost += placement->second;
      if (q1 != q2) {
        node.validMappedTwoQubitGates.insert(interaction.logical);
      }
      if (placement->second == interaction.minExecutionCost) {
        continue;
      }
    }
    extraCost =
        std::max(extraCost, calibratedInteractionBound(interaction, current) -
                                interaction.minExecutionCost);
  }
  node.costFixed = node.swapFidelityCost + executionCost;
  // The same SWAP can serve more than one interaction. Use the maximum
  // additional cost over their separate lower bounds.
  node.costHeur = minLayerExecutionCost + extraCost - executionCost;
}

void HeuristicMapper::pseudoRouteCircuit(bool reverse) {
  // save original global data for restoring it later
  const auto originalResults = results;
  const auto originalLayers = layers;

  auto& config = results.config;
  config.dataLoggingPath = ""; // disable data logging for pseudo routing
  config.debug = false;

  for (std::size_t i = 0; i < layers.size(); ++i) {
    const auto layerIndex = (reverse ? layers.size() - i - 1 : i);
    const Node result = aStarMap(layerIndex, reverse);

    qubits = result.qubits;
    locations = result.locations;

    if (config.verbose) {
      printLocations(std::clog);
      printQubits(std::clog);
    }
  }

  // restore original global data
  results = originalResults;
  layers = originalLayers;
}

void HeuristicMapper::routeCircuit() {
  const auto& config = results.config;

  std::vector<qc::Operation*> swapsAndDeferredGates;
  results.output.gates = 0U;
  for (std::size_t layerIndex = 0; layerIndex < layers.size(); ++layerIndex) {
    const Node result = aStarMap(layerIndex, false);

    qubits = result.qubits;
    locations = result.locations;

    if (config.verbose) {
      printLocations(std::clog);
      printQubits(std::clog);
    }

    if (layerIndex != 0 && config.addBarriersBetweenLayers) {
      qcMapped.barrier();
    }

    // initial layer needs no swaps
    if (layerIndex != 0 || config.swapOnFirstLayer) {
      for (const auto& swap : result.swaps) {
        if (swap.op == qc::SWAP) {
          if (config.verbose) {
            std::clog << "SWAP: " << swap.first << " <-> " << swap.second
                      << "\n";
          }
          // check if SWAP is placed on a valid edge
          assert(
              architecture->isEdgeConnected({swap.first, swap.second}, false));
          qcMapped.swap(swap.first, swap.second);
          swapsAndDeferredGates.push_back(qcMapped.back().get());
          results.output.swaps++;
        }
      }
    }

    // Add gates at the placement chosen for the layer.
    for (const auto& gate : layers.at(layerIndex).gates) {
      const auto first = locations.at(gate.first);
      if (first == DEFAULT_POSITION) {
        assert(gate.singleQubit());
        appendMappedGate(gate, gate.first);
        swapsAndDeferredGates.push_back(qcMapped.back().get());
      } else {
        appendMappedGate(gate, static_cast<std::uint16_t>(first));
      }
    }
  }

  if (config.debug && results.heuristicBenchmark.expandedNodes > 0) {
    auto& benchmark = results.heuristicBenchmark;
    benchmark.secondsPerNode /= static_cast<double>(benchmark.expandedNodes);
    benchmark.averageBranchingFactor =
        static_cast<double>(benchmark.generatedNodes - layers.size()) /
        static_cast<double>(benchmark.expandedNodes);
    for (const auto& layer : results.layerHeuristicBenchmark) {
      benchmark.effectiveBranchingFactor +=
          layer.effectiveBranchingFactor *
          (static_cast<double>(layer.expandedNodes) /
           static_cast<double>(benchmark.expandedNodes));
    }
  }

  qcMapped.outputPermutation.clear();
  placeRemainingArchitectureQubits();
  for (qc::Qubit logical = 0; logical < qc.getNqubits(); ++logical) {
    const auto physical = static_cast<qc::Qubit>(locations.at(logical));
    qcMapped.outputPermutation.emplace(physical, logical);
  }

  // Undo routing SWAPs to locate deferred gates and recover the initial layout.
  for (auto* op : std::views::reverse(swapsAndDeferredGates)) {
    if (op->getType() == qc::SWAP) {
      auto& logical0 = qubits.at(op->getTargets().at(0));
      auto& logical1 = qubits.at(op->getTargets().at(1));
      std::swap(locations.at(static_cast<std::size_t>(logical0)),
                locations.at(static_cast<std::size_t>(logical1)));
      std::swap(logical0, logical1);
    } else {
      const auto target = op->getTargets().at(0);
      op->setTargets({static_cast<qc::Qubit>(locations.at(target))});
    }
  }
  for (auto& [physical, logical] : qcMapped.initialLayout) {
    logical = static_cast<qc::Qubit>(qubits.at(physical));
  }
}

HeuristicMapper::Node HeuristicMapper::aStarMap(size_t layer, bool reverse) {
  const auto& config = results.config;
  nextNodeId = 0;

  const auto layerForLog = [&] {
    struct LayerLog {
      qc::CompoundOperation ops;
      SingleQubitMultiplicity singles;
      TwoQubitMultiplicity pairs;
    };
    LayerLog data{qc::CompoundOperation{},
                  SingleQubitMultiplicity(architecture->getNqubits()),
                  {}};
    for (const auto& gate : layers.at(layer).gates) {
      data.ops.emplace_back(gate.op->clone());
      if (gate.second) {
        auto& [forward, backward] = data.pairs[gate.pair()];
        ++(gate.first < *gate.second ? forward : backward);
      } else {
        ++data.singles.at(gate.first);
      }
    }
    return data;
  };
  const auto initialQubits =
      fidelityAwareHeur ? qubits : std::vector<std::int16_t>{};
  const auto initialLocations =
      fidelityAwareHeur ? locations : std::vector<std::int16_t>{};
  bool searchStarted = false;
  const auto retrySplitLayer = [&](const bool requireCalibration) {
    if (searchStarted && config.dataLoggingEnabled()) {
      const auto [ops, singles, pairs] = layerForLog();
      dataLogger->logFinalizeLayer(layer, ops, singles, pairs, qubits, 0, 0, 0,
                                   0, {}, {}, 0);
      dataLogger->splitLayer();
    }
    while (!nodes.empty()) {
      nodes.pop();
    }
    if (fidelityAwareHeur) {
      qubits = initialQubits;
      locations = initialLocations;
    }
    if (requireCalibration) {
      splitLayerAt(layer, layers.at(layer).gates.size() / 2);
    } else {
      splitLayer(layer);
    }
    if (config.verbose) {
      std::clog << "Split layer\n";
    }
    return aStarMap(reverse ? layer + 1 : layer, reverse);
  };

  Node node(architecture->getNqubits(), nextNodeId++);
  Node bestDoneNode(architecture->getNqubits(), 0);
  bool validMapping = false;

  if (fidelityAwareHeur) {
    const auto& current = layers.at(layer);
    if (!prepareCalibratedInteractions(current)) {
      if (current.gates.size() > 1) {
        return retrySplitLayer(true);
      }
      throw QMAPException("No calibrated placement for " +
                          current.gates.front().op->getName() + ".");
    }
    if (!mapUnmappedCalibratedGates()) {
      if (current.gates.size() > 1) {
        return retrySplitLayer(true);
      }
      throw QMAPException(
          "The current layout cannot reach a calibrated placement for " +
          current.gates.front().op->getName() + ".");
    }
    if (config.initialLayout == InitialLayout::Static) {
      finishStaticInitialMapping();
    }
  } else {
    mapUnmappedGates(layer);
  }

  node.locations = locations;
  node.qubits = qubits;
  if (fidelityAwareHeur) {
    updateFidelityCosts(node);
  } else {
    recalculateFixedCost(layer, node);
    updateHeuristicCost(layer, node);
    updateLookaheadPenalty(layer, node);
  }

  searchStarted = true;
  if (config.dataLoggingEnabled()) {
    dataLogger->logSearchNode(layer, node.id, node.parent,
                              node.costFixed + node.costFixedReversals,
                              node.costHeur, node.lookaheadPenalty, node.qubits,
                              node.validMapping, node.swaps, node.depth);
  }
  if (std::isfinite(node.getTotalCost())) {
    nodes.push(node);
  }

  const auto start = std::chrono::steady_clock::now();
  std::size_t expandedNodes = 0;
  std::size_t expandedNodesAfterFirstSolution = 0;
  std::size_t expandedNodesAfterOptimalSolution = 0;
  std::size_t solutionNodes = 0;
  std::size_t solutionNodesAfterOptimalSolution = 0;
  bool earlyTermination = false;

  const bool splittable =
      config.automaticLayerSplits ? isLayerSplittable(layer) : false;

  // Future calibrated costs depend only on active qubit locations. Ignore
  // repeated placements reached through equal or more expensive SWAP paths.
  std::map<std::vector<std::int16_t>, double> expandedFidelityPlacements;
  while (!nodes.empty() &&
         (!validMapping ||
          nodes.top().getTotalCost() < bestDoneNode.getTotalFixedCost())) {
    if (splittable && expandedNodes >= config.automaticLayerSplitsNodeLimit) {
      return retrySplitLayer(false);
    }
    Node current = nodes.top();
    if (fidelityAwareHeur) {
      std::vector<std::int16_t> placement;
      for (const auto logical : layers.at(layer).activeQubits) {
        placement.push_back(current.locations.at(logical));
      }
      const auto [it, inserted] = expandedFidelityPlacements.try_emplace(
          std::move(placement), current.swapFidelityCost);
      if (!inserted && it->second <= current.swapFidelityCost) {
        nodes.pop();
        continue;
      }
      it->second = current.swapFidelityCost;
    }
    if (current.validMapping) {
      ++solutionNodes;
      if (!validMapping ||
          current.getTotalFixedCost() < bestDoneNode.getTotalFixedCost()) {
        bestDoneNode = current;
        expandedNodesAfterOptimalSolution = 0;
        solutionNodesAfterOptimalSolution = 0;
      } else {
        ++solutionNodesAfterOptimalSolution;
      }
      validMapping = true;
      if (tightHeur) {
        break;
      }
    }
    nodes.pop();
    expandNode(current, layer);
    ++expandedNodes;
    if (validMapping) {
      ++expandedNodesAfterFirstSolution;
      ++expandedNodesAfterOptimalSolution;

      if (config.earlyTermination != EarlyTermination::None) {
        std::size_t n = 0;
        switch (config.earlyTermination) {
        case EarlyTermination::ExpandedNodes:
          n = expandedNodes;
          break;
        case EarlyTermination::ExpandedNodesAfterFirstSolution:
          n = expandedNodesAfterFirstSolution;
          break;
        case EarlyTermination::ExpandedNodesAfterCurrentOptimalSolution:
          n = expandedNodesAfterOptimalSolution;
          break;
        case EarlyTermination::SolutionNodes:
          n = solutionNodes;
          break;
        case EarlyTermination::SolutionNodesAfterCurrentOptimalSolution:
          n = solutionNodesAfterOptimalSolution;
          break;
        default:
          break;
        }
        if (n >= config.earlyTerminationLimit) {
          earlyTermination = true;
          break;
        }
      }
    }
  }

  if (!validMapping) {
    if (fidelityAwareHeur) {
      const auto& gates = layers.at(layer).gates;
      if (gates.size() > 1) {
        return retrySplitLayer(true);
      }
      throw QMAPException(
          "The current layout cannot reach a calibrated placement for " +
          gates.front().op->getName() + ".");
    }
    throw QMAPException("No viable mapping found.");
  }

  Node result = bestDoneNode;
  if (config.debug) {
    const auto end = std::chrono::steady_clock::now();
    results.layerHeuristicBenchmark.emplace_back();
    const auto layerResultsIt = results.layerHeuristicBenchmark.rbegin();
    layerResultsIt->expandedNodes = expandedNodes;
    results.heuristicBenchmark.expandedNodes += expandedNodes;

    layerResultsIt->solutionDepth = result.depth;
    layerResultsIt->expandedNodesAfterFirstSolution =
        expandedNodesAfterFirstSolution;
    layerResultsIt->expandedNodesAfterOptimalSolution =
        expandedNodesAfterOptimalSolution;
    layerResultsIt->solutionNodes = solutionNodes;
    layerResultsIt->solutionNodesAfterOptimalSolution =
        solutionNodesAfterOptimalSolution;
    layerResultsIt->earlyTermination = earlyTermination;

    const std::chrono::duration<double> diff = end - start;
    results.heuristicBenchmark.secondsPerNode += diff.count();

    layerResultsIt->generatedNodes = nextNodeId;
    results.heuristicBenchmark.generatedNodes += layerResultsIt->generatedNodes;

    if (layerResultsIt->expandedNodes > 0) {
      layerResultsIt->secondsPerNode =
          diff.count() / static_cast<double>(layerResultsIt->expandedNodes);
      layerResultsIt->averageBranchingFactor =
          static_cast<double>(layerResultsIt->generatedNodes - 1) /
          static_cast<double>(layerResultsIt->expandedNodes);
    }

    layerResultsIt->effectiveBranchingFactor = computeEffectiveBranchingRate(
        layerResultsIt->expandedNodes + 1, result.depth);
  }

  if (config.dataLoggingEnabled()) {
    const auto [ops, singles, pairs] = layerForLog();
    dataLogger->logFinalizeLayer(layer, ops, singles, pairs, qubits, result.id,
                                 result.costFixed, result.costHeur,
                                 result.lookaheadPenalty, result.qubits,
                                 result.swaps, result.depth);
  }

  // clear nodes
  while (!nodes.empty()) {
    nodes.pop();
  }

  return result;
}

void HeuristicMapper::expandNode(Node& node, const std::size_t layer) {
  const auto& considered = getConsideredQubits(layer);
  for (const auto& [edge, cost] : routingSwaps) {
    const auto first = node.qubits.at(edge.first);
    const auto second = node.qubits.at(edge.second);
    if ((first != DEFAULT_POSITION &&
         considered.contains(static_cast<std::uint16_t>(first))) ||
        (second != DEFAULT_POSITION &&
         considered.contains(static_cast<std::uint16_t>(second)))) {
      expandNodeAddOneSwap(edge, node, layer, cost);
    }
  }
}

void HeuristicMapper::expandNodeAddOneSwap(const Edge& swap, const Node& node,
                                           const std::size_t layer,
                                           const double swapCost) {
  Node newNode(nextNodeId++, node.id, node.qubits, node.locations, node.swaps,
               node.validMappedTwoQubitGates, node.costFixed,
               node.costFixedReversals, node.depth + 1, node.sharedSwaps);
  newNode.swapFidelityCost = node.swapFidelityCost;
  applySWAP(swap, layer, newNode, swapCost);

  if (std::isfinite(newNode.getTotalCost())) {
    nodes.push(newNode);
  }
  if (results.config.dataLoggingEnabled()) {
    dataLogger->logSearchNode(layer, newNode.id, newNode.parent,
                              newNode.costFixed + newNode.costFixedReversals,
                              newNode.costHeur, newNode.lookaheadPenalty,
                              newNode.qubits, newNode.validMapping,
                              newNode.swaps, newNode.depth);
  }
}

void HeuristicMapper::recalculateFixedCost(std::size_t layer, Node& node) {
  node.validMappedTwoQubitGates.clear();
  for (const auto& [edge, mult] : layers.at(layer).interactions) {
    const auto [q1, q2] = edge;
    const auto physQ1 = static_cast<std::uint16_t>(node.locations.at(q1));
    const auto physQ2 = static_cast<std::uint16_t>(node.locations.at(q2));

    if (isPairMapped(*architecture, {physQ1, physQ2}, mult)) {
      // validly mapped
      node.validMappedTwoQubitGates.emplace(q1, q2);
    }
  }

  recalculateFixedCostReversals(layer, node);
}

void HeuristicMapper::recalculateFixedCostReversals(std::size_t layer,
                                                    Node& node) {
  node.costFixedReversals = 0.;
  if (architecture->bidirectional() || fidelityAwareHeur ||
      node.validMappedTwoQubitGates.size() !=
          layers.at(layer).interactions.size()) {
    // costFixedReversals should only be non-zero in goal nodes for
    // non-fidelity-aware heuristics and if there are unidirectional
    // edges in the architecture
    return;
  }

  // only consider reversal costs as fixed in goal nodes
  for (const auto& [edge, mult] : layers.at(layer).interactions) {
    const auto [q1, q2] = edge;
    const auto physQ1 = static_cast<std::uint16_t>(node.locations.at(q1));
    const auto physQ2 = static_cast<std::uint16_t>(node.locations.at(q2));
    node.costFixedReversals +=
        reversalCost(*architecture, {physQ1, physQ2}, mult);
  }
}

void HeuristicMapper::applySWAP(const Edge& swap, const std::size_t layer,
                                Node& node, const double swapCost) {
  assert(architecture->isEdgeConnected(swap, false));
  const auto q1 = node.qubits.at(swap.first);
  const auto q2 = node.qubits.at(swap.second);

  if (!fidelityAwareHeur) {
    updateSharedSwaps(swap, layer, node);
  }

  node.qubits.at(swap.first) = q2;
  node.qubits.at(swap.second) = q1;
  if (q1 != DEFAULT_POSITION) {
    node.locations.at(static_cast<std::size_t>(q1)) =
        static_cast<std::int16_t>(swap.second);
  }
  if (q2 != DEFAULT_POSITION) {
    node.locations.at(static_cast<std::size_t>(q2)) =
        static_cast<std::int16_t>(swap.first);
  }
  node.swaps.emplace_back(swap.first, swap.second, qc::SWAP);

  if (fidelityAwareHeur) {
    node.swapFidelityCost += swapCost;
    updateFidelityCosts(node);
    return;
  }

  // Check whether the SWAP created or destroyed any legal placements.
  for (const auto& [edge, counts] : layers.at(layer).interactions) {
    const auto [q3, q4] = edge;
    if (std::cmp_equal(q3, q1) || std::cmp_equal(q3, q2) ||
        std::cmp_equal(q4, q1) || std::cmp_equal(q4, q2)) {
      const auto physQ3 = static_cast<std::uint16_t>(node.locations.at(q3));
      const auto physQ4 = static_cast<std::uint16_t>(node.locations.at(q4));
      if (isPairMapped(*architecture, {physQ3, physQ4}, counts)) {
        node.validMappedTwoQubitGates.emplace(edge);
      } else {
        node.validMappedTwoQubitGates.erase(edge);
      }
    }
  }

  node.costFixed += swapCost;
  recalculateFixedCostReversals(layer, node);
  updateHeuristicCost(layer, node);
  if (results.config.lookaheadHeuristic != LookaheadHeuristic::None) {
    updateLookaheadPenalty(layer, node);
  }
}

void HeuristicMapper::updateSharedSwaps(const Edge& swap, std::size_t layer,
                                        Node& node) {
  const auto& consideredQubits = getConsideredQubits(layer);
  const auto& twoQubitGateMultiplicity = layers.at(layer).interactions;

  const auto q1 = node.qubits.at(swap.first);
  const auto q2 = node.qubits.at(swap.second);
  if (q1 == -1 || q2 == -1 ||
      !consideredQubits.contains(static_cast<std::uint16_t>(q1)) ||
      !consideredQubits.contains(static_cast<std::uint16_t>(q2))) {
    // the given swap can only be a shared swap if both qubits are active in
    // the current layer
    return;
  }

  Edge logEdge1 = {q1, q1};
  Edge logEdge2 = {q2, q2};
  for (const auto& [edge, multiplicity] : twoQubitGateMultiplicity) {
    if (std::cmp_equal(edge.first, q1)) {
      logEdge1.second = edge.second;
    } else if (std::cmp_equal(edge.second, q1)) {
      logEdge1.second = edge.first;
    }
    if (std::cmp_equal(edge.first, q2)) {
      logEdge2.second = edge.second;
    } else if (std::cmp_equal(edge.second, q2)) {
      logEdge2.second = edge.first;
    }
  }
  if ( // if both swapped qubits are acted on by a 2q gate
      std::cmp_not_equal(logEdge1.second, q1) &&
      std::cmp_not_equal(logEdge2.second, q2) &&
      // if it is not the same 2q gate acting on both qubits
      std::cmp_not_equal(logEdge1.second, q2)) {
    auto physQ3 =
        static_cast<std::uint16_t>(node.locations.at(logEdge1.second));
    auto physQ4 =
        static_cast<std::uint16_t>(node.locations.at(logEdge2.second));

    double logEdge1DistanceBefore = 0.;
    double logEdge1DistanceNew = 0.;
    double logEdge2DistanceBefore = 0.;
    double logEdge2DistanceNew = 0.;
    logEdge1DistanceBefore =
        std::min(architecture->distance(swap.first, physQ3, false),
                 architecture->distance(physQ3, swap.first, false));
    logEdge1DistanceNew =
        std::min(architecture->distance(swap.second, physQ3, false),
                 architecture->distance(physQ3, swap.second, false));
    logEdge2DistanceBefore =
        std::min(architecture->distance(swap.second, physQ4, false),
                 architecture->distance(physQ4, swap.second, false));
    logEdge2DistanceNew =
        std::min(architecture->distance(swap.first, physQ4, false),
                 architecture->distance(physQ4, swap.first, false));
    if (logEdge1DistanceNew < logEdge1DistanceBefore &&
        logEdge2DistanceNew < logEdge2DistanceBefore) {
      ++node.sharedSwaps;
    }
  }
}

void HeuristicMapper::updateHeuristicCost(std::size_t layer, Node& node) {
  // the mapping is valid, only if all qubit pairs are mapped next to each other
  node.validMapping = (node.validMappedTwoQubitGates.size() ==
                       layers.at(layer).interactions.size());

  switch (results.config.heuristic) {
  case Heuristic::GateCountMaxDistance:
    node.costHeur = heuristicGateCountMaxDistance(layer, node);
    break;
  case Heuristic::GateCountSumDistance:
    node.costHeur = heuristicGateCountSumDistance(layer, node);
    break;
  case Heuristic::GateCountSumDistanceMinusSharedSwaps:
    node.costHeur = heuristicGateCountSumDistanceMinusSharedSwaps(layer, node);
    break;
  case Heuristic::GateCountMaxDistanceOrSumDistanceMinusSharedSwaps:
    node.costHeur =
        heuristicGateCountMaxDistanceOrSumDistanceMinusSharedSwaps(layer, node);
    break;
  default:
    throw QMAPException("Unknown heuristic.");
  }
}

double HeuristicMapper::heuristicGateCountMaxDistance(std::size_t layer,
                                                      Node& node) {
  if (node.validMapping) {
    return 0.;
  }
  double costHeur = 0.;

  for (const auto& [edge, counts] : layers.at(layer).interactions) {
    const auto [q1, q2] = edge;
    const Edge physical{static_cast<std::uint16_t>(node.locations.at(q1)),
                        static_cast<std::uint16_t>(node.locations.at(q2))};
    const double cost = node.validMappedTwoQubitGates.contains(edge)
                            ? reversalCost(*architecture, physical, counts)
                            : pairDistance(*architecture, physical, counts);
    costHeur = std::max(costHeur, cost);
  }

  return costHeur;
}

double HeuristicMapper::heuristicGateCountSumDistance(std::size_t layer,
                                                      Node& node) {
  if (node.validMapping) {
    return 0.;
  }
  double costHeur = 0.;

  for (const auto& [edge, counts] : layers.at(layer).interactions) {
    const auto [q1, q2] = edge;
    const Edge physical{static_cast<std::uint16_t>(node.locations.at(q1)),
                        static_cast<std::uint16_t>(node.locations.at(q2))};
    costHeur += node.validMappedTwoQubitGates.contains(edge)
                    ? reversalCost(*architecture, physical, counts)
                    : pairDistance(*architecture, physical, counts);
  }

  return costHeur;
}

double HeuristicMapper::heuristicGateCountSumDistanceMinusSharedSwaps(
    std::size_t layer, Node& node) {
  if (node.validMapping) {
    return 0.;
  }
  const auto& twoQubitGateMultiplicity = layers.at(layer).interactions;
  double costHeur = 0.;
  double costReversals = 0.;
  std::vector<std::size_t> nSwaps{};
  nSwaps.reserve(twoQubitGateMultiplicity.size());

  for (const auto& [edge, counts] : twoQubitGateMultiplicity) {
    const auto [q1, q2] = edge;
    const auto physQ1 = static_cast<std::uint16_t>(node.locations.at(q1));
    const auto physQ2 = static_cast<std::uint16_t>(node.locations.at(q2));

    if (architecture->unidirectional()) {
      // At least one direction needs reversal on a one-way coupling.
      costReversals += std::min(counts.forward.cnots, counts.backward.cnots) *
                       COST_DIRECTION_REVERSE;
    }

    if (node.validMappedTwoQubitGates.contains(edge)) {
      continue;
    }

    const double swapCost = architecture->distance(physQ1, physQ2, false);
    costHeur += swapCost;

    // infer maximum number of swaps in this distance
    if (architecture->unidirectional()) {
      nSwaps.emplace_back(
          static_cast<std::size_t>(swapCost / COST_UNIDIRECTIONAL_SWAP));
    } else {
      nSwaps.emplace_back(
          static_cast<std::size_t>(swapCost / COST_BIDIRECTIONAL_SWAP));
    }
  }

  // sort number of swaps in descending order
  std::ranges::sort(nSwaps, std::greater<>());

  // infer maximum number of shared swaps
  std::size_t maxSharedSwaps = 0;
  for (std::size_t i = 0; i + 1 < nSwaps.size(); ++i) {
    std::size_t maxSharedSwapsEdge =
        0; // maximum number of shared swaps for this edge
    for (std::size_t j = i + 1;
         j < nSwaps.size() && maxSharedSwapsEdge < nSwaps[i]; ++j) {
      if (nSwaps[j] > 0) {
        ++maxSharedSwapsEdge;
        --nSwaps[j];
      }
    }
    maxSharedSwaps += maxSharedSwapsEdge;
  }
  if (node.sharedSwaps < maxSharedSwaps) {
    maxSharedSwaps -= node.sharedSwaps;
  } else {
    maxSharedSwaps = 0;
  }

  double sharedSwapCostReduction = 0;
  if (architecture->bidirectional()) {
    sharedSwapCostReduction =
        static_cast<double>(maxSharedSwaps * COST_BIDIRECTIONAL_SWAP);
  } else {
    sharedSwapCostReduction =
        static_cast<double>(maxSharedSwaps * COST_UNIDIRECTIONAL_SWAP);
  }

  return std::max(0., costHeur - sharedSwapCostReduction) + costReversals;
}

double
HeuristicMapper::heuristicGateCountMaxDistanceOrSumDistanceMinusSharedSwaps(
    std::size_t layer, Node& node) {
  return std::max(heuristicGateCountMaxDistance(layer, node),
                  heuristicGateCountSumDistanceMinusSharedSwaps(layer, node));
}

void HeuristicMapper::updateLookaheadPenalty(const std::size_t layer,
                                             HeuristicMapper::Node& node) {
  const auto& config = results.config;
  node.lookaheadPenalty = 0.;
  auto nextLayer = getNextLayer(layer);
  double factor = config.firstLookaheadFactor;

  for (std::size_t i = 0; i < config.nrLookaheads; ++i) {
    if (nextLayer == std::numeric_limits<std::size_t>::max()) {
      break;
    }

    double penalty = 0.;
    switch (config.lookaheadHeuristic) {
    case LookaheadHeuristic::GateCountMaxDistance:
      penalty = lookaheadGateCountDistance(nextLayer, node, false);
      break;
    case LookaheadHeuristic::GateCountSumDistance:
      penalty = lookaheadGateCountDistance(nextLayer, node, true);
      break;
    default:
      break;
    }

    node.lookaheadPenalty += factor * penalty;
    factor *= config.lookaheadFactor;
    nextLayer = getNextLayer(nextLayer); // TODO: consider single qubits here
                                         // for better fidelity lookahead
  }
}

double HeuristicMapper::lookaheadGateCountDistance(const std::size_t layer,
                                                   const Node& node,
                                                   const bool sum) {
  double penalty = 0.;
  for (const auto& [edge, counts] : layers.at(layer).interactions) {
    const auto [q1, q2] = edge;
    const auto first = node.locations.at(q1);
    const auto second = node.locations.at(q2);
    if (first == DEFAULT_POSITION && second == DEFAULT_POSITION) {
      continue;
    }

    double cost = std::numeric_limits<double>::max();
    if (first == DEFAULT_POSITION || second == DEFAULT_POSITION) {
      for (std::uint16_t physical = 0; physical < architecture->getNqubits();
           ++physical) {
        if (node.qubits.at(physical) == DEFAULT_POSITION) {
          const Edge candidate{
              first == DEFAULT_POSITION ? physical
                                        : static_cast<std::uint16_t>(first),
              second == DEFAULT_POSITION ? physical
                                         : static_cast<std::uint16_t>(second)};
          cost = std::min(
              cost, lookaheadPairDistance(*architecture, candidate, counts));
        }
      }
    } else {
      cost = lookaheadPairDistance(*architecture,
                                   {static_cast<std::uint16_t>(first),
                                    static_cast<std::uint16_t>(second)},
                                   counts);
    }

    penalty = sum ? penalty + cost : std::max(penalty, cost);
  }
  return penalty;
}
