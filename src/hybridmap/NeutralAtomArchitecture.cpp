//
// This file is part of the MQT QMAP library released under the MIT license.
// See README.md or go to https://github.com/cda-tum/qmap for more information.
//

#include "hybridmap/NeutralAtomArchitecture.hpp"

#include "datastructures/SymmetricMatrix.hpp"
#include "hybridmap/NeutralAtomDefinitions.hpp"
#include "hybridmap/NeutralAtomUtils.hpp"
#include "ir/Definitions.hpp"
#include "ir/operations/AodOperation.hpp"
#include "ir/operations/OpType.hpp"
#include "ir/operations/Operation.hpp"
#include "na/entities/Location.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
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
    qc::fp const swapGateTime =
        (gateTimes.at("cz") * 3) + (gateTimes.at("h") * 4);
    qc::fp const swapGateFidelity =
        std::pow(gateAverageFidelities.at("cz"), 3) *
        std::pow(gateAverageFidelities.at("h"), 6);
    this->parameters.gateTimes.emplace("swap", swapGateTime);
    this->parameters.gateAverageFidelities.emplace("swap", swapGateFidelity);

    // compute values for Bridge gate
    // precompute bridge circuits
    for (size_t i = 3; i < 10; i++) {
      qc::fp const bridgeGateTime =
          (static_cast<qc::fp>(bridgeCircuits.czDepth[i]) *
           gateTimes.at("cz")) +
          (static_cast<qc::fp>(bridgeCircuits.hDepth[i]) * gateTimes.at("h"));
      qc::fp const bridgeFidelity =
          std::pow(gateAverageFidelities.at("cz"), bridgeCircuits.czs[i]) *
          std::pow(gateAverageFidelities.at("h"), bridgeCircuits.hs[i]);
      this->parameters.gateTimes.emplace("bridge" + std::to_string(i),
                                         bridgeGateTime);
      this->parameters.gateAverageFidelities.emplace(
          "bridge" + std::to_string(i), bridgeFidelity);
    }

    this->parameters.shuttlingTimes = shuttlingTimes;
    std::map<qc::OpType, qc::fp> shuttlingAverageFidelities;
    for (const auto& [key, value] :
         jsonDataParameters["shuttlingAverageFidelities"].items()) {
      shuttlingAverageFidelities.emplace(qc::opTypeFromString(key), value);
    }
    this->parameters.shuttlingAverageFidelities = shuttlingAverageFidelities;

    this->parameters.decoherenceTimes =
        NeutralAtomArchitecture::Parameters::DecoherenceTimes{
            jsonDataParameters["decoherenceTimes"]["t1"],
            jsonDataParameters["decoherenceTimes"]["t2"]};

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
        Location{static_cast<double>(i % this->properties.getNcolumns()),
                 // NOLINTNEXTLINE(bugprone-integer-division)
                 static_cast<double>(i / this->properties.getNcolumns())});
  }
}
NeutralAtomArchitecture::NeutralAtomArchitecture(const std::string& filename) {
  this->loadJson(filename);
}

void NeutralAtomArchitecture::computeSwapDistances(
    const qc::fp interactionRadius) {
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
          Location{0.0, 0.0},
          Location{static_cast<double>(i), static_cast<double>(j)});
      if (dist <= interactionRadius) {
        if (dist == 0) {
          continue;
        }
        diagonalDistances.emplace_back(DiagonalDistance{i, j, dist});
        if (i != j) {
          diagonalDistances.emplace_back(DiagonalDistance{j, i, dist});
        }
      } else {
        break;
      }
    }
  }
  // sort diagonal distances by distance
  std::sort(diagonalDistances.begin(), diagonalDistances.end(),
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
  this->nearbyCoordinates =
      std::vector(this->getNpositions(), std::set<CoordIndex>());
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

std::vector<CoordIndex>
NeutralAtomArchitecture::getNN(const CoordIndex idx) const {
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
std::string NeutralAtomArchitecture::getAnimationMachine(
    const qc::fp shuttlingSpeedFactor) const {
  std::string animationMachine = "name: \"Hyrbid_" + this->name + "\"\n";

  animationMachine +=
      "movement {\n\tmax_speed: " +
      std::to_string(this->getShuttlingTime(qc::OpType::AodMove) *
                     shuttlingSpeedFactor) +
      "\n}\n";

  animationMachine +=
      "time {\n\tload: " +
      std::to_string(this->getShuttlingTime(qc::OpType::AodActivate) *
                     shuttlingSpeedFactor) +
      "\n\tstore: " +
      std::to_string(this->getShuttlingTime(qc::OpType::AodDeactivate) *
                     shuttlingSpeedFactor) +
      "\n\trz: " + std::to_string(this->getGateTime("x")) +
      "\n\try: " + std::to_string(this->getGateTime("x")) +
      "\n\tcz: " + std::to_string(this->getGateTime("cz")) +
      "\n\tunit: \"us\"\n}\n";

  animationMachine += "distance {\n\tinteraction: " +
                      std::to_string(this->getInteractionRadius() *
                                     this->getInterQubitDistance()) +
                      "\n\tunit: \"um\"\n}\n";
  const auto zoneStart = -this->getInterQubitDistance();
  const auto zoneEndX = this->getNcolumns() * this->getInterQubitDistance() +
                        this->getInterQubitDistance();
  const auto zoneEndY = this->getNrows() * this->getInterQubitDistance() +
                        this->getInterQubitDistance();
  animationMachine += "zone hybrid {\n\tfrom: (" + std::to_string(zoneStart) +
                      ", " + std::to_string(zoneStart) + ")\n\tto: (" +
                      std::to_string(zoneEndX) + ", " +
                      std::to_string(zoneEndY) + ") \n}\n";

  for (size_t colIdx = 0; colIdx < this->getNcolumns(); colIdx++) {
    for (size_t rowIdx = 0; rowIdx < this->getNrows(); rowIdx++) {
      const auto coordIdx = colIdx + (rowIdx * this->getNcolumns());
      animationMachine +=
          "trap trap" + std::to_string(coordIdx) + " {\n\tposition: (" +
          std::to_string(colIdx * this->getInterQubitDistance()) + ", " +
          std::to_string(rowIdx * this->getInterQubitDistance()) + ")\n}\n";
    }
  }

  return animationMachine;
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
  for (size_t i = 0; i < op->getNqubits() - 1; ++i) {
    opName += "c";
  }
  if (op->getType() == qc::OpType::P) {
    // use time of theta = pi and linearly scale
    opName += "z";
    auto param = abs(op->getParameter().back());
    constexpr auto pi = 3.14159265358979323846;
    while (param > pi) {
      param = abs(param - (2 * pi));
    }
    return getGateTime(opName) * param / 3.14159265358979323846;
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
  for (size_t i = 0; i < op->getNqubits() - 1; ++i) {
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
  for (auto coord : op->getUsedQubits()) {
    // qubits in ancilla register
    while (coord >= getNpositions()) {
      coord -= getNpositions();
    }
    for (uint32_t i = 0; i < getNpositions(); ++i) {
      if (i == coord) {
        continue;
      }
      // do a preselection
      // now check exact difference
      const auto distance = getEuclideanDistance(coord, i);
      if (distance <= getBlockingFactor() * getInteractionRadius()) {
        blockedCoordIndices.emplace(i);
        blockedCoordIndices.emplace(i + getNpositions());
        blockedCoordIndices.emplace(i + (2 * getNpositions()));
      }
    }
  }
  return blockedCoordIndices;
}
} // namespace na
