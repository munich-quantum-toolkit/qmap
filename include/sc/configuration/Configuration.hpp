//
// This file is part of the MQT QMAP library released under the MIT license.
// See README.md or go to https://github.com/cda-tum/qmap for more information.
//

#pragma once

#include "CommanderGrouping.hpp"
#include "EarlyTermination.hpp"
#include "Encoding.hpp"
#include "Heuristic.hpp"
#include "InitialLayout.hpp"
#include "Layering.hpp"
#include "LookaheadHeuristic.hpp"
#include "Method.hpp"
#include "SwapReduction.hpp"

#include <cstddef>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <set>
#include <string>

// NOLINTNEXTLINE(clang-analyzer-optin.performance.Padding)
struct Configuration {
  Configuration() = default;

  // which method to use
  Method method = Method::Heuristic;
  Heuristic heuristic = Heuristic::GateCountMaxDistance;

  bool preMappingOptimizations = true;
  bool postMappingOptimizations = true;

  bool addMeasurementsToMappedCircuit = true;
  bool swapOnFirstLayer = false;
  bool addBarriersBetweenLayers = false;

  bool verbose = false;
  bool debug = false;
  std::string dataLoggingPath;

  // map to particular subgraph of architecture (in exact mapper)
  std::set<std::uint16_t> subgraph;

  // how to cluster the gates into layers
  Layering layering = Layering::IndividualGates;

  // initial layout to use for heuristic approach
  InitialLayout initialLayout = InitialLayout::Dynamic;

  // iterative bidirectional routing, i.e. after an initial layout is found,
  // the circuit is routed multiple times back and forth (using settings
  // optimized for time-efficiency) without actually inserting any swaps;
  // this gradually improves the initial layout; after all passes are done,
  // one final full routing pass is performed
  //
  // G. Li, Y. Ding, and Y. Xie, "Tackling the qubit mapping problem for
  // NISQ-era quantum devices", Proc. 24th Int. Conf. on Architectural Support
  // for Program. Languages and Oper. Syst. (ASPLOS)
  // https://arxiv.org/abs/1809.02573
  bool iterativeBidirectionalRouting = false;
  std::size_t iterativeBidirectionalRoutingPasses = 0;

  // lookahead scheme settings
  LookaheadHeuristic lookaheadHeuristic =
      LookaheadHeuristic::GateCountMaxDistance;
  std::size_t nrLookaheads = 15;
  double firstLookaheadFactor = 0.75;
  double lookaheadFactor = 0.5;

  // timeout merely affects exact mapper
  std::size_t timeout = 3600000; // 60min timeout

  // if layers should be automatically split after a certain number of expanded
  // nodes, thereby reducing the search space (but potentially eliminating
  // opportunities for cost savings); acts as a control between runtime and
  // result quality
  bool automaticLayerSplits = true;
  std::size_t automaticLayerSplitsNodeLimit = 5000;

  // strategy for terminating the heuristic search early (i.e. once a goal node
  // has been found, but before it is guaranteed that the optimal solution has
  // been found)
  EarlyTermination earlyTermination = EarlyTermination::None;
  std::size_t earlyTerminationLimit = 0;

  // encoding of at most and exactly one constraints in exact mapper
  Encoding encoding = Encoding::Commander;
  CommanderGrouping commanderGrouping = CommanderGrouping::Fixed3;

  // use qubit subsets in exact mapper
  bool useSubsets = true;

  // include WCNF file in results of exact mapper
  bool includeWCNF = false;

  // limit the number of considered swaps
  bool enableSwapLimits = true;
  SwapReduction swapReduction = SwapReduction::CouplingLimit;
  std::size_t swapLimit = 0;

  [[nodiscard]] nlohmann::basic_json<> json() const;
  [[nodiscard]] std::string toString() const { return json().dump(2); }

  [[nodiscard]] bool dataLoggingEnabled() const {
    return !dataLoggingPath.empty();
  }

  void setTimeout(const std::size_t sec) { timeout = sec; }
  [[nodiscard]] bool swapLimitsEnabled() const {
    return (swapReduction != SwapReduction::None) && enableSwapLimits;
  }
};
