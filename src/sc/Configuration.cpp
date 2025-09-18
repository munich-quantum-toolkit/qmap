//
// This file is part of the MQT QMAP library released under the MIT license.
// See README.md or go to https://github.com/cda-tum/qmap for more information.
//

#include "sc/configuration/Configuration.hpp"

#include "sc/configuration/CommanderGrouping.hpp"
#include "sc/configuration/Encoding.hpp"
#include "sc/configuration/Heuristic.hpp"
#include "sc/configuration/InitialLayout.hpp"
#include "sc/configuration/Layering.hpp"
#include "sc/configuration/LookaheadHeuristic.hpp"
#include "sc/configuration/Method.hpp"
#include "sc/configuration/SwapReduction.hpp"

#include <nlohmann/json.hpp>

nlohmann::basic_json<> Configuration::json() const {
  nlohmann::basic_json config{};
  config["method"] = ::toString(method);
  config["layering_strategy"] = ::toString(layering);
  if (!subgraph.empty()) {
    config["subgraph"] = subgraph;
  }
  config["pre_mapping_optimizations"] = preMappingOptimizations;
  config["post_mapping_optimizations"] = postMappingOptimizations;
  config["add_measurements_to_mapped_circuit"] = addMeasurementsToMappedCircuit;
  config["verbose"] = verbose;
  config["debug"] = debug;

  if (method == Method::Heuristic) {
    auto& heuristicJson = config["settings"];
    heuristicJson["heuristic"] = ::toString(heuristic);
    auto& heuristicPropertiesJson = heuristicJson["heuristic_properties"];
    heuristicPropertiesJson["admissible"] = isAdmissible(heuristic);
    heuristicPropertiesJson["principally_admissible"] =
        isPrincipallyAdmissible(heuristic);
    heuristicPropertiesJson["tight"] = isTight(heuristic);
    heuristicPropertiesJson["fidelity_aware"] = isFidelityAware(heuristic);
    heuristicJson["initial_layout"] = ::toString(initialLayout);
    if (lookaheadHeuristic != LookaheadHeuristic::None) {
      auto& lookaheadSettings = heuristicJson["lookahead"];
      lookaheadSettings["heuristic"] = ::toString(lookaheadHeuristic);
      auto& lookaheadHeuristicPropertiesJson =
          lookaheadSettings["heuristic_properties"];
      lookaheadHeuristicPropertiesJson["fidelity_aware"] =
          isFidelityAware(lookaheadHeuristic);
      lookaheadSettings["lookaheads"] = nrLookaheads;
      lookaheadSettings["first_factor"] = firstLookaheadFactor;
      lookaheadSettings["factor"] = lookaheadFactor;
    }
  }

  if (method == Method::Exact) {
    auto& exact = config["settings"];
    exact["timeout"] = timeout;
    exact["encoding"] = ::toString(encoding);
    if (encoding == Encoding::Commander || encoding == Encoding::Bimander) {
      exact["commander_grouping"] = ::toString(commanderGrouping);
    }
    exact["include_WCNF"] = includeWCNF;
    exact["use_subsets"] = useSubsets;
    if (enableSwapLimits) {
      auto& limits = exact["limits"];
      limits["swap_reduction"] = ::toString(swapReduction);
      if (swapLimit > 0) {
        limits["swap_limit"] = swapLimit;
      }
    }
  }

  return config;
}
