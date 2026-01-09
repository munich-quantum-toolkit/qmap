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
// The header <nlohmann/json.hpp> is used, but clang-tidy confuses it with the
// wrong forward header <nlohmann/json_fwd.hpp>
// NOLINTNEXTLINE(misc-include-cleaner)
#include <nlohmann/json.hpp>
#include <pybind11/attr.h>
#include <pybind11/cast.h>
#include <pybind11/detail/common.h>
#include <pybind11/native_enum.h>
#include <pybind11/pybind11.h>
// NOLINTNEXTLINE(misc-include-cleaner)
#include <pybind11_json/pybind11_json.hpp>
#include <spdlog/common.h>
#include <string>

namespace py = pybind11;
using namespace pybind11::literals;

PYBIND11_MODULE(MQT_QMAP_MODULE_NAME, m, py::mod_gil_not_used()) {
  py::class_<na::zoned::Architecture> architecture(
      m, "ZonedNeutralAtomArchitecture");
  architecture.def_static("from_json_file",
                          &na::zoned::Architecture::fromJSONFile, "filename"_a);
  architecture.def_static("from_json_string",
                          &na::zoned::Architecture::fromJSONString, "json"_a);
  architecture.def(
      "to_namachine_file",
      [](na::zoned::Architecture& self, const std::string& filename) -> void {
        self.exportNAVizMachine(filename);
      },
      "filename"_a);
  architecture.def("to_namachine_string",
                   [](na::zoned::Architecture& self) -> std::string {
                     return self.exportNAVizMachine();
                   });

  //===--------------------------------------------------------------------===//
  // Placement Method Enum
  //===--------------------------------------------------------------------===//
  py::native_enum<na::zoned::HeuristicPlacer::Config::Method>(
      m, "PlacementMethod", "enum.Enum")
      .value("astar", na::zoned::HeuristicPlacer::Config::Method::ASTAR)
      .value("ids", na::zoned::HeuristicPlacer::Config::Method::IDS)
      .export_values()
      .finalize();

  //===--------------------------------------------------------------------===//
  // Routing Method Enum
  //===--------------------------------------------------------------------===//
  py::native_enum<na::zoned::IndependentSetRouter::Config::Method>(
      m, "RoutingMethod", "enum.Enum")
      .value("strict", na::zoned::IndependentSetRouter::Config::Method::STRICT)
      .value("relaxed",
             na::zoned::IndependentSetRouter::Config::Method::RELAXED)
      .export_values()
      .finalize();

  //===--------------------------------------------------------------------===//
  // Routing-agnostic Compiler
  //===--------------------------------------------------------------------===//
  py::class_<na::zoned::RoutingAgnosticCompiler> routingAgnosticCompiler(
      m, "RoutingAgnosticCompiler");
  {
    const na::zoned::RoutingAgnosticCompiler::Config defaultConfig;
    routingAgnosticCompiler.def(
        py::init([](const na::zoned::Architecture& arch,
                    const std::string& logLevel, const double maxFillingFactor,
                    const bool useWindow, const size_t windowSize,
                    const bool dynamicPlacement,
                    const na::zoned::IndependentSetRouter::Config::Method
                        routingMethod,
                    const double preferSplit, const bool warnUnsupportedGates)
                     -> na::zoned::RoutingAgnosticCompiler {
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
          return {arch, config};
        }),
        py::keep_alive<1, 2>(), "arch"_a,
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
            defaultConfig.codeGeneratorConfig.warnUnsupportedGates);
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
      "arch"_a, "json"_a);
  routingAgnosticCompiler.def(
      "compile",
      [](na::zoned::RoutingAgnosticCompiler& self,
         const qc::QuantumComputation& qc) -> std::string {
        return self.compile(qc).toString();
      },
      "qc"_a);
  routingAgnosticCompiler.def(
      "stats",
      [](const na::zoned::RoutingAgnosticCompiler& self) -> nlohmann::json {
        return self.getStatistics();
      });

  //===--------------------------------------------------------------------===//
  // Routing-aware Compiler
  //===--------------------------------------------------------------------===//
  py::class_<na::zoned::RoutingAwareCompiler> routingAwareCompiler(
      m, "RoutingAwareCompiler");
  {
    const na::zoned::RoutingAwareCompiler::Config defaultConfig;
    routingAwareCompiler.def(
        py::init(
            [](const na::zoned::Architecture& arch, const std::string& logLevel,
               const double maxFillingFactor, const bool useWindow,
               const size_t windowMinWidth, const double windowRatio,
               const double windowShare,
               const na::zoned::HeuristicPlacer::Config::Method placementMethod,
               const float deepeningFactor, const float deepeningValue,
               const float lookaheadFactor, const float reuseLevel,
               const size_t maxNodes, const size_t trials,
               const size_t queueCapacity,
               const na::zoned::IndependentSetRouter::Config::Method
                   routingMethod,
               const double preferSplit, const bool warnUnsupportedGates)
                -> na::zoned::RoutingAwareCompiler {
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
              return {arch, config};
            }),
        py::keep_alive<1, 2>(), "arch"_a,
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
            defaultConfig.codeGeneratorConfig.warnUnsupportedGates);
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
      "arch"_a, "json"_a);
  routingAwareCompiler.def(
      "compile",
      [](na::zoned::RoutingAwareCompiler& self,
         const qc::QuantumComputation& qc) -> std::string {
        return self.compile(qc).toString();
      },
      "qc"_a);
  routingAwareCompiler.def(
      "stats",
      [](const na::zoned::RoutingAwareCompiler& self) -> nlohmann::json {
        return self.getStatistics();
      });

  //===--------------------------------------------------------------------===//
  // Placement and Routing-aware Compiler
  //===--------------------------------------------------------------------===//
  py::class_<na::zoned::PlacementAndRoutingAwareCompiler>
      placementAndRoutingAwareCompiler(m, "PlacementAndRoutingAwareCompiler");
  {
    const na::zoned::PlacementAndRoutingAwareCompiler::Config defaultConfig;
    placementAndRoutingAwareCompiler.def(
        py::init(
            [](const na::zoned::Architecture& arch, const std::string& logLevel,
               const double maxFillingFactor, const bool useWindow,
               const size_t windowMinWidth, const double windowRatio,
               const double windowShare,
               const na::zoned::HeuristicPlacer::Config::Method placementMethod,
               const float deepeningFactor, const float deepeningValue,
               const float lookaheadFactor, const float reuseLevel,
               const size_t maxNodes, const size_t trials,
               const size_t queueCapacity,
               const na::zoned::IndependentSetRouter::Config::Method
                   routingMethod,
               const double preferSplit, const bool warnUnsupportedGates)
                -> na::zoned::PlacementAndRoutingAwareCompiler {
              na::zoned::PlacementAndRoutingAwareCompiler::Config config;
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
              return {arch, config};
            }),
        py::keep_alive<1, 2>(), "arch"_a,
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
            defaultConfig.codeGeneratorConfig.warnUnsupportedGates);
  }
  placementAndRoutingAwareCompiler.def_static(
      "from_json_string",
      [](const na::zoned::Architecture& arch, const std::string& json)
          -> na::zoned::PlacementAndRoutingAwareCompiler {
        // The correct header <nlohmann/json.hpp> is included, but clang-tidy
        // confuses it with the wrong forward header <nlohmann/json_fwd.hpp>
        // NOLINTNEXTLINE(misc-include-cleaner)
        return {arch, nlohmann::json::parse(json)};
      },
      "arch"_a, "json"_a);
  placementAndRoutingAwareCompiler.def(
      "compile",
      [](na::zoned::PlacementAndRoutingAwareCompiler& self,
         const qc::QuantumComputation& qc) -> std::string {
        return self.compile(qc).toString();
      },
      "qc"_a);
  placementAndRoutingAwareCompiler.def(
      "stats",
      [](const na::zoned::PlacementAndRoutingAwareCompiler& self)
          -> nlohmann::json { return self.getStatistics(); });
}
