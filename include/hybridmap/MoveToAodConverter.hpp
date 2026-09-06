/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#pragma once

#include "hybridmap/HardwareQubits.hpp"
#include "hybridmap/NeutralAtomArchitecture.hpp"
#include "hybridmap/NeutralAtomDefinitions.hpp"
#include "hybridmap/NeutralAtomUtils.hpp"
#include "ir/Definitions.hpp"
#include "ir/QuantumComputation.hpp"
#include "ir/operations/OpType.hpp"
#include "na/ir/entities/Location.hpp"
#include "na/ir/operations/AodOperation.hpp"
#include "na/ir/operations/NAOpType.hpp"

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace na {
/**
 * @brief Converts abstract atom moves into concrete AOD activation/move
 * sequences.
 * @details Groups parallelizable moves, computes safe offset maneuvers for
 * loading/unloading AODs, and emits AOD operations (activate, move, deactivate)
 * respecting device constraints.
 */
class MoveToAodConverter {
  /// Result of merging one move into an AOD transition.
  enum class TransitionMergeType : std::uint8_t {
    Impossible,
    Trivial,
    Merge,
    Append
  };

  /// Merge decisions for the X and Y dimensions of one transition phase.
  struct DimensionMergeTypes {
    TransitionMergeType x;
    TransitionMergeType y;
  };

  /// Merge decisions for the activation and deactivation phases.
  struct PhaseMergeTypes {
    TransitionMergeType activation;
    TransitionMergeType deactivation;
  };

  struct AncillaAtom {
    struct XAndY {
      std::uint32_t x;
      std::uint32_t y;
      XAndY(const std::uint32_t xCoord, const std::uint32_t yCoord)
          : x(xCoord), y(yCoord) {}
    };

    XAndY coord;
    XAndY coordDodged;
    XAndY offset;
    XAndY offsetDodged;
    AncillaAtom() = delete;
    AncillaAtom(const XAndY c, const XAndY o)
        : coord(c), coordDodged(c), offset(o), offsetDodged(o) {}
  };
  using AncillaAtoms = std::vector<AncillaAtom>;

protected:
  /**
   * @brief Builds the operations for one AOD transition phase.
   * @details Tracks per-dimension moves with offsets and associated atom moves;
   * produces operations for either activation or deactivation.
   */
  struct AodTransitionBuilder {
    /**
     * @brief Single AOD movement in one dimension (x or y).
     * @details Stores initial position, movement delta, required offset to
     * avoid crossing, and whether this phase transfers an atom between static
     * and mobile traps.
     */
    struct DimensionMove {
      /// Start of the move.
      uint32_t initialPosition;
      /// Whether this phase transfers an atom between trap types.
      bool requiresAtomTransfer;
      /// Offset move required to avoid crossing.
      int32_t offset;
      /// Delta of the actual move.
      qc::fp delta;

      DimensionMove(const uint32_t initialPosition, const qc::fp delta,
                    const int32_t offset, const bool requiresAtomTransfer)
          : initialPosition(initialPosition),
            requiresAtomTransfer(requiresAtomTransfer), offset(offset),
            delta(delta) {}
    };
    /**
     * @brief Aggregate of per-dimension moves plus logical atom moves.
     * @details Represents either activation or deactivation depending on
     * context.
     */
    struct Transition {
      std::vector<std::shared_ptr<DimensionMove>> xMoves;
      std::vector<std::shared_ptr<DimensionMove>> yMoves;
      std::vector<AtomMove> moves;

      Transition(const DimensionMove& xMove, const DimensionMove& yMove,
                 const AtomMove& move)
          : moves({move}) {
        xMoves.emplace_back(std::make_unique<DimensionMove>(xMove));
        yMoves.emplace_back(std::make_unique<DimensionMove>(yMove));
      }
      Transition(const AodOperation::Dimension dimension,
                 const DimensionMove& dimensionMove, const AtomMove& move)
          : moves({move}) {
        if (dimension == AodOperation::Dimension::X) {
          xMoves.emplace_back(std::make_unique<DimensionMove>(dimensionMove));
        } else {
          yMoves.emplace_back(std::make_unique<DimensionMove>(dimensionMove));
        }
      }

      [[nodiscard]] const std::vector<std::shared_ptr<DimensionMove>>&
      getDimensionMoves(const AodOperation::Dimension dimension) const {
        if (dimension == AodOperation::Dimension::X) {
          return xMoves;
        }
        return yMoves;
      }
    };

    /// Architecture providing the necessary hardware information.
    const NeutralAtomArchitecture* arch;
    std::vector<Transition> transitions;
    /// Operation emitted for this activation or deactivation phase.
    NAOpType phaseOperationType;

    // Constructor
    AodTransitionBuilder() = delete;
    AodTransitionBuilder(const AodTransitionBuilder&) = delete;
    AodTransitionBuilder(AodTransitionBuilder&&) = delete;
    AodTransitionBuilder(const NeutralAtomArchitecture& architecture,
                         const NAOpType phaseOperationType)
        : arch(&architecture), phaseOperationType(phaseOperationType) {}

    // Methods

    /**
     * @brief Return all internal moves along a dimension that start at a given
     * position.
     * @param dimension Dimension (X or Y).
     * @param initialPosition Initial position index.
     * @return Vector of matching dimension moves.
     */
    [[nodiscard]] std::vector<std::shared_ptr<DimensionMove>>
    getDimensionMovesFromInitialPosition(AodOperation::Dimension dimension,
                                         uint32_t initialPosition) const;

    // Transition management
    /**
     * @brief Merge an atom move into the current transitions according to merge
     * policy.
     * @details Uses per-dimension merge types to either merge, append, or
     * reject combining with in-flight transitions; records offsets and
     * load/unload handling.
     * @param mergeTypes Merge policies for X and Y.
     * @param origin Origin location.
     * @param move Atom move descriptor.
     * @param moveVector Geometric move vector.
     * @param requiresAtomTransfer Whether this phase transfers the atom
     * between static and mobile traps.
     */
    void addTransition(const DimensionMergeTypes& mergeTypes,
                       const Location& origin, const AtomMove& move,
                       const MoveVector& moveVector, bool requiresAtomTransfer);

    void addFlyingAncillaTransition(const Location& origin,
                                    const AtomMove& move,
                                    const MoveVector& moveVector,
                                    bool requiresAtomTransfer);
    /**
     * @brief Merge a transition into the aggregate along a specific dimension.
     * @param dimension Dimension to merge.
     * @param dimensionTransition Transition to merge for that dimension.
     * @param complementaryDimensionTransition Transition for the complementary
     * dimension.
     */
    void mergeTransitionDimension(
        AodOperation::Dimension dimension,
        const Transition& dimensionTransition,
        const Transition& complementaryDimensionTransition);
    /**
     * @brief Reorder offset moves to avoid crossing.
     * @param dimensionMoves Collection of offset moves to reorder.
     * @param sign Direction of offsets (+1/-1 for right/left or down/up).
     */
    static void
    reassignOffsets(std::vector<std::shared_ptr<DimensionMove>>& dimensionMoves,
                    int32_t sign);

    /**
     * @brief Maximum absolute offset at a position along a dimension.
     * @param dimension Dimension.
     * @param initialPosition Initial position.
     * @param sign Direction (+1/-1).
     * @return Maximum offset value.
     */
    [[nodiscard]] uint32_t
    getMaxOffsetAtInitialPosition(AodOperation::Dimension dimension,
                                  uint32_t initialPosition, int32_t sign) const;

    /**
     * @brief Check whether additional offset space is available at a position.
     * @param dimension Dimension.
     * @param initialPosition Initial position.
     * @param sign Direction (+1/-1).
     * @return True if more offset steps fit; false otherwise.
     */
    [[nodiscard]] bool
    hasIntermediateSpaceAtInitialPosition(AodOperation::Dimension dimension,
                                          uint32_t initialPosition,
                                          int32_t sign) const;

    void computeInitialAndOffsetSegments(
        AodOperation::Dimension dimension,
        const std::shared_ptr<DimensionMove>& dimensionMove,
        std::vector<AodOperation::Segment>& initialSegments,
        std::vector<AodOperation::Segment>& offsetSegments) const;
    /**
     * @brief Convert one transition into operations for this builder's phase.
     * @details Emits the phase operation followed by its offset move.
     * @param transition Transition aggregate to convert.
     * @return Vector of emitted AOD operations.
     */
    [[nodiscard]] std::vector<AodOperation>
    buildPhaseOperations(const Transition& transition) const;
    /**
     * @brief Convert all stored transitions into operations for this phase.
     * @return Concatenated vector of emitted phase operations.
     */
    [[nodiscard]] std::vector<AodOperation> buildPhaseOperations() const;
  };

  [[nodiscard]] static PhaseMergeTypes
  canAddTransition(const AodTransitionBuilder& activationBuilder,
                   const AodTransitionBuilder& deactivationBuilder,
                   const Location& origin, const MoveVector& moveVector,
                   const Location& final, const MoveVector& reverseMoveVector,
                   AodOperation::Dimension dimension);

  /**
   * @brief Move operations within a move group can be executed in parallel
   * @details A move group contains:
   * - the moves that can be executed in parallel
   * - the AOD operations to load, shuttle and unload the atoms
   * - the qubits that are used by the gates in the move group
   */
  struct MoveGroup {
    // the moves and the index they appear in the original quantum circuit (to
    // insert them back later)
    std::vector<std::pair<AtomMove, uint32_t>> moves;
    std::vector<std::pair<AtomMove, uint32_t>> flyingAncillaMoves;
    std::vector<AodOperation> activationOperations;
    std::vector<AodOperation> deactivationOperations;
    AodOperation shuttlingOperation;
    std::vector<CoordIndex> qubitsUsedByGates;

    // Constructor
    explicit MoveGroup() = default;

    // Methods
    /**
     * @brief Check if a move can be added to the current group.
     * @param move Move to check.
     * @param archArg Architecture for geometric/constraint checks.
     * @return True if compatible with group; false otherwise.
     */
    bool canAddMove(const AtomMove& move,
                    const NeutralAtomArchitecture& archArg);
    /**
     * @brief Add a move to the group.
     * @param move Move to add.
     * @param circuitIndex Circuit index of the move.
     */
    void addMove(const AtomMove& move, uint32_t circuitIndex);
    /**
     * @brief Circuit index of the earliest move in the group.
     * @return Minimum circuit index across stored moves.
     */

    [[nodiscard]] uint32_t getFirstCircuitIndex() const {
      assert(!moves.empty() || !flyingAncillaMoves.empty());
      if (moves.empty()) {
        return flyingAncillaMoves.front().second;
      }
      if (flyingAncillaMoves.empty()) {
        return moves.front().second;
      }
      return std::min(moves.front().second, flyingAncillaMoves.front().second);
    }
    /**
     * @brief Check if two moves are parallelizable.
     * @param v1 First move vector.
     * @param v2 Second move vector.
     * @return True if they can execute in parallel; false otherwise.
     */
    static bool parallelCheck(const MoveVector& v1, const MoveVector& v2);

    /**
     * @brief Build the shuttling operation connecting load and unload phases.
     * @param activationBuilder Builder for the loading phase.
     * @param deactivationBuilder Builder for the unloading phase.
     * @return Constructed AOD shuttling operation.
     */
    static AodOperation
    buildShuttlingOperation(const AodTransitionBuilder& activationBuilder,
                            const AodTransitionBuilder& deactivationBuilder);
  };

  const NeutralAtomArchitecture& arch;
  qc::QuantumComputation scheduledCircuit;
  std::vector<MoveGroup> moveGroups;
  const HardwareQubits& hardwareQubits;
  AncillaAtoms ancillas;

  AtomMove convertOperationToMove(const qc::Operation& operation) const;

  void initFlyingAncillas();

  /**
   * @brief Partition moves into groups that can execute in parallel.
   * @param circuit Quantum circuit to schedule.
   */
  void initMoveGroups(
      qc::QuantumComputation& circuit); //, qc::Permutation& hwToCoordIdx);
  /**
   * @brief Convert move groups into concrete AOD operations.
   * @details Uses activation/deactivation builders to emit load/move/unload
   * sequences; splits groups when parallelism constraints require it.
   */
  void processMoveGroups();

  std::pair<std::vector<AtomMove>, MoveGroup>
  processMoves(const std::vector<std::pair<AtomMove, uint32_t>>& moves,
               AodTransitionBuilder& activationBuilder,
               AodTransitionBuilder& deactivationBuilder) const;
  void processFlyingAncillaMoves(
      const std::vector<std::pair<AtomMove, uint32_t>>& flyingAncillaMoves,
      AodTransitionBuilder& activationBuilder,
      AodTransitionBuilder& deactivationBuilder) const;

public:
  MoveToAodConverter() = delete;
  MoveToAodConverter(const MoveToAodConverter&) = delete;
  MoveToAodConverter(MoveToAodConverter&&) = delete;
  explicit MoveToAodConverter(const NeutralAtomArchitecture& archArg,
                              const HardwareQubits& hardwareQubitsArg,
                              const HardwareQubits& flyingAncillas)
      : arch(archArg), scheduledCircuit(arch.getNpositions()),
        hardwareQubits(hardwareQubitsArg) {
    scheduledCircuit.addAncillaryRegister(arch.getNpositions());
    scheduledCircuit.addAncillaryRegister(arch.getNpositions(), "fa");
    for (std::uint32_t i = 0; i < flyingAncillas.getInitHwPos().size(); ++i) {
      const auto coord =
          flyingAncillas.getInitHwPos().at(i) + (2 * arch.getNpositions());
      const auto col = coord % arch.getNcolumns();
      const auto row = coord / arch.getNcolumns();
      const AncillaAtom ancillaAtom({col, row}, {i + 1, i + 1});
      ancillas.emplace_back(ancillaAtom);
    }
  }

  /**
   * @brief Schedule a circuit: replace abstract moves by AOD load/move/unload.
   * @param circuit Quantum circuit to schedule.
   * @return New circuit containing AOD operations.
   */
  qc::QuantumComputation schedule(qc::QuantumComputation& circuit);

  /**
   * @brief Get number of constructed move groups.
   * @return Count of move groups.
   */
  [[nodiscard]] auto getNMoveGroups() const { return moveGroups.size(); }
};

} // namespace na
