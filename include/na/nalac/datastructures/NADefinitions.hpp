/*
 * Copyright (c) 2025 Chair for Design Automation, TUM
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#pragma once

#include "ir/Definitions.hpp"
#include "ir/operations/CompoundOperation.hpp"
#include "ir/operations/OpType.hpp"
#include "ir/operations/Operation.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <ostream>
#include <sstream>
#include <string>

namespace na::nalac {
/// Class to store two-dimensional coordinates
struct Point {
  std::int64_t x;
  std::int64_t y;
  Point(const std::int64_t xp, const std::int64_t yp) : x(xp), y(yp) {};
  Point(const Point& p) = default;
  virtual ~Point() = default;
  auto operator=(const Point& p) -> Point& = default;
  auto operator-(const Point& p) const -> Point { return {x - p.x, y - p.y}; }
  auto operator-(const Point&& p) const -> Point { return {x - p.x, y - p.y}; }
  auto operator+(const Point& p) const -> Point { return {x + p.x, y + p.y}; }
  auto operator+(const Point&& p) const -> Point { return {x + p.x, y + p.y}; }
  [[nodiscard]] auto length() const -> std::uint64_t {
    return static_cast<std::uint64_t>(std::round(std::sqrt(x * x + y * y)));
  }
  [[nodiscard]] auto toString() const -> std::string {
    std::stringstream ss;
    ss << "(" << x << ", " << y << ")";
    return ss.str();
  }
  friend auto operator<<(std::ostream& os, const Point& obj) -> std::ostream& {
    return os << obj.toString(); // Using toString() method
  }
  [[nodiscard]] auto operator==(const Point& other) const -> bool {
    return x == other.x && y == other.y;
  }
  [[maybe_unused]] [[nodiscard]] auto
  getEuclideanDistance(const Point& c) const {
    const auto delta = *this - c;
    return delta.length();
  }

  [[maybe_unused]] [[nodiscard]] auto
  getManhattanDistanceX(const Point& c) const -> std::int64_t {
    if (x > c.x) {
      return x - c.x;
    }
    return c.x - x;
  }
  [[maybe_unused]] [[nodiscard]] auto
  getManhattanDistanceY(const Point& c) const -> std::int64_t {
    if (y > c.y) {
      return y - c.y;
    }
    return c.y - y;
  }
};

/**
 * @brief Checks whether a gate is global.
 * @details A StandardOperation is global if it acts on all qubits.
 * A CompoundOperation is global if all its sub-operations are
 * StandardOperations of the same type with the same parameters acting on all
 * qubits. The latter is what a QASM line like `ry(π) q;` is translated to in
 * MQT-core. All other operations are not global.
 */
[[nodiscard]] inline auto isGlobal(const qc::Operation& op,
                                   const std::size_t nQubits) -> bool {
  if (op.isStandardOperation()) {
    return op.getUsedQubits().size() == nQubits;
  }
  if (op.isCompoundOperation()) {
    const auto ops = dynamic_cast<const qc::CompoundOperation&>(op);
    const auto& params = ops.at(0)->getParameter();
    const auto& type = ops.at(0)->getType();
    return op.getUsedQubits().size() == nQubits &&
           std::all_of(ops.cbegin(), ops.cend(), [&](const auto& operation) {
             return operation->isStandardOperation() &&
                    operation->getNcontrols() == 0 &&
                    operation->getType() == type &&
                    operation->getParameter() == params;
           });
  }
  return false;
}

} // namespace na::nalac

/// Hash function for OpType, e.g., for use in unordered_map
template <> struct std::hash<std::pair<qc::OpType, std::size_t>> {
  auto operator()(const std::pair<qc::OpType, std::size_t>& t) const noexcept
      -> std::size_t {
    const std::size_t h1 = std::hash<qc::OpType>{}(t.first);
    const std::size_t h2 = std::hash<std::size_t>{}(t.second);
    return qc::combineHash(h1, h2);
  }
};

/// Hash function for Point, e.g., for use in unordered_map
template <> struct std::hash<na::nalac::Point> {
  auto operator()(const na::nalac::Point& p) const noexcept -> std::size_t {
    const std::size_t h1 = std::hash<decltype(p.x)>{}(p.x);
    const std::size_t h2 = std::hash<decltype(p.y)>{}(p.y);
    return qc::combineHash(h1, h2);
  }
};
