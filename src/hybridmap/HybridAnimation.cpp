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
#include <cstdint>
#include <cstdlib>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>

namespace na {
void AnimationAtoms::initPositions(
    const std::map<HwQubit, CoordIndex>& initHwPos,
    const std::map<HwQubit, CoordIndex>& initFaPos) {
  const auto nCols = arch.getNcolumns();
  for (const auto& [id, coord] : initHwPos) {
    coordIdxToId[coord] = id;
    const auto column = coord % nCols;
    const auto row = coord / nCols;
    idToCoord[id] = {column * arch.getInterQubitDistance(),
                     row * arch.getInterQubitDistance()};
  }

  auto flyingAncillaidxPlusOne = 0;
  for (const auto& [id, coord] : initFaPos) {
    flyingAncillaidxPlusOne++;
    coordIdxToId[coord + (2 * arch.getNpositions())] = id + initHwPos.size();
    const auto column = coord % nCols;
    const auto row = coord / nCols;
    const auto offset =
        arch.getInterQubitDistance() / arch.getNAodIntermediateLevels();
    idToCoord[id + initHwPos.size()] = {
        (column * arch.getInterQubitDistance()) +
            flyingAncillaidxPlusOne * offset,
        (row * arch.getInterQubitDistance()) +
            flyingAncillaidxPlusOne * offset};
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
    const auto CoordIndices = op->getTargets();
    // use that coord indices are pairs of origin and target indices
    for (size_t i = 0; i < CoordIndices.size(); i++) {
      if (i % 2 == 0) {
        const auto coordIdx = CoordIndices[i];
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
        idToCoord.at(id) = {newX, newY};
      } else {
        // this is the target index -> update coordIdxToId
        const auto coordIdx = CoordIndices[i];
        const auto id = coordIdxToId.at(CoordIndices[i - 1]);
        coordIdxToId.erase(CoordIndices[i - 1]);
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

// std::string AnimationAtoms::getEndString(const qc::fp endTime) {
//   std::string initString;
//   for (const auto& [id, coord] : idToCoord) {
//     initString += std::to_string(endTime) + ";" + std::to_string(id) + ";"
//     +
//                   std::to_string(coord.first) + ";" +
//                   std::to_string(coord.second) + ";1;0;0;0;0;0;0;0\n";
//   }
//   return initString;
// }
//
// AnimationAtoms::axesId AnimationAtoms::addAxis(const HwQubit id) {
//   if (axesIds.find(id) == axesIds.end()) {
//     axesIdCounter++;
//     axesIds[id] = axesIdCounter;
//   } else {
//     throw std::invalid_argument(
//         "Tried to add axis but axis already exists for qubit " +
//         std::to_string(id));
//   }
//   return axesIds[id];
// }
// AnimationAtoms::marginId AnimationAtoms::addMargin(const HwQubit id) {
//   if (marginIds.find(id) == marginIds.end()) {
//     marginIdCounter++;
//     marginIds[id] = marginIdCounter;
//   } else {
//     throw std::invalid_argument(
//         "Tried to add margin but margin already exists for qubit " +
//         std::to_string(id));
//   }
//   return marginIds[id];
// }
//
// std::string
// AnimationAtoms::createCsvOp(const std::unique_ptr<qc::Operation>& op,
//                             qc::fp startTime, qc::fp endTime,
//                             const NeutralAtomArchitecture& arch) {
//   std::string csvLine;
//
//   for (const auto& coordIdx : op->getUsedQubits()) {
//     // if coordIdx unmapped -> continue except it is an AodDeactivate
//     if (qc::OpType::AodDeactivate != op->getType() &&
//         coordIdxToId.find(coordIdx) == coordIdxToId.end()) {
//       continue;
//     }
//     if (op->getType() == qc::OpType::AodDeactivate) {
//       // check if there is a qubit at coordIdx
//       // if yes -> update coordIdxToId with new coordIdx
//       // if not -> throw exception
//       for (const auto& idAndCoord : idToCoord) {
//         auto id = idAndCoord.first;
//         auto coord = idAndCoord.second;
//         auto col = coordIdx % arch.getNcolumns();
//         auto row = coordIdx / arch.getNcolumns();
//         if (std::abs(coord.first - (col * arch.getInterQubitDistance())) <
//                 0.0001 &&
//             std::abs(coord.second - (row * arch.getInterQubitDistance())) <
//                 0.0001) {
//           // remove old coordIdx with same id
//           for (const auto& [oldCoordIdx, oldId] : coordIdxToId) {
//             if (oldId == id) {
//               coordIdxToId.erase(oldCoordIdx);
//               break;
//             }
//           }
//           // add new coordIdx with id
//           coordIdxToId[coordIdx] = id;
//           break;
//         }
//       }
//     }
//     if (coordIdxToId.find(coordIdx) == coordIdxToId.end() ||
//         idToCoord.find(coordIdxToId.at(coordIdx)) == idToCoord.end()) {
//       throw std::invalid_argument(
//           "Tried to create csv line for qubit at coordIdx " +
//           std::to_string(coordIdx) + " but there is no qubit at this
//           coordIdx");
//     }
//     auto id = coordIdxToId.at(coordIdx);
//     auto coord = idToCoord.at(id);
//     if (op->getType() == qc::OpType::AodActivate) {
//       addAxis(id);
//       if (axesIds.find(id) == axesIds.end()) {
//         throw std::invalid_argument(
//             "Tried to activate qubit at coordIdx " +
//             std::to_string(coordIdx)
//             + " but there is no axis for qubit " + std::to_string(id));
//       }
//       csvLine += createCsvLine(startTime, id, coord.first, coord.second, 1,
//                                colorSlm, true, axesIds.at(id));
//       csvLine += createCsvLine(endTime, id, coord.first, coord.second, 1,
//                                colorAod, true, axesIds.at(id));
//     } else if (op->getType() == qc::OpType::AodDeactivate) {
//       if (axesIds.find(id) == axesIds.end()) {
//         throw std::invalid_argument("Tried to deactivate qubit at coordIdx
//         "
//         +
//                                     std::to_string(coordIdx) +
//                                     " but there is no axis for qubit " +
//                                     std::to_string(id));
//       }
//       csvLine += createCsvLine(startTime, id, coord.first, coord.second, 1,
//                                colorAod, true, axesIds.at(id));
//       csvLine += createCsvLine(endTime, id, coord.first, coord.second, 1,
//                                colorSlm, true, axesIds.at(id));
//       removeAxis(id);
//
//     } else if (op->getType() == qc::OpType::AodMove) {
//       if (axesIds.find(id) == axesIds.end()) {
//         throw std::invalid_argument(
//             "Tried to move qubit at coordIdx " + std::to_string(coordIdx) +
//             " but there is no axis for qubit " + std::to_string(id));
//       }
//       csvLine += createCsvLine(startTime, id, coord.first, coord.second, 1,
//                                colorAod, true, axesIds.at(id));
//
//       // update atom coordinates
//       auto startsX =
//           dynamic_cast<AodOperation*>(op.get())->getStarts(Dimension::X);
//       auto endsX =
//       dynamic_cast<AodOperation*>(op.get())->getEnds(Dimension::X); auto
//       startsY =
//           dynamic_cast<AodOperation*>(op.get())->getStarts(Dimension::Y);
//       auto endsY =
//       dynamic_cast<AodOperation*>(op.get())->getEnds(Dimension::Y); for
//       (size_t i = 0; i < startsX.size(); i++) {
//         if (std::abs(startsX[i] - coord.first) < 0.0001) {
//           coord.first = endsX[i];
//         }
//       }
//       for (size_t i = 0; i < startsY.size(); i++) {
//         if (std::abs(startsY[i] - coord.second) < 0.0001) {
//           coord.second = endsY[i];
//         }
//       }
//       // save new coordinates
//       idToCoord[id] = coord;
//       csvLine += createCsvLine(endTime, id, coord.first, coord.second, 1,
//                                colorAod, true, axesIds.at(id));
//     } else if (op->getUsedQubits().size() > 1) { // multi qubit gates
//       addMargin(id);
//       csvLine += createCsvLine(startTime, id, coord.first, coord.second, 1,
//                                colorSlm, false, 0, false,
//                                marginIds.at(id));
//       auto midTime = (startTime + endTime) / 2;
//       csvLine +=
//           createCsvLine(midTime, id, coord.first, coord.second, 1, colorCz,
//                         false, 0, true, marginIds.at(id),
//                         arch.getBlockingFactor() *
//                         arch.getInteractionRadius() *
//                             arch.getInterQubitDistance());
//       csvLine += createCsvLine(endTime, id, coord.first, coord.second, 1,
//                                colorSlm, false, 0, false,
//                                marginIds.at(id));
//       removeMargin(id);
//
//     } else { // single qubit gates
//       csvLine +=
//           createCsvLine(startTime, id, coord.first, coord.second, 1,
//           colorSlm);
//       auto midTime = (startTime + endTime) / 2;
//       csvLine +=
//           createCsvLine(midTime, id, coord.first, coord.second, 1,
//           colorLocal);
//       csvLine +=
//           createCsvLine(endTime, id, coord.first, coord.second, 1,
//           colorSlm);
//     }
//   }
//   return csvLine;
// }
// std::string AnimationAtoms::createCsvLine(const qc::fp startTime,
//                                           const HwQubit id, const qc::fp x,
//                                           const qc::fp y, const uint32_t
//                                           size, const uint32_t color, const
//                                           bool axes, const axesId axId,
//                                           const bool margin, const marginId
//                                           marginId, const qc::fp
//                                           marginSize)
//                                           {
//   std::string csvLine;
//   csvLine +=
//       std::to_string(startTime) + ";" + std::to_string(id) + ";" +
//       std::to_string(x) + ";" + std::to_string(y) + ";" +
//       std::to_string(size) +
//       ";" + std::to_string(color) + ";" + std::to_string(color) + ";" +
//       std::to_string(static_cast<int>(axes)) + ";" + std::to_string(axId) +
//       ";" + std::to_string(static_cast<int>(margin)) + ";" +
//       std::to_string(marginId) + ";" + std::to_string(marginSize) + "\n";
//   return csvLine;
// }

} // namespace na
