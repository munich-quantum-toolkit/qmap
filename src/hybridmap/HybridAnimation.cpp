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
AnimationAtoms::AnimationAtoms(const std::map<HwQubit, HwQubit>& initHwPos,
                               const NeutralAtomArchitecture& arch) {
  auto nCols = arch.getNcolumns();

  for (const auto& [id, coord] : initHwPos) {
    coordIdxToId[coord] = id;
    auto column = coord % nCols;
    auto row = coord / nCols;
    idToCoord[id] = {column * arch.getInterQubitDistance(),
                     row * arch.getInterQubitDistance()};
  }
}

std::string AnimationAtoms::getInitString() {
  std::string initString;
  initString +=
      "time;id;x;y;size;fill;color;axes;axesId;margin;marginId;marginSize\n";
  for (const auto& [id, coord] : idToCoord) {
    initString += "0.000;" + std::to_string(id) + ";" +
                  std::to_string(coord.first) + ";" +
                  std::to_string(coord.second) + ";1;0;0;0;0;0;0;0\n";
  }
  return initString;
}

std::string AnimationAtoms::getEndString(qc::fp endTime) {
  std::string initString;
  for (const auto& [id, coord] : idToCoord) {
    initString += std::to_string(endTime) + ";" + std::to_string(id) + ";" +
                  std::to_string(coord.first) + ";" +
                  std::to_string(coord.second) + ";1;0;0;0;0;0;0;0\n";
  }
  return initString;
}

AnimationAtoms::axesId AnimationAtoms::addAxis(HwQubit id) {
  if (!axesIds.contains(id)) {
    axesIdCounter++;
    axesIds[id] = axesIdCounter;
  } else {
    throw std::invalid_argument(
        "Tried to add axis but axis already exists for qubit " +
        std::to_string(id));
  }
  return axesIds[id];
}
AnimationAtoms::marginId AnimationAtoms::addMargin(HwQubit id) {
  if (!marginIds.contains(id)) {
    marginIdCounter++;
    marginIds[id] = marginIdCounter;
  } else {
    throw std::invalid_argument(
        "Tried to add margin but margin already exists for qubit " +
        std::to_string(id));
  }
  return marginIds[id];
}

std::string
AnimationAtoms::createCsvOp(const std::unique_ptr<qc::Operation>& op,
                            qc::fp startTime, qc::fp endTime,
                            const NeutralAtomArchitecture& arch) {
  std::string csvLine;

  for (const auto& coordIdx : op->getUsedQubits()) {
    // if coordIdx unmapped -> continue except it is an AodDeactivate
    if (qc::OpType::AodDeactivate != op->getType() &&
        !coordIdxToId.contains(coordIdx)) {
      continue;
    }
    if (op->getType() == qc::OpType::AodDeactivate) {
      // check if there is a qubit at coordIdx
      // if yes -> update coordIdxToId with new coordIdx
      // if not -> throw exception
      for (const auto& idAndCoord : idToCoord) {
        auto id = idAndCoord.first;
        auto coord = idAndCoord.second;
        auto col = coordIdx % arch.getNcolumns();
        auto row = coordIdx / arch.getNcolumns();
        if (std::abs(coord.first - col * arch.getInterQubitDistance()) <
                0.0001 &&
            std::abs(coord.second - row * arch.getInterQubitDistance()) <
                0.0001) {
          // remove old coordIdx with same id
          for (const auto& [oldCoordIdx, oldId] : coordIdxToId) {
            if (oldId == id) {
              coordIdxToId.erase(oldCoordIdx);
              break;
            }
          }
          // add new coordIdx with id
          coordIdxToId[coordIdx] = id;
          break;
        }
      }
    }
    if (!coordIdxToId.contains(coordIdx) ||
        !idToCoord.contains(coordIdxToId.at(coordIdx))) {
      throw std::invalid_argument(
          "Tried to create csv line for qubit at coordIdx " +
          std::to_string(coordIdx) + " but there is no qubit at this coordIdx");
    }
    auto id = coordIdxToId.at(coordIdx);
    auto coord = idToCoord.at(id);
    if (op->getType() == qc::OpType::AodActivate) {
      addAxis(id);
      if (!axesIds.contains(id)) {
        throw std::invalid_argument(
            "Tried to activate qubit at coordIdx " + std::to_string(coordIdx) +
            " but there is no axis for qubit " + std::to_string(id));
      }
      csvLine += createCsvLine(startTime, id, coord.first, coord.second, 1,
                               colorSlm, true, axesIds.at(id));
      csvLine += createCsvLine(endTime, id, coord.first, coord.second, 1,
                               colorAod, true, axesIds.at(id));
    } else if (op->getType() == qc::OpType::AodDeactivate) {
      if (!axesIds.contains(id)) {
        throw std::invalid_argument("Tried to deactivate qubit at coordIdx " +
                                    std::to_string(coordIdx) +
                                    " but there is no axis for qubit " +
                                    std::to_string(id));
      }
      csvLine += createCsvLine(startTime, id, coord.first, coord.second, 1,
                               colorAod, true, axesIds.at(id));
      csvLine += createCsvLine(endTime, id, coord.first, coord.second, 1,
                               colorSlm, true, axesIds.at(id));
      removeAxis(id);

    } else if (op->getType() == qc::OpType::AodMove) {
      if (!axesIds.contains(id)) {
        throw std::invalid_argument(
            "Tried to move qubit at coordIdx " + std::to_string(coordIdx) +
            " but there is no axis for qubit " + std::to_string(id));
      }
      csvLine += createCsvLine(startTime, id, coord.first, coord.second, 1,
                               colorAod, true, axesIds.at(id));

      // update atom coordinates
      auto startsX =
          dynamic_cast<AodOperation*>(op.get())->getStarts(Dimension::X);
      auto endsX = dynamic_cast<AodOperation*>(op.get())->getEnds(Dimension::X);
      auto startsY =
          dynamic_cast<AodOperation*>(op.get())->getStarts(Dimension::Y);
      auto endsY = dynamic_cast<AodOperation*>(op.get())->getEnds(Dimension::Y);
      for (size_t i = 0; i < startsX.size(); i++) {
        if (std::abs(startsX[i] - coord.first) < 0.0001) {
          coord.first = endsX[i];
        }
      }
      for (size_t i = 0; i < startsY.size(); i++) {
        if (std::abs(startsY[i] - coord.second) < 0.0001) {
          coord.second = endsY[i];
        }
      }
      // save new coordinates
      idToCoord[id] = coord;
      csvLine += createCsvLine(endTime, id, coord.first, coord.second, 1,
                               colorAod, true, axesIds.at(id));
    } else if (op->getUsedQubits().size() > 1) { // multi qubit gates
      addMargin(id);
      csvLine += createCsvLine(startTime, id, coord.first, coord.second, 1,
                               colorSlm, false, 0, false, marginIds.at(id));
      auto midTime = (startTime + endTime) / 2;
      csvLine +=
          createCsvLine(midTime, id, coord.first, coord.second, 1, colorCz,
                        false, 0, true, marginIds.at(id),
                        arch.getBlockingFactor() * arch.getInteractionRadius() *
                            arch.getInterQubitDistance());
      csvLine += createCsvLine(endTime, id, coord.first, coord.second, 1,
                               colorSlm, false, 0, false, marginIds.at(id));
      removeMargin(id);

    } else { // single qubit gates
      csvLine +=
          createCsvLine(startTime, id, coord.first, coord.second, 1, colorSlm);
      auto midTime = (startTime + endTime) / 2;
      csvLine +=
          createCsvLine(midTime, id, coord.first, coord.second, 1, colorLocal);
      csvLine +=
          createCsvLine(endTime, id, coord.first, coord.second, 1, colorSlm);
    }
  }
  return csvLine;
}
std::string AnimationAtoms::createCsvLine(
    qc::fp startTime, HwQubit id, qc::fp x, qc::fp y, uint32_t size,
    uint32_t color, bool axes, AnimationAtoms::axesId axId, bool margin,
    AnimationAtoms::marginId marginId, qc::fp marginSize) {
  std::string csvLine;
  csvLine +=
      std::to_string(startTime) + ";" + std::to_string(id) + ";" +
      std::to_string(x) + ";" + std::to_string(y) + ";" + std::to_string(size) +
      ";" + std::to_string(color) + ";" + std::to_string(color) + ";" +
      std::to_string(static_cast<int>(axes)) + ";" + std::to_string(axId) +
      ";" + std::to_string(static_cast<int>(margin)) + ";" +
      std::to_string(marginId) + ";" + std::to_string(marginSize) + "\n";
  return csvLine;
}

} // namespace na
