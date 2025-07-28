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

#include "LayoutSynthesizerBase.hpp"
#include "na/zoned/Types.hpp"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <unordered_set>
#include <vector>

namespace na::zoned {
#define SELF (*static_cast<ConcreteType*>(this))

/**
 * @brief Layout Synthesizer based on separate placement and routing
 * strategies.
 *
 * @details This class synthesizes a layout for a quantum computation by first
 * placing the qubits using a specified placer and then routing the qubits using
 * a specified router. It is designed to be used with different placer and
 * router implementations, allowing for flexibility in the synthesis process.
 *
 * @tparam ConcreteType is the type of the concrete class that inherits from
 * this PlacementAndRoutingSynthesizer. This is used to implement the Curiously
 * Recurring Template Pattern (CRTP) to allow the concrete class to access the
 * methods of the placer and router.
 * @tparam Placer is the type of the placer used for placing qubits. It should
 * implement a `place` method that takes the number of qubits, two-qubit gate
 * layers, and reuse qubits, and returns a vector of `Placement` objects.
 * @tparam Router is the type of the router used for routing qubits. It should
 * implement a `route` method that takes a vector of `Placement` objects and
 * returns a vector of `Routing` objects.
 */
template <class ConcreteType, class Placer, class Router>
class PlacementAndRoutingSynthesizer : public LayoutSynthsizerBase,
                                       protected Placer,
                                       protected Router {
  friend ConcreteType;

public:
  /**
   * Collection of the configuration parameters for the placer and
   * router.
   */
  struct Config {
    /// Configuration for the placer
    typename Placer::Config placerConfig{};
    /// Configuration for the router
    typename Router::Config routerConfig{};
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Config, placerConfig,
                                                routerConfig);
  };
  /**
   * Collection of statistics collected during the synthesis process for the
   * placement and routing synthesizer.
   */
  struct Statistics {
    int64_t placementTime; ///< Time taken for placement in us
    int64_t routingTime;   ///< Time taken for routing in us
    int64_t totalTime;     ///< Total time taken for the synthesis in us
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_ONLY_SERIALIZE(Statistics, placementTime,
                                                  routingTime, totalTime);
  };

private:
  /// The statistics collected during the synthesis process
  Statistics statistics_;
  /**
   * Construct a PlacementAndRoutingSynthesizer instance with the given
   * configuration.
   *
   * @param config The configuration for the placer and router.
   */
  explicit PlacementAndRoutingSynthesizer(const Config& config)
      : Placer(config.placerConfig), Router(config.routerConfig) {}

public:
  [[nodiscard]] auto synthesize(
      size_t nQubits, const std::vector<TwoQubitGateLayer>& twoQubitGateLayers,
      const std::vector<std::unordered_set<qc::Qubit>>& reuseQubits) -> Layout {
    SPDLOG_INFO("*** Placement and Routing Layout Synthesizer ***");

    const auto& placementStart = std::chrono::system_clock::now();
    const auto& placement =
        SELF.place(nQubits, twoQubitGateLayers, reuseQubits);
    statistics_.placementTime =
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now() - placementStart)
            .count();
    SPDLOG_INFO("Time for placement: {}us", statistics_.placementTime);

    const auto& routingStart = std::chrono::system_clock::now();
    const auto& routing = SELF.route(placement);
    statistics_.routingTime =
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now() - routingStart)
            .count();
    SPDLOG_INFO("Time for routing: {}us", statistics_.routingTime);

    return {placement, routing};
  }
  /// @returns the statistics collected during the synthesis process.
  [[nodiscard]] auto getStatistics() const -> const Statistics& {
    return statistics_;
  }
};
} // namespace na::zoned
