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
#include <vector>

namespace na::nalac {
class NAGlobalOperation : public NAOperation {
protected:
  qc::OpType opType_;
  std::size_t ctrls_;
  std::vector<qc::fp> params_;

public:
  explicit NAGlobalOperation(const qc::OpType opType, const std::size_t ctrls,
                             const std::vector<qc::fp>& params)
      : opType_(opType), ctrls_(ctrls), params_(params) {
    if (!isSingleQubitGate(opType)) {
      throw std::invalid_argument("Operation is not single qubit.");
    }
  }
  explicit NAGlobalOperation(const qc::OpType opType, const std::size_t ctrls)
      : NAGlobalOperation(opType, ctrls, {}) {}
  [[nodiscard]] auto getParams() const -> const std::vector<qc::fp>& {
    return params_;
  }
  [[nodiscard]] auto getType() const -> std::pair<qc::OpType, std::size_t> {
    return {opType_, ctrls_};
  }
  [[nodiscard]] auto isGlobalOperation() const -> bool override { return true; }
  [[nodiscard]] auto toString() const -> std::string override;
  [[nodiscard]] auto clone() const -> std::unique_ptr<NAOperation> override {
    return std::make_unique<NAGlobalOperation>(*this);
  }
};
} // namespace na::nalac
