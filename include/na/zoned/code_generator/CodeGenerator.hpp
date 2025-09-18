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

#include "na/NAComputation.hpp"
#include "na/entities/Atom.hpp"
#include "na/entities/Zone.hpp"
#include "na/zoned/Architecture.hpp"
#include "na/zoned/Types.hpp"

#include <cstddef>
#include <vector>

namespace na::zoned {
/**
 * The class CodeGenerator implements the code generation for the zoned neutral
 * atom compiler.
 */
class CodeGenerator {
  /// The architecture of the neutral atom system
  std::reference_wrapper<const Architecture> architecture_;

public:
  /// The configuration of the CodeGenerator
  struct Config {
    /// The offset for parking spots
    size_t parkingOffset = 1;
    /**
     * Warn if a gate not belonging to the basis gates (local rz, global ry) is
     * used
     */
    bool warnUnsupportedGates = true;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Config, parkingOffset,
                                                warnUnsupportedGates);
  };

private:
  /// The configuration of the CodeGenerator
  Config config_;

public:
  /**
   * Create a new CodeGenerator.
   * @details The code generation is based on the given architecture and the
   * placement and routing of the qubits. It generates a neutral atom
   * computation.
   * @param architecture is the architecture of the neutral atom system
   * @param config is the configuration of the code generator.
   */
  CodeGenerator(const Architecture& architecture, const Config& config)
      : architecture_(architecture), config_(config) {}
  /**
   * Generate the neutral atom computation based on the results of the previous
   * steps in the compiler.
   * @param singleQubitGateLayers is a list of layers of single-qubit gates
   * @param placement is the placement of the qubits. The very first entry is
   * the initial placement of the atoms. Every consecutive pair of entries
   * encloses one layer of two-qubit gates.
   * @param routing is the routing of the qubits. It consists of groups of atoms
   * that can be moved together to establish the next placement.
   * @return the neutral atom computation
   */
  [[nodiscard]] auto
  generate(const std::vector<SingleQubitGateLayer>& singleQubitGateLayers,
           const std::vector<Placement>& placement,
           const std::vector<Routing>& routing) const -> NAComputation;

private:
  /// Append all single-qubit gates of a layer to the code
  auto appendSingleQubitGates(
      size_t nQubits, const SingleQubitGateLayer& singleQubitGates,
      const std::vector<std::reference_wrapper<const Atom>>& atoms,
      const Zone& globalZone, NAComputation& code) const -> void;

  /// Append all necessary operations to perform the next set of two-qubit gates
  auto appendTwoQubitGates(
      const Placement& currentPlacement, const Routing& executionRouting,
      const Placement& executionPlacement, const Routing& targetRouting,
      const Placement& targetPlacement,
      const std::vector<std::reference_wrapper<const Atom>>& atoms,
      const std::vector<std::reference_wrapper<const Zone>>& zones,
      NAComputation& code) const -> void;

  /// Append all necessary operations to rearrange the atoms
  auto appendRearrangement(
      const Placement& startPlacement, const Routing& routing,
      const Placement& targetPlacement,
      const std::vector<std::reference_wrapper<const Atom>>& atoms,
      NAComputation& code) const -> void;
};
} // namespace na::zoned
