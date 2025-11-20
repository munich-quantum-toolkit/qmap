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

#include <cstddef>
#include <cstdlib>
#include <limits>
#include <map>
#include <memory>
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

std::string AnimationAtoms::placeInitAtoms() {
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
    const auto startsX =
        dynamic_cast<AodOperation*>(op.get())->getStarts(Dimension::X);
    const auto endsX =
        dynamic_cast<AodOperation*>(op.get())->getEnds(Dimension::X);
    const auto startsY =
        dynamic_cast<AodOperation*>(op.get())->getStarts(Dimension::Y);
    const auto endsY =
        dynamic_cast<AodOperation*>(op.get())->getEnds(Dimension::Y);
    const auto coordIndices = op->getTargets(); // renamed
    // use that coord indices are pairs of origin and target indices
    for (size_t i = 0; i < coordIndices.size(); i++) {
      if (i % 2 == 0) {
        const auto coordIdx = coordIndices[i];
        const auto id = coordIdxToId.at(coordIdx);
        bool foundX = false;
        auto newX = std::numeric_limits<qc::fp>::max();
        bool foundY = false;
        auto newY = std::numeric_limits<qc::fp>::max();
        for (size_t j = 0; j < startsX.size(); j++) {
          if (std::abs(startsX[j] - idToCoord.at(id).first) < 0.0001) {
            newX = endsX[j];
            foundX = true;
            break;
          }
        }
        if (!foundX) {
          // X coord is the same as before
          newX = idToCoord.at(id).first;
        }

        for (size_t j = 0; j < startsY.size(); j++) {
          if (std::abs(startsY[j] - idToCoord.at(id).second) < 0.0001) {
            newY = endsY[j];
            foundY = true;
            break;
          }
        }
        if (!foundY) {
          // Y coord is the same as before
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
        const auto id = coordIdxToId.at(coordIndices[i - 1]);
        coordIdxToId.erase(coordIndices[i - 1]);
        coordIdxToId[coordIdx] = id;
      }
    }
    // must be a gate
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
