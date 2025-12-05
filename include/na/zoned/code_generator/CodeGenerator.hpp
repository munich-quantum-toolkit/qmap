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
    /**
     * Warn if a gate not belonging to the basis gates (local rz, global ry) is
     * used
     */
    bool warnUnsupportedGates = true;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Config, warnUnsupportedGates);
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

  /**
   * @brief An auxiliary class to create one rearrangement step.
   * @details One rearrangement step comprises all shuttling operations from
   * the first load operation to the last store operation before the next load
   * operation or unitary operation (gate).
   */
  class RearrangementGenerator {
    /// The architecture of the neutral atom system
    std::reference_wrapper<const Architecture> architecture_;
    struct QubitMovement {
      enum class SiteKind {
        STORAGE,
        ENTANGLEMENT_LEFT,
        ENTANGLEMENT_RIGHT,
      };
      SiteKind sourceSite;
      int64_t sourceX;
      int64_t sourceY;
      SiteKind targetSite;
      int64_t targetX;
      int64_t targetY;
    };
    std::unordered_map<qc::Qubit, QubitMovement> movements_;
    int64_t sourceDx_ = 0;
    int64_t sourceDy_ = 0;
    int64_t sourceMinX_ = 0;
    int64_t sourceMinY_ = 0;
    int64_t sourceMaxX_ = 0;
    int64_t sourceMaxY_ = 0;
    int64_t targetDx_ = 0;
    int64_t targetDy_ = 0;
    int64_t targetMinX_ = 0;
    int64_t targetMinY_ = 0;
    int64_t targetMaxX_ = 0;
    int64_t targetMaxY_ = 0;
    int64_t pairSep_ = 0;
    // Since rows cannot split, this map collects the start (key) and end
    // (value) y-position of each row that must be moved. It is intentionally an
    // 'ordered' map to save the sorting afterward.
    std::map<int64_t, int64_t> verticalMoves_;
    // Since columns cannot split, this map collects the start (key) and end
    // (value) x-position of each column that must be moved. It is intentionally
    // an 'ordered' map to save the sorting afterward.
    std::map<int64_t, int64_t> horizontalMoves_;
    bool identicalRowOrder_ = false;
    bool identicalColumnOrder_ = false;
    enum class RearrangementDirection {
      UP,
      DOWN,
    };
    RearrangementDirection rearrangementDirection_ = RearrangementDirection::UP;
    // A map from activated AOD columns to their current x-coordinate. This is
    // intentionally an 'ordered' map to ease the pushing of activated columns.
    std::map<size_t, int64_t> aodColsToX_;
    // A map from activated AOD rows to their current y-coordinate. This is
    // intentionally an 'ordered' map to ease the pushing of activated rows.
    std::map<size_t, int64_t> aodRowsToY_;
    // A map of shuttling qubits to their current location. This is required to
    // compare their latest position with their new position to check whether
    // they moved and need to be included in a move operation. This map is
    // intentionally an 'ordered' map to ensure deterministic (ordered atoms)
    // code generation.
    std::map<qc::Qubit, std::pair<int64_t, int64_t>>
        shuttlingQubitToCurrentLocation_;

    [[nodiscard]] auto getLocationFromSite(const Site& site)
        -> std::pair<int64_t, int64_t>;
    [[nodiscard]] static auto getSiteKindFromSite(const Site& site)
        -> QubitMovement::SiteKind;
    auto
    addSourceMove(const std::unordered_map<int64_t, size_t>& sourceXToAodCol,
                  const std::unordered_map<int64_t, size_t>& sourceYToAodRow,
                  const std::vector<std::reference_wrapper<const Atom>>& atoms,
                  NAComputation& code) -> void;
    auto
    addTargetMove(const std::unordered_map<int64_t, size_t>& targetXToAodCol,
                  const std::unordered_map<int64_t, size_t>& targetYToAodRow,
                  const std::vector<std::reference_wrapper<const Atom>>& atoms,
                  NAComputation& code) -> void;
    auto
    loadRowByRow(const std::vector<std::reference_wrapper<const Atom>>& atoms,
                 NAComputation& code) -> void;
    auto loadColumnByColumn(
        const std::vector<std::reference_wrapper<const Atom>>& atoms,
        NAComputation& code) -> void;
    auto
    storeRowByRow(const std::vector<std::reference_wrapper<const Atom>>& atoms,
                  NAComputation& code) -> void;
    auto storeColumnByColumn(
        const std::vector<std::reference_wrapper<const Atom>>& atoms,
        NAComputation& code) -> void;

  public:
    RearrangementGenerator(const Architecture& arch,
                           const Placement& sourcePlacement,
                           const Placement& targetPlacement,
                           const std::vector<qc::Qubit>& qubits);
    auto generate(const std::vector<std::reference_wrapper<const Atom>>& atoms,
                  NAComputation& code) -> void;
  };
};
} // namespace na::zoned
