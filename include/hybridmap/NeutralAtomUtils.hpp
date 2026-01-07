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

// Enums for the different initial mapping strategies
/**
 * @brief Strategy for assigning initial physical coordinates to atoms.
 * @details "Trivial" assigns coordinates in order without randomness; "Random"
 * samples coordinates uniformly from the available lattice/set.
 */
enum InitialCoordinateMapping : uint8_t { Trivial, Random };
/**
 * @brief Strategy for initializing the logical-to-physical qubit mapping.
 * @details Identity keeps logical indices matched to physical indices; Graph
 * performs a topology-aware assignment (e.g., based on interaction graph
 * heuristics).
 */
enum InitialMapping : uint8_t { Identity, Graph };
/**
 * @brief Method used to realize two-qubit interactions that are not adjacent
 * under the current mapping.
 * @details Different strategies trade off added gates vs. atom motion: Swap
 * introduces SWAP chains, Bridge builds entangling bridges, Move performs
 * physical repositioning, FlyingAncilla uses a mobile ancilla, PassBy leverages
 * relative motion without stopping.
 */
enum MappingMethod : uint8_t {
  SwapMethod,
  BridgeMethod,
  MoveMethod,
  FlyingAncillaMethod,
  PassByMethod
};

/**
 * @brief Parse an InitialCoordinateMapping from a string token.
 * @param initialCoordinateMapping Accepted values: "trivial"/"0" or
 * "random"/"1" (case-sensitive).
 * @return Corresponding InitialCoordinateMapping enum value.
 * @throw std::invalid_argument If the value is not recognized.
 */
[[maybe_unused]] static InitialCoordinateMapping
initialCoordinateMappingFromString(
    const std::string& initialCoordinateMapping) {
  if (initialCoordinateMapping == "trivial" ||
      initialCoordinateMapping == "0") {
    return Trivial;
  }
  if (initialCoordinateMapping == "random" || initialCoordinateMapping == "1") {
    return Random;
  }
  throw std::invalid_argument(
      "Invalid initial coordinate mapping value (expected \"trivial\"/\"0\" "
      "or \"random\"/\"1\"): " +
      initialCoordinateMapping);
}

/**
 * @brief Parse an InitialMapping from a string token.
 * @param initialMapping Accepted values: "identity"/"0" or "graph"/"1"
 * (case-sensitive).
 * @return Corresponding InitialMapping enum value.
 * @throw std::invalid_argument If the value is not recognized.
 */
[[maybe_unused]] static InitialMapping
initialMappingFromString(const std::string& initialMapping) {
  if (initialMapping == "identity" || initialMapping == "0") {
    return Identity;
  }
  if (initialMapping == "graph" || initialMapping == "1") {
    return Graph;
  }
  throw std::invalid_argument(
      "Invalid initial mapping value (expected \"identity\"/\"0\" or "
      "\"graph\"/\"1\"): " +
      initialMapping);
}

/**
 * @brief Helper struct representing a 2D direction via sign bits.
 * @details Booleans encode the sign of deltas: x==true implies non-negative x
 * movement; y==true implies non-negative y movement per the chosen coordinate
 * convention.
 */
struct Direction {
  bool x;
  bool y;

  /**
   * @brief Construct a direction from deltas.
   * @param deltaX Signed x delta.
   * @param deltaY Signed y delta.
   */
  Direction(const qc::fp deltaX, const qc::fp deltaY)
      : x(deltaX >= 0), y(deltaY >= 0) {}

  /**
   * @brief Equality comparison.
   * @param other Direction to compare against.
   * @return True if both sign bits are identical.
   */
  [[nodiscard]] bool operator==(const Direction& other) const {
    return x == other.x && y == other.y;
  }
  /**
   * @brief Inequality comparison.
   * @param other Direction to compare against.
   * @return True if any sign bit differs.
   */
  [[nodiscard]] bool operator!=(const Direction& other) const {
    return !(*this == other);
  }
  /**
   * @brief Get signed unit for x.
   * @return +1 if x>=0 else -1.
   */
  [[nodiscard]] int32_t getSignX() const { return x ? 1 : -1; }
  /**
   * @brief Get signed unit for y.
   * @return +1 if y>=0 else -1.
   */
  [[nodiscard]] int32_t getSignY() const { return y ? 1 : -1; }
  /**
   * @brief Get signed unit for a given dimension.
   * @param dim Dimension (X or Y).
   * @return +1 if non-negative movement else -1.
   */
  [[nodiscard]] int32_t getSign(const Dimension dim) const {
    return dim == Dimension::X ? getSignX() : getSignY();
  }
};

/**
 * @brief Represents a single atom move from a start to an end position.
 * @details Encapsulates start/end coordinates, derived direction, and spatial
 * predicates (direction, length, overlap, inclusion).
 */
struct MoveVector {
  qc::fp xStart;
  qc::fp yStart;
  qc::fp xEnd;
  qc::fp yEnd;
  Direction direction;

  /**
   * @brief Construct a move vector.
   * @param xStart Starting x coordinate.
   * @param yStart Starting y coordinate.
   * @param xEnd Ending x coordinate.
   * @param yEnd Ending y coordinate.
   */
  MoveVector(const qc::fp xStart, const qc::fp yStart, const qc::fp xEnd,
             const qc::fp yEnd)
      : xStart(xStart), yStart(yStart), xEnd(xEnd), yEnd(yEnd),
        direction(xEnd - xStart, yEnd - yStart) {}

  /**
   * @brief Check if two moves share identical direction signs.
   * @param other Other move.
   * @return True if both x and y direction signs match.
   */
  [[nodiscard]] [[maybe_unused]] bool
  sameDirection(const MoveVector& other) const {
    return direction == other.direction;
  }
  /**
   * @brief Euclidean length of the move.
   * @return Hypotenuse of (dx, dy).
   */
  [[nodiscard]] qc::fp getLength() const {
    return std::hypot(xEnd - xStart, yEnd - yStart);
  }
  /**
   * @brief Check if axis-aligned projections overlap in at least one dimension.
   * @param other Other move.
   * @return True if x intervals overlap OR y intervals overlap (inclusive
   * bounds).
   */
  [[nodiscard]] bool overlap(const MoveVector& other) const;
  /**
   * @brief Check if this move's interval is strictly included by another's in
   * any dimension.
   * @param other Other move that may include this.
   * @return True if other strictly contains this in x OR in y.
   */
  [[nodiscard]] bool include(const MoveVector& other) const;
};

/**
 * @brief Metadata for a flying ancilla interaction step.
 * @details Stores origin and two target data atom coordinates plus an index for
 * ordering.
 */
struct FlyingAncilla {
  /** Origin coordinate index of the flying ancilla. */
  CoordIndex origin;
  /** First data atom coordinate involved in interaction. */
  CoordIndex q1;
  /** Second data atom coordinate involved in interaction. */
  CoordIndex q2;
  /** Optional bookkeeping index to order/identify moves. */
  size_t index;
};

/**
 * @brief Combination of sequential flying ancilla moves for one operation.
 * @param moves Sequence realizing the interaction.
 * @param op Operation implemented (non-owning pointer).
 */
struct FlyingAncillaComb {
  /** Sequence of flying ancilla moves realizing an interaction. */
  std::vector<FlyingAncilla> moves;
  /** Operation this combination implements or is associated with. */
  const qc::Operation* op = nullptr;
};

/**
 * @brief Pass-by maneuver representation for an operation.
 * @details Aggregates physical atom moves needed to realize a pass-by style
 * interaction.
 */
struct PassByComb {
  /** Sequence of atom moves realizing a pass-by maneuver. */
  std::vector<AtomMove> moves;
  /** Operation this combination implements or is associated with. */
  const qc::Operation* op = nullptr;
};

/**
 * @brief Aggregates related atom moves forming a candidate realization.
 * @details Examples include preparatory clearance moves plus the main move.
 * Collectively evaluated for cost heuristics and selection.
 */
struct MoveComb {
  std::vector<AtomMove> moves;
  /** Aggregated cost heuristic; defaults to +inf until computed. */
  qc::fp cost = std::numeric_limits<qc::fp>::max();
  /** Operation this move combination aims to realize (optional). */
  const qc::Operation* op = nullptr;
  /** Best known positions for the operation after applying the moves. */
  CoordIndices bestPos;

  MoveComb(std::vector<AtomMove> mov, const qc::fp c, const qc::Operation* o,
           CoordIndices pos)
      : moves(std::move(mov)), cost(c), op(o), bestPos(std::move(pos)) {}
  MoveComb(AtomMove mov, const qc::fp c, const qc::Operation* o,
           CoordIndices pos)
      : moves({mov}), cost(c), op(o), bestPos(std::move(pos)) {}

  MoveComb() = default;
  explicit MoveComb(std::vector<AtomMove> mov) : moves(std::move(mov)) {}
  explicit MoveComb(AtomMove mov) : moves({mov}) {}

  /**
   * @brief Equality comparison (ignores cost/op/bestPos).
   * @param other Other combination.
   * @return True if underlying move sequence is identical.
   */
  [[nodiscard]] bool operator==(const MoveComb& other) const {
    return moves == other.moves;
  }
  /**
   * @brief Inequality comparison.
   * @param other Other combination.
   * @return True if move sequence differs.
   */
  [[nodiscard]] bool operator!=(const MoveComb& other) const {
    return !(*this == other);
  }

  /**
   * @brief Append a single move.
   * @param addMove Move to append.
   * @note Resets cost to +inf for later recomputation.
   */
  void append(AtomMove addMove) {
    moves.emplace_back(addMove);
    cost = std::numeric_limits<qc::fp>::max();
  }
  /**
   * @brief Concatenate moves from another combination.
   * @param addMoveComb Source combination.
   * @note Resets cost to +inf for later recomputation.
   */
  void append(const MoveComb& addMoveComb) {
    moves.insert(moves.end(), addMoveComb.moves.begin(),
                 addMoveComb.moves.end());
    cost = std::numeric_limits<qc::fp>::max();
  }
  /**
   * @brief Number of moves contained.
   * @return Size of move sequence.
   */
  [[nodiscard]] size_t size() const { return moves.size(); }
  /**
   * @brief Check emptiness.
   * @return True if no moves stored.
   */
  [[nodiscard]] bool empty() const { return moves.empty(); }
};

/**
 * @brief Container managing a set of unique move combinations.
 * @detail Prevents duplicate sequences, propagates operation metadata, and
 * offers pruning utilities.
 */
struct MoveCombs {
  std::vector<MoveComb> moveCombs;

  MoveCombs() = default;
  explicit MoveCombs(std::vector<MoveComb> combs)
      : moveCombs(std::move(combs)) {}

  /**
   * @brief Check if container is empty.
   * @return True if no combinations stored.
   */
  [[nodiscard]] bool empty() const { return moveCombs.empty(); }
  /**
   * @brief Number of stored combinations.
   * @return Count of combinations.
   */
  [[nodiscard]] size_t size() const { return moveCombs.size(); }

  // define iterators that iterate over the moveCombs vector
  using iterator = std::vector<MoveComb>::iterator;
  using const_iterator = std::vector<MoveComb>::const_iterator;
  iterator begin() { return moveCombs.begin(); }
  iterator end() { return moveCombs.end(); }
  [[nodiscard]] const_iterator begin() const { return moveCombs.cbegin(); }
  [[nodiscard]] const_iterator end() const { return moveCombs.cend(); }

  /**
   * @brief Set associated operation and best positions for all combinations.
   * @param op Operation pointer (non-owning).
   * @param pos Best coordinate indices for op.
   */
  void setOperation(const qc::Operation* op, const CoordIndices& pos) {
    for (auto& moveComb : moveCombs) {
      moveComb.op = op;
      moveComb.bestPos = pos;
    }
  }

  /**
   * @brief Insert a combination, deduplicating existing sequences.
   * @details If an equal sequence exists, that existing entry's cost is
   * invalidated (set to +inf) instead of inserting a duplicate.
   * @param moveComb Combination to add.
   */
  void addMoveComb(const MoveComb& moveComb);
  /**
   * @brief Bulk insert all combinations from another container.
   * @param otherMoveCombs Source container.
   */
  void addMoveCombs(const MoveCombs& otherMoveCombs);
  /**
   * @brief Prune combinations longer than the current minimum length.
   * @details Computes minimal number of moves and removes any with a greater
   * count.
   */
  void removeLongerMoveCombs();
};

/**
 * @brief Position and move count summary for a multi-qubit gate.
 * @param coords Coordinate indices involved.
 * @param nMoves Number of atom moves required.
 */
struct MultiQubitMovePos {
  CoordIndices coords;
  size_t nMoves{0};
};

/**
 * @brief Precomputes CZ-based bridge circuits and gate metrics for linear
 * chains.
 * @details For lengths in [3, maxLength) stores circuits plus aggregate H/CZ
 * counts and per-qubit depth maxima to support cost estimation.
 */
class BridgeCircuits {
public:
  /** Bridge circuits indexed by chain length (number of qubits). */
  std::vector<qc::QuantumComputation> bridgeCircuits;
  /** Total number of H gates per length. */
  std::vector<size_t> hs;
  /** Total number of CZ gates per length (counted via Z after MCX->MCZ). */
  std::vector<size_t> czs;
  /** Maximum number of H gates on any single qubit for a given length. */
  std::vector<size_t> hDepth;
  /** Maximum number of CZ involvements on any single qubit for a length. */
  std::vector<size_t> czDepth;

  /**
   * @brief Construct and precompute bridge data up to given length.
   * @param maxLength Exclusive upper bound on chain length table size.
   */
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
  /**
   * @brief Compute aggregate gate counts and per-qubit depth metrics.
   * @param length Chain length whose circuit has been generated.
   */
  void computeGates(size_t length);
  /**
   * @brief Build base bridge circuit for a chain length.
   * @param length Target chain length.
   * @details Starts from length 3 pattern and recurses via minimal-load
   * expansion.
   */
  void computeBridgeCircuit(size_t length);

  /**
   * @brief Recursively expand circuit choosing insertion minimizing current
   * load.
   * @param qcBridge Existing bridge circuit.
   * @param length Desired final length.
   * @return Expanded circuit of requested length.
   */
  static qc::QuantumComputation
  recursiveBridgeIncrease(qc::QuantumComputation qcBridge, size_t length);

  /**
   * @brief Expand circuit by inserting a qubit between positions qubit and
   * qubit+1.
   * @param qcBridge Circuit to expand.
   * @param qubit Insertion position.
   * @return New circuit with inserted qubit.
   */
  static qc::QuantumComputation
  bridgeExpand(const qc::QuantumComputation& qcBridge, size_t qubit);
};
} // namespace na
