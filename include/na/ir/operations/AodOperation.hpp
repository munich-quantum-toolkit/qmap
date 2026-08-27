/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

/** @file AodOperation.hpp
 * @brief Atom-array operation representation.
 */

#pragma once

#include "ir/Definitions.hpp"
#include "ir/Register.hpp"
#include "ir/operations/Control.hpp"
#include "ir/operations/Operation.hpp"
#include "na/ir/operations/NeutralAtomOpType.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <ostream>
#include <string>
#include <tuple>
#include <vector>

namespace na {

class AodOperation final : public qc::Operation {
public:
  /// Cartesian dimension addressed by an AOD segment.
  enum class Dimension : std::uint8_t { X = 0, Y = 1 };

  /// One dimension-specific AOD movement segment.
  struct Segment {
    Dimension dimension;
    qc::fp start;
    qc::fp end;

    Segment(const Dimension segmentDimension, const qc::fp segmentStart,
            const qc::fp segmentEnd)
        : dimension(segmentDimension), start(segmentStart), end(segmentEnd) {}

    bool operator==(const Segment&) const = default;

    [[nodiscard]] std::string toQASMString() const;
  };

private:
  NeutralAtomOpType neutralAtomOpType = NeutralAtomOpType::None;
  std::vector<Segment> segments;

  static NeutralAtomOpType
  validateType(NeutralAtomOpType candidateNeutralAtomOpType);

  static std::vector<Dimension>
  convertToDimensions(const std::vector<std::uint32_t>& dimensions);

  void setNeutralAtomOpType(NeutralAtomOpType newNeutralAtomOpType);

public:
  AodOperation() = default;
  AodOperation(NeutralAtomOpType newNeutralAtomOpType,
               std::vector<qc::Qubit> operationTargets,
               const std::vector<Dimension>& dimensions,
               const std::vector<qc::fp>& starts,
               const std::vector<qc::fp>& ends);
  AodOperation(NeutralAtomOpType newNeutralAtomOpType,
               std::vector<qc::Qubit> operationTargets,
               const std::vector<std::uint32_t>& dimensions,
               const std::vector<qc::fp>& starts,
               const std::vector<qc::fp>& ends);
  AodOperation(const std::string& typeName,
               std::vector<qc::Qubit> operationTargets,
               const std::vector<std::uint32_t>& dimensions,
               const std::vector<qc::fp>& starts,
               const std::vector<qc::fp>& ends);
  AodOperation(
      NeutralAtomOpType newNeutralAtomOpType,
      std::vector<qc::Qubit> operationTargets,
      const std::vector<std::tuple<Dimension, qc::fp, qc::fp>>& segments);
  AodOperation(NeutralAtomOpType newNeutralAtomOpType,
               std::vector<qc::Qubit> operationTargets,
               std::vector<Segment> operationSegments);

  /// Returns the neutral-atom operation type.
  [[nodiscard]] NeutralAtomOpType getNeutralAtomOpType() const {
    return neutralAtomOpType;
  }

  [[nodiscard]] std::unique_ptr<qc::Operation> clone() const override {
    return std::make_unique<AodOperation>(*this);
  }

  void setGate(qc::OpType operationType) override;

  void addControl([[maybe_unused]] qc::Control control) override {}
  void clearControls() override {}
  void removeControl([[maybe_unused]] qc::Control control) override {}
  qc::Controls::iterator
  removeControl(const qc::Controls::iterator it) override {
    return controls.erase(it);
  }

  [[nodiscard]] std::vector<qc::fp> getEnds(Dimension dimension) const;

  [[nodiscard]] std::vector<qc::fp> getStarts(Dimension dimension) const;

  [[nodiscard]] qc::fp getMaxDistance(Dimension dimension) const;

  [[nodiscard]] std::vector<qc::fp> getDistances(Dimension dimension) const;

  [[nodiscard]] bool equals(const qc::Operation& operation,
                            const qc::Permutation& permutation1,
                            const qc::Permutation& permutation2) const override;

  [[nodiscard]] bool equals(const qc::Operation& operation) const override;

  std::ostream& print(std::ostream& os, const qc::Permutation& permutation,
                      std::size_t prefixWidth,
                      std::size_t nQubits) const override;

  void dumpOpenQASM(std::ostream& of,
                    const qc::QubitIndexToRegisterMap& qubitMap,
                    const qc::BitIndexToRegisterMap& bitMap, std::size_t indent,
                    bool openQASM3) const override;

  void invert() override;
};

} // namespace na
