/*
 * Copyright (c) 2025 Chair for Design Automation, TUM
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#pragma once

#include "../NADefinitions.hpp"
#include "NAOperation.hpp"

#include <cmath>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
namespace na::nalac {

enum ShuttleType : std::uint8_t { LOAD, MOVE, STORE };

class NAShuttlingOperation : public NAOperation {
protected:
  ShuttleType type;
  std::vector<std::shared_ptr<Point>> start;
  std::vector<std::shared_ptr<Point>> end;

public:
  explicit NAShuttlingOperation(
      const ShuttleType shuttleType,
      const std::vector<std::shared_ptr<Point>>& startConfig,
      const std::vector<std::shared_ptr<Point>>& endConfig)
      : type(shuttleType), start(startConfig), end(endConfig) {
    if (startConfig.size() != endConfig.size()) {
      throw std::logic_error("Shuttling operation must have the same number of "
                             "start and end qubits.");
    }
  }
  explicit NAShuttlingOperation(const ShuttleType shuttleType,
                                std::shared_ptr<Point> startPoint,
                                std::shared_ptr<Point> endPoint)
      : NAShuttlingOperation(
            shuttleType,
            std::vector<std::shared_ptr<Point>>{std::move(startPoint)},
            std::vector<std::shared_ptr<Point>>{std::move(endPoint)}) {}
  [[nodiscard]] auto getType() const -> ShuttleType { return type; }
  [[nodiscard]] auto getStart() const
      -> const std::vector<std::shared_ptr<Point>>& {
    return start;
  }
  [[nodiscard]] auto getEnd() const
      -> const std::vector<std::shared_ptr<Point>>& {
    return end;
  }
  [[nodiscard]] auto isShuttlingOperation() const -> bool override {
    return true;
  }
  [[nodiscard]] auto toString() const -> std::string override;
  [[nodiscard]] auto clone() const -> std::unique_ptr<NAOperation> override {
    return std::make_unique<NAShuttlingOperation>(*this);
  }
};
} // namespace na::nalac
