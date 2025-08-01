/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "hybridmap/NeutralAtomArchitecture.hpp"

#include "datastructures/SymmetricMatrix.hpp"
#include "hybridmap/NeutralAtomDefinitions.hpp"
#include "ir/Definitions.hpp"
#include "ir/operations/AodOperation.hpp"
#include "ir/operations/OpType.hpp"
#include "ir/operations/Operation.hpp"
#include "na/entities/Location.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <fstream>
#include <map>
#include <nlohmann/json.hpp>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace na {

void NeutralAtomArchitecture::loadJson(const std::string& filename) {
  nlohmann::basic_json<> jsonData;
  std::ifstream architectureFile(filename);

  if (!architectureFile.is_open()) {
    throw std::runtime_error("Could not open file " + filename);
  }
  try {
    architectureFile >> jsonData;
    architectureFile.close();

    // Load properties
    nlohmann::basic_json<> jsonDataProperties = jsonData["properties"];
    this->properties = Properties(
        jsonDataProperties["nRows"], jsonDataProperties["nColumns"],
        jsonDataProperties["nAods"], jsonDataProperties["nAodCoordinates"],
        jsonDataProperties["interQubitDistance"],
        jsonDataProperties["interactionRadius"],
        jsonDataProperties["blockingFactor"],
        jsonDataProperties["minimalAodDistance"]);

    // Load parameters
    const nlohmann::basic_json<> jsonDataParameters = jsonData["parameters"];
    this->parameters = Parameters();
    this->parameters.nQubits = jsonDataParameters["nQubits"];

    // check if qubits can fit in the architecture
    if (this->parameters.nQubits > this->properties.getNpositions()) {
      throw std::runtime_error("Number of qubits exceeds number of positions");
    }

    std::map<std::string, qc::fp> gateTimes;
    for (const auto& [key, value] : jsonDataParameters["gateTimes"].items()) {
      gateTimes.emplace(key, value);
    }
    this->parameters.gateTimes = gateTimes;
    std::map<std::string, qc::fp> gateAverageFidelities;
    for (const auto& [key, value] :
         jsonDataParameters["gateAverageFidelities"].items()) {
      gateAverageFidelities.emplace(key, value);
    }
    this->parameters.gateAverageFidelities = gateAverageFidelities;
    std::map<qc::OpType, qc::fp> shuttlingTimes;

    for (const auto& [key, value] :
         jsonDataParameters["shuttlingTimes"].items()) {
      shuttlingTimes.emplace(qc::opTypeFromString(key), value);
    }
    // compute values for SWAP gate
    qc::fp swapGateTime = 0;
    qc::fp swapGateFidelity = 1;
    for (size_t i = 0; i < 3; ++i) {
      swapGateTime += gateTimes.at("cz");
      swapGateFidelity *= gateAverageFidelities.at("cz");
    }
    for (size_t i = 0; i < 6; ++i) {
      swapGateTime += gateTimes.at("h");
      swapGateFidelity *= gateAverageFidelities.at("h");
    }
    this->parameters.gateTimes.emplace("swap", swapGateTime);
    this->parameters.gateAverageFidelities.emplace("swap", swapGateFidelity);

    this->parameters.shuttlingTimes = shuttlingTimes;
    std::map<qc::OpType, qc::fp> shuttlingAverageFidelities;
    for (const auto& [key, value] :
         jsonDataParameters["shuttlingAverageFidelities"].items()) {
      shuttlingAverageFidelities.emplace(qc::opTypeFromString(key), value);
    }
    this->parameters.shuttlingAverageFidelities = shuttlingAverageFidelities;

    this->parameters.decoherenceTimes =
        NeutralAtomArchitecture::Parameters::DecoherenceTimes{
            .t1 = jsonDataParameters["decoherenceTimes"]["t1"],
            .t2 = jsonDataParameters["decoherenceTimes"]["t2"]};

  } catch (std::exception& e) {
    throw std::runtime_error("Could not parse JSON file " + filename + ": " +
                             e.what());
  }

  // apply changes to the object
  this->name = jsonData["name"];

  this->createCoordinates();
  this->computeSwapDistances(this->properties.getInteractionRadius());
  this->computeNearbyCoordinates();
}
void NeutralAtomArchitecture::createCoordinates() {
  coordinates.reserve(properties.getNpositions());
  for (std::uint16_t i = 0; i < this->properties.getNpositions(); i++) {
    this->coordinates.emplace_back(
        Location{.x = static_cast<double>(i % this->properties.getNcolumns()),
                 // NOLINTNEXTLINE(bugprone-integer-division)
                 .y = static_cast<double>(i / this->properties.getNcolumns())});
  }
}
NeutralAtomArchitecture::NeutralAtomArchitecture(const std::string& filename) {
  this->loadJson(filename);
}

void NeutralAtomArchitecture::computeSwapDistances(qc::fp interactionRadius) {
  // compute diagonal distances
  struct DiagonalDistance {
    std::uint32_t x;
    std::uint32_t y;
    qc::fp distance;
  };
  std::vector<DiagonalDistance> diagonalDistances;

  for (uint32_t i = 0; i < this->getNcolumns() && i < interactionRadius; i++) {
    for (uint32_t j = i; j < this->getNrows(); j++) {
      const auto dist = NeutralAtomArchitecture::getEuclideanDistance(
          Location{.x = 0.0, .y = 0.0},
          Location{.x = static_cast<double>(i), .y = static_cast<double>(j)});
      if (dist <= interactionRadius) {
        if (dist == 0) {
          continue;
        }
        diagonalDistances.emplace_back(
            DiagonalDistance{.x = i, .y = j, .distance = dist});
        if (i != j) {
          diagonalDistances.emplace_back(
              DiagonalDistance{.x = j, .y = i, .distance = dist});
        }
      } else {
        break;
      }
    }
  }
  // sort diagonal distances by distance
  std::ranges::sort(diagonalDistances,
                    [](const DiagonalDistance& a, const DiagonalDistance& b) {
                      return a.distance < b.distance;
                    });

  // compute swap distances
  this->swapDistances =
      qc::SymmetricMatrix<SwapDistance>(this->getNpositions());

  for (uint32_t coordIndex1 = 0; coordIndex1 < this->getNpositions();
       coordIndex1++) {
    for (uint32_t coordIndex2 = 0; coordIndex2 < coordIndex1; coordIndex2++) {
      auto deltaX = this->getManhattanDistanceX(coordIndex1, coordIndex2);
      auto deltaY = this->getManhattanDistanceY(coordIndex1, coordIndex2);

      // check if one can go diagonal to reduce the swap distance
      int32_t swapDistance = 0;
      for (auto it = diagonalDistances.rbegin(); it != diagonalDistances.rend();
           ++it) {
        const auto& diagonalDistance = *it;
        while (deltaX >= diagonalDistance.x && deltaY >= diagonalDistance.y) {
          swapDistance += 1;
          deltaX -= diagonalDistance.x;
          deltaY -= diagonalDistance.y;
        }
      }
      // save swap distance in matrix
      this->swapDistances(coordIndex1, coordIndex2) = swapDistance - 1;
      this->swapDistances(coordIndex2, coordIndex1) = swapDistance - 1;
    }
  }
}

void NeutralAtomArchitecture::computeNearbyCoordinates() {
  this->nearbyCoordinates = std::vector<std::set<CoordIndex>>(
      this->getNpositions(), std::set<CoordIndex>());
  for (CoordIndex coordIndex = 0; coordIndex < this->getNpositions();
       coordIndex++) {
    for (CoordIndex otherCoordIndex = 0; otherCoordIndex < coordIndex;
         otherCoordIndex++) {
      if (this->getSwapDistance(coordIndex, otherCoordIndex) == 0) {
        this->nearbyCoordinates.at(coordIndex).emplace(otherCoordIndex);
        this->nearbyCoordinates.at(otherCoordIndex).emplace(coordIndex);
      }
    }
  }
}

std::vector<CoordIndex> NeutralAtomArchitecture::getNN(CoordIndex idx) const {
  std::vector<CoordIndex> nn;
  if (idx % this->getNcolumns() != 0) {
    nn.emplace_back(idx - 1);
  }
  if (idx % this->getNcolumns() != this->getNcolumns() - 1U) {
    nn.emplace_back(idx + 1);
  }
  if (idx >= this->getNcolumns()) {
    nn.emplace_back(idx - this->getNcolumns());
  }
  if (idx <
      static_cast<CoordIndex>(this->getNpositions() - this->getNcolumns())) {
    nn.emplace_back(idx + this->getNcolumns());
  }
  return nn;
}

qc::fp NeutralAtomArchitecture::getOpTime(const qc::Operation* op) const {
  if (op->getType() == qc::OpType::AodActivate ||
      op->getType() == qc::OpType::AodDeactivate) {
    return getShuttlingTime(op->getType());
  }
  if (op->getType() == qc::OpType::AodMove) {
    const auto v = this->parameters.shuttlingTimes.at(op->getType());
    const auto* const opAodMove = dynamic_cast<const AodOperation*>(op);
    const auto distanceX = opAodMove->getMaxDistance(Dimension::X);
    const auto distanceY = opAodMove->getMaxDistance(Dimension::Y);
    return (distanceX + distanceY) / v;
  }
  std::string opName;
  for (size_t i = 0; i < op->getNcontrols(); ++i) {
    opName += "c";
  }
  opName += op->getName();
  return getGateTime(opName);
}

qc::fp NeutralAtomArchitecture::getOpFidelity(const qc::Operation* op) const {
  if (op->getType() == qc::OpType::AodActivate ||
      op->getType() == qc::OpType::AodDeactivate ||
      op->getType() == qc::OpType::AodMove) {
    return getShuttlingAverageFidelity(op->getType());
  }
  std::string opName;
  for (size_t i = 0; i < op->getNcontrols(); ++i) {
    opName += "c";
  }
  opName += op->getName();
  return getGateAverageFidelity(opName);
}

std::set<CoordIndex>
NeutralAtomArchitecture::getBlockedCoordIndices(const qc::Operation* op) const {
  if (op->getNqubits() == 1 || op->getType() == qc::OpType::AodActivate ||
      op->getType() == qc::OpType::AodDeactivate ||
      op->getType() == qc::OpType::AodMove) {
    return op->getUsedQubits();
  }
  std::set<CoordIndex> blockedCoordIndices;
  for (const auto& coord : op->getUsedQubits()) {
    for (uint32_t i = 0; i < getNqubits(); ++i) {
      if (i == coord) {
        continue;
      }
      // do a preselection
      // now check exact difference
      const auto distance = getEuclideanDistance(coord, i);
      if (distance <= getBlockingFactor() * getInteractionRadius()) {
        blockedCoordIndices.emplace(i);
      }
    }
  }
  return blockedCoordIndices;
}
} // namespace na
