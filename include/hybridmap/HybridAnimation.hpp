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

#include "NeutralAtomArchitecture.hpp"
#include "NeutralAtomDefinitions.hpp"
#include "ir/Definitions.hpp"
#include "ir/operations/Operation.hpp"

#include <map>
#include <memory>
#include <string>
#include <utility>

namespace na {
/**
 * @brief Helper to generate NaViz-style animation strings for neutral-atom
 * layouts.
 * @details Maintains bidirectional mappings between coordinate indices and
 * atom identifiers, as well as continuous coordinates derived from the
 * architecture's grid geometry. Provides utilities to emit initial placement
 * lines and per-operation animation snippets.
 * @note The architecture reference passed to the constructor must remain valid
 * for the lifetime of this object.
 */
class AnimationAtoms {
protected:
  /** Map from discrete coordinate index to atom id. */
  std::map<CoordIndex, HwQubit> coordIdxToId;
  /** Map from atom id to continuous (x,y) coordinates. */
  std::map<HwQubit, std::pair<qc::fp, qc::fp>> idToCoord;
  const NeutralAtomArchitecture* arch;

  /**
   * @brief Initialize coordinate/id maps from initial hardware and ancilla
   * positions.
   * @details For hardware qubits, places atoms at grid points based on
   * architecture columns and inter-qubit distance. Flying ancilla qubits are
   * offset from the grid using a small per-index displacement that depends on
   * the architecture's AOD intermediate levels, and their id space follows the
   * hardware ids.
   * @param initHwPos Initial mapping of hardware qubit id -> coordinate index.
   * @param initFaPos Initial mapping of flying-ancilla id -> coordinate index.
   */
  void initPositions(const std::map<HwQubit, CoordIndex>& initHwPos,
                     const std::map<HwQubit, CoordIndex>& initFaPos);

public:
  /**
   * @brief Construct animation helper with initial positions.
   * @param initHwPos Initial hardware id -> coordinate index mapping.
   * @param initFaPos Initial flying-ancilla id -> coordinate index mapping.
   * @param architecture Reference to the neutral atom architecture providing
   * grid geometry and distances.
   */
  AnimationAtoms(const std::map<HwQubit, CoordIndex>& initHwPos,
                 const std::map<HwQubit, CoordIndex>& initFaPos,
                 const NeutralAtomArchitecture& architecture)
      : arch(&architecture) {
    initPositions(initHwPos, initFaPos);
  }

  /**
   * @brief Emit NaViz lines that place all initial atoms.
   * @details One line per atom with absolute coordinates and an atom label
   * (e.g., "atom (x, y) atom<ID>").
   * @return Concatenated lines suitable for a NaViz animation file.
   */
  [[nodiscard]] std::string placeInitAtoms() const;
  /**
   * @brief Convert a quantum operation into NaViz animation commands.
   * @details Supports AOD load/store/move operations and standard gates.
   * - AodActivate: emits a load block with listed atoms.
   * - AodDeactivate: emits a store block with listed atoms.
   * - AodMove: updates internal coordinates by matching AOD start/end lists and
   *   emits move commands for affected atoms; also updates coordIdxToId for
   *   origin/target coordinate-index pairs.
   * - Multi-qubit gates: emits a grouped cz with all involved atoms.
   * - Single-qubit gates: emits a simple rz 1 for the target atom (independent
   * of the exact gate -> does not matter for visualization).
   * - Other operations are currently not supported for animation output.
   * @param op The operation to translate (uses coordinate indices as qubits).
   * @param startTime The animation timestamp to annotate the command with.
   * @return NaViz command string for the operation at the given time.
   */
  std::string opToNaViz(const std::unique_ptr<qc::Operation>& op,
                        qc::fp startTime);
};

} // namespace na
