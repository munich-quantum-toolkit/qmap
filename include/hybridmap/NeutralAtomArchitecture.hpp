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

#include "datastructures/SymmetricMatrix.hpp"
#include "hybridmap/NeutralAtomDefinitions.hpp"
#include "hybridmap/NeutralAtomUtils.hpp"
#include "ir/Definitions.hpp"
#include "ir/QuantumComputation.hpp"
#include "ir/operations/OpType.hpp"
#include "ir/operations/Operation.hpp"
#include "na/entities/Location.hpp"

#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace na {
/**
 * @brief Device model for a neutral atom architecture.
 * @details Holds fixed device properties and run-time parameters loaded from
 * JSON, derives coordinate layout, connectivity (swap distances), and proximity
 * lists. Provides timing and fidelity queries, distance helpers, and optional
 * animation export.
 * It requires at last one default "none" entry in gate times and fidelities.
 */
class NeutralAtomArchitecture {
  /**
   * @brief Fixed, immutable properties of a device.
   * @details Encodes grid layout and geometry: rows/columns, number of AODs and
   * coordinates, inter-qubit distance, interaction radius, blocking factor, and
   * derived AOD intermediate levels.
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
    /**
     * @brief Default-construct with zeroed properties.
     */
    Properties() = default;
    /**
     * @brief Construct with explicit device properties.
     * @param rows Grid rows.
     * @param columns Grid columns.
     * @param aods Number of AODs.
     * @param aodCoordinates Number of AOD coordinates.
     * @param qubitDistance Inter-qubit spacing (grid unit).
     * @param radius Interaction radius (in grid units or scaled distance).
     * @param blockingFac Blocking factor for concurrent operations.
     * @param aodDist Minimum AOD step distance used to derive intermediate
     * levels.
     */
    Properties(const std::uint16_t rows, const std::uint16_t columns,
               const std::uint16_t aods, const std::uint16_t aodCoordinates,
               const qc::fp qubitDistance, const qc::fp radius,
               const qc::fp blockingFac, const qc::fp aodDist)
        : nRows(rows), nColumns(columns), nAods(aods),
          nAodCoordinates(aodCoordinates), interQubitDistance(qubitDistance),
          interactionRadius(radius), blockingFactor(blockingFac) {
      assert(aodDist > 0);
      nAodIntermediateLevels = static_cast<uint16_t>(qubitDistance / aodDist);
      assert(nAodIntermediateLevels >= 1);
    }
    /**
     * @brief Total grid sites (rows*columns).
     * @return Number of positions.
     */
    [[nodiscard]] std::uint16_t getNpositions() const {
      return nRows * nColumns;
    }
    /**
     * @brief Grid rows.
     */
    [[nodiscard]] std::uint16_t getNrows() const { return nRows; }
    /**
     * @brief Grid columns.
     */
    [[nodiscard]] std::uint16_t getNcolumns() const { return nColumns; }
    /**
     * @brief Number of AODs.
     */
    [[nodiscard]] std::uint16_t getNAods() const { return nAods; }
    /**
     * @brief Number of AOD coordinates.
     */
    [[nodiscard]] std::uint16_t getNAodCoordinates() const {
      return nAodCoordinates;
    }
    /**
     * @brief Number of intermediate AOD steps between neighboring sites.
     */
    [[nodiscard]] std::uint16_t getNAodIntermediateLevels() const {
      return nAodIntermediateLevels;
    }
    /**
     * @brief Inter-qubit spacing.
     */
    [[nodiscard]] qc::fp getInterQubitDistance() const {
      return interQubitDistance;
    }
    /**
     * @brief Interaction radius.
     */
    [[nodiscard]] qc::fp getInteractionRadius() const {
      return interactionRadius;
    }
    /**
     * @brief Blocking factor.
     */
    [[nodiscard]] qc::fp getBlockingFactor() const { return blockingFactor; }
  };

  /**
   * @brief Run-time parameters of a device (may vary per run).
   * @details Includes number of active qubits, gate and shuttling
   * times/fidelities, and decoherence times loaded from JSON.
   */
  struct Parameters {
    /**
     * @brief Longitudinal and transverse decoherence times.
     * @details Provides an effective time tEff = (T1*T2)/(T1+T2) when both are
     * non-zero.
     */
    struct DecoherenceTimes {
      qc::fp t1 = 0;
      qc::fp t2 = 0;

      /**
       * @brief Effective decoherence time.
       * @return 0 if both T1 and T2 are zero; otherwise (T1*T2)/(T1+T2).
       */
      [[nodiscard]] qc::fp tEff() const {
        if (t1 == 0 && t2 == 0) {
          return 0;
        }
        return t1 * t2 / (t1 + t2);
      }
    };
    CoordIndex nQubits = 0;
    std::map<std::string, qc::fp> gateTimes;
    std::map<std::string, qc::fp> gateAverageFidelities;
    std::map<qc::OpType, qc::fp> shuttlingTimes;
    std::map<qc::OpType, qc::fp> shuttlingAverageFidelities;
    DecoherenceTimes decoherenceTimes;
  };

protected:
  Properties properties{};
  Parameters parameters;

  std::vector<Location> coordinates;
  qc::SymmetricMatrix<SwapDistance> swapDistances;
  std::vector<std::set<CoordIndex>> nearbyCoordinates;

  // Bridges only makes sense for short distances (3-9) so we limit its size
  BridgeCircuits bridgeCircuits = BridgeCircuits(10);

  /**
   * @brief Create grid coordinates for each position on the device.
   */
  void createCoordinates();
  /**
   * @brief Precompute swap distances between coordinates.
   * @details Build connectivity graph based on interaction radius and compute
   * shortest-path distances in number of edges.
   * @param interactionRadius Interaction radius used to define connectivity.
   */
  void computeSwapDistances(qc::fp interactionRadius);
  /**
   * @brief Precompute per-site lists of nearby coordinates.
   * @details Nearby coordinates are those within interaction radius forming
   * edges in the connectivity graph.
   */
  void computeNearbyCoordinates();

public:
  std::string name;

  /**
   * @brief Construct an architecture by loading its JSON description.
   * @param filename Path to the JSON file.
   * @details Loads properties and parameters, then derives coordinates,
   * connectivity, and proximity tables.
   * @throw std::runtime_error If the file cannot be opened or parsed (depending
   * on JSON loader implementation).
   */
  explicit NeutralAtomArchitecture(const std::string& filename);

  /**
   * @brief Load (or reload) properties and parameters from JSON.
   * @param filename Path to the JSON file.
   * @throw std::runtime_error If the file cannot be opened or parsed (depending
   * on JSON loader implementation).
   */
  void loadJson(const std::string& filename);

  // Getters
  /**
   * @brief Get the number of rows.
   * @return Row count.
   */
  [[nodiscard]] std::uint16_t getNrows() const { return properties.getNrows(); }
  /**
   * @brief Get the number of columns.
   * @return Column count.
   */
  [[nodiscard]] std::uint16_t getNcolumns() const {
    return properties.getNcolumns();
  }
  /**
   * @brief Get the number of grid positions.
   * @return Positions (rows*columns).
   */
  [[nodiscard]] std::uint16_t getNpositions() const {
    return properties.getNpositions();
  }
  /**
   * @brief Get the number of AODs.
   * @return AOD count.
   */
  [[maybe_unused]] [[nodiscard]] std::uint16_t getNAods() const {
    return properties.getNAods();
  }
  /**
   * @brief Get the number of AOD coordinates.
   * @return AOD coordinate count.
   */
  [[nodiscard]] [[maybe_unused]] std::uint16_t getNAodCoordinates() const {
    return properties.getNAodCoordinates();
  }
  /**
   * @brief Get the number of qubits.
   * @return Active qubit count.
   */
  [[nodiscard]] CoordIndex getNqubits() const { return parameters.nQubits; }
  /**
   * @brief Get the inter-qubit distance.
   * @return Spacing between neighboring sites.
   */
  [[nodiscard]] qc::fp getInterQubitDistance() const {
    return properties.getInterQubitDistance();
  }
  /**
   * @brief Distance represented by one AOD intermediate level.
   * @return Inter-qubit distance divided by number of AOD intermediate levels.
   */
  [[nodiscard]] qc::fp getOffsetDistance() const {
    return getInterQubitDistance() / getNAodIntermediateLevels();
  }

  /**
   * @brief Get the interaction radius.
   * @return Interaction radius.
   */
  [[nodiscard]] qc::fp getInteractionRadius() const {
    return properties.getInteractionRadius();
  }
  /**
   * @brief Get the blocking factor.
   * @return Blocking factor.
   */
  [[nodiscard]] qc::fp getBlockingFactor() const {
    return properties.getBlockingFactor();
  }
  /**
   * @brief Get precomputed swap distance between two coordinates.
   * @param idx1 First coordinate index.
   * @param idx2 Second coordinate index.
   * @return Swap distance (#edges) between the coordinates.
   */
  [[nodiscard]] SwapDistance getSwapDistance(const CoordIndex idx1,
                                             const CoordIndex idx2) const {
    return swapDistances(idx1, idx2);
  }
  /**
   * @brief Get precomputed swap distance between two coordinates.
   * @param c1 First coordinate.
   * @param c2 Second coordinate.
   * @return Swap distance (#edges) between the coordinates.
   */
  [[nodiscard]] SwapDistance getSwapDistance(const Location& c1,
                                             const Location& c2) const {
    return swapDistances(
        static_cast<size_t>(c1.x + (c1.y * properties.getNcolumns())),
        static_cast<size_t>(c2.x + (c2.y * properties.getNcolumns())));
  }

  /**
   * @brief Number of AOD intermediate levels (positions between two neighbors).
   * @return AOD intermediate levels.
   */
  [[nodiscard]] uint16_t getNAodIntermediateLevels() const {
    return properties.getNAodIntermediateLevels();
  }
  /**
   * @brief Get the execution time of an operation.
   * @param op Operation pointer.
   * @return Duration of the operation on this device.
   */
  [[nodiscard]] qc::fp getOpTime(const qc::Operation* op) const;
  /**
   * @brief Get the average fidelity of an operation.
   * @param op Operation pointer.
   * @return Average fidelity for the operation on this device.
   */
  [[nodiscard]] qc::fp getOpFidelity(const qc::Operation* op) const;
  /**
   * @brief Get indices of nearby coordinates blocked by an operation.
   * @param op Operation pointer.
   * @return Set of coordinate indices blocked while executing op.
   */
  [[nodiscard]] std::set<CoordIndex>
  getBlockedCoordIndices(const qc::Operation* op) const;

  // Getters for the parameters
  /**
   * @brief Retrieve the base time for a named gate.
   * @param s Gate name.
   * @return Gate time if present; otherwise falls back to the time of "none"
   * and prints a message.
   * @note If the fallback entry "none" is not present, an exception from
   * std::map::at may be thrown.
   */
  [[nodiscard]] qc::fp getGateTime(const std::string& s) const {
    if (!parameters.gateTimes.contains(s)) {
      SPDLOG_WARN("Gate time for '{}' not found. Returning default value.", s);
      return parameters.gateTimes.at("none");
    }
    return parameters.gateTimes.at(s);
  }
  /**
   * @brief Retrieve the average fidelity for a named gate.
   * @param s Gate name.
   * @return Gate average fidelity if present; otherwise falls back to the
   * fidelity of "none" and prints a message.
   * @note If the fallback entry "none" is not present, an exception from
   * std::map::at may be thrown.
   */
  [[nodiscard]] qc::fp getGateAverageFidelity(const std::string& s) const {
    if (!parameters.gateAverageFidelities.contains(s)) {
      SPDLOG_WARN(
          "Gate average fidelity for '{}' not found. Returning default value.",
          s);
      return parameters.gateAverageFidelities.at("none");
    }
    return parameters.gateAverageFidelities.at(s);
  }
  /**
   * @brief Get the shuttling time of an operation type.
   * @param shuttlingType Shuttling operation type (OpType).
   * @return Shuttling time for the given type.
   * @throw std::out_of_range If the operation type is unknown.
   */
  [[nodiscard]] qc::fp getShuttlingTime(const qc::OpType shuttlingType) const {
    return parameters.shuttlingTimes.at(shuttlingType);
  }
  /**
   * @brief Get the average fidelity of a shuttling operation type.
   * @param shuttlingType Shuttling operation type (OpType).
   * @return Average shuttling fidelity for the given type.
   * @throw std::out_of_range If the operation type is unknown.
   */
  [[nodiscard]] qc::fp
  getShuttlingAverageFidelity(const qc::OpType shuttlingType) const {
    return parameters.shuttlingAverageFidelities.at(shuttlingType);
  }
  /**
   * @brief Get the effective decoherence time of the device.
   * @return Effective T (computed from T1 and T2).
   */
  [[nodiscard]] qc::fp getDecoherenceTime() const {
    return parameters.decoherenceTimes.tEff();
  }

  // Converters between indices and coordinates
  /**
   * @brief Convert an index to a grid coordinate.
   * @param idx Coordinate index.
   * @return Location at the given index.
   */
  [[nodiscard]] Location getCoordinate(const CoordIndex idx) const {
    return coordinates[idx];
  }
  /**
   * @brief Convert a grid coordinate to its index.
   * @param c Location.
   * @return Linearized index for the coordinate.
   */
  [[nodiscard]] [[maybe_unused]] CoordIndex getIndex(const Location& c) const {
    return static_cast<CoordIndex>(c.x + (c.y * properties.getNcolumns()));
  }

  /**
   * @brief Retrieve a precomputed bridge circuit of a given chain length.
   * @param length Linear chain length.
   * @return QuantumComputation holding the bridge circuit.
   */
  [[nodiscard]] [[maybe_unused]] qc::QuantumComputation
  getBridgeCircuit(const size_t length) const {
    assert(length < bridgeCircuits.bridgeCircuits.size());
    return bridgeCircuits.bridgeCircuits[length];
  }

  // Distance functions
  /**
   * @brief Euclidean distance between two coordinate indices.
   * @param idx1 First coordinate index.
   * @param idx2 Second coordinate index.
   * @return Euclidean distance.
   */
  [[nodiscard]] qc::fp getEuclideanDistance(const CoordIndex idx1,
                                            const CoordIndex idx2) const {
    return coordinates.at(idx1).getEuclideanDistance(coordinates.at(idx2));
  }
  /**
   * @brief Sum of pairwise Euclidean distances among a set of indices.
   * @param coords Set of coordinate indices.
   * @return Sum of distances across ordered pairs (i!=j).
   */
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
  /**
   * @brief Total Euclidean distance covered by an aggregate move combination.
   * @param moveComb Combination of atom moves.
   * @return Sum of Euclidean distances for each move.
   */
  [[nodiscard]] qc::fp
  getMoveCombEuclideanDistance(const MoveComb& moveComb) const {
    qc::fp dist = 0;
    for (const auto& move : moveComb.moves) {
      dist += getEuclideanDistance(move.c1, move.c2);
    }
    return dist;
  }

  /**
   * @brief Estimated path length for a flying-ancilla combination.
   * @param faComb Flying ancilla move combination.
   * @return Sum of distances along origin->q1 and twice q1->q2 per step.
   */
  [[nodiscard]] qc::fp
  getFaEuclideanDistance(const FlyingAncillaComb& faComb) const {
    qc::fp dist = 0;
    for (const auto& fa : faComb.moves) {
      dist += getEuclideanDistance(fa.origin, fa.q1);
      dist += getEuclideanDistance(fa.q1, fa.q2) * 2;
    }
    return dist;
  }

  /**
   * @brief Estimated path length for a pass-by combination.
   * @param pbComb Pass-by move combination.
   * @return Twice the sum of distances of its constituent moves.
   */
  [[nodiscard]] qc::fp
  getPassByEuclideanDistance(const PassByComb& pbComb) const {
    qc::fp dist = 0;
    for (const auto& fa : pbComb.moves) {
      dist += getEuclideanDistance(fa.c1, fa.c2) * 2;
    }
    return dist;
  }

  /**
   * @brief Euclidean distance between two coordinates.
   * @param c1 First coordinate.
   * @param c2 Second coordinate.
   * @return Euclidean distance.
   */
  [[nodiscard]] static qc::fp getEuclideanDistance(const Location& c1,
                                                   const Location& c2) {
    return c1.getEuclideanDistance(c2);
  }
  /**
   * @brief Manhattan distance in X between two indices.
   * @param idx1 First index.
   * @param idx2 Second index.
   * @return |x1-x2|.
   */
  [[nodiscard]] CoordIndex getManhattanDistanceX(const CoordIndex idx1,
                                                 const CoordIndex idx2) const {
    return static_cast<CoordIndex>(
        coordinates.at(idx1).getManhattanDistanceX(coordinates.at(idx2)));
  }
  /**
   * @brief Manhattan distance in Y between two indices.
   * @param idx1 First index.
   * @param idx2 Second index.
   * @return |y1-y2|.
   */
  [[nodiscard]] CoordIndex getManhattanDistanceY(const CoordIndex idx1,
                                                 const CoordIndex idx2) const {
    return static_cast<CoordIndex>(
        coordinates.at(idx1).getManhattanDistanceY(coordinates.at(idx2)));
  }

  // Nearby coordinates
  /**
   * @brief Get precomputed nearby coordinates for an index.
   * @param idx Coordinate index.
   * @return Set of indices within interaction radius of idx.
   */
  [[nodiscard]] std::set<CoordIndex>
  getNearbyCoordinates(const CoordIndex idx) const {
    return nearbyCoordinates[idx];
  }
  /**
   * @brief Get coordinates exactly one grid step away (von Neumann
   * neighborhood).
   * @param idx Coordinate index.
   * @return Indices of neighbors above, below, left, and right.
   */
  [[nodiscard]] std::vector<CoordIndex> getNN(CoordIndex idx) const;

  // MoveVector functions
  /**
   * @brief Construct a MoveVector between two coordinate indices.
   * @param idx1 Start index.
   * @param idx2 End index.
   * @return MoveVector from start to end.
   */
  [[nodiscard]] MoveVector getVector(const CoordIndex idx1,
                                     const CoordIndex idx2) const {
    return {coordinates[idx1].x, coordinates[idx1].y, coordinates[idx2].x,
            coordinates[idx2].y};
  }
  /**
   * @brief Estimate time to move along a MoveVector.
   * @param v MoveVector path.
   * @return Shuttling time proportional to Euclidean length and device speed.
   */
  [[nodiscard]] qc::fp getVectorShuttlingTime(const MoveVector& v) const {
    return v.getLength() * getInterQubitDistance() /
           getShuttlingTime(qc::OpType::Move);
  }

  /**
   * @brief Generate CSV content describing the device for animation.
   * @param shuttlingSpeedFactor Scaling factor applied to shuttling speeds.
   * @return CSV string describing layout and timing parameters.
   */
  [[nodiscard]] std::string
  getAnimationMachine(qc::fp shuttlingSpeedFactor) const;

  /**
   * @brief Save the device animation CSV to a file.
   * @param filename Output CSV filename.
   * @param shuttlingSpeedFactor Scaling factor applied to shuttling speeds.
   */
  [[maybe_unused]] void
  saveAnimationMachine(const std::string& filename,
                       const qc::fp shuttlingSpeedFactor) const {
    std::ofstream file(filename);
    file << getAnimationMachine(shuttlingSpeedFactor);
  }
};

} // namespace na
