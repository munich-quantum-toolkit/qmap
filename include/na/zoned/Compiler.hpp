/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#pragma once

#include "Architecture.hpp"
#include "code_generator/CodeGenerator.hpp"
#include "ir/QuantumComputation.hpp"
#include "ir/operations/Operation.hpp"
#include "layout_synthesizer/PlaceAndRouteSynthesizer.hpp"
#include "layout_synthesizer/placer/AStarPlacer.hpp"
#include "layout_synthesizer/placer/VertexMatchingPlacer.hpp"
#include "layout_synthesizer/router/IndependentSetRouter.hpp"
#include "na/NAComputation.hpp"
#include "reuse_analyzer/VertexMatchingReuseAnalyzer.hpp"
#include "scheduler/ASAPScheduler.hpp"

#include <cassert>
#include <chrono>
#include <functional>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace na::zoned {
#define SELF (*static_cast<ConcreteType*>(this))

/** @brief Compiler class that combines various components to compile quantum
 * circuits for neutral atom architectures.
 *
 * @details This class is a template that allows for different implementations
 * of the scheduler, reuse analyzer, placer, router, and code generator. It
 * provides a unified interface to compile quantum computations into
 * NAComputation objects. The components are linked together at compile time,
 * allowing for better performance than having the components as members of the
 * compiler and setting them at runtime.
 */
template <class ConcreteType, class Scheduler, class ReuseAnalyzer,
          class LayoutSynthesizer, class CodeGenerator>
class Compiler : protected Scheduler,
                 protected ReuseAnalyzer,
                 protected LayoutSynthesizer,
                 protected CodeGenerator {
  friend ConcreteType;

public:
  /**
   * Collection of the configuration parameters for the different components
   * of the compiler.
   */
  struct Config {
    /// Configuration for the scheduler
    typename Scheduler::Config schedulerConfig{};
    /// Configuration for the reuse analyzer
    typename ReuseAnalyzer::Config reuseAnalyzerConfig{};
    /// Configuration for the layout synthesizer
    typename LayoutSynthesizer::Config layoutSynthesizerConfig{};
    /// Configuration for the code generator
    typename CodeGenerator::Config codeGeneratorConfig{};
    /// Log level for the compiler
    spdlog::level::level_enum logLevel = spdlog::level::info;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Config, schedulerConfig,
                                                reuseAnalyzerConfig,
                                                layoutSynthesizerConfig,
                                                codeGeneratorConfig, logLevel);
  };
  /**
   * Collection of statistics collected during the compilation process for the
   * different components.
   */
  struct Statistics {
    int64_t schedulingTime;    ///< Time taken for scheduling in us
    int64_t reuseAnalysisTime; ///< Time taken for reuse analysis in us
    /// Statistics collected during layout synthesis.
    typename LayoutSynthesizer::Statistics layoutSynthesizerStatistics;
    int64_t layoutSynthesisTime; ///< Time taken for layout synthesis in us
    int64_t codeGenerationTime;  ///< Time taken for code generation in us
    int64_t totalTime;           ///< Total time taken for the compilation in us
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_ONLY_SERIALIZE(Statistics, schedulingTime,
                                                  reuseAnalysisTime,
                                                  layoutSynthesizerStatistics,
                                                  layoutSynthesisTime,
                                                  codeGenerationTime,
                                                  totalTime);
  };

private:
  std::reference_wrapper<const Architecture> architecture_;
  nlohmann::json config_;
  Statistics statistics_;

  /**
   * Construct a Compiler instance with the given architecture and
   * configuration.
   *
   * @param architecture The architecture to compile for.
   * @param config The configuration for the compiler.
   */
  Compiler(const Architecture& architecture, const Config& config)
      : Scheduler(architecture, config.schedulerConfig),
        ReuseAnalyzer(architecture, config.reuseAnalyzerConfig),
        LayoutSynthesizer(architecture, config.layoutSynthesizerConfig),
        CodeGenerator(architecture, config.codeGeneratorConfig),
        architecture_(architecture), config_(config) {
    spdlog::set_level(config.logLevel);
  }

  /**
   * Construct a Compiler instance with the given architecture and
   * default configuration.
   *
   * @param architecture The architecture to compile for.
   */
  explicit Compiler(const Architecture& architecture)
      : Compiler(architecture, {}) {}

public:
  /**
   * Compile a quantum computation into a neutral atom computation.
   *
   * @param qComp is the quantum computation to compile.
   * @return an NAComputation object that represents the compiled quantum
   * circuit.
   */
  [[nodiscard]] auto compile(const qc::QuantumComputation& qComp)
      -> NAComputation {
    SPDLOG_INFO("*** MQT QMAP Zoned Neutral Atom Compiler ***");
#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_DEBUG
    if (spdlog::should_log(spdlog::level::debug)) {
      SPDLOG_DEBUG("Used compiler settings:");
      std::string jsonStr =
          config_.dump(2); // Pretty-print with 2-space indentation
      std::istringstream iss(jsonStr);
      std::string line;
      while (std::getline(iss, line)) {
        SPDLOG_DEBUG(line);
      }
      SPDLOG_DEBUG("Number of qubits: {}", qComp.getNqubits());
      const auto nTwoQubitGates = static_cast<size_t>(
          std::count_if(qComp.cbegin(), qComp.cend(),
                        [](const std::unique_ptr<qc::Operation>& op) {
                          return op->getNqubits() == 2;
                        }));
      SPDLOG_DEBUG("Number of two-qubit gates: {}", nTwoQubitGates);
      const auto nSingleQubitGates = static_cast<size_t>(
          std::count_if(qComp.cbegin(), qComp.cend(),
                        [](const std::unique_ptr<qc::Operation>& op) {
                          return op->getNqubits() == 1;
                        }));
      SPDLOG_DEBUG("Number of single-qubit gates: {}", nSingleQubitGates);
    }
#endif // SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_DEBUG

    const auto& schedulingStart = std::chrono::system_clock::now();
    // CodeQL was not very happy about the structural binding here, hence I
    // removed it.
    const auto& schedule = SELF.schedule(qComp);
    const auto& singleQubitGateLayers = schedule.first;
    const auto& twoQubitGateLayers = schedule.second;
    const auto& schedulingEnd = std::chrono::system_clock::now();
    const auto& reuseQubits = SELF.analyzeReuse(twoQubitGateLayers);
    const auto& reuseAnalysisEnd = std::chrono::system_clock::now();
    const auto& [placement, routing] = LayoutSynthesizer::synthesize(
        qComp.getNqubits(), twoQubitGateLayers, reuseQubits);
    const auto& layoutSynthesisEnd = std::chrono::system_clock::now();
    NAComputation code =
        SELF.generate(singleQubitGateLayers, placement, routing);
    const auto& codeGenerationEnd = std::chrono::system_clock::now();
    assert(code.validate().first);

    statistics_.schedulingTime =
        std::chrono::duration_cast<std::chrono::microseconds>(schedulingEnd -
                                                              schedulingStart)
            .count();
    SPDLOG_INFO("Time for scheduling: {}us", statistics_.schedulingTime);
#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_DEBUG
    SPDLOG_DEBUG("Number of single-qubit gate layers: {}",
                 singleQubitGateLayers.size());
    SPDLOG_DEBUG("Number of two-qubit gate layers: {}",
                 twoQubitGateLayers.size());
    if (!twoQubitGateLayers.empty() &&
        spdlog::should_log(spdlog::level::debug)) {
      const auto& [min, sum, max] = std::accumulate(
          twoQubitGateLayers.cbegin(), twoQubitGateLayers.cend(),
          std::array<size_t, 3>{std::numeric_limits<size_t>::max(), 0UL, 0UL},
          [](const auto& acc, const auto& layer) -> std::array<size_t, 3> {
            const auto& [minAcc, sumAcc, maxAcc] = acc;
            const auto n = layer.size();
            return {std::min(minAcc, n), sumAcc + n, std::max(maxAcc, n)};
          });
      const auto avg = static_cast<double>(sum) /
                       static_cast<double>(twoQubitGateLayers.size());
      SPDLOG_DEBUG(
          "Number of two-qubit gates per layer: min: {}, avg: {}, max: {}", min,
          avg, max);
    }
#endif // SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_DEBUG
    statistics_.reuseAnalysisTime =
        std::chrono::duration_cast<std::chrono::microseconds>(reuseAnalysisEnd -
                                                              schedulingEnd)
            .count();
    SPDLOG_INFO("Time for reuse analysis: {}us", statistics_.reuseAnalysisTime);
    statistics_.layoutSynthesisTime =
        std::chrono::duration_cast<std::chrono::microseconds>(
            layoutSynthesisEnd - reuseAnalysisEnd)
            .count();
    statistics_.layoutSynthesizerStatistics =
        SELF.getLayoutSynthesisStatistics();
    SPDLOG_INFO("Time for layout synthesis: {}us",
                statistics_.layoutSynthesisTime);
    statistics_.codeGenerationTime =
        std::chrono::duration_cast<std::chrono::microseconds>(
            codeGenerationEnd - layoutSynthesisEnd)
            .count();
    SPDLOG_INFO("Time for code generation: {}us",
                statistics_.codeGenerationTime);
    statistics_.totalTime =
        std::chrono::duration_cast<std::chrono::microseconds>(
            codeGenerationEnd - schedulingStart)
            .count();
    SPDLOG_INFO("Total time: {}us", statistics_.totalTime);
    return code;
  }
  /// @return the statistics collected during the compilation process.
  [[nodiscard]] auto getStatistics() const -> const Statistics& {
    return statistics_;
  }
};

class RoutingAgnosticSynthesizer
    : public PlaceAndRouteSynthesizer<RoutingAgnosticSynthesizer,
                                      VertexMatchingPlacer,
                                      IndependentSetRouter> {
public:
  RoutingAgnosticSynthesizer(const Architecture& architecture,
                             const Config& config)
      : PlaceAndRouteSynthesizer(architecture, config) {}
  explicit RoutingAgnosticSynthesizer(const Architecture& architecture)
      : PlaceAndRouteSynthesizer(architecture) {}
};
class RoutingAgnosticCompiler final
    : public Compiler<RoutingAgnosticCompiler, ASAPScheduler,
                      VertexMatchingReuseAnalyzer, RoutingAgnosticSynthesizer,
                      CodeGenerator> {
public:
  RoutingAgnosticCompiler(const Architecture& architecture,
                          const Config& config)
      : Compiler(architecture, config) {}
  RoutingAgnosticCompiler(const Architecture& architecture)
      : Compiler(architecture) {}
};

class RoutingAwareSynthesizer
    : public PlaceAndRouteSynthesizer<RoutingAwareSynthesizer, AStarPlacer,
                                      IndependentSetRouter> {
public:
  RoutingAwareSynthesizer(const Architecture& architecture,
                          const Config& config)
      : PlaceAndRouteSynthesizer(architecture, config) {}
  explicit RoutingAwareSynthesizer(const Architecture& architecture)
      : PlaceAndRouteSynthesizer(architecture) {}
};
class RoutingAwareCompiler final
    : public Compiler<RoutingAwareCompiler, ASAPScheduler,
                      VertexMatchingReuseAnalyzer, RoutingAwareSynthesizer,
                      CodeGenerator> {
public:
  RoutingAwareCompiler(const Architecture& architecture, const Config& config)
      : Compiler(architecture, config) {}
  RoutingAwareCompiler(const Architecture& architecture)
      : Compiler(architecture) {}
};
} // namespace na::zoned
