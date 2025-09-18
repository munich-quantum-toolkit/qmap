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

#include <cstdint>
#include <map>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace na {
// Possible types two Move combination can be combined to
enum class ActivationMergeType : uint8_t { Impossible, Trivial, Merge, Append };
// Used to indicate how AodOperations can be merged
using MergeTypeXY = std::pair<ActivationMergeType, ActivationMergeType>;

/**
 * @brief Class to convert abstract move operations to AOD movements on a
 * neutral atom architecture
 * @details The scheduler takes a quantum circuit containing abstract move
 * operations and tries to merge them into parallel AO movements. It also
 * manages the small offset movements required while loading or unloading of
 * AODs.
 */
class MoveToAodConverter {
  struct AncillaAtom {
    struct xAndY {
      int x;
      int y;
      xAndY(std::uint32_t x, std::uint32_t y) : x(x), y(y) {}
    };

    xAndY coord;
    xAndY coordDodged;
    xAndY offset;
    xAndY offsetDodged;
    AncillaAtom() = delete;
    AncillaAtom(xAndY c, xAndY o)
        : coord(c), coordDodged(c), offset(o), offsetDodged(o) {}
  };
  using AncillaAtoms = std::vector<AncillaAtom>;

protected:
  /**
   * @brief Struct to store information about specific AOD activations.
   * @details It contains:
   * - the offset moves in x and y direction
   * - the actual moves
   */
  struct AodActivationHelper {
    /**
     * @brief Describes a single AOD movement in either x or y direction
     * @details It contains:
     * - the initial position of the AOD
     * - the delta of the AOD movement
     * - the size of the offset of the AOD movement
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

      AodMove() = default;

      AodMove(uint32_t initMove, qc::fp deltaMove, int32_t offsetMove,
              bool loadMove)
          : init(initMove), offset(offsetMove), delta(deltaMove),
            load(loadMove) {}
    };
    /**
     * @brief Manages the activation of an atom using an AOD.
     * @details The same struct is used also to deactivate the AOD, just
     * reversed.
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
      getActivates(Dimension dim) const {
        if (dim == Dimension::X) {
          return activateXs;
        }
        return activateYs;
      }
    };

    // AODScheduler
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
                        qc::OpType opType, AncillaAtoms* ancillas)
        : arch(&architecture), type(opType), ancillas(ancillas) {}

    // Methods

    /**
     * @brief Returns all AOD moves in the given dimension/direction which start
     * at the given initial position
     * @param dim The dimension/direction to check
     * @param init The initial position to check
     * @return A vector of AOD moves
     */
    [[nodiscard]] std::vector<std::shared_ptr<AodMove>>
    getAodMovesFromInit(Dimension dim, uint32_t init) const;

    // Activation management
    /**
     * @brief Adds the move to the current activations
     * @details The move is merged into the current activations depending on the
     * given merge types
     * @param merge The merge types in x and y direction
     * @param origin The origin of the move
     * @param move The move to add
     * @param v The move vector of the move
     * @param needLoad
     */
    void
    addActivation(std::pair<ActivationMergeType, ActivationMergeType> merge,
                  const Location& origin, const AtomMove& move, MoveVector v,
                  bool needLoad);

    void addActivationFa(const Location& origin, const AtomMove& move,
                         MoveVector v, bool needLoad);
    /**
     * @brief Merges the given activation into the current activations
     * @param dim The dimension/direction of the activation
     * @param activationDim The activation to merge in the given
     * dimension/direction
     * @param activationOtherDim The activation to merge/add in the other
     * dimension/direction
     */
    void mergeActivationDim(Dimension dim, const AodActivation& activationDim,
                            const AodActivation& activationOtherDim);
    /**
     * @brief Orders the aod offset moves such that they will not cross each
     * other
     * @param aodMoves The aod offset moves to order
     * @param sign The direction of the offset moves (right/left or down/up)
     */
    static void reAssignOffsets(std::vector<std::shared_ptr<AodMove>>& aodMoves,
                                int32_t sign);

    /**
     * @brief Returns the maximum offset in the given dimension/direction from
     * the given initial position
     * @param dim The dimension/direction to check
     * @param init The initial position to check
     * @param sign The direction of the offset moves (right/left or down/up)
     * @return The maximum offset
     */
    [[nodiscard]] uint32_t getMaxOffsetAtInit(Dimension dim, uint32_t init,
                                              int32_t sign) const;

    /**
     * @brief Checks if there is still space at the given initial position and
     * the given direction
     * @param dim The dimension/direction to check
     * @param init The initial position to check
     * @param sign The direction of the offset moves (right/left or down/up)
     * @return True if there is still space, false otherwise
     */
    [[nodiscard]] bool checkIntermediateSpaceAtInit(Dimension dim,
                                                    uint32_t init,
                                                    int32_t sign) const;

    void computeInitAndOffsetOperations(
        Dimension dimension, const std::shared_ptr<AodMove>& move,
        std::vector<SingleOperation>& initOperations,
        std::vector<SingleOperation>& offsetOperations) const;
    // Convert activation to AOD operations
    /**
     * @brief Converts activation into AOD operation (activate, move,
     * deactivate)
     * @param activation The activation to convert
     * @param arch The neutral atom architecture to call necessary hardware
     * information
     * @param type The type of the activation (loading or unloading)
     * @return The activation as AOD operation
     */
    [[nodiscard]] std::vector<AodOperation>
    getAodOperation(const AodActivation& activation) const;
    /**
     * @brief Converts all activations into AOD operations
     * @return All activations of the AOD activation helper as AOD operations
     */
    [[nodiscard]] std::vector<AodOperation> getAodOperations() const;

    [[nodiscard]] static std::vector<AodOperation>
    reverseActivations(const std::vector<AodOperation>& ops);
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
     * @brief Checks if the given move can be added to the move group
     * @param move Move to check
     * @return True if the move can be added, false otherwise
     */
    bool canAddMove(const AtomMove& move,
                    const NeutralAtomArchitecture& archArg);
    /**
     * @brief Adds the given move to the move group
     * @param move Move to add
     * @param idx Index of the move in the original quantum circuit
     */
    void addMove(const AtomMove& move, uint32_t idx);
    /**
     * @brief Returns the circuit index of the first move in the move group
     * @return Circuit index of the first move in the move group
     */

    [[nodiscard]] uint32_t getFirstIdx() const {
      if (moves.size() == 0) {
        return movesFa.front().second;
      }
      if (movesFa.size() == 0) {
        return moves.front().second;
      }
      return std::min(moves.front().second, movesFa.front().second);
    }
    /**
     * @brief Checks if the two moves can be executed in parallel
     * @param v1 The first move
     * @param v2 The second move
     * @return True if the moves can be executed in parallel, false otherwise
     */
    static bool parallelCheck(const MoveVector& v1, const MoveVector& v2);

    /**
     * @brief Helper function to create the actual shuttling operation between
     * the loading at the initial position and the unloading at the final
     * position
     * @param opsInit Loading operations
     * @param opsFinal Unloading operations
     * @return The shuttling operation between the loading and unloading
     * operations
     */
    static AodOperation
    connectAodOperations(const AodActivationHelper& aodActivationHelper,
                         const AodActivationHelper& aodDeactivationHelper);
  };

  const NeutralAtomArchitecture& arch;
  qc::QuantumComputation qcScheduled;
  std::vector<MoveGroup> moveGroups;
  const na::HardwareQubits& hardwareQubits;
  AncillaAtoms ancillas;

  AtomMove convertOpToMove(qc::Operation* get);

  void initFlyingAncillas();

  /**
   * @brief Assigns move operations into groups that can be executed in parallel
   * @param qc Quantum circuit to schedule
   */
  void initMoveGroups(
      qc::QuantumComputation& qc); //, qc::Permutation& hwToCoordIdx);
  /**
   * @brief Converts the move groups into the actual AOD operations
   * @details For this the following steps are performed:
   * - ActivationHelper to manage the loading
   * - ActivationHelper to manage the unloading
   * If not the whole move group can be executed in parallel, a new move group
   * is created for the remaining moves.
   */
  void processMoveGroups();
  void postProcessMoveGroups();

  std::pair<std::vector<AtomMove>, MoveGroup>
  processMoves(const std::vector<std::pair<AtomMove, uint32_t>>& moves,
               AodActivationHelper& aodActivationHelper,
               AodActivationHelper& aodDeactivationHelper);
  void processMovesFa(const std::vector<std::pair<AtomMove, uint32_t>>& movesFa,
                      AodActivationHelper& aodActivationHelper,
                      AodActivationHelper& aodDeactivationHelper) const;

public:
  MoveToAodConverter() = delete;
  MoveToAodConverter(const MoveToAodConverter&) = delete;
  MoveToAodConverter(MoveToAodConverter&&) = delete;
  explicit MoveToAodConverter(const NeutralAtomArchitecture& archArg,
                              const na::HardwareQubits& hardwareQubitsArg,
                              const HardwareQubits& flyingAncillas)
      : arch(archArg), qcScheduled(arch.getNpositions()),
        hardwareQubits(hardwareQubitsArg) {
    qcScheduled.addAncillaryRegister(arch.getNpositions());
    qcScheduled.addAncillaryRegister(arch.getNpositions(), "fa");
    if (flyingAncillas.getNumQubits() > arch.getNcolumns() ||
        flyingAncillas.getNumQubits() > arch.getNrows()) {
      throw std::invalid_argument(
          "Number of flying ancillas must be smaller than the number of "
          "columns and rows of the neutral atom architecture.");
    }
    for (auto i = 0; i < flyingAncillas.getInitHwPos().size(); ++i) {
      const auto coord =
          flyingAncillas.getInitHwPos().at(i) + (2 * arch.getNpositions());
      const auto col = coord % arch.getNcolumns();
      const auto row = coord / arch.getNcolumns();
      const AncillaAtom ancillaAtom({col, row},
                                    {static_cast<std::uint32_t>(i + 1),
                                     static_cast<std::uint32_t>(i + 1)});
      ancillas.emplace_back(ancillaAtom);
    }
  }

  /**
   * @brief Schedules the given quantum circuit using AODs
   * @param qc Quantum circuit to schedule
   * @return Scheduled quantum circuit, containing AOD operations
   */
  qc::QuantumComputation
  schedule(qc::QuantumComputation& qc); //, qc::Permutation hwToCoordIdx);

  /**
   * @brief Returns the number of move groups
   * @return Number of move groups
   */
  [[nodiscard]] auto getNMoveGroups() const { return moveGroups.size(); }
};

} // namespace na
