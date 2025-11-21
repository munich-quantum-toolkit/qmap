/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "hybridmap/HybridAnimation.hpp"

#include "hybridmap/NeutralAtomArchitecture.hpp"
#include "hybridmap/NeutralAtomDefinitions.hpp"
#include "ir/Definitions.hpp"
#include "ir/operations/AodOperation.hpp"
#include "ir/operations/OpType.hpp"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>

namespace na {
void AnimationAtoms::initPositions(
    const std::map<HwQubit, CoordIndex>& initHwPos,
    const std::map<HwQubit, CoordIndex>& initFaPos) {
  const auto nCols = arch->getNcolumns();
  for (const auto& [id, coord] : initHwPos) {
    coordIdxToId[coord] = id;
    const auto column = coord % nCols;
    const auto row = coord / nCols;
    idToCoord[id] = {column * arch->getInterQubitDistance(),
                     row * arch->getInterQubitDistance()};
  }

  auto flyingAncillaIdxPlusOne = 0;
  const auto hwCount = static_cast<HwQubit>(initHwPos.size());
  for (const auto& [id, coord] : initFaPos) {
    flyingAncillaIdxPlusOne++;
    coordIdxToId[(coord + static_cast<CoordIndex>(2 * arch->getNpositions()))] =
        id + hwCount;
    const auto column = coord % nCols;
    const auto row = coord / nCols;
    const auto offset =
        arch->getInterQubitDistance() / arch->getNAodIntermediateLevels();
    idToCoord[(id + hwCount)] = {(column * arch->getInterQubitDistance()) +
                                     flyingAncillaIdxPlusOne * offset,
                                 (row * arch->getInterQubitDistance()) +
                                     flyingAncillaIdxPlusOne * offset};
  }
}

std::string AnimationAtoms::placeInitAtoms() const {
  std::string initString;
  for (const auto& [id, coords] : idToCoord) {
    initString += "atom (" + std::to_string(coords.first) + ", " +
                  std::to_string(coords.second) + ") atom" +
                  std::to_string(id) + "\n";
  }
  return initString;
}
std::string AnimationAtoms::opToNaViz(const std::unique_ptr<qc::Operation>& op,
                                      qc::fp startTime) {
  std::string opString;

  if (op->getType() == qc::OpType::AodActivate) {
    opString += "@" + std::to_string(startTime) + " load [\n";
    for (const auto& coordIdx : op->getTargets()) {
      const auto id = coordIdxToId.at(coordIdx);
      opString += "\t atom" + std::to_string(id) + "\n";
    }
    opString += "]\n";
  } else if (op->getType() == qc::OpType::AodDeactivate) {
    opString += "@" + std::to_string(startTime) + " store [\n";
    for (const auto& coordIdx : op->getTargets()) {
      const auto id = coordIdxToId.at(coordIdx);
      opString += "\t atom" + std::to_string(id) + "\n";
    }
    opString += "]\n";
  } else if (op->getType() == qc::OpType::AodMove) {
    // update atom coordinates
    const auto* aodOp = dynamic_cast<AodOperation*>(op.get());
    assert(aodOp != nullptr &&
           "OpType::AodMove must be backed by AodOperation");
    const auto startsX = aodOp->getStarts(Dimension::X);
    const auto endsX = aodOp->getEnds(Dimension::X);
    const auto startsY = aodOp->getStarts(Dimension::Y);
    const auto endsY = aodOp->getEnds(Dimension::Y);
    assert(startsX.size() == endsX.size());
    assert(startsY.size() == endsY.size());
    const auto& coordIndices = op->getTargets(); // renamed
    // The list of targets for an AodMove operation must contain pairs of
    // (origin, destination) coordinate indices.
    if (coordIndices.size() % 2 != 0) {
      throw std::logic_error(
          "AodMove targets must be pairs of origin and target indices.");
    }

    // Tolerance for floating point comparisons when matching start coordinates.

    // use that coord indices are pairs of origin and target indices
    for (size_t i = 0; i < coordIndices.size(); i++) {
      if (i % 2 == 0) {
        constexpr qc::fp fpTolerance = 0.0001;
        const auto coordIdx = coordIndices[i];
        if (!coordIdxToId.contains(coordIdx)) {
          throw std::logic_error("AodMove origin index " +
                                 std::to_string(coordIdx) +
                                 " not found in coordIdxToId map.");
        }
        const auto id = coordIdxToId.at(coordIdx);
        if (!idToCoord.contains(id)) {
          throw std::logic_error("Atom ID " + std::to_string(id) +
                                 " not found in idToCoord map.");
        }
        bool foundX = false;
        auto newX = std::numeric_limits<qc::fp>::max();
        bool foundY = false;
        auto newY = std::numeric_limits<qc::fp>::max();
        for (size_t j = 0; j < startsX.size(); j++) {
          if (std::abs(startsX[j] - idToCoord.at(id).first) < fpTolerance) {
            newX = endsX[j];
            foundX = true;
            break;
          }
        }
        if (!foundX) {
          // X coord is the same as before if no matching start is found.
          newX = idToCoord.at(id).first;
        }

        for (size_t j = 0; j < startsY.size(); j++) {
          if (std::abs(startsY[j] - idToCoord.at(id).second) < fpTolerance) {
            newY = endsY[j];
            foundY = true;
            break;
          }
        }
        if (!foundY) {
          // Y coord is the same as before if no matching start is found.
          newY = idToCoord.at(id).second;
        }
        opString += "@" + std::to_string(startTime) + " move (" +
                    std::to_string(newX) + ", " + std::to_string(newY) +
                    ") atom" + std::to_string(id) + "\n";
        auto& coords = idToCoord.at(id);
        coords.first = newX;
        coords.second = newY;
      } else {
        // this is the target index -> update coordIdxToId
        const auto coordIdx = coordIndices[i];
        const auto prevCoordIdx = coordIndices[i - 1];
        if (!coordIdxToId.contains(prevCoordIdx)) {
          throw std::logic_error(
              "AodMove origin index " + std::to_string(prevCoordIdx) +
              " not found in coordIdxToId map during update.");
        }
        const auto id = coordIdxToId.at(prevCoordIdx);
        coordIdxToId.erase(prevCoordIdx);
        coordIdxToId[coordIdx] = id;
      }
    }
    // must be a gate
    // For visualization:
    // - All multi-qubit gates → cz
    // - All single-qubit gates → rz
  } else if (op->getNqubits() > 1) {
    opString += "@" + std::to_string(startTime) + " cz {";
    for (const auto& coordIdx : op->getUsedQubits()) {
      const auto id = coordIdxToId.at(coordIdx);
      opString += " atom" + std::to_string(id) + ",";
    }
    opString.pop_back();
    opString += "}\n";
  } else {
    // single qubit gate
    const auto coordIdx = op->getTargets().front();
    const auto id = coordIdxToId.at(coordIdx);
    opString += "@" + std::to_string(startTime) + " rz 1" + " atom" +
                std::to_string(id) + "\n";
  }

  return opString;
}

} // namespace na
