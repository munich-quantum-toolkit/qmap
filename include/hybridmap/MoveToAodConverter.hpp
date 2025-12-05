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

#include "hybridmap/HardwareQubits.hpp"
#include "hybridmap/NeutralAtomArchitecture.hpp"
#include "hybridmap/NeutralAtomDefinitions.hpp"
#include "hybridmap/NeutralAtomUtils.hpp"
#include "ir/Definitions.hpp"
#include "ir/QuantumComputation.hpp"
#include "ir/operations/AodOperation.hpp"
#include "ir/operations/OpType.hpp"
#include "na/entities/Location.hpp"

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace na {
/**
 * @brief Result type for merging two move-derived activations.
 * @details Indicates whether merging is impossible, trivial, a full merge, or
 * requires appending.
 */
enum class ActivationMergeType : uint8_t { Impossible, Trivial, Merge, Append };
/**
 * @brief Pair of merge types for X and Y dimensions.
 */
using MergeTypeXY = std::pair<ActivationMergeType, ActivationMergeType>;

/**
 * @brief Converts abstract atom moves into concrete AOD activation/move
 * sequences.
 * @details Groups parallelizable moves, computes safe offset maneuvers for
 * loading/unloading AODs, and emits AOD operations (activate, move, deactivate)
 * respecting device constraints.
 */
class MoveToAodConverter {
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
   * @brief Helper for constructing and merging AOD activations/moves.
   * @details Tracks per-dimension AOD moves with offsets and associated atom
   * moves; produces AodOperation sequences for activation/move/deactivation.
   */
  struct AodActivationHelper {
    /**
     * @brief Single AOD movement in one dimension (x or y).
     * @details Stores initial position, movement delta, required offset to
     * avoid crossing, and whether a load/unload is needed.
     */
    struct AodMove {
      // start of the move
      uint32_t init;
      // need load/unload or not
      bool load;
      // need offset move to avoid crossing
      int32_t offset;
      // delta of the actual move
      qc::fp delta;

      AodMove(const uint32_t initMove, const qc::fp deltaMove,
              const int32_t offsetMove, const bool loadMove)
          : init(initMove), load(loadMove), offset(offsetMove),
            delta(deltaMove) {}
    };
    /**
     * @brief Aggregate of per-dimension activation moves plus logical atom
     * move.
     * @details Represents either activation or deactivation depending on
     * context. Holds x- and y- dimension AOD moves and the associated AtomMove.
     */
    struct AodActivation {
      // first: x, second: delta x, third: offset x
      std::vector<std::shared_ptr<AodMove>> activateXs;
      std::vector<std::shared_ptr<AodMove>> activateYs;
      std::vector<AtomMove> moves;

      AodActivation(const AodMove& activateX, const AodMove& activateY,
                    const AtomMove& move)
          : moves({move}) {
        activateXs.emplace_back(std::make_unique<AodMove>(activateX));
        activateYs.emplace_back(std::make_unique<AodMove>(activateY));
      }
      AodActivation(const Dimension dim, const AodMove& activate,
                    const AtomMove& move)
          : moves({move}) {
        if (dim == Dimension::X) {
          activateXs.emplace_back(std::make_unique<AodMove>(activate));
        } else {
          activateYs.emplace_back(std::make_unique<AodMove>(activate));
        }
      }

      [[nodiscard]] std::vector<std::shared_ptr<AodMove>>
      getActivates(const Dimension dim) const {
        if (dim == Dimension::X) {
          return activateXs;
        }
        return activateYs;
      }
    };

    // NeutralAtomArchitecture to call necessary hardware information
    const NeutralAtomArchitecture* arch;
    std::vector<AodActivation> allActivations;
    // Differentiate between loading and unloading
    qc::OpType type;
    AncillaAtoms* ancillas;

    // Constructor
    AodActivationHelper() = delete;
    AodActivationHelper(const AodActivationHelper&) = delete;
    AodActivationHelper(AodActivationHelper&&) = delete;
    AodActivationHelper(const NeutralAtomArchitecture& architecture,
                        const qc::OpType opType, AncillaAtoms* ancillas)
        : arch(&architecture), type(opType), ancillas(ancillas) {}

    // Methods

    /**
     * @brief Return all AOD moves along a dimension that start at a given
     * position.
     * @param dim Dimension (X or Y).
     * @param init Initial position index.
     * @return Vector of matching AOD move descriptors.
     */
    [[nodiscard]] std::vector<std::shared_ptr<AodMove>>
    getAodMovesFromInit(Dimension dim, uint32_t init) const;

    // Activation management
    /**
     * @brief Merge an atom move into current activations according to merge
     * policy.
     * @details Uses per-dimension merge types to either merge, append, or
     * reject combining with in-flight activations; records offsets and
     * load/unload handling.
     * @param merge Merge policy for X and Y.
     * @param origin Origin location.
     * @param move Atom move descriptor.
     * @param v Geometric move vector.
     * @param needLoad Whether an AOD load is required.
     */
    void addActivation(
        const std::pair<ActivationMergeType, ActivationMergeType>& merge,
        const Location& origin, const AtomMove& move, const MoveVector& v,
        bool needLoad);

    void addActivationFa(const Location& origin, const AtomMove& move,
                         const MoveVector& v, bool needLoad);
    /**
     * @brief Merge an activation into the aggregate along a specific dimension.
     * @param dim Dimension of the activation.
     * @param activationDim Activation to merge for the specified dimension.
     * @param activationOtherDim Complementary activation for the other
     * dimension.
     */
    void mergeActivationDim(Dimension dim, const AodActivation& activationDim,
                            const AodActivation& activationOtherDim);
    /**
     * @brief Reorder offset moves to avoid crossing.
     * @param aodMoves Collection of offset moves to reorder.
     * @param sign Direction of offsets (+1/-1 for right/left or down/up).
     */
    static void reAssignOffsets(std::vector<std::shared_ptr<AodMove>>& aodMoves,
                                int32_t sign);

    /**
     * @brief Maximum absolute offset at a position along a dimension.
     * @param dim Dimension.
     * @param init Initial position.
     * @param sign Direction (+1/-1).
     * @return Maximum offset value.
     */
    [[nodiscard]] uint32_t getMaxOffsetAtInit(Dimension dim, uint32_t init,
                                              int32_t sign) const;

    /**
     * @brief Check whether additional offset space is available at a position.
     * @param dim Dimension.
     * @param init Initial position.
     * @param sign Direction (+1/-1).
     * @return True if more offset steps fit; false otherwise.
     */
    [[nodiscard]] bool checkIntermediateSpaceAtInit(Dimension dim,
                                                    uint32_t init,
                                                    int32_t sign) const;

    void computeInitAndOffsetOperations(
        Dimension dimension, const std::shared_ptr<AodMove>& aodMove,
        std::vector<SingleOperation>& initOperations,
        std::vector<SingleOperation>& offsetOperations) const;
    // Convert activation to AOD operations
    /**
     * @brief Convert a single activation aggregate into AOD operations.
     * @details Emission order: activate, move, deactivate.
     * @param activation Activation aggregate to convert.
     * @return Vector of emitted AOD operations.
     */
    [[nodiscard]] std::vector<AodOperation>
    getAodOperation(const AodActivation& activation) const;
    /**
     * @brief Convert all stored activations into AOD operations.
     * @return Concatenated vector of emitted AOD operations.
     */
    [[nodiscard]] std::vector<AodOperation> getAodOperations() const;
  };

  [[nodiscard]] static std::pair<ActivationMergeType, ActivationMergeType>
  canAddActivation(const AodActivationHelper& activationHelper,
                   const AodActivationHelper& deactivationHelper,
                   const Location& origin, const MoveVector& v,
                   const Location& final, const MoveVector& vReverse,
                   Dimension dim);

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
    std::vector<std::pair<AtomMove, uint32_t>> movesFa;
    std::vector<AodOperation> processedOpsInit;
    std::vector<AodOperation> processedOpsFinal;
    AodOperation processedOpShuttle;
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
     * @param idx Circuit index of the move.
     */
    void addMove(const AtomMove& move, uint32_t idx);
    /**
     * @brief Circuit index of the earliest move in the group.
     * @return Minimum circuit index across stored moves.
     */

    [[nodiscard]] uint32_t getFirstIdx() const {
      assert(!moves.empty() || !movesFa.empty());
      if (moves.empty()) {
        return movesFa.front().second;
      }
      if (movesFa.empty()) {
        return moves.front().second;
      }
      return std::min(moves.front().second, movesFa.front().second);
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
     * @param aodActivationHelper Activation helper (loading phase info).
     * @param aodDeactivationHelper Deactivation helper (unloading phase info).
     * @return Constructed AOD shuttling operation.
     */
    static AodOperation
    connectAodOperations(const AodActivationHelper& aodActivationHelper,
                         const AodActivationHelper& aodDeactivationHelper);
  };

  const NeutralAtomArchitecture& arch;
  qc::QuantumComputation qcScheduled;
  std::vector<MoveGroup> moveGroups;
  const HardwareQubits& hardwareQubits;
  AncillaAtoms ancillas;

  AtomMove convertOpToMove(qc::Operation* get) const;

  void initFlyingAncillas();

  /**
   * @brief Partition moves into groups that can execute in parallel.
   * @param qc Quantum circuit to schedule.
   */
  void initMoveGroups(
      qc::QuantumComputation& qc); //, qc::Permutation& hwToCoordIdx);
  /**
   * @brief Convert move groups into concrete AOD operations.
   * @details Uses activation/deactivation helpers to emit load/move/unload
   * sequences; splits groups when parallelism constraints require it.
   */
  void processMoveGroups();

  std::pair<std::vector<AtomMove>, MoveGroup>
  processMoves(const std::vector<std::pair<AtomMove, uint32_t>>& moves,
               AodActivationHelper& aodActivationHelper,
               AodActivationHelper& aodDeactivationHelper) const;
  void processMovesFa(const std::vector<std::pair<AtomMove, uint32_t>>& movesFa,
                      AodActivationHelper& aodActivationHelper,
                      AodActivationHelper& aodDeactivationHelper) const;

public:
  MoveToAodConverter() = delete;
  MoveToAodConverter(const MoveToAodConverter&) = delete;
  MoveToAodConverter(MoveToAodConverter&&) = delete;
  explicit MoveToAodConverter(const NeutralAtomArchitecture& archArg,
                              const HardwareQubits& hardwareQubitsArg,
                              const HardwareQubits& flyingAncillas)
      : arch(archArg), qcScheduled(arch.getNpositions()),
        hardwareQubits(hardwareQubitsArg) {
    qcScheduled.addAncillaryRegister(arch.getNpositions());
    qcScheduled.addAncillaryRegister(arch.getNpositions(), "fa");
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
   * @param qc Quantum circuit to schedule.
   * @return New circuit containing AOD operations.
   */
  qc::QuantumComputation schedule(qc::QuantumComputation& qc);

  /**
   * @brief Get number of constructed move groups.
   * @return Count of move groups.
   */
  [[nodiscard]] auto getNMoveGroups() const { return moveGroups.size(); }
};

} // namespace na
