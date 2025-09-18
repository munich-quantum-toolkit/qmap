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

#include "circuit_optimizer/CircuitOptimizer.hpp"
#include "hybridmap/NeutralAtomDefinitions.hpp"
#include "ir/Definitions.hpp"
#include "ir/QuantumComputation.hpp"
#include "ir/operations/AodOperation.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace na {

class NeutralAtomException final : public std::runtime_error {
  std::string msg;

public:
  explicit NeutralAtomException(std::string m)
      : std::runtime_error("Neutral Atom Mapper Exception"), msg(std::move(m)) {
  }

  [[nodiscard]] const char* what() const noexcept override {
    return msg.c_str();
  }
};

// Enums for the different initial mappings strategies
enum InitialCoordinateMapping : uint8_t { Trivial, Random };
enum InitialMapping : uint8_t { Identity, Graph };
enum MappingMethod : uint8_t {
  SwapMethod,
  BridgeMethod,
  MoveMethod,
  FlyingAncillaMethod,
  PassByMethod
};

[[maybe_unused]] static InitialCoordinateMapping
initialCoordinateMappingFromString(
    const std::string& initialCoordinateMapping) {
  if (initialCoordinateMapping == "trivial" ||
      initialCoordinateMapping == "0") {
    return InitialCoordinateMapping::Trivial;
  }
  if (initialCoordinateMapping == "random" || initialCoordinateMapping == "1") {
    return InitialCoordinateMapping::Random;
  }
  throw std::invalid_argument("Invalid initial coordinate mapping value: " +
                              initialCoordinateMapping);
}

[[maybe_unused]] static InitialMapping
initialMappingFromString(const std::string& initialMapping) {
  if (initialMapping == "identity" || initialMapping == "0") {
    return InitialMapping::Identity;
  }
  if (initialMapping == "graph" || initialMapping == "1") {
    return InitialMapping::Graph;
  }
  throw std::invalid_argument("Invalid initial mapping value: " +
                              initialMapping);
}

/**
 * @brief Helper class to represent a direction in x and y coordinates.
 * @details The boolean value corresponds to right/left and down/up.
 */
struct Direction {
  bool x;
  bool y;

  [[maybe_unused]] Direction(const bool xDir, const bool yDir)
      : x(xDir), y(yDir) {}
  Direction(const qc::fp deltaX, const qc::fp deltaY)
      : x(deltaX >= 0), y(deltaY >= 0) {}

  [[nodiscard]] bool operator==(const Direction& other) const {
    return x == other.x && y == other.y;
  }
  [[nodiscard]] bool operator!=(const Direction& other) const {
    return !(*this == other);
  }
  [[nodiscard]] int32_t getSignX() const { return x ? 1 : -1; }
  [[nodiscard]] int32_t getSignY() const { return y ? 1 : -1; }
  [[nodiscard]] int32_t getSign(const Dimension dim) const {
    return dim == Dimension::X ? getSignX() : getSignY();
  }
};

/**
 * @brief Helper class to represent a move of an atom from one position to
 * another.
 * @details Each move consists in a start and end coordinate and the direction.
 */
struct MoveVector {
  qc::fp xStart;
  qc::fp yStart;
  qc::fp xEnd;
  qc::fp yEnd;
  Direction direction;

  MoveVector(const qc::fp xStart, const qc::fp yStart, const qc::fp xEnd,
             const qc::fp yEnd)
      : xStart(xStart), yStart(yStart), xEnd(xEnd), yEnd(yEnd),
        direction(xEnd - xStart, yEnd - yStart) {}
  MoveVector(const std::int64_t xStart, const std::int64_t yStart,
             const std::int64_t xEnd, const std::int64_t yEnd)
      : xStart(static_cast<qc::fp>(xStart)),
        yStart(static_cast<qc::fp>(yStart)), xEnd(static_cast<qc::fp>(xEnd)),
        yEnd(static_cast<qc::fp>(yEnd)),
        direction(static_cast<qc::fp>(xEnd - xStart),
                  static_cast<qc::fp>(yEnd - yStart)) {}

  [[nodiscard]] [[maybe_unused]] bool
  sameDirection(const MoveVector& other) const {
    return direction == other.direction;
  }
  [[nodiscard]] qc::fp getLength() const {
    return std::hypot(xEnd - xStart, yEnd - yStart);
  }
  [[nodiscard]] bool overlap(const MoveVector& other) const;
  [[nodiscard]] bool include(const MoveVector& other) const;
};

struct FlyingAncilla {
  CoordIndex origin;
  CoordIndex q1;
  CoordIndex q2;
  size_t index;
};

struct FlyingAncillaComb {
  std::vector<FlyingAncilla> moves;
  const qc::Operation* op;
};

struct PassByComb {
  std::vector<AtomMove> moves;
  const qc::Operation* op;
};

/**
 * @brief Helper class to manage multiple atom moves which belong together.
 * @details E.g. a move-away combined with the actual move. These are combined
 * in a MoveComb to facilitate the cost calculation.
 */
struct MoveComb {
  std::vector<AtomMove> moves;
  qc::fp cost = std::numeric_limits<qc::fp>::max();
  const qc::Operation* op = nullptr;
  CoordIndices bestPos;

  MoveComb(std::vector<AtomMove> mov, const qc::fp c, const qc::Operation* o,
           CoordIndices pos)
      : moves(std::move(mov)), cost(c), op(o), bestPos(std::move(pos)) {}
  MoveComb(AtomMove mov, const qc::fp c, const qc::Operation* o,
           CoordIndices pos)
      : moves({std::move(mov)}), cost(c), op(o), bestPos(std::move(pos)) {}

  MoveComb() = default;
  explicit MoveComb(std::vector<AtomMove> mov) : moves(std::move(mov)) {}
  explicit MoveComb(AtomMove mov) : moves({std::move(mov)}) {}

  /**
   * @brief Get the first move of the combination
   * @return The first move of the combination
   */
  [[nodiscard]] AtomMove getFirstMove() const { return moves.front(); }

  /**
   * @brief Get the last move of the combination
   * @return The last move of the combination
   */
  [[nodiscard]] [[maybe_unused]] AtomMove getLastMove() const {
    return moves.back();
  }

  // implement == operator for AtomMove
  [[nodiscard]] bool operator==(const MoveComb& other) const {
    return moves == other.moves;
  }
  [[nodiscard]] bool operator!=(const MoveComb& other) const {
    return !(*this == other);
  }

  /**
   * @brief Append a single move to the end of the combination.
   * @param addMove The move to append
   */
  void append(AtomMove addMove) {
    moves.emplace_back(addMove);
    cost = std::numeric_limits<qc::fp>::max();
  }
  /**
   * @brief Append all moves of another combination to the end of this one.
   * @param addMoveComb The other combination to append
   */
  void append(const MoveComb& addMoveComb) {
    moves.insert(moves.end(), addMoveComb.moves.begin(),
                 addMoveComb.moves.end());
    cost = std::numeric_limits<qc::fp>::max();
  }
  [[nodiscard]] size_t size() const { return moves.size(); }
  [[nodiscard]] bool empty() const { return moves.empty(); }
};

/**
 * @brief Helper class to manage multiple move combinations.
 */
struct MoveCombs {
  std::vector<MoveComb> moveCombs;

  MoveCombs() = default;
  explicit MoveCombs(std::vector<MoveComb> combs)
      : moveCombs(std::move(combs)) {}

  [[nodiscard]] bool empty() const { return moveCombs.empty(); }
  [[nodiscard]] size_t size() const { return moveCombs.size(); }

  // define iterators that iterate over the moveCombs vector
  using iterator = std::vector<MoveComb>::iterator;
  using const_iterator = std::vector<MoveComb>::const_iterator;
  iterator begin() { return moveCombs.begin(); }
  iterator end() { return moveCombs.end(); }
  [[nodiscard]] const_iterator begin() const { return moveCombs.cbegin(); }
  [[nodiscard]] const_iterator end() const { return moveCombs.cend(); }

  void setOperation(const qc::Operation* op, const CoordIndices& pos) {
    for (auto& moveComb : moveCombs) {
      moveComb.op = op;
      moveComb.bestPos = pos;
    }
  }

  /**
   * @brief Add a move combination to the list of move combinations.
   * @param moveComb The move combination to add.
   */
  void addMoveComb(const MoveComb& moveComb);
  /**
   * @brief Add all move combinations of another MoveCombs object to the list of
   * move combinations.
   * @param otherMoveCombs The other MoveCombs object to add.
   */
  void addMoveCombs(const MoveCombs& otherMoveCombs);
  /**
   * @brief Remove all move combinations that are longer than the shortest move
   * combination.
   */
  void removeLongerMoveCombs();
};

/**
 * @brief Helper struct to store the position of a multi qubit gate and the
 * number of moves needed to execute it.
 */
struct MultiQubitMovePos {
  CoordIndices coords;
  size_t nMoves{0};
};

class BridgeCircuits {
public:
  std::vector<qc::QuantumComputation> bridgeCircuits;
  std::vector<size_t> hs;
  std::vector<size_t> czs;
  std::vector<size_t> hDepth;
  std::vector<size_t> czDepth;

  explicit BridgeCircuits(const size_t maxLength) {
    bridgeCircuits.resize(maxLength, qc::QuantumComputation());
    hs.resize(maxLength, 0);
    czs.resize(maxLength, 0);
    hDepth.resize(maxLength, 0);
    czDepth.resize(maxLength, 0);
    for (size_t i = 3; i < maxLength; ++i) {
      computeBridgeCircuit(i);
      computeGates(i);
    }
  }

protected:
  void computeGates(size_t length);
  void computeBridgeCircuit(size_t length);

  static qc::QuantumComputation
  recursiveBridgeIncrease(qc::QuantumComputation qcBridge, size_t length);

  static qc::QuantumComputation
  bridgeExpand(const qc::QuantumComputation& qcBridge, size_t qubit);
};
} // namespace na
