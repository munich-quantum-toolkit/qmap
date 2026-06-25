/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "ir/QuantumComputation.hpp"
#include "na/zoned/Architecture.hpp"
#include "na/zoned/Compiler.hpp"
#include "na/zoned/code_generator/CodeGenerator.hpp"
#include "na/zoned/layout_synthesizer/PlaceAndRouteSynthesizer.hpp"
#include "na/zoned/layout_synthesizer/placer/HeuristicPlacer.hpp"
#include "na/zoned/layout_synthesizer/placer/VertexMatchingPlacer.hpp"
#include "na/zoned/layout_synthesizer/router/IndependentSetRouter.hpp"

#include <cstddef>
#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>      // NOLINT(misc-include-cleaner)
#include <nanobind/stl/string_view.h> // NOLINT(misc-include-cleaner)
// The header <nlohmann/json.hpp> is used, but clang-tidy confuses it with the
// wrong forward header <nlohmann/json_fwd.hpp>
// NOLINTNEXTLINE(misc-include-cleaner)
#include <nlohmann/json.hpp>
#include <spdlog/common.h>
#include <string>

namespace nb = nanobind;
using namespace nb::literals;

// NOLINTNEXTLINE(misc-use-internal-linkage)
void registerZoned(nb::module_& m) {
  nb::module_::import_("mqt.core.ir");

  nb::class_<na::zoned::Architecture> architecture(
      m, "ZonedNeutralAtomArchitecture",
      "Class representing a zoned neutral atom architecture.");

  architecture.def_static("from_json_file",
                          &na::zoned::Architecture::fromJSONFile, "filename"_a,
                          R"pb(Create an architecture from a JSON file.

Args:
    filename: The path to the JSON file

Returns:
    The architecture

Raises:
    ValueError: if the file does not exist or is not a valid JSON file)pb");

  architecture.def_static("from_json_string",
                          &na::zoned::Architecture::fromJSONString, "json"_a,
                          R"pb(Create an architecture from a JSON string.

Args:
    json: The JSON string

Returns:
    The architecture

Raises:
    ValueError: If the string is not a valid JSON string)pb");
  architecture.def(
      "to_namachine_file",
      [](na::zoned::Architecture& self, const std::string& filename) -> void {
        self.exportNAVizMachine(filename);
      },
      "filename"_a, R"pb(Write the architecture to a .namachine file.

Args:
    filename: The path to the .namachine file)pb");

  architecture.def(
      "to_namachine_string",
      [](na::zoned::Architecture& self) -> std::string {
        return self.exportNAVizMachine();
      },
      R"pb(Get the architecture as a .namachine string.

Returns:
    The architecture as a .namachine string)pb");

  //===--------------------------------------------------------------------===//
  // Placement Method Enum
  //===--------------------------------------------------------------------===//
  nb::enum_<na::zoned::HeuristicPlacer::Config::Method>(
      m, "PlacementMethod",
      "Enumeration of the available placement methods for the heuristic "
      "placer.")
      .value("astar", na::zoned::HeuristicPlacer::Config::Method::ASTAR,
             "A-star algorithm.")
      .value("ids", na::zoned::HeuristicPlacer::Config::Method::IDS,
             "Iterative diving search.");

  //===--------------------------------------------------------------------===//
  // Routing Method Enum
  //===--------------------------------------------------------------------===//
  nb::enum_<na::zoned::IndependentSetRouter::Config::Method>(
      m, "RoutingMethod",
      "Enumeration of the available routing methods for the independent set "
      "router.")
      .value("strict", na::zoned::IndependentSetRouter::Config::Method::STRICT,
             "Strict routing, i.e., the relative order of atoms must be "
             "maintained throughout a movement.")
      .value("relaxed",
             na::zoned::IndependentSetRouter::Config::Method::RELAXED,
             "Relaxed routing, i.e., the relative order of atoms may change "
             "throughout a movement by applying offsets during pick-up and "
             "drop-off.");

  //===--------------------------------------------------------------------===//
  // Routing-agnostic Compiler
  //===--------------------------------------------------------------------===//
  nb::class_<na::zoned::RoutingAgnosticCompiler> routingAgnosticCompiler(
      m, "RoutingAgnosticCompiler",
      "Routing-agnostic zoned neutral atom compiler.");
  {
    const na::zoned::RoutingAgnosticCompiler::Config defaultConfig;
    routingAgnosticCompiler.def(
        "__init__",
        [](na::zoned::RoutingAgnosticCompiler* self,
           const na::zoned::Architecture& arch, const std::string& logLevel,
           const double maxFillingFactor, const bool useWindow,
           const size_t windowSize, const bool dynamicPlacement,
           const na::zoned::IndependentSetRouter::Config::Method routingMethod,
           const double preferSplit, const bool warnUnsupportedGates) {
          na::zoned::RoutingAgnosticCompiler::Config config;
          config.logLevel = spdlog::level::from_str(logLevel);
          config.schedulerConfig.maxFillingFactor = maxFillingFactor;
          config.layoutSynthesizerConfig.placerConfig = {
              .useWindow = useWindow,
              .windowSize = windowSize,
              .dynamicPlacement = dynamicPlacement};
          config.layoutSynthesizerConfig.routerConfig = {
              .method = routingMethod, .preferSplit = preferSplit};
          config.codeGeneratorConfig = {.warnUnsupportedGates =
                                            warnUnsupportedGates};
          new (self) na::zoned::RoutingAgnosticCompiler{arch, config};
        },
        nb::keep_alive<1, 2>(), "arch"_a,
        "log_level"_a = spdlog::level::to_short_c_str(defaultConfig.logLevel),
        "max_filling_factor"_a = defaultConfig.schedulerConfig.maxFillingFactor,
        "use_window"_a =
            defaultConfig.layoutSynthesizerConfig.placerConfig.useWindow,
        "window_size"_a =
            defaultConfig.layoutSynthesizerConfig.placerConfig.windowSize,
        "dynamic_placement"_a =
            defaultConfig.layoutSynthesizerConfig.placerConfig.dynamicPlacement,
        "routing_method"_a =
            defaultConfig.layoutSynthesizerConfig.routerConfig.method,
        "prefer_split"_a =
            defaultConfig.layoutSynthesizerConfig.routerConfig.preferSplit,
        "warn_unsupported_gates"_a =
            defaultConfig.codeGeneratorConfig.warnUnsupportedGates,
        R"pb(Create a routing-agnostic compiler for the given architecture and configurations.

Args:
    arch: The zoned neutral atom architecture
    log_level: The log level for the compiler, possible values are "debug"/"D", "info"/"I", "warning"/"W", "error"/"E", and "critical"/"C"
    max_filling_factor: The maximum filling factor for the entanglement zone, i.e., it sets the limit for the maximum number of entangling gates that are scheduled in parallel
    use_window: Whether to use a window for the placer
    window_size: The size of the window for the placer
    dynamic_placement: Whether to use dynamic placement for the placer
    routing_method: The routing method that should be used for the independent set router
    prefer_split: The threshold factor for group merging decisions during routing.
    warn_unsupported_gates: Whether to warn about unsupported gates in the code generator)pb");
  }

  routingAgnosticCompiler.def_static(
      "from_json_string",
      [](const na::zoned::Architecture& arch,
         const std::string& json) -> na::zoned::RoutingAgnosticCompiler {
        // The correct header <nlohmann/json.hpp> is included, but clang-tidy
        // confuses it with the wrong forward header <nlohmann/json_fwd.hpp>
        // NOLINTNEXTLINE(misc-include-cleaner)
        return {arch, nlohmann::json::parse(json)};
      },
      "arch"_a, "json"_a,
      R"pb(Create a compiler for the given architecture and with configurations from a JSON string.

Args:
    arch: The zoned neutral atom architecture
    json: The JSON string

Returns:
    The initialized compiler

Raises:
    ValueError: If the string is not a valid JSON string)pb");

  routingAgnosticCompiler.def(
      "compile",
      [](na::zoned::RoutingAgnosticCompiler& self,
         const qc::QuantumComputation& qc) -> std::string {
        return self.compile(qc).toString();
      },
      "qc"_a,
      R"pb(Compile a quantum circuit for the zoned neutral atom architecture.

Args:
    qc: The quantum circuit

Returns:
    The compilation result as a string in the .naviz format.)pb");

  routingAgnosticCompiler.def(
      "stats",
      [](const na::zoned::RoutingAgnosticCompiler& self) {
        const auto json = nb::module_::import_("json");
        const auto loads = json.attr("loads");
        const nlohmann::json stats = self.getStatistics();
        const auto dict = loads(stats.dump());
        return nb::cast<nb::typed<nb::dict, nb::str, nb::float_>>(dict);
      },
      R"pb(Get the statistics of the last compilation as a JSON-style dictionary.

Returns:
    The statistics as a JSON-style dictionary)pb");

  //===--------------------------------------------------------------------===//
  // Routing-aware Compiler
  //===--------------------------------------------------------------------===//
  nb::class_<na::zoned::RoutingAwareCompiler> routingAwareCompiler(
      m, "RoutingAwareCompiler", "Routing-aware zoned neutral atom compiler.");
  {
    const na::zoned::RoutingAwareCompiler::Config defaultConfig;
    routingAwareCompiler.def(
        "__init__",
        [](na::zoned::RoutingAwareCompiler* self,
           const na::zoned::Architecture& arch, const std::string& logLevel,
           const double maxFillingFactor, const bool useWindow,
           const size_t windowMinWidth, const double windowRatio,
           const double windowShare,
           const na::zoned::HeuristicPlacer::Config::Method placementMethod,
           const float deepeningFactor, const float deepeningValue,
           const float lookaheadFactor, const float reuseLevel,
           const size_t maxNodes, const size_t trials,
           const size_t queueCapacity,
           const na::zoned::IndependentSetRouter::Config::Method routingMethod,
           const double preferSplit, const bool warnUnsupportedGates) {
          na::zoned::RoutingAwareCompiler::Config config;
          config.logLevel = spdlog::level::from_str(logLevel);
          config.schedulerConfig.maxFillingFactor = maxFillingFactor;
          config.layoutSynthesizerConfig.placerConfig = {
              .useWindow = useWindow,
              .windowMinWidth = windowMinWidth,
              .windowRatio = windowRatio,
              .windowShare = windowShare,
              .method = placementMethod,
              .deepeningFactor = deepeningFactor,
              .deepeningValue = deepeningValue,
              .lookaheadFactor = lookaheadFactor,
              .reuseLevel = reuseLevel,
              .maxNodes = maxNodes,
              .trials = trials,
              .queueCapacity = queueCapacity,
          };
          config.layoutSynthesizerConfig.routerConfig = {
              .method = routingMethod, .preferSplit = preferSplit};
          config.codeGeneratorConfig = {.warnUnsupportedGates =
                                            warnUnsupportedGates};
          new (self) na::zoned::RoutingAwareCompiler{arch, config};
        },
        nb::keep_alive<1, 2>(), "arch"_a,
        "log_level"_a = spdlog::level::to_short_c_str(defaultConfig.logLevel),
        "max_filling_factor"_a = defaultConfig.schedulerConfig.maxFillingFactor,
        "use_window"_a =
            defaultConfig.layoutSynthesizerConfig.placerConfig.useWindow,
        "window_min_width"_a =
            defaultConfig.layoutSynthesizerConfig.placerConfig.windowMinWidth,
        "window_ratio"_a =
            defaultConfig.layoutSynthesizerConfig.placerConfig.windowRatio,
        "window_share"_a =
            defaultConfig.layoutSynthesizerConfig.placerConfig.windowShare,
        "placement_method"_a =
            defaultConfig.layoutSynthesizerConfig.placerConfig.method,
        "deepening_factor"_a =
            defaultConfig.layoutSynthesizerConfig.placerConfig.deepeningFactor,
        "deepening_value"_a =
            defaultConfig.layoutSynthesizerConfig.placerConfig.deepeningValue,
        "lookahead_factor"_a =
            defaultConfig.layoutSynthesizerConfig.placerConfig.lookaheadFactor,
        "reuse_level"_a =
            defaultConfig.layoutSynthesizerConfig.placerConfig.reuseLevel,
        "max_nodes"_a =
            defaultConfig.layoutSynthesizerConfig.placerConfig.maxNodes,
        "trials"_a = defaultConfig.layoutSynthesizerConfig.placerConfig.trials,
        "queue_capacity"_a =
            defaultConfig.layoutSynthesizerConfig.placerConfig.queueCapacity,
        "routing_method"_a =
            defaultConfig.layoutSynthesizerConfig.routerConfig.method,
        "prefer_split"_a =
            defaultConfig.layoutSynthesizerConfig.routerConfig.preferSplit,
        "warn_unsupported_gates"_a =
            defaultConfig.codeGeneratorConfig.warnUnsupportedGates,
        R"pb(Create a routing-aware compiler for the given architecture and configurations.

Args:
    arch: The zoned neutral atom architecture
    log_level: The log level for the compiler, possible values are "debug"/"D", "info"/"I", "warning"/"W", "error"/"E", and "critical"/"C"
    max_filling_factor: The maximum filling factor for the entanglement zone, i.e., it sets the limit for the maximum number of entangling gates that are scheduled in parallel
    use_window: Whether to use a window for the placer
    window_min_width: The minimum width of the window for the placer
    window_ratio: The ratio between the height and the width of the window
    window_share: The share of free sites in the window in relation to the number of atoms to be moved in this step
    placement_method: The placement method that should be used for the heuristic placer
    deepening_factor: Controls the impact of the term in the heuristic of the A* search that resembles the standard deviation of the differences between the current and target sites of the atoms to be moved in every orientation
    deepening_value: Is added to the sum of standard deviations before it is multiplied with the number of unplaced nodes and :attr:`deepening_factor`
    lookahead_factor: Controls the lookahead's influence that considers the distance of atoms to their interaction partner in the next layer
    reuse_level: The reuse level that corresponds to the estimated extra fidelity loss due to the extra trap transfers when the atom is not reused and instead moved to the storage zone and back to the entanglement zone
    max_nodes: The maximum number of nodes that are considered in the A* search.
        If this number is exceeded, the search is aborted and an error is raised.
        In the current implementation, one node roughly consumes 120 Byte.
        Hence, allowing 50,000,000 nodes results in memory consumption of about 6 GB plus the size of the rest of the data structures.
    trials: The number of restarts during IDS.
    queue_capacity: The maximum capacity of the priority queue used during IDS.
    routing_method: The routing method that should be used for the independent set router
    prefer_split: The threshold factor for group merging decisions during routing.
    warn_unsupported_gates: Whether to warn about unsupported gates in the code generator)pb");
  }

  routingAwareCompiler.def_static(
      "from_json_string",
      [](const na::zoned::Architecture& arch,
         const std::string& json) -> na::zoned::RoutingAwareCompiler {
        // The correct header <nlohmann/json.hpp> is included, but clang-tidy
        // confuses it with the wrong forward header <nlohmann/json_fwd.hpp>
        // NOLINTNEXTLINE(misc-include-cleaner)
        return {arch, nlohmann::json::parse(json)};
      },
      "arch"_a, "json"_a,
      R"pb(Create a compiler for the given architecture and configurations from a JSON string.

Args:
    arch: The zoned neutral atom architecture
    json: The JSON string

Returns:
    The initialized compiler

Raises:
    ValueError: If the string is not a valid JSON string)pb");

  routingAwareCompiler.def(
      "compile",
      [](na::zoned::RoutingAwareCompiler& self,
         const qc::QuantumComputation& qc) -> std::string {
        return self.compile(qc).toString();
      },
      "qc"_a,
      R"pb(Compile a quantum circuit for the zoned neutral atom architecture.

Args:
    qc: The quantum circuit

Returns:
    The compilation result as a string in the .naviz format.)pb");

  routingAwareCompiler.def(
      "stats",
      [](const na::zoned::RoutingAwareCompiler& self) {
        const auto json = nb::module_::import_("json");
        const auto loads = json.attr("loads");
        const nlohmann::json stats = self.getStatistics();
        const auto dict = loads(stats.dump());
        return nb::cast<nb::typed<nb::dict, nb::str, nb::float_>>(dict);
      },
      R"pb(Get the statistics of the last compilation.

Returns:
    The statistics as a dictionary)pb");
}
