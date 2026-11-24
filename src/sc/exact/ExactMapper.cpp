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
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

void ExactMapper::map(const Configuration& settings) {
  results.config = settings;
  const auto& config = settings;

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
  mappingSwaps.reserve(reducedLayerIndices.size());
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
            static_cast<double>(settings.timeout) *
            (static_cast<double>(limit) * 0.5) /
            static_cast<double>(std::max<std::size_t>(1, maxLimit)));
        timeout = std::max(timeout, static_cast<std::size_t>(10000U));
        if (settings.verbose) {
          std::cout << "Timeout: " << timeout
                    << "  Max-Timeout: " << settings.timeout << '\n';
        }
      } else {
        timeout = settings.timeout;
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
  auto layerIterator = reducedLayerIndices.begin();
  auto swapsIterator = mappingSwaps.begin();

  if (settings.verbose) {
    auto it = reducedLayerIndices.begin();
    for (const auto& layer : mappingSwaps) {
      std::cout << *it << ": ";
      for (const auto& swap : layer) {
        std::cout << "(" << swap.first << "<->" << swap.second << ") ";
      }
      ++it;
      std::cout << '\n';
    }
  }

  for (std::size_t i = 0U; i < layers.size(); ++i) {
    if (i == 0U) {
      for (const auto& [physical, logical] : *swapsIterator) {
        locations.at(logical) = static_cast<std::int16_t>(physical);
        qubits.at(physical) = static_cast<std::int16_t>(logical);
        qcMapped.initialLayout[physical] = logical;
      }
      placeRemainingArchitectureQubits();

      if (settings.verbose) {
        std::cout << "Qubits: ";
        for (auto q = 0U; q < architecture->getNqubits(); ++q) {
          std::cout << qubits.at(q) << " ";
        }
        std::cout << " Locations: ";
        for (std::size_t q = 0; q < qc.getNqubits(); ++q) {
          std::cout << locations.at(q) << " ";
        }
        std::cout << "\n";
      }
      ++swapsIterator;
    }

    // apply all gates of layer
    for (const auto& gate : layers.at(i).gates) {
      const auto first = qcMapped.size();
      appendMappedGate(gate,
                       static_cast<std::uint16_t>(locations.at(gate.first)));
      if (settings.verbose) {
        for (auto j = first; j < qcMapped.size(); ++j) {
          qcMapped.at(j)->print(std::cout << i << ": ", qcMapped.getNqubits())
              << '\n';
        }
      }
    }

    if (!mappingSwaps.empty() && swapsIterator != mappingSwaps.end() &&
        layerIterator != reducedLayerIndices.end() && i == *layerIterator) {
      // apply swaps before layer
      for (auto [q0, q1] : std::ranges::reverse_view(*swapsIterator)) {
        const auto logical0 = static_cast<qc::Qubit>(qubits.at(q0));
        const auto logical1 = static_cast<qc::Qubit>(qubits.at(q1));
        qcMapped.swap(q0, q1);
        std::swap(qubits.at(q0), qubits.at(q1));
        locations.at(logical0) = static_cast<std::int16_t>(q1);
        locations.at(logical1) = static_cast<std::int16_t>(q0);

        if (settings.verbose) {
          std::cout << "Qubits: ";
          for (auto q = 0U; q < architecture->getNqubits(); ++q) {
            std::cout << qubits.at(q) << " ";
          }
          std::cout << " Locations: ";
          for (std::size_t q = 0; q < qc.getNqubits(); ++q) {
            std::cout << locations.at(q) << " ";
          }
          std::cout << "\n";
        }
      }

      ++swapsIterator;
      ++layerIterator;
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

void ExactMapper::coreMappingRoutine(
    const QubitChoice& qubitChoice, const QubitChoice& logicalQubits,
    const CouplingMap& rcm, MappingResults& choiceResults,
    std::vector<std::vector<std::pair<std::uint16_t, std::uint16_t>>>& swaps,
    const std::size_t limit, const std::size_t timeout) {
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

  std::vector<std::uint16_t> pi(qubitChoice.begin(), qubitChoice.end());
  std::uint64_t piCount{};
  std::uint64_t internalPiCount{};
  std::unordered_set<std::uint64_t> skippedPi{};
  std::unordered_map<std::uint16_t, std::uint16_t> physicalQubitIndex{};
  std::uint16_t qIdx = 0;
  for (const auto& qubit : qubitChoice) {
    physicalQubitIndex[qubit] = qIdx;
    ++qIdx;
  }

  //////////////////////////////////////////
  /// 	Check necessary permutations	//
  //////////////////////////////////////////
  if (config.swapLimitsEnabled()) {
    do {
      auto picost = architecture->minimumNumberOfSwaps(
          pi, static_cast<std::int64_t>(limit));
      if (picost > limit) {
        skippedPi.insert(piCount);
      }
      ++piCount;
    } while (std::ranges::next_permutation(pi).found);
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

  /*
permutation variables y_k_pi
k	before layer k
pi	arbitrary permutation of the m qubits
number of variables: (|L|-1) * m!
*/
  LogicMatrix y{};
  std::stringstream yName{};
  for (std::size_t k = 1; k < reducedLayerIndices.size(); ++k) {
    y.emplace_back();
    piCount = 0;
    do {
      if (!skippedPi.contains(piCount) || !config.swapLimitsEnabled()) {
        yName.str("");
        yName << "y_" << k << '_' << piCount;
        y.back().emplace_back(lb->makeVariable(yName.str(), CType::BOOL));
      }
      ++piCount;
    } while (std::ranges::next_permutation(pi).found);
  }

  //////////////////////////////////////////
  /// 	Consistency Constraints			//
  //////////////////////////////////////////
  if (config.encoding == Encoding::Naive) {
    for (std::size_t k = 0; k < reducedLayerIndices.size(); ++k) {
      for (std::size_t i = 0; i < qubitChoice.size(); ++i) {
        auto rowConsistency = LogicTerm(0);
        for (const auto j : logicalQubits) {
          rowConsistency =
              rowConsistency +
              LogicTerm::ite(x[k][j][i], LogicTerm(1), LogicTerm(0));
        }
        lb->assertFormula(rowConsistency <= LogicTerm(1));
      }

      for (const auto j : logicalQubits) {
        auto colConsistency = LogicTerm(0);
        for (std::size_t i = 0; i < qubitChoice.size(); ++i) {
          colConsistency =
              colConsistency +
              LogicTerm::ite(x[k][j][i], LogicTerm(1), LogicTerm(0));
        }
        lb->assertFormula(colConsistency == LogicTerm(1));
      }
    }
  } else if (config.encoding == Encoding::Commander) {
    for (std::size_t k = 0; k < reducedLayerIndices.size(); ++k) {
      for (std::size_t i = 0; i < qubitChoice.size(); ++i) {
        std::vector<LogicTerm> varIDs;
        varIDs.reserve(logicalQubits.size());
        for (const auto j : logicalQubits) {
          varIDs.emplace_back(x[k][j][i]);
        }
        if (config.commanderGrouping == CommanderGrouping::Fixed2) {
          lb->assertFormula(
              encodings::atMostOneCmdr(encodings::groupVars(varIDs, 2),
                                       LogicTerm::noneTerm(), lb.get()));
        } else if (config.commanderGrouping == CommanderGrouping::Fixed3) {
          lb->assertFormula(
              encodings::atMostOneCmdr(encodings::groupVars(varIDs, 3),
                                       LogicTerm::noneTerm(), lb.get()));
        } else if (config.commanderGrouping == CommanderGrouping::Logarithm) {
          lb->assertFormula(encodings::atMostOneCmdr(
              encodings::groupVars(
                  varIDs, static_cast<std::size_t>(std::log(varIDs.size()))),
              LogicTerm::noneTerm(), lb.get()));
        } else if (config.commanderGrouping == CommanderGrouping::Halves) {
          lb->assertFormula(encodings::atMostOneCmdr(
              encodings::groupVars(varIDs, varIDs.size() / 2),
              LogicTerm::noneTerm(), lb.get()));
        }
      }

      for (const auto j : logicalQubits) {
        const auto& varIDs = x[k][j];

        if (config.commanderGrouping == CommanderGrouping::Fixed2) {
          lb->assertFormula(
              encodings::exactlyOneCmdr(encodings::groupVars(varIDs, 2),
                                        LogicTerm::noneTerm(), lb.get()));
        } else if (config.commanderGrouping == CommanderGrouping::Fixed3) {
          lb->assertFormula(
              encodings::exactlyOneCmdr(encodings::groupVars(varIDs, 3),
                                        LogicTerm::noneTerm(), lb.get()));
        } else if (config.commanderGrouping == CommanderGrouping::Logarithm) {
          lb->assertFormula(encodings::exactlyOneCmdr(
              encodings::groupVars(
                  varIDs, static_cast<std::size_t>(std::log(varIDs.size()))),
              LogicTerm::noneTerm(), lb.get()));
        } else if (config.commanderGrouping == CommanderGrouping::Halves) {
          lb->assertFormula(encodings::exactlyOneCmdr(
              encodings::groupVars(varIDs, varIDs.size() / 2),
              LogicTerm::noneTerm(), lb.get()));
        }
      }
    }
  } else if (config.encoding == Encoding::Bimander) {
    for (std::size_t k = 0; k < reducedLayerIndices.size(); ++k) {
      for (std::size_t i = 0; i < qubitChoice.size(); ++i) {
        std::vector<LogicTerm> vars;
        vars.reserve(logicalQubits.size());
        for (const auto j : logicalQubits) {
          vars.emplace_back(x[k][j][i]);
        }
        lb->assertFormula(encodings::atMostOneBiMander(vars, lb.get()));
      }

      for (const auto j : logicalQubits) { // There is no exactly one Bimander
        const auto& varIDs = x[k][j];

        if (config.commanderGrouping == CommanderGrouping::Fixed2) {
          lb->assertFormula(
              encodings::exactlyOneCmdr(encodings::groupVars(varIDs, 2),
                                        LogicTerm::noneTerm(), lb.get()));
        } else if (config.commanderGrouping == CommanderGrouping::Fixed3) {
          lb->assertFormula(
              encodings::exactlyOneCmdr(encodings::groupVars(varIDs, 3),
                                        LogicTerm::noneTerm(), lb.get()));
        } else if (config.commanderGrouping == CommanderGrouping::Logarithm) {
          lb->assertFormula(encodings::exactlyOneCmdr(
              encodings::groupVars(
                  varIDs, static_cast<std::size_t>(std::log(varIDs.size()))),
              LogicTerm::noneTerm(), lb.get()));
        } else if (config.commanderGrouping == CommanderGrouping::Halves) {
          lb->assertFormula(encodings::exactlyOneCmdr(
              encodings::groupVars(varIDs, varIDs.size() / 2),
              LogicTerm::noneTerm(), lb.get()));
        }
      }
    }
  }

  //////////////////////////////////////////
  ///		Coupling Constraints			//
  //////////////////////////////////////////
  for (std::size_t k = 0; k < reducedLayerIndices.size(); ++k) {
    auto allCouplings = LogicTerm(true);
    for (const auto& gate : layers.at(reducedLayerIndices.at(k)).gates) {
      if (gate.singleQubit()) {
        continue;
      }

      const auto& first = x[k][gate.first];
      const auto& second = x[k][*gate.second];
      auto coupling = LogicTerm(false);
      auto reverse = LogicTerm(false);
      for (const auto& [from, to] : rcm) {
        const auto i = physicalQubitIndex.at(from);
        const auto j = physicalQubitIndex.at(to);
        coupling = coupling || (first[i] && second[j]);
        if (gate.reversal != Reversal::None && !rcm.contains({to, from})) {
          reverse = reverse || (second[i] && first[j]);
        }
      }
      coupling = coupling || reverse;
      if (!architecture->bidirectional() && gate.reversal == Reversal::CNOT) {
        lb->weightedTerm(reverse, GATES_OF_DIRECTION_REVERSE);
      }
      allCouplings = allCouplings && coupling;
    }
    lb->assertFormula(allCouplings);
  }

  //////////////////////////////////////////
  /// 	Permutation Constraints			//
  //////////////////////////////////////////
  for (std::size_t k = 1; k < reducedLayerIndices.size(); ++k) {
    piCount = 0;
    internalPiCount = 0;
    auto& i = x[k - 1];
    auto& j = x[k];
    do {
      if (!skippedPi.contains(piCount) || !config.swapLimitsEnabled()) {
        auto equal = LogicTerm(true);
        for (const auto qubit : qubitChoice) {
          for (const auto q : logicalQubits) {
            const auto before = i[q][physicalQubitIndex[qubit]];
            const auto after =
                j[q][physicalQubitIndex[pi[physicalQubitIndex[qubit]]]];
            equal = equal && (before == after);
          }
        }
        lb->assertFormula(LogicTerm::implies(y[k - 1][internalPiCount], equal));
        ++internalPiCount;
      }
      ++piCount;
    } while (std::ranges::next_permutation(pi).found);
  }

  // Allow only 1 y_k_pi to be true
  if (config.encoding == Encoding::Naive) {
    for (std::size_t k = 1; k < reducedLayerIndices.size(); ++k) {
      auto onlyOne = LogicTerm(0);
      piCount = 0;
      internalPiCount = 0;
      do {
        if (!skippedPi.contains(piCount) || !config.swapLimitsEnabled()) {
          onlyOne = onlyOne + LogicTerm::ite(y[k - 1][internalPiCount],
                                             LogicTerm(1), LogicTerm(0));
          ++internalPiCount;
        }
        ++piCount;
      } while (std::ranges::next_permutation(pi).found);
      lb->assertFormula(onlyOne == LogicTerm(1));
    }
  } else {
    for (std::size_t k = 1; k < reducedLayerIndices.size(); ++k) {
      std::vector<LogicTerm> varIDs;
      piCount = 0;
      internalPiCount = 0;
      do {
        if (!skippedPi.contains(piCount) || !config.swapLimitsEnabled()) {
          varIDs.push_back(y[k - 1][internalPiCount]);
          ++internalPiCount;
        }
        ++piCount;
      } while (std::ranges::next_permutation(pi).found);
      if (config.commanderGrouping == CommanderGrouping::Fixed2) {
        lb->assertFormula(encodings::exactlyOneCmdr(
            encodings::groupVars(varIDs, 2), LogicTerm::noneTerm(), lb.get()));
      } else if (config.commanderGrouping == CommanderGrouping::Fixed3) {
        lb->assertFormula(encodings::exactlyOneCmdr(
            encodings::groupVars(varIDs, 3), LogicTerm::noneTerm(), lb.get()));
      } else if (config.commanderGrouping == CommanderGrouping::Logarithm) {
        lb->assertFormula(encodings::exactlyOneCmdr(
            encodings::groupVars(
                varIDs, static_cast<std::size_t>(std::log(varIDs.size()))),
            LogicTerm::noneTerm(), lb.get()));
      } else if (config.commanderGrouping == CommanderGrouping::Halves) {
        lb->assertFormula(encodings::exactlyOneCmdr(
            encodings::groupVars(varIDs, varIDs.size() / 2),
            LogicTerm::noneTerm(), lb.get()));
      }
    }
  }
  //////////////////////////////////////////
  /// 	Objective Function				//
  //////////////////////////////////////////
  // cost for permutations
  piCount = 0;
  internalPiCount = 0;
  const auto cost = LogicTerm(0);
  do {
    if (!skippedPi.contains(piCount) || !config.swapLimitsEnabled()) {
      auto picost = architecture->minimumNumberOfSwaps(pi);
      if (architecture->bidirectional()) {
        picost *= GATES_OF_BIDIRECTIONAL_SWAP;
      } else {
        picost *= GATES_OF_UNIDIRECTIONAL_SWAP;
      }
      for (std::size_t k = 1; k < reducedLayerIndices.size(); ++k) {
        lb->weightedTerm(y[k - 1][internalPiCount],
                         static_cast<double>(picost));
      }
      ++internalPiCount;
    }
    ++piCount;
  } while (std::ranges::next_permutation(pi).found);

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
    // Decode each logical qubit's physical position once per layer.
    std::vector<std::vector<std::uint16_t>> placements(
        reducedLayerIndices.size(),
        std::vector<std::uint16_t>(qc.getNqubits()));
    for (std::size_t k = 0; k < placements.size(); ++k) {
      for (const auto logical : logicalQubits) {
        for (const auto physical : qubitChoice) {
          if (m->getBoolValue(x[k][logical][physicalQubitIndex.at(physical)],
                              lb.get())) {
            placements[k][logical] = physical;
            break;
          }
        }
      }
      if (!architecture->bidirectional()) {
        for (const auto& gate : layers[reducedLayerIndices[k]].gates) {
          if (gate.reversal == Reversal::CNOT &&
              !rcm.contains(
                  {placements[k][gate.first], placements[k][*gate.second]})) {
            ++choiceResults.output.directionReverse;
          }
        }
      }
    }
    choiceResults.output.gates +=
        choiceResults.output.directionReverse * GATES_OF_DIRECTION_REVERSE;
    for (const auto logical : logicalQubits) {
      swaps.front().emplace_back(placements.front()[logical], logical);
    }

    for (std::size_t k = 1; k < placements.size(); ++k) {
      if (qubitChoice.size() == logicalQubits.size()) {
        for (const auto logical : logicalQubits) {
          pi[physicalQubitIndex.at(placements[k - 1][logical])] =
              placements[k][logical];
        }
      } else {
        // When more qubits of the architecture are being considered than are in
        // the circuit, the assignment of the logical to the physical qubits
        // cannot be a bijection. Hence, the permutation variables y have to be
        // used to infer the permutation of the qubits in each layer. This is
        // mainly because the additional qubits movement cannot be inferred
        // from the assignment matrices X.
        piCount = 0;
        internalPiCount = 0;
        // sort the permutation of the qubits to start fresh
        std::ranges::sort(pi);
        do {
          if (!skippedPi.contains(piCount) || !config.swapLimitsEnabled()) {
            if (m->getBoolValue(y[k - 1][internalPiCount], lb.get())) {
              break;
            }
            ++internalPiCount;
          }
          ++piCount;
        } while (std::ranges::next_permutation(pi).found);
      }

      architecture->minimumNumberOfSwaps(pi, swaps.at(k));
      choiceResults.output.swaps += swaps.at(k).size();
      if (architecture->bidirectional()) {
        choiceResults.output.gates +=
            GATES_OF_BIDIRECTIONAL_SWAP * swaps.at(k).size();
      } else {
        choiceResults.output.gates +=
            GATES_OF_UNIDIRECTIONAL_SWAP * swaps.at(k).size();
      }
    }
  }
  (*lb).reset();
}
