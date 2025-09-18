//
// This file is part of the MQT QMAP library released under the MIT license.
// See README.md or go to https://github.com/cda-tum/qmap for more information.
//

#include "sc/exact/ExactMapper.hpp"

#include "Logic.hpp"
#include "LogicTerm.hpp"
#include "ir/Definitions.hpp"
#include "ir/operations/StandardOperation.hpp"
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
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

void ExactMapper::map(const Configuration& settings) {
  results.config = settings;
  const auto& config = results.config;

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
  std::size_t k = 0;
  for (const auto& layer : layers) {
    bool onlySingleQubit = true;
    for (const auto& gate : layer) {
      if (!gate.singleQubit()) {
        onlySingleQubit = false;
        break;
      }
    }
    if (!onlySingleQubit) {
      reducedLayerIndices.emplace_back(k);
    }
    ++k;
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

  // 2a) If only a subgraph should be considered, reduce the architecture.
  if (!config.subgraph.empty()) {
    const auto subgraphQubits = config.subgraph.size();
    if (subgraphQubits < qc.getNqubits()) {
      std::cerr << "The subgraph must contain at least as many qubits as the "
                   "circuit has physical qubits.\n";
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
        static_cast<std::uint16_t>(qc.getNqubits()));
  } else {
    allPossibleQubitChoices.emplace_back(qubitRange.begin(), qubitRange.end());
  }

  // 3) determine exact mapping for this qubit choice
  std::vector<Swaps> swaps(reducedLayerIndices.size(), Swaps{});
  mappingSwaps.reserve(reducedLayerIndices.size());
  std::size_t runs = 1;
  for (auto& choice : allPossibleQubitChoices) {
    std::size_t limit = 0U;
    std::size_t maxLimit = 0U;
    const std::size_t upperLimit = config.swapLimit;
    if (config.useSubsets) {
      maxLimit = architecture->getCouplingLimit(choice) - 1U;
    } else {
      maxLimit = architecture->getCouplingLimit() - 1U;
    }
    if (config.swapReduction == SwapReduction::CouplingLimit) {
      if (!architecture->bidirectional()) {
        // on a directed architecture, one more SWAP might be needed overall
        // due to the directionality of the edges and direction reversal not
        // being possible for every gate.
        maxLimit += 1U;
      }
      limit = maxLimit;
    } else if (config.swapReduction == SwapReduction::Increasing) {
      limit = 0U;
    } else { // CustomLimit
      limit = upperLimit;
    }

    std::size_t timeout = 0U;
    do {
      if (config.swapReduction == SwapReduction::Increasing) {
        timeout += static_cast<std::size_t>(
            static_cast<double>(settings.timeout) *
            (static_cast<double>(limit) * 0.5) /
            static_cast<double>(maxLimit < upperLimit ? upperLimit : maxLimit));
        if (timeout <= 10000U) {
          timeout = 10000U;
        }
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
      coreMappingRoutine(choice, reducedCouplingMap, choiceResults, swaps,
                         limit, timeout);

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
      if (limit == 0) {
        limit = 1;
      } else {
        limit += runs;
        runs++;
      }
    } while (config.swapReduction == SwapReduction::Increasing &&
             (limit <= upperLimit || config.swapLimit == 0) &&
             limit < architecture->getCouplingLimit());

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

  for (const auto& q : qubitRange) {
    locations.at(q) = static_cast<std::int16_t>(q);
  }

  for (std::size_t i = 0U; i < layers.size(); ++i) {
    if (i == 0U) {
      qcMapped.initialLayout.clear();

      // no swaps but initial permutation
      for (const auto& [physical, logical] : *swapsIterator) {
        locations.at(logical) = static_cast<std::int16_t>(physical);
        qubits.at(physical) = static_cast<std::int16_t>(logical);
        qcMapped.initialLayout[static_cast<qc::Qubit>(physical)] =
            static_cast<qc::Qubit>(logical);
      }

      // place remaining architecture qubits
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
    for (const auto& gate : layers.at(i)) {
      auto* op = dynamic_cast<qc::StandardOperation*>(gate.op);
      if (op == nullptr) {
        throw QMAPException(
            "Cast to StandardOperation not possible during mapping. Check that "
            "circuit contains only StandardOperations");
      }

      if (gate.singleQubit()) {
        if (settings.verbose) {
          std::cout << i << ": Added single qubit gate with target: "
                    << locations.at(gate.target) << "\n";
        }

        qcMapped.emplace_back<qc::StandardOperation>(
            locations.at(gate.target), op->getType(), op->getParameter());
      } else {
        const Edge cnot = {locations.at(static_cast<std::size_t>(gate.control)),
                           locations.at(gate.target)};

        if (architecture->getCouplingMap().find(cnot) ==
            architecture->getCouplingMap().end()) {
          const Edge reverse = {cnot.second, cnot.first};
          if (architecture->getCouplingMap().find(reverse) ==
              architecture->getCouplingMap().end()) {
            throw QMAPException(
                "Invalid CNOT: " + std::to_string(reverse.first) + "-" +
                std::to_string(reverse.second));
          }
          if (settings.verbose) {
            std::cout
                << i
                << ": Added (direction-reversed) cnot with control and target: "
                << cnot.first << " " << cnot.second << "\n";
          }
          qcMapped.h(reverse.first);
          qcMapped.h(reverse.second);
          qcMapped.cx(qc::Control{static_cast<qc::Qubit>(reverse.first)},
                      reverse.second);
          qcMapped.h(reverse.second);
          qcMapped.h(reverse.first);
        } else {
          if (settings.verbose) {
            std::cout << i
                      << ": Added cnot with control and target: " << cnot.first
                      << " " << cnot.second << "\n";
          }
          qcMapped.cx(qc::Control{static_cast<qc::Qubit>(cnot.first)},
                      cnot.second);
        }
      }
    }

    if (!mappingSwaps.empty() && swapsIterator != mappingSwaps.end() &&
        layerIterator != reducedLayerIndices.end() && i == *layerIterator) {
      // apply swaps before layer
      for (auto it = (*swapsIterator).rbegin(); it != (*swapsIterator).rend();
           ++it) {
        const auto& [q0, q1] = *it;
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

  auto end = std::chrono::high_resolution_clock::now();
  const std::chrono::duration<double> diff = end - start;
  results.time = diff.count();
}

void ExactMapper::coreMappingRoutine(
    const std::set<std::uint16_t>& qubitChoice, const CouplingMap& rcm,
    MappingResults& choiceResults,
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
    } while (std::next_permutation(pi.begin(), pi.end()));
  }
  //////////////////////////////////////////
  /// 	Boolean Variable Definitions	//
  //////////////////////////////////////////

  /*
  locical/physical qubit variables x_k_i_j
  k	before layer k
  i	physical qubit i
  j	logical qubit j
  number of variables: (|L|) * m * n
  */
  LogicMatrix3D x{};
  std::stringstream xName{};
  for (std::size_t k = 0; k < reducedLayerIndices.size(); ++k) {
    x.emplace_back();
    for (auto qubit : qubitChoice) {
      x.back().emplace_back();
      for (std::size_t q = 0; q < qc.getNqubits(); ++q) {
        xName.str("");
        xName << "x_" << k << '_' << qubit << '_' << q;
        x.back().back().emplace_back(
            lb->makeVariable(xName.str(), CType::BOOL));
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
      if (skippedPi.count(piCount) == 0 || !config.swapLimitsEnabled()) {
        yName.str("");
        yName << "y_" << k << '_' << piCount;
        y.back().emplace_back(lb->makeVariable(yName.str(), CType::BOOL));
      }
      ++piCount;
    } while (std::next_permutation(pi.begin(), pi.end()));
  }

  //////////////////////////////////////////
  /// 	Consistency Constraints			//
  //////////////////////////////////////////
  if (config.encoding == Encoding::Naive) {
    for (std::size_t k = 0; k < reducedLayerIndices.size(); ++k) {
      for (std::size_t i = 0; i < qubitChoice.size(); ++i) {
        auto rowConsistency = LogicTerm(0);
        for (std::size_t j = 0; j < qc.getNqubits(); ++j) {
          rowConsistency =
              rowConsistency +
              LogicTerm::ite(x[k][i][j], LogicTerm(1), LogicTerm(0));
        }
        lb->assertFormula(rowConsistency <= LogicTerm(1));
      }

      for (std::size_t j = 0; j < qc.getNqubits(); ++j) {
        auto colConsistency = LogicTerm(0);
        for (std::size_t i = 0; i < qubitChoice.size(); ++i) {
          colConsistency =
              colConsistency +
              LogicTerm::ite(x[k][i][j], LogicTerm(1), LogicTerm(0));
        }
        lb->assertFormula(colConsistency == LogicTerm(1));
      }
    }
  } else if (config.encoding == Encoding::Commander) {
    for (std::size_t k = 0; k < reducedLayerIndices.size(); ++k) {
      for (std::size_t i = 0; i < qubitChoice.size(); ++i) {
        std::vector<LogicTerm> varIDs;
        for (std::size_t j = 0; j < qc.getNqubits(); ++j) {
          varIDs.push_back(x[k][i][j]);
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

      for (std::size_t j = 0; j < qc.getNqubits(); ++j) {
        std::vector<LogicTerm> varIDs;
        for (std::size_t i = 0; i < qubitChoice.size(); ++i) {
          varIDs.push_back(x[k][i][j]);
        }

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
        std::vector<std::size_t> varIDs;
        for (std::size_t j = 0; j < qc.getNqubits(); ++j) {
          vars.emplace_back(x[k][i][j]);
          varIDs.emplace_back(j);
        }
        lb->assertFormula(encodings::atMostOneBiMander(vars, lb.get()));
      }

      for (std::size_t j = 0; j < qc.getNqubits();
           ++j) { // There is no exactly one Bimander
        std::vector<LogicTerm> varIDs;
        for (std::size_t i = 0; i < qubitChoice.size(); ++i) {
          varIDs.push_back(x[k][i][j]);
        }

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
    for (const auto& gate : layers.at(reducedLayerIndices.at(k))) {
      if (gate.singleQubit()) {
        continue;
      }

      auto coupling = LogicTerm(false);
      if (architecture->bidirectional()) {
        for (const auto& edge : rcm) {
          auto indexFC = x[k][physicalQubitIndex[edge.first]]
                          [static_cast<std::size_t>(gate.control)];
          auto indexST = x[k][physicalQubitIndex[edge.second]][gate.target];
          coupling = coupling || (indexFC && indexST);
        }
      } else {
        for (const auto& edge : rcm) {
          auto indexFC = x[k][physicalQubitIndex[edge.first]]
                          [static_cast<std::size_t>(gate.control)];
          auto indexST = x[k][physicalQubitIndex[edge.second]][gate.target];
          auto indexFT = x[k][physicalQubitIndex[edge.first]][gate.target];
          auto indexSC = x[k][physicalQubitIndex[edge.second]]
                          [static_cast<std::size_t>(gate.control)];

          coupling = coupling || ((indexFC && indexST) || (indexFT && indexSC));
        }
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
      if (skippedPi.count(piCount) == 0 || !config.swapLimitsEnabled()) {
        auto equal = LogicTerm(true);
        for (const auto qubit : qubitChoice) {
          for (std::size_t q = 0; q < qc.getNqubits(); ++q) {
            auto before = i[physicalQubitIndex[qubit]][q];
            auto after =
                j[physicalQubitIndex[pi[physicalQubitIndex[qubit]]]][q];
            equal = equal && (before == after);
          }
        }
        lb->assertFormula(LogicTerm::implies(y[k - 1][internalPiCount], equal));
        ++internalPiCount;
      }
      ++piCount;
    } while (std::next_permutation(pi.begin(), pi.end()));
  }

  // Allow only 1 y_k_pi to be true
  if (config.encoding == Encoding::Naive) {
    for (std::size_t k = 1; k < reducedLayerIndices.size(); ++k) {
      auto onlyOne = LogicTerm(0);
      piCount = 0;
      internalPiCount = 0;
      do {
        if (skippedPi.count(piCount) == 0 || !config.swapLimitsEnabled()) {
          onlyOne = onlyOne + LogicTerm::ite(y[k - 1][internalPiCount],
                                             LogicTerm(1), LogicTerm(0));
          ++internalPiCount;
        }
        ++piCount;
      } while (std::next_permutation(pi.begin(), pi.end()));
      lb->assertFormula(onlyOne == LogicTerm(1));
    }
  } else {
    for (std::size_t k = 1; k < reducedLayerIndices.size(); ++k) {
      std::vector<LogicTerm> varIDs;
      piCount = 0;
      internalPiCount = 0;
      do {
        if (skippedPi.count(piCount) == 0 || !config.swapLimitsEnabled()) {
          varIDs.push_back(y[k - 1][internalPiCount]);
          ++internalPiCount;
        }
        ++piCount;
      } while (std::next_permutation(pi.begin(), pi.end()));
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
  auto cost = LogicTerm(0);
  do {
    if (skippedPi.count(piCount) == 0 || !config.swapLimitsEnabled()) {
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
  } while (std::next_permutation(pi.begin(), pi.end()));

  // cost for reversed directions
  if (!architecture->bidirectional()) {
    const auto numLayers = reducedLayerIndices.size();
    for (std::size_t k = 0; k < numLayers; ++k) {
      for (const auto& gate : layers.at(reducedLayerIndices.at(k))) {
        if (gate.singleQubit()) {
          continue;
        }

        auto reverse = LogicTerm(false);
        for (const auto& [q0, q1] : rcm) {
          const auto indexFT = x[k][physicalQubitIndex[q0]][gate.target];
          const auto indexSC = x[k][physicalQubitIndex[q1]]
                                [static_cast<std::size_t>(gate.control)];
          reverse = reverse || (indexFT && indexSC);
        }
        lb->weightedTerm(reverse, ::GATES_OF_DIRECTION_REVERSE);
      }
    }
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
    choiceResults.timeout = results.timeout = false;

    // quickly determine cost
    choiceResults.output.singleQubitGates =
        choiceResults.input.singleQubitGates;
    choiceResults.output.cnots = choiceResults.input.cnots;
    choiceResults.output.gates =
        choiceResults.output.singleQubitGates + choiceResults.output.cnots;
    assert(choiceResults.output.swaps == 0U);
    assert(choiceResults.output.directionReverse == 0U);
    // swaps
    for (std::size_t k = 1; k < reducedLayerIndices.size(); ++k) {
      if (qubitChoice.size() == qc.getNqubits()) {
        // When as many qubits of the architecture are being considered
        // as in the circuit, the assignment of the logical to the physical
        // qubits is a bijection. Hence, we the assignment matrices X can be
        // used to directly infer the permutation of the qubits in each layer.
        auto& oldAssignment = x[k - 1];
        auto& newAssignment = x[k];
        for (const auto physicalQubit : qubitChoice) {
          for (std::size_t logicalQubit = 0; logicalQubit < qc.getNqubits();
               ++logicalQubit) {
            if (const auto oldIndex = physicalQubitIndex[physicalQubit];
                m->getBoolValue(oldAssignment[oldIndex][logicalQubit],
                                lb.get())) {
              for (const auto newPhysicalQubit : qubitChoice) {
                if (const auto newIndex = physicalQubitIndex[newPhysicalQubit];
                    m->getBoolValue(newAssignment[newIndex][logicalQubit],
                                    lb.get())) {
                  pi[oldIndex] = newPhysicalQubit;
                  break;
                }
              }
              break;
            }
          }
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
        std::sort(pi.begin(), pi.end());
        do {
          if (skippedPi.count(piCount) == 0 || !config.swapLimitsEnabled()) {
            if (m->getBoolValue(y[k - 1][internalPiCount], lb.get())) {
              break;
            }
            ++internalPiCount;
          }
          ++piCount;
        } while (std::next_permutation(pi.begin(), pi.end()));
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

    // direction reverse
    if (!architecture->bidirectional()) {
      for (std::size_t k = 0; k < reducedLayerIndices.size(); ++k) {
        for (const auto& gate : layers.at(reducedLayerIndices.at(k))) {
          if (gate.singleQubit()) {
            continue;
          }
          for (const auto& edge : rcm) {
            auto indexFT = x[k][physicalQubitIndex[edge.first]][gate.target];
            auto indexSC = x[k][physicalQubitIndex[edge.second]]
                            [static_cast<std::size_t>(gate.control)];
            if (m->getBoolValue(indexFT, lb.get()) &&
                m->getBoolValue(indexSC, lb.get())) {
              choiceResults.output.directionReverse++;
              choiceResults.output.gates += GATES_OF_DIRECTION_REVERSE;
            }
          }
        }
      }
    }

    // save initial layout for later
    for (const auto& qubit : qubitChoice) {
      for (std::size_t q = 0; q < qc.getNqubits(); ++q) {
        const bool set =
            m->getBoolValue(x[0][physicalQubitIndex[qubit]][q], lb.get());
        if (set) {
          swaps.at(0).emplace_back(qubit, q);
        }
      }
    }

  } else {
    results.timeout = true;
  }
  lb->reset();
}
