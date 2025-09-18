/*
 * Copyright (c) 2025 Chair for Design Automation, TUM
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#pragma once

#include "NAOperation.hpp"
#include "ir/Definitions.hpp"
#include "na/nalac/datastructures/NADefinitions.hpp"

#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace na::nalac {
class NALocalOperation : public NAOperation {
protected:
  qc::OpType opType_;
  std::size_t ctrls_;
  std::vector<qc::fp> params_;
  std::vector<std::shared_ptr<Point>> positions_;

public:
  NALocalOperation(const qc::OpType opType, const std::size_t ctrls,
                   const std::vector<qc::fp>& params,
                   const std::vector<std::shared_ptr<Point>>& positions)
      : opType_(opType), ctrls_(ctrls), params_(params), positions_(positions) {
    if (!isSingleQubitGate(opType)) {
      throw std::invalid_argument("Operation is not single qubit.");
    }
    if (ctrls > 0) {
      throw std::logic_error("Control qubits are not supported.");
    }
  }
  explicit NALocalOperation(const qc::OpType opType, const std::size_t ctrls,
                            const std::vector<std::shared_ptr<Point>>& pos)
      : NALocalOperation(opType, ctrls, {}, pos) {}
  explicit NALocalOperation(const qc::OpType opType, const std::size_t ctrls,
                            const std::vector<qc::fp>& parameters,
                            std::shared_ptr<Point> pos)
      : NALocalOperation(opType, ctrls, parameters,
                         std::vector<std::shared_ptr<Point>>{std::move(pos)}) {}
  explicit NALocalOperation(const qc::OpType opType, const std::size_t ctrls,
                            std::shared_ptr<Point> pos)
      : NALocalOperation(opType, ctrls, {}, std::move(pos)) {}
  [[nodiscard]] auto getPositions() const
      -> const std::vector<std::shared_ptr<Point>>& {
    return positions_;
  }
  [[nodiscard]] auto getParams() const -> const std::vector<qc::fp>& {
    return params_;
  }
  [[nodiscard]] auto getType() const -> std::pair<qc::OpType, std::size_t> {
    return {opType_, ctrls_};
  }
  [[nodiscard]] auto isLocalOperation() const -> bool override { return true; }
  [[nodiscard]] auto toString() const -> std::string override;
  [[nodiscard]] auto clone() const -> std::unique_ptr<NAOperation> override {
    return std::make_unique<NALocalOperation>(*this);
  }
};
} // namespace na::nalac
