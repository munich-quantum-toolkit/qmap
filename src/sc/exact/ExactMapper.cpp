/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "sc/exact/ExactMapper.hpp"

#include "Logic.hpp"
#include "LogicTerm.hpp"
#include "ir/Definitions.hpp"
#include "logicblocks/Encodings.hpp"
#include "logicblocks/LogicBlock.hpp"
#include "logicblocks/Model.hpp"
#include "logicblocks/util_logicblock.hpp"
#include "sc/Architecture.hpp"
#include "sc/configuration/CommanderGrouping.hpp"
#include "sc/configuration/Configuration.hpp"
#include "sc/configuration/Encoding.hpp"
#include "sc/configuration/SwapReduction.hpp"
#include "sc/utils.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <ranges>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

void ExactMapper::map(const Configuration& config) {
  results.config = config;

  const std::chrono::high_resolution_clock::time_point start =
      std::chrono::high_resolution_clock::now();
  initResults();

  // 0) perform pre-mapping optimizations
  preMappingOptimizations(config);

  // 1) create layers according to different criteria
  createLayers();
  if (config.verbose) {
    printLayering(std::cout);
  }
  for (std::size_t k = 0; k < layers.size(); ++k) {
    if (!layers[k].interactions.empty()) {
      reducedLayerIndices.emplace_back(k);
    }
  }

  // quickly terminate if the circuit only contains single-qubit gates
  if (reducedLayerIndices.empty()) {
    qcMapped = qc;
    postMappingOptimizations(config);
    results.output.gates = 0U;
    countGates(qcMapped, results.output);
    finalizeMappedCircuit();

    results.output.layers = results.input.layers;
    results.time = static_cast<std::chrono::duration<double>>(
                       std::chrono::high_resolution_clock::now() - start)
                       .count();
    results.timeout = false;
    return;
  }

  // Only qubits used by gates need assignment variables in the solver.
  QubitChoice logicalQubits;
  for (const auto& layer : layers) {
    logicalQubits.insert(layer.activeQubits.begin(), layer.activeQubits.end());
  }

  // 2a) If only a subgraph should be considered, reduce the architecture.
  if (!config.subgraph.empty()) {
    const auto subgraphQubits = config.subgraph.size();
    if (subgraphQubits < logicalQubits.size()) {
      std::cerr << "The subgraph must contain at least as many qubits as the "
                   "circuit has active qubits.\n";
      return;
    }

    CouplingMap reducedCouplingMap{};
    architecture->getReducedCouplingMap(config.subgraph, reducedCouplingMap);

    // check if the subgraph is connected
    if (!Architecture::isConnected(config.subgraph, reducedCouplingMap)) {
      std::cerr << "The subgraph is not connected.\n";
      return;
    }

    architecture->setCouplingMap(reducedCouplingMap);
    assumeIdealSingleQubits = architecture->hasCompleteCnotCalibration();
  }

  // 2b) If configured to use subsets, collect all k (=m over n) possibilities
  // to pick n qubits from m device qubits. Otherwise, consider all qubits.
  std::vector<std::uint16_t> qubitRange =
      Architecture::getQubitList(architecture->getCouplingMap());

  std::vector<QubitChoice> allPossibleQubitChoices{};
  if (config.useSubsets) {
    allPossibleQubitChoices = architecture->getAllConnectedSubsets(
        static_cast<std::uint16_t>(logicalQubits.size()));
  } else {
    allPossibleQubitChoices.emplace_back(qubitRange.begin(), qubitRange.end());
  }

  // 3) determine exact mapping for this qubit choice
  std::vector<Swaps> swaps(reducedLayerIndices.size(), Swaps{});
  std::vector<Swaps> mappingSwaps;
  for (const auto& choice : allPossibleQubitChoices) {
    auto maxLimit = config.useSubsets ? architecture->getCouplingLimit(choice)
                                      : architecture->getCouplingLimit();
    // Adjacent qubits may still need to exchange positions on a one-way edge.
    if (architecture->bidirectional() && maxLimit > 0) {
      --maxLimit;
    }
    std::size_t limit = config.swapLimit;
    if (config.swapReduction == SwapReduction::CouplingLimit) {
      limit = maxLimit;
    } else if (config.swapReduction == SwapReduction::Increasing) {
      limit = 0;
      if (config.swapLimit > 0) {
        maxLimit = std::min(maxLimit, config.swapLimit);
      }
    }

    std::size_t increment = 1;
    std::size_t timeout = 0U;
    do {
      if (config.swapReduction == SwapReduction::Increasing) {
        timeout += static_cast<std::size_t>(
            static_cast<double>(config.timeout) *
            (static_cast<double>(limit) * 0.5) /
            static_cast<double>(std::max<std::size_t>(1, maxLimit)));
        timeout = std::max(timeout, static_cast<std::size_t>(10000U));
        if (config.verbose) {
          std::cout << "Timeout: " << timeout
                    << "  Max-Timeout: " << config.timeout << '\n';
        }
      } else {
        timeout = config.timeout;
      }

      // reset swaps
      for (auto& layer : swaps) {
        layer.clear();
      }

      MappingResults choiceResults{};
      choiceResults.copyInput(results);
      choiceResults.config.swapLimit = limit;
      choiceResults.output.swaps = 0U;
      choiceResults.output.directionReverse = 0U;
      choiceResults.output.gates = std::numeric_limits<std::size_t>::max();

      // 4) reduce coupling map
      CouplingMap reducedCouplingMap = {};
      architecture->getReducedCouplingMap(choice, reducedCouplingMap);

      if (reducedCouplingMap.empty()) {
        break;
      }

      if (config.verbose) {
        std::cout << "-------- qubit choice: ";
        for (const auto q : choice) {
          std::cout << q << " ";
        }
        std::cout << "---------- ";
        if (config.swapReduction != SwapReduction::None) {
          std::cout << "SWAP limit: " << limit;
        }
        std::cout << "\n";
      }

      // 6) call actual mapping routine
      coreMappingRoutine(choice, logicalQubits, reducedCouplingMap,
                         choiceResults, swaps, limit, timeout);

      if (config.verbose) {
        if (!choiceResults.timeout) {
          std::cout << "Costs: " << choiceResults.output.swaps << " SWAP(s)";
          if (!architecture->bidirectional()) {
            std::cout << ", " << choiceResults.output.directionReverse
                      << " direction reverses";
          }
          std::cout << "\n";
        } else {
          std::cout << "Did not yield a result\n";
        }
      }

      // 7) Check if new optimum found
      if (!choiceResults.timeout &&
          choiceResults.output.gates < results.output.gates) {
        results = choiceResults;
        mappingSwaps = swaps;
      }
      if (config.swapReduction != SwapReduction::Increasing ||
          limit == maxLimit ||
          (!results.timeout && results.output.swaps == 0 &&
           results.output.directionReverse == 0)) {
        break;
      }
      limit = std::min(limit + increment, maxLimit);
      ++increment;
    } while (true);

    // stop if a perfect result has been found
    if (!results.timeout && results.output.swaps == 0U &&
        results.output.directionReverse == 0U) {
      break;
    }
  }

  // return in case no result has been found
  if (results.timeout) {
    return;
  }

  // 8) Write best result and statistics
  results.output.directionReverse = 0U;
  if (config.verbose) {
    for (std::size_t k = 0; k < mappingSwaps.size(); ++k) {
      std::cout << reducedLayerIndices[k] << ": ";
      for (const auto& [first, second] : mappingSwaps[k]) {
        std::cout << "(" << first << "<->" << second << ") ";
      }
      std::cout << '\n';
    }
  }

  for (const auto& [physical, logical] : mappingSwaps.front()) {
    locations.at(logical) = static_cast<std::int16_t>(physical);
    qubits.at(physical) = static_cast<std::int16_t>(logical);
    qcMapped.initialLayout[physical] = logical;
  }
  placeRemainingArchitectureQubits();
  if (config.verbose) {
    printQubits(std::cout);
    printLocations(std::cout);
  }

  std::size_t k = 0;
  for (std::size_t i = 0; i < layers.size(); ++i) {
    for (const auto& gate : layers[i].gates) {
      const auto first = qcMapped.size();
      appendMappedGate(gate,
                       static_cast<std::uint16_t>(locations.at(gate.first)));
      if (config.verbose) {
        for (auto j = first; j < qcMapped.size(); ++j) {
          qcMapped.at(j)->print(std::cout << i << ": ", qcMapped.getNqubits())
              << '\n';
        }
      }
    }

    if (k + 1 < mappingSwaps.size() && i == reducedLayerIndices[k]) {
      // Move to the placement for the next routed layer.
      for (const auto& [q0, q1] :
           std::ranges::reverse_view(mappingSwaps[++k])) {
        const auto logical0 = static_cast<qc::Qubit>(qubits.at(q0));
        const auto logical1 = static_cast<qc::Qubit>(qubits.at(q1));
        qcMapped.swap(q0, q1);
        std::swap(qubits.at(q0), qubits.at(q1));
        locations.at(logical0) = static_cast<std::int16_t>(q1);
        locations.at(logical1) = static_cast<std::int16_t>(q0);

        if (config.verbose) {
          printQubits(std::cout);
          printLocations(std::cout);
        }
      }
    }
  }

  // set output permutation
  qcMapped.outputPermutation.clear();
  for (qc::Qubit logical = 0; logical < qc.getNqubits(); ++logical) {
    const auto physical = static_cast<qc::Qubit>(locations.at(logical));
    qcMapped.outputPermutation[physical] = logical;
  }

  // 9) apply post mapping optimizations
  postMappingOptimizations(config);

  // 10) re-count gates
  results.output.singleQubitGates = 0U;
  results.output.cnots = 0U;
  results.output.gates = 0U;
  countGates(qcMapped, results.output);

  // 11) final post-processing
  finalizeMappedCircuit();

  const auto end = std::chrono::high_resolution_clock::now();
  const std::chrono::duration<double> diff = end - start;
  results.time = diff.count();
}

void ExactMapper::coreMappingRoutine(const QubitChoice& qubitChoice,
                                     const QubitChoice& logicalQubits,
                                     const CouplingMap& rcm,
                                     MappingResults& choiceResults,
                                     std::vector<Swaps>& swaps,
                                     const std::size_t limit,
                                     const std::size_t timeout) {
  const auto& config = results.config;
  using namespace logicbase;
  // LogicBlock
  bool success = false;
  logicutil::Params params;
  params.addParam("timeout", static_cast<std::uint32_t>(timeout));
  params.addParam("pb.compile_equality", true);
  params.addParam("pp.wcnf", true);
  params.addParam("maxres.hill_climb", true);
  params.addParam("maxres.pivot_on_correction_set", false);
  std::unique_ptr<LogicBlockOptimizer> lb =
      logicutil::getZ3LogicOptimizer(success, true, params);
  if (!success) {
    throw QMAPException("Could not initialize Z3 logic block optimizer");
  }

  std::vector<std::uint16_t> sites(qubitChoice.begin(), qubitChoice.end());
  std::vector<std::size_t> physicalQubitIndex(architecture->getNqubits());
  for (std::size_t i = 0; i < sites.size(); ++i) {
    physicalQubitIndex.at(sites[i]) = i;
  }

  // Each transition between layers can use the same set of permutations.
  struct Transition {
    std::vector<std::uint16_t> sites;
    std::uint64_t swaps;
    std::size_t rank;
  };
  std::vector<Transition> transitions;
  if (reducedLayerIndices.size() > 1) {
    auto permutation = sites;
    std::size_t rank = 0;
    do {
      const auto swapsNeeded = architecture->minimumNumberOfSwaps(
          permutation,
          config.swapLimitsEnabled() ? static_cast<std::int64_t>(limit) : -1);
      if (!config.swapLimitsEnabled() || swapsNeeded <= limit) {
        transitions.push_back(
            {.sites = permutation, .swaps = swapsNeeded, .rank = rank});
      }
      ++rank;
    } while (std::ranges::next_permutation(permutation).found);
  }

  //////////////////////////////////////////
  /// 	Boolean Variable Definitions	//
  //////////////////////////////////////////

  // Store assignment variables by layer, logical qubit, and physical index.
  // Idle logical qubits have empty rows.
  LogicMatrix3D x{};
  std::stringstream xName{};
  for (std::size_t k = 0; k < reducedLayerIndices.size(); ++k) {
    x.emplace_back(qc.getNqubits());
    for (const auto qubit : qubitChoice) {
      for (const auto q : logicalQubits) {
        xName.str("");
        xName << "x_" << k << '_' << qubit << '_' << q;
        x.back()[q].emplace_back(lb->makeVariable(xName.str(), CType::BOOL));
      }
    }
  }

  // y[k][p] selects transition p between routed layers k and k + 1.
  LogicMatrix y(reducedLayerIndices.size() - 1);
  for (std::size_t k = 0; k < y.size(); ++k) {
    for (const auto& transition : transitions) {
      const auto name =
          "y_" + std::to_string(k + 1) + "_" + std::to_string(transition.rank);
      y[k].emplace_back(lb->makeVariable(name, CType::BOOL));
    }
  }

  // Each logical qubit occupies one site. Each site holds at most one logical
  // qubit.
  const auto cardinality = [&](const std::vector<LogicTerm>& vars,
                               bool exactlyOne) {
    if (config.encoding == Encoding::Naive) {
      auto count = LogicTerm(0);
      for (const auto& var : vars) {
        count = count + LogicTerm::ite(var, LogicTerm(1), LogicTerm(0));
      }
      return exactlyOne ? count == LogicTerm(1) : count <= LogicTerm(1);
    }
    if (!exactlyOne && config.encoding == Encoding::Bimander) {
      return encodings::atMostOneBiMander(vars, lb.get());
    }
    std::size_t groupSize = 0;
    switch (config.commanderGrouping) {
    case CommanderGrouping::Fixed2:
      groupSize = 2;
      break;
    case CommanderGrouping::Fixed3:
      groupSize = 3;
      break;
    case CommanderGrouping::Logarithm:
      groupSize = std::max<std::size_t>(
          2, static_cast<std::size_t>(std::log(vars.size())));
      break;
    case CommanderGrouping::Halves:
      groupSize = vars.size() / 2;
      break;
    }
    const auto groups = encodings::groupVars(vars, groupSize);
    return exactlyOne ? encodings::exactlyOneCmdr(groups, LogicTerm::noneTerm(),
                                                  lb.get())
                      : encodings::atMostOneCmdr(groups, LogicTerm::noneTerm(),
                                                 lb.get());
  };
  for (const auto& assignment : x) {
    for (std::size_t i = 0; i < sites.size(); ++i) {
      std::vector<LogicTerm> column;
      column.reserve(logicalQubits.size());
      for (const auto q : logicalQubits) {
        column.emplace_back(assignment[q][i]);
      }
      lb->assertFormula(cardinality(column, false));
    }
    for (const auto q : logicalQubits) {
      lb->assertFormula(cardinality(assignment[q], true));
    }
  }

  //////////////////////////////////////////
  ///		Coupling Constraints			//
  //////////////////////////////////////////
  for (std::size_t k = 0; k < reducedLayerIndices.size(); ++k) {
    const auto supported = [&](const Edge& logical) {
      const auto& first = x[k][logical.first];
      const auto& second = x[k][logical.second];
      auto allowed = LogicTerm(false);
      for (const auto& [from, to] : rcm) {
        allowed = allowed || (first[physicalQubitIndex[from]] &&
                              second[physicalQubitIndex[to]]);
      }
      return allowed;
    };
    for (const auto& [pair, counts] :
         layers[reducedLayerIndices[k]].interactions) {
      const auto forward = supported(pair);
      if (architecture->bidirectional()) {
        lb->assertFormula(forward);
        continue;
      }
      const auto backward = supported({pair.second, pair.first});
      if (counts.forward.requiresOrder) {
        lb->assertFormula(forward);
      }
      if (counts.backward.requiresOrder) {
        lb->assertFormula(backward);
      }
      if (!counts.forward.requiresOrder && !counts.backward.requiresOrder) {
        lb->assertFormula(forward || backward);
      }
      if (counts.forward.cnots != 0) {
        lb->weightedTerm(!forward, static_cast<double>(counts.forward.cnots) *
                                       GATES_OF_DIRECTION_REVERSE);
      }
      if (counts.backward.cnots != 0) {
        lb->weightedTerm(!backward, static_cast<double>(counts.backward.cnots) *
                                        GATES_OF_DIRECTION_REVERSE);
      }
    }
  }

  // Apply one permutation to the placement between successive layers.
  const auto swapGates = architecture->bidirectional()
                             ? GATES_OF_BIDIRECTIONAL_SWAP
                             : GATES_OF_UNIDIRECTIONAL_SWAP;
  for (std::size_t k = 0; k < y.size(); ++k) {
    for (std::size_t p = 0; p < transitions.size(); ++p) {
      const auto& transition = transitions[p];
      auto equal = LogicTerm(true);
      for (std::size_t from = 0; from < sites.size(); ++from) {
        const auto to = physicalQubitIndex[transition.sites[from]];
        for (const auto q : logicalQubits) {
          equal = equal && (x[k][q][from] == x[k + 1][q][to]);
        }
      }
      lb->assertFormula(LogicTerm::implies(y[k][p], equal));
      lb->weightedTerm(y[k][p],
                       static_cast<double>(transition.swaps * swapGates));
    }
    lb->assertFormula(cardinality(y[k], true));
  }
  lb->makeMinimize();

  if (config.includeWCNF) {
    choiceResults.wcnf = lb->dumpInternalSolver();
  }

  //////////////////////////////////////////
  /// 	Solving							//
  //////////////////////////////////////////
  lb->produceInstance();
  const auto res = lb->solve();
  if (Result::SAT == res) {
    auto* const m = lb->getModel();
    choiceResults.timeout = false;

    // quickly determine cost
    choiceResults.output.singleQubitGates =
        choiceResults.input.singleQubitGates;
    choiceResults.output.cnots = choiceResults.input.cnots;
    choiceResults.output.gates = choiceResults.input.gates;
    assert(choiceResults.output.swaps == 0U);
    assert(choiceResults.output.directionReverse == 0U);
    // Read the initial placement and the transitions selected by the solver.
    std::vector<std::uint16_t> placement(qc.getNqubits());
    for (const auto logical : logicalQubits) {
      for (const auto physical : sites) {
        if (m->getBoolValue(x.front()[logical][physicalQubitIndex[physical]],
                            lb.get())) {
          placement[logical] = physical;
          swaps.front().emplace_back(physical, logical);
          break;
        }
      }
    }
    for (std::size_t k = 0; k < reducedLayerIndices.size(); ++k) {
      if (k != 0) {
        const auto& choices = y[k - 1];
        const auto chosen =
            std::ranges::find_if(choices, [&](const LogicTerm& term) {
              return m->getBoolValue(term, lb.get());
            });
        assert(chosen != choices.end());
        auto& permutation =
            transitions[static_cast<std::size_t>(chosen - choices.begin())]
                .sites;
        architecture->minimumNumberOfSwaps(permutation, swaps[k]);
        choiceResults.output.swaps += swaps[k].size();
        for (const auto q : logicalQubits) {
          placement[q] = permutation[physicalQubitIndex[placement[q]]];
        }
      }
      if (!architecture->bidirectional()) {
        for (const auto& [pair, counts] :
             layers[reducedLayerIndices[k]].interactions) {
          const auto first = placement[pair.first];
          const auto second = placement[pair.second];
          if (!rcm.contains({first, second})) {
            choiceResults.output.directionReverse += counts.forward.cnots;
          }
          if (!rcm.contains({second, first})) {
            choiceResults.output.directionReverse += counts.backward.cnots;
          }
        }
      }
    }
    choiceResults.output.gates +=
        choiceResults.output.swaps * swapGates +
        choiceResults.output.directionReverse * GATES_OF_DIRECTION_REVERSE;
  }
  (*lb).reset();
}
