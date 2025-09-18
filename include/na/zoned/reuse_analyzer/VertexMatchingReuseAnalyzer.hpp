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

#include "ir/Definitions.hpp"
#include "na/zoned/Architecture.hpp"
#include "na/zoned/Types.hpp"
#include "na/zoned/reuse_analyzer/ReuseAnalyzerBase.hpp"

#include <cstddef>
#include <optional>
#include <unordered_set>
#include <utility>
#include <vector>

namespace na::zoned {
/**
 * The class VertexMatchingReuseAnalyzer implements the default reuse analysis
 * for the zoned neutral atom compiler that uses a bipartite maximum matching.
 */
class VertexMatchingReuseAnalyzer : public ReuseAnalyzerBase {
  friend class
      VertexMatchingReuseAnalyzerMaximumBipartiteMatchingTest_Direct_Test;
  friend class
      VertexMatchingReuseAnalyzerMaximumBipartiteMatchingTest_Inverse_Test;
  friend class
      VertexMatchingReuseAnalyzerMaximumBipartiteMatchingInvertedTest_Direct_Test;

public:
  /**
   * The configuration of the VertexMatchingReuseAnalyzer
   * @note VertexMatchingReuseAnalyzer does not have any configuration
   * parameters.
   */
  struct Config {
    template <typename BasicJsonType>
    friend void to_json(BasicJsonType& /* unused */,
                        const Config& /* unused */) {}
    template <typename BasicJsonType>
    friend void from_json(const BasicJsonType& /* unused */,
                          Config& /* unused */) {}
  };
  /**
   * Create a new VertexMatchingReuseAnalyzer.
   * @note Both parameters are unused. Hence, the constructor does nothing
   * and the function @ref analyzeReuse is a static function.
   */
  VertexMatchingReuseAnalyzer(const Architecture& /* unused */,
                              const Config& /* unused */) {}
  /// Analyze the reuse of qubits in the given two-qubit gate layers.
  [[nodiscard]] auto
  analyzeReuse(const std::vector<TwoQubitGateLayer>& twoQubitGateLayers)
      -> std::vector<std::unordered_set<qc::Qubit>>;

private:
  /**
   * Computes a maximum matching in a bipartite graph
   * https://epubs.siam.org/doi/pdf/10.1137/0202019?download=true
   */
  [[nodiscard]] static auto maximumBipartiteMatching(
      const std::vector<std::vector<std::size_t>>& sparseMatrix,
      bool inverted = false) -> std::vector<std::optional<std::size_t>>;
};
} // namespace na::zoned
