//
// This file is part of the MQT QMAP library released under the MIT license.
// See README.md or go to https://github.com/cda-tum/qmap for more information.
//

#pragma once

#include "Definitions.hpp"
#include "datastructures/SymmetricMatrix.hpp"
#include "hybridmap/NeutralAtomDefinitions.hpp"
#include "hybridmap/NeutralAtomUtils.hpp"
#include "hybridmap/default_style.hpp"
#include "ir/QuantumComputation.hpp"
#include "ir/operations/OpType.hpp"
#include "ir/operations/Operation.hpp"
#include "na/NADefinitions.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace na {
/**
 * @brief Class to store the properties of a neutral atom architecture
 * @details
 * The properties of a neutral atom architecture are:
 * - number of rows
 * - number of columns
 * - number of AODs
 * - number of AOD coordinates
 * - inter-qubit distance
 * - interaction radius
 * - blocking factor
 * - minimal AOD distance
 * The properties are loaded from a JSON file.
 *
 * The class also provides functions to compute the swap distances between
 * qubits and the nearby qubits for each qubit.
 */
class NeutralAtomArchitecture {
  /**
   * @brief Class to store the properties of a neutral atom architecture
   * @details
   * The properties of a neutral atom architecture are:
   * - number of rows
   * - number of columns
   * - number of AODs
   * - number of AOD coordinates
   * - inter-qubit distance
   * - interaction radius
   * - blocking factor
   * - minimal AOD distance
   * The properties are loaded from a JSON file and are fixed for each
   * architecture.
   */
  class Properties {
  protected:
    std::uint16_t nRows;
    std::uint16_t nColumns;
    std::uint16_t nAods;
    std::uint16_t nAodIntermediateLevels;
    std::uint16_t nAodCoordinates;
    qc::fp interQubitDistance;
    qc::fp interactionRadius;
    qc::fp blockingFactor;

  public:
    Properties() = default;
    Properties(const std::uint16_t rows, const std::uint16_t columns,
               const std::uint16_t aods, const std::uint16_t aodCoordinates,
               const qc::fp qubitDistance, const qc::fp radius,
               const qc::fp blockingFac, const qc::fp aodDist)
        : nRows(rows), nColumns(columns), nAods(aods),
          nAodIntermediateLevels(
              static_cast<uint16_t>(qubitDistance / aodDist)),
          nAodCoordinates(aodCoordinates), interQubitDistance(qubitDistance),
          interactionRadius(radius), blockingFactor(blockingFac) {}
    [[nodiscard]] std::uint16_t getNpositions() const {
      return nRows * nColumns;
    }
    [[nodiscard]] std::uint16_t getNrows() const { return nRows; }
    [[nodiscard]] std::uint16_t getNcolumns() const { return nColumns; }
    [[nodiscard]] std::uint16_t getNAods() const { return nAods; }
    [[nodiscard]] std::uint16_t getNAodCoordinates() const {
      return nAodCoordinates;
    }
    [[nodiscard]] std::uint16_t getNAodIntermediateLevels() const {
      return nAodIntermediateLevels;
    }
    [[nodiscard]] qc::fp getInterQubitDistance() const {
      return interQubitDistance;
    }
    [[nodiscard]] qc::fp getInteractionRadius() const {
      return interactionRadius;
    }
    [[nodiscard]] qc::fp getBlockingFactor() const { return blockingFactor; }
  };

  /**
   * @brief Class to store the parameters of a neutral atom architecture
   * @details
   * The parameters of a neutral atom architecture are:
   * - number of qubits
   * - gate times
   * - gate average fidelities
   * - shuttling times
   * - shuttling average fidelities
   * - decoherence times
   * The parameters are loaded from a JSON file.
   * The difference to the properties is that the parameters can change
   * from run to run.
   */
  struct Parameters {
    /**
     * @brief Struct to store the decoherence times of a neutral atom
     * architecture
     * @details
     * The decoherence times of a neutral atom architecture are:
     * - T1 [µs]
     * - T2 [µs]
     * - effective decoherence time [µs]
     */
    struct DecoherenceTimes {
      qc::fp t1 = 0;
      qc::fp t2 = 0;

      [[nodiscard]] qc::fp tEff() const {
        if (t1 == 0 && t2 == 0) {
          return 0;
        }
        return t1 * t2 / (t1 + t2);
      }
    };
    CoordIndex nQubits;
    std::map<std::string, qc::fp> gateTimes;
    std::map<std::string, qc::fp> gateAverageFidelities;
    std::map<qc::OpType, qc::fp> shuttlingTimes;
    std::map<qc::OpType, qc::fp> shuttlingAverageFidelities;
    DecoherenceTimes decoherenceTimes;
  };

protected:
  Properties properties{};
  Parameters parameters;

  std::vector<Point> coordinates;
  SymmetricMatrix<SwapDistance> swapDistances;
  std::vector<std::set<CoordIndex>> nearbyCoordinates;

  BridgeCircuits bridgeCircuits = BridgeCircuits(10);

  /**
   * @brief Create the coordinates.
   */
  void createCoordinates();
  /**
   * @brief Compute the swap distances between the coordinates
   * @details
   * The swap distances are computed using the coordinates of the qubits.
   * The swap distance is the distance between the qubits in terms of
   * edges in the resulting connectivity graph. This can be computed
   * beforehand.
   */
  void computeSwapDistances(qc::fp interactionRadius);
  /**
   * @brief Compute the nearby coordinates for each coordinate
   * @details
   * The nearby qubits are the qubits that are close enough to be connected
   * by an edge in the resulting connectivity graph. This can be be computed
   * beforehand.
   */
  void computeNearbyCoordinates();

public:
  std::string name;

  /**
   * @brief Construct a new Neutral Atom Architecture object
   * @details
   * The properties of the architecture are loaded from a JSON file.
   * @param filename The name of the JSON file
   */
  explicit NeutralAtomArchitecture(const std::string& filename);

  /**
   * @brief Load the properties of the architecture from a JSON file
   * @param filename The name of the JSON file
   */
  void loadJson(const std::string& filename);

  // Getters
  /**
   * @brief Get the number of rows
   * @return The number of rows
   */
  [[nodiscard]] std::uint16_t getNrows() const { return properties.getNrows(); }
  /**
   * @brief Get the number of columns
   * @return The number of columns
   */
  [[nodiscard]] std::uint16_t getNcolumns() const {
    return properties.getNcolumns();
  }
  /**
   * @brief Get the number of positions
   * @return The number of positions
   */
  [[nodiscard]] std::uint16_t getNpositions() const {
    return properties.getNpositions();
  }
  /**
   * @brief Get the number of AODs
   * @return The number of AODs
   */
  [[maybe_unused]] [[nodiscard]] std::uint16_t getNAods() const {
    return properties.getNAods();
  }
  /**
   * @brief Get the number of AOD coordinates
   * @return The number of AOD coordinates
   */
  [[nodiscard]] [[maybe_unused]] std::uint16_t getNAodCoordinates() const {
    return properties.getNAodCoordinates();
  }
  /**
   * @brief Get the number of qubits
   * @return The number of qubits
   */
  [[nodiscard]] CoordIndex getNqubits() const { return parameters.nQubits; }
  /**
   * @brief Get the inter-qubit distance
   * @return The inter-qubit distance
   */
  [[nodiscard]] qc::fp getInterQubitDistance() const {
    return properties.getInterQubitDistance();
  }

  [[nodiscard]] qc::fp getOffsetDistance() const {
    return getInterQubitDistance() / getNAodIntermediateLevels();
  }

  /**
   * @brief Get the interaction radius
   * @return The interaction radius
   */
  [[nodiscard]] qc::fp getInteractionRadius() const {
    return properties.getInteractionRadius();
  }
  /**
   * @brief Get the blocking factor
   * @return The blocking factor
   */
  [[nodiscard]] qc::fp getBlockingFactor() const {
    return properties.getBlockingFactor();
  }
  /**
   * @brief Get precomputed swap distance between two coordinates
   * @param idx1 The index of the first coordinate
   * @param idx2 The index of the second coordinate
   * @return The swap distance between the two coordinates
   */
  [[nodiscard]] SwapDistance getSwapDistance(const CoordIndex idx1,
                                             const CoordIndex idx2) const {
    return swapDistances(idx1, idx2);
  }
  /**
   * @brief Get precomputed swap distance between two coordinates
   * @param c1 The first coordinate
   * @param c2 The second coordinate
   * @return The swap distance between the two coordinates
   */
  [[nodiscard]] SwapDistance getSwapDistance(const Point& c1,
                                             const Point& c2) const {
    return swapDistances(
        static_cast<size_t>(c1.x + c1.y) * properties.getNcolumns(),
        static_cast<size_t>(c2.x + c2.y) * properties.getNcolumns());
  }

  /**
   * @brief Get the number of AOD intermediate levels, i.e. the number of
   * possible positions between two coordinates.
   * @return The number of AOD intermediate levels
   */
  [[nodiscard]] uint16_t getNAodIntermediateLevels() const {
    return properties.getNAodIntermediateLevels();
  }
  /**
   * @brief Get the execution time of an operation
   * @param op The operation
   * @return The execution time of the operation
   */
  [[nodiscard]] qc::fp getOpTime(const qc::Operation* op) const;
  /**
   * @brief Get the fidelity of an operation
   * @param op The operation
   * @return The fidelity of the operation
   */
  [[nodiscard]] qc::fp getOpFidelity(const qc::Operation* op) const;
  /**
   * @brief Get indices of the nearby coordinates that are blocked by an
   * operation
   * @param op The operation
   * @return The indices of the nearby coordinates that are blocked by the
   * operation
   */
  [[nodiscard]] std::set<CoordIndex>
  getBlockedCoordIndices(const qc::Operation* op) const;

  // Getters for the parameters
  [[nodiscard]] qc::fp getGateTime(const std::string& s) const {
    if (parameters.gateTimes.find(s) == parameters.gateTimes.end()) {
      std::cout << "Gate time for " << s << " not found\n"
                << "Returning default value\n";
      return parameters.gateTimes.at("none");
    }
    return parameters.gateTimes.at(s);
  }
  /**
   * @brief Retrieves the average fidelity of a gate.
   *
   * This function is responsible for fetching the average fidelity of a gate
   * specified by its name. If the gate is not found in the parameters, it will
   * print a message to the console and return the average fidelity of a default
   * gate.
   *
   * @param s The name of the gate.
   * @return The average fidelity of the specified gate or the default gate if
   * the specified gate is not found.
   */
  [[nodiscard]] qc::fp getGateAverageFidelity(const std::string& s) const {
    if (parameters.gateAverageFidelities.find(s) ==
        parameters.gateAverageFidelities.end()) {
      std::cout << "Gate average fidelity for " << s << " not found\n"
                << "Returning default value\n";
      return parameters.gateAverageFidelities.at("none");
    }
    return parameters.gateAverageFidelities.at(s);
  }
  /**
   * @brief Get the shuttling time of a shuttling operation
   * @param shuttlingType The type of the shuttling operation
   * @return The shuttling time of the shuttling operation
   */
  [[nodiscard]] qc::fp getShuttlingTime(const qc::OpType shuttlingType) const {
    return parameters.shuttlingTimes.at(shuttlingType);
  }
  /**
   * @brief Get the average fidelity of a shuttling operation
   * @param shuttlingType The type of the shuttling operation
   * @return The average fidelity of the shuttling operation
   */
  [[nodiscard]] qc::fp
  getShuttlingAverageFidelity(const qc::OpType shuttlingType) const {
    return parameters.shuttlingAverageFidelities.at(shuttlingType);
  }
  /**
   * @brief Get the decoherence time
   * @return The decoherence time
   */
  [[nodiscard]] qc::fp getDecoherenceTime() const {
    return parameters.decoherenceTimes.tEff();
  }

  // Converters between indices and coordinates
  /**
   * @brief Get a coordinate corresponding to an index
   * @param idx The index
   * @return The coordinate corresponding to the index
   */
  [[nodiscard]] Point getCoordinate(const CoordIndex idx) const {
    return coordinates[idx];
  }
  /**
   * @brief Get the index corresponding to a coordinate
   * @param c The coordinate
   * @return The index corresponding to the coordinate
   */
  [[nodiscard]] [[maybe_unused]] CoordIndex getIndex(const Point& c) const {
    return static_cast<CoordIndex>(c.x + (c.y * properties.getNcolumns()));
  }

  [[nodiscard]] [[maybe_unused]] qc::QuantumComputation
  getBridgeCircuit(const size_t length) const {
    return bridgeCircuits.bridgeCircuits[length];
  }

  // Distance functions
  /**
   * @brief Get the Euclidean distance between two coordinate indices
   * @param idx1 The index of the first coordinate
   * @param idx2 The index of the second coordinate
   * @return The Euclidean distance between the two coordinate indices
   */
  [[nodiscard]] qc::fp getEuclideanDistance(const CoordIndex idx1,
                                            const CoordIndex idx2) const {
    return this->coordinates.at(idx1).getEuclideanDistanceFp(
        this->coordinates.at(idx2));
  }
  [[nodiscard]] qc::fp
  getAllToAllEuclideanDistance(const std::set<CoordIndex>& coords) const {
    qc::fp dist = 0;
    for (auto const c1 : coords) {
      for (auto const c2 : coords) {
        if (c1 == c2) {
          continue;
        }
        dist += getEuclideanDistance(c1, c2);
      }
    }
    return dist;
  }
  [[nodiscard]] qc::fp
  getMoveCombEuclideanDistance(const MoveComb& moveComb) const {
    qc::fp dist = 0;
    for (const auto& move : moveComb.moves) {
      dist += getEuclideanDistance(move.c1, move.c2);
    }
    return dist;
  }

  [[nodiscard]] qc::fp
  getFaEuclideanDistance(const FlyingAncillaComb& faComb) const {
    qc::fp dist = 0;
    for (const auto& fa : faComb.moves) {
      dist += getEuclideanDistance(fa.origin, fa.q1);
      dist += getEuclideanDistance(fa.q1, fa.q2) * 2;
    }
    return dist;
  }

  [[nodiscard]] qc::fp
  getPassByEuclideanDistance(const PassByComb& pbComb) const {
    qc::fp dist = 0;
    for (const auto& fa : pbComb.moves) {
      dist += getEuclideanDistance(fa.c1, fa.c2) * 2;
    }
    return dist;
  }

  /**
   * @brief Get the Euclidean distance between two coordinates
   * @param c1 The first coordinate
   * @param c2 The second coordinate
   * @return The Euclidean distance between the two coordinates
   */
  [[nodiscard]] static qc::fp getEuclideanDistance(const Point& c1,
                                                   const Point& c2) {
    return c1.getEuclideanDistanceFp(c2);
  }
  /**
   * @brief Get the Manhattan distance between two coordinate indices
   * @param idx1 The index of the first coordinate
   * @param idx2 The index of the second coordinate
   * @return The Manhattan distance between the two coordinate indices
   */
  [[nodiscard]] CoordIndex getManhattanDistanceX(const CoordIndex idx1,
                                                 const CoordIndex idx2) const {
    return static_cast<CoordIndex>(
        this->coordinates.at(idx1).getManhattanDistanceX(
            this->coordinates.at(idx2)));
  }
  /**
   * @brief Get the Manhattan distance between two coordinate indices
   * @param idx1 The index of the first coordinate
   * @param idx2 The index of the second coordinate
   * @return The Manhattan distance between the two coordinate indices
   */
  [[nodiscard]] CoordIndex getManhattanDistanceY(const CoordIndex idx1,
                                                 const CoordIndex idx2) const {
    return static_cast<CoordIndex>(
        this->coordinates.at(idx1).getManhattanDistanceY(
            this->coordinates.at(idx2)));
  }

  // Nearby coordinates
  /**
   * @brief Get the precomputed nearby coordinates for a coordinate index
   * @param idx The index of the coordinate
   * @return The precomputed nearby coordinates for the coordinate index
   */
  [[nodiscard]] std::set<CoordIndex>
  getNearbyCoordinates(const CoordIndex idx) const {
    return nearbyCoordinates[idx];
  }
  /**
   * @brief Get the coordinates which are exactly one step away from a
   * coordinate index, i.e. the ones above, below, left and right.
   * @param idx The index of the coordinate
   * @return The coordinates which are exactly one step away from the
   * coordinate index
   */
  [[nodiscard]] std::vector<CoordIndex> getNN(CoordIndex idx) const;

  /**
   * @brief Get the maximum gate size for multi-qubit size. Gets derived
   * from the interaction radius.
   * @return The maximum gate size for multi-qubit size
   */
  [[nodiscard]] size_t getMaxGateSize() const {
    size_t maxGateSize = 0;
    const auto intRad = getInteractionRadius();
    const auto xMax = static_cast<size_t>(intRad);
    auto y = static_cast<size_t>(intRad);
    size_t x = 0;
    while (x <= xMax) {
      if (static_cast<double>((x * x) + (y * y)) > intRad * intRad) {
        y--;
      } else {
        maxGateSize += y + 1;
        x++;
      }
    }
    return maxGateSize;
  }

  // MoveVector functions
  /**
   * @brief Get the MoveVector between two coordinate indices
   * @param idx1 The index of the first coordinate
   * @param idx2 The index of the second coordinate
   * @return The MoveVector between the two coordinate indices
   */
  [[nodiscard]] MoveVector getVector(const CoordIndex idx1,
                                     const CoordIndex idx2) const {
    return {this->coordinates[idx1].x, this->coordinates[idx1].y,
            this->coordinates[idx2].x, this->coordinates[idx2].y};
  }
  /**
   * @brief Computes the time it takes to move a qubit along a MoveVector
   * @param v The MoveVector
   * @return The time it takes to move a qubit along the MoveVector
   */
  [[nodiscard]] qc::fp getVectorShuttlingTime(const MoveVector& v) const {
    return v.getLength() * this->getInterQubitDistance() /
           this->getShuttlingTime(qc::OpType::Move);
  }

  /**
   * @brief Returns a csv string for the animation of the architecture
   * @return The csv string for the animation of the architecture
   */
  [[nodiscard]] std::string
  getAnimationMachine(qc::fp shuttlingSpeedFactor) const;

  [[nodiscard]] std::string
  getAnimationStyle(const std::string& stylePath) const {
    std::string style(defaultStyle);
    if (!stylePath.empty()) {
      std::ifstream file(stylePath);
      if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
          style += line + "\n";
        }
        file.close();
      } else {
        std::cerr << "Could not open file " << stylePath
                  << "! Using default style.\n";
      }
    }
    const std::string toReplace = "XXX";
    const std::string replaceWith =
        std::to_string(getInteractionRadius() * getInterQubitDistance());

    size_t pos = 0;
    while ((pos = style.find(toReplace, pos)) != std::string::npos) {
      style.replace(pos, toReplace.length(), replaceWith);
      pos += replaceWith.length();
    }
    return style;
  }

  /**
   * @brief Save the animation of the architecture to a csv file
   * @param filename The name of the csv file
   */
  [[maybe_unused]] void
  saveAnimationMachine(const std::string& filename,
                       const qc::fp shuttlingSpeedFactor) const {
    std::ofstream file(filename);
    file << getAnimationMachine(shuttlingSpeedFactor);
  }

  [[maybe_unused]] void
  saveAnimationStyle(const std::string& filename,
                     const std::string& stylePath = "") const {
    std::ofstream file(filename);
    file << getAnimationStyle(stylePath);
  }
};

} // namespace na
