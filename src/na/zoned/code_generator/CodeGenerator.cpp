/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "na/zoned/code_generator/CodeGenerator.hpp"

#include "ir/Definitions.hpp"
#include "ir/operations/CompoundOperation.hpp"
#include "ir/operations/OpType.hpp"
#include "ir/operations/Operation.hpp"
#include "na/ir/NAComputation.hpp"
#include "na/ir/entities/Atom.hpp"
#include "na/ir/entities/Location.hpp"
#include "na/ir/entities/Zone.hpp"
#include "na/ir/operations/NAComputationGlobalCZOperation.hpp"
#include "na/ir/operations/NAComputationGlobalRYOperation.hpp"
#include "na/ir/operations/NAComputationLoadOperation.hpp"
#include "na/ir/operations/NAComputationLocalRZOperation.hpp"
#include "na/ir/operations/NAComputationLocalUOperation.hpp"
#include "na/ir/operations/NAComputationMoveOperation.hpp"
#include "na/ir/operations/NAComputationStoreOperation.hpp"
#include "na/zoned/Architecture.hpp"
#include "na/zoned/Types.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iterator>
#include <limits>
#include <map>
#include <nlohmann/json.hpp>
#include <ranges>
#include <set>
#include <spdlog/spdlog.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace na::zoned {
namespace {
auto appendLocalGate(const qc::Operation& gate,
                     std::vector<const Atom*> targets,
                     bool warnUnsupportedGates, NAComputation& code) -> void {
  const auto type = gate.getType();
  const auto& params = gate.getParameter();
  const auto rz = [&](qc::fp angle) {
    code.emplaceBack<NAComputationLocalRZOperation>(std::move(targets), angle);
  };
  if (type == qc::RZ || type == qc::P) {
    rz(params.front());
  } else if (type == qc::Z) {
    rz(qc::PI);
  } else if (type == qc::S) {
    rz(qc::PI_2);
  } else if (type == qc::Sdg) {
    rz(-qc::PI_2);
  } else if (type == qc::T) {
    rz(qc::PI_4);
  } else if (type == qc::Tdg) {
    rz(-qc::PI_4);
  } else {
    if (warnUnsupportedGates) {
      SPDLOG_WARN(
          "Gate not part of basis gates will be inserted as U3 gate: {}",
          qc::toString(type));
    }
    const auto u3 = [&](qc::fp theta, qc::fp phi, qc::fp lambda) {
      code.emplaceBack<NAComputationLocalUOperation>(std::move(targets), theta,
                                                     phi, lambda);
    };
    if (type == qc::U) {
      u3(params.front(), params.at(1), params.at(2));
    } else if (type == qc::U2) {
      u3(qc::PI_2, params.front(), params.at(1));
    } else if (type == qc::RX) {
      u3(params.front(), -qc::PI_2, qc::PI_2);
    } else if (type == qc::RY) {
      u3(params.front(), 0, 0);
    } else if (type == qc::H) {
      u3(qc::PI_2, 0, qc::PI);
    } else if (type == qc::X) {
      u3(qc::PI, 0, qc::PI);
    } else if (type == qc::Y) {
      u3(qc::PI, qc::PI_2, qc::PI_2);
    } else if (type == qc::Vdg) {
      u3(-qc::PI_2, qc::PI_2, -qc::PI_2);
    } else if (type == qc::SX) {
      u3(qc::PI_2, -qc::PI_2, qc::PI_2);
    } else if (type == qc::SXdg || type == qc::V) {
      u3(-qc::PI_2, -qc::PI_2, qc::PI_2);
    } else {
      std::ostringstream oss;
      oss << "\033[1;31m[ERROR]\033[0m Unsupported single-qubit gate: " << type
          << "\n";
      throw std::invalid_argument(oss.str());
    }
  }
}
} // namespace

auto CodeGenerator::appendSingleQubitGates(
    const SingleQubitGateLayer& singleQubitGates,
    const std::vector<std::reference_wrapper<const Atom>>& atoms,
    const Zone& globalZone, NAComputation& code) const -> void {
  const auto append = [&](const qc::Operation& gate, const auto& qubits) {
    if (!gate.isSingleQubitGate()) {
      throw std::invalid_argument("Expected a single-qubit gate, got " +
                                  qc::toString(gate.getType()));
    }
    const auto type = gate.getType();
    if (qubits.size() == atoms.size() && (type == qc::RY || type == qc::Y)) {
      const auto angle = type == qc::Y ? qc::PI : gate.getParameter().front();
      code.emplaceBack<NAComputationGlobalRYOperation>(globalZone, angle);
      return;
    }
    std::vector<const Atom*> targets;
    targets.reserve(qubits.size());
    for (const auto qubit : qubits) {
      targets.emplace_back(&atoms.at(qubit).get());
    }
    appendLocalGate(gate, std::move(targets), config_.warnUnsupportedGates,
                    code);
  };

  for (const auto& op : singleQubitGates) {
    if (const auto* compound =
            dynamic_cast<const qc::CompoundOperation*>(op.get())) {
      // Emit one instruction when each atom receives the same gate exactly
      // once.
      if (!compound->empty() && compound->size() == atoms.size() &&
          compound->isGlobal(atoms.size()) &&
          std::ranges::all_of(*compound, [](const auto& gate) {
            return gate->isSingleQubitGate() && gate->getNtargets() == 1;
          })) {
        append(*compound->front(),
               std::views::iota(std::size_t{0}, atoms.size()));
      } else {
        for (const auto& gate : *compound) {
          append(*gate, gate->getTargets());
        }
      }
    } else {
      append(*op, op->getTargets());
    }
  }
}
auto CodeGenerator::appendTwoQubitGates(
    const Placement& currentPlacement, const Routing& executionRouting,
    const Placement& executionPlacement, const Routing& targetRouting,
    const Placement& targetPlacement,
    const std::vector<std::reference_wrapper<const Atom>>& atoms,
    const std::vector<std::reference_wrapper<const Zone>>& zones,
    NAComputation& code) const -> void {
  appendRearrangement(currentPlacement, executionRouting, executionPlacement,
                      atoms, code);
  std::vector<const Zone*> zonePtrs;
  zonePtrs.reserve(zones.size());
  std::transform(zones.begin(), zones.end(), std::back_inserter(zonePtrs),
                 [](const auto& zone) { return &zone.get(); });
  code.emplaceBack<NAComputationGlobalCZOperation>(zonePtrs);
  appendRearrangement(executionPlacement, targetRouting, targetPlacement, atoms,
                      code);
}
namespace {
enum class Axis { Row, Column };
enum class Transfer { Load, Store };
enum class SiteKind { Storage, Left, Right };

struct Position {
  int64_t x;
  int64_t y;
  SiteKind kind;
};

auto getPosition(const Architecture& architecture, const Site& site)
    -> Position {
  const auto& [slm, row, col] = site;
  const auto [x, y] = architecture.exactSLMLocation(slm, row, col);
  auto kind = SiteKind::Storage;
  if (const auto* zone = slm.get().entanglementZone_) {
    const auto& left =
        zone->front().location.first < zone->back().location.first
            ? zone->front()
            : zone->back();
    kind = slm.get() == left ? SiteKind::Left : SiteKind::Right;
  }
  return {
      .x = static_cast<int64_t>(x), .y = static_cast<int64_t>(y), .kind = kind};
}

struct Movement {
  Position source;
  Position target;
  [[nodiscard]] auto position(Transfer transfer) const -> const Position& {
    return transfer == Transfer::Load ? source : target;
  }
};

/// The first column selects storage or paired spacing for the whole batch.
auto batchKind(SiteKind first, SiteKind candidate) -> SiteKind {
  if (first == SiteKind::Storage) {
    return SiteKind::Storage;
  }
  return candidate == SiteKind::Left ? SiteKind::Left : SiteKind::Right;
}

/// Spacing of active AOD lines along the transfer axis.
struct LineSpacing {
  Axis axis;
  int64_t pitch;
  int64_t min;
  int64_t max;
  int64_t regularOffset;
  int64_t pairSeparation;

  [[nodiscard]] auto paired(SiteKind kind) const -> bool {
    return axis == Axis::Column && kind != SiteKind::Storage;
  }
  /// Divide the space between entanglement pairs into three gaps.
  [[nodiscard]] auto pairOffset() const -> int64_t {
    return (pitch - pairSeparation) / 3;
  }
  [[nodiscard]] auto offset(SiteKind kind) const -> int64_t {
    if (!paired(kind)) {
      return regularOffset;
    }
    return kind == SiteKind::Left ? -pairOffset() : pairOffset();
  }
  [[nodiscard]] auto above(SiteKind kind) const -> int64_t {
    if (!paired(kind)) {
      return pitch + regularOffset;
    }
    return kind == SiteKind::Left ? pairSeparation + pairOffset()
                                  : pitch - pairSeparation - pairOffset();
  }
  [[nodiscard]] auto below(SiteKind kind) const -> int64_t {
    if (!paired(kind)) {
      return regularOffset;
    }
    return kind == SiteKind::Left ? pitch - pairSeparation - pairOffset()
                                  : pairSeparation + pairOffset();
  }
  [[nodiscard]] auto loadThreshold(int64_t next, SiteKind kind) const
      -> int64_t {
    return next - (paired(kind) ? above(kind) : regularOffset);
  }
  [[nodiscard]] auto storeThreshold(int64_t next, SiteKind kind,
                                    int64_t freeLines) const -> int64_t {
    if (!paired(kind)) {
      return next - regularOffset + freeLines * pitch;
    }
    const auto oddGap =
        kind == SiteKind::Left ? pairOffset() : pitch - pairOffset();
    return next + offset(kind) + (freeLines / 2) * pitch +
           (freeLines % 2) * oddGap;
  }

  /// Step to the next virtual line. Paired columns alternate between two gaps.
  [[nodiscard]] auto nextGap(bool inside, SiteKind wideSide,
                             SiteKind& kind) const -> int64_t {
    if (!paired(kind)) {
      return inside ? pitch : pitch / 2;
    }
    const auto distance =
        inside && kind == wideSide ? pitch - pairOffset() : pairOffset();
    kind = kind == SiteKind::Left ? SiteKind::Right : SiteKind::Left;
    return distance;
  }
};

auto getSpacing(const SLM& slm, Axis axis, int64_t verticalSign,
                int64_t pairSeparation) -> LineSpacing {
  const bool row = axis == Axis::Row;
  const auto pitch = static_cast<int64_t>(row ? slm.siteSeparation.second
                                              : slm.siteSeparation.first);
  const auto min =
      static_cast<int64_t>(row ? slm.location.second : slm.location.first);
  const auto count = static_cast<int64_t>(row ? slm.nRows : slm.nCols);
  return {.axis = axis,
          .pitch = pitch,
          .min = min,
          .max = min + pitch * count,
          .regularOffset = row ? verticalSign * pitch / 2 : pitch / 2,
          .pairSeparation = pairSeparation};
}

struct Line {
  int64_t id; // AOD column or row
  SiteKind kind;
  std::vector<qc::Qubit> qubits;
  std::set<int64_t> crossings; // Occupied coordinates on the perpendicular axis
};
using Lines = std::map<int64_t, Line>;
using ActiveLines =
    std::map<int64_t, int64_t>; // AOD line to current coordinate

/// AOD columns follow source x order and rows follow target y order.
/// The router guarantees that lines neither split nor merge within a group.
auto collectLines(const std::map<qc::Qubit, Movement>& movements,
                  Transfer transfer, Axis axis) -> Lines {
  Lines lines;
  const bool row = axis == Axis::Row;
  for (const auto& [qubit, move] : movements) {
    const auto& site = move.position(transfer);
    auto [it, inserted] = lines.try_emplace(row ? site.y : site.x);
    auto& line = it->second;
    if (inserted) {
      line.id = row ? move.target.y : move.source.x;
      line.kind = site.kind;
    }
    line.qubits.emplace_back(qubit);
    line.crossings.emplace(row ? site.x : site.y);
  }
  return lines;
}

auto pushLower(ActiveLines& active, ActiveLines::iterator line, int64_t next,
               const LineSpacing& spacing, SiteKind kind) -> void {
  for (auto it = std::make_reverse_iterator(line);
       it != active.rend() && it->second > next; ++it) {
    it->second = next;
    next -= spacing.nextGap(next > spacing.min, SiteKind::Left, kind);
  }
}

/// Offset the first line of a batch and make room below it.
auto alignFirst(ActiveLines& active, Lines::const_iterator first,
                const LineSpacing& spacing) -> ActiveLines::iterator {
  const auto& [coordinate, line] = *first;
  const auto position = coordinate + spacing.offset(line.kind);
  const auto anchor = active.insert_or_assign(line.id, position).first;
  // Rows step a full pitch from the offset position, including for odd pitches.
  const auto below = spacing.axis == Axis::Row
                         ? position - spacing.pitch
                         : coordinate - spacing.below(line.kind);
  pushLower(active, anchor, below, spacing, line.kind);
  return anchor;
}

/// Spread active lines around the first destination before the first STORE.
auto spreadForStore(ActiveLines& active, const Line& first,
                    const LineSpacing& spacing) -> void {
  const auto anchor = active.find(first.id);
  const auto spread = [&](auto it, auto end, bool below) {
    auto position = anchor->second;
    auto kind = first.kind;
    for (; it != end; ++it) {
      const bool inside = below || spacing.axis == Axis::Column
                              ? position > spacing.min
                              : position < spacing.max;
      position += below ? -spacing.nextGap(inside, SiteKind::Right, kind)
                        : spacing.nextGap(inside, SiteKind::Left, kind);
      if (below ? it->second > position : it->second < position) {
        it->second = position;
      } else {
        if (spacing.paired(kind) &&
            (it->second - position) % spacing.pitch != 0) {
          kind = kind == SiteKind::Left ? SiteKind::Right : SiteKind::Left;
        }
        position = it->second;
      }
    }
  };
  spread(std::make_reverse_iterator(anchor), active.rend(), true);
  spread(std::next(anchor), active.end(), false);
}

using Batch = std::vector<Lines::const_iterator>;

/// Load compatible lines into the gaps between already active AOD lines.
auto packLoad(const Lines& lines, Lines::const_iterator first,
              ActiveLines& active, const LineSpacing& spacing) -> Batch {
  const auto& [coordinate, line] = *first;
  auto kind = line.kind;
  const auto anchor = alignFirst(active, first, spacing);
  Batch batch{first};
  auto next = coordinate + spacing.above(kind);
  for (auto upper = std::next(anchor);; ++upper) {
    for (auto it = lines.lower_bound(spacing.loadThreshold(next, kind));
         it != lines.end(); ++it) {
      const auto& candidate = it->second;
      if (candidate.id <= std::prev(upper)->first ||
          (upper != active.end() && candidate.id >= upper->first) ||
          candidate.crossings != line.crossings) {
        continue;
      }
      batch.emplace_back(it);
      kind = batchKind(line.kind, candidate.kind);
      active.emplace(candidate.id, it->first + spacing.offset(kind));
      next = it->first + spacing.above(kind);
    }
    if (upper == active.end()) {
      break;
    }
    if (upper->second < next) {
      upper->second = next;
      next += spacing.nextGap(next < spacing.max, SiteKind::Right, kind);
    }
  }
  return batch;
}

/// Align active lines that can store the same perpendicular coordinates.
auto packStore(Lines::const_iterator first, ActiveLines& active,
               const std::map<int64_t, Lines::const_iterator>& targets,
               const LineSpacing& spacing) -> Batch {
  const auto& [coordinate, line] = *first;
  auto kind = line.kind;
  const auto anchor = alignFirst(active, first, spacing);
  Batch batch{first};
  auto next = coordinate + spacing.above(kind);
  // Count active lines left in place since the last alignment.
  int64_t freeLines = 0;
  for (auto it = std::next(anchor); it != active.end(); ++it) {
    const auto candidate = targets.at(it->first);
    const auto& [target, other] = *candidate;
    if (target >= spacing.storeThreshold(next, kind, freeLines) &&
        other.crossings == line.crossings) {
      batch.emplace_back(candidate);
      kind = batchKind(line.kind, other.kind);
      it->second = target + spacing.offset(kind);
      pushLower(active, it, target - spacing.below(kind), spacing, kind);
      next = target + spacing.above(kind);
      freeLines = 0;
    } else if (it->second < next) {
      it->second = next;
      next += spacing.nextGap(next < spacing.max, SiteKind::Right, kind);
      if (spacing.axis == Axis::Column) {
        freeLines = 0;
      }
    } else {
      ++freeLines;
    }
  }
  return batch;
}

/// Collect qubits in emission order across all lines in a batch.
auto batchedQubits(const Batch& batch) -> std::vector<qc::Qubit> {
  std::vector<qc::Qubit> qubits;
  for (const auto entry : batch) {
    const auto& group = entry->second.qubits;
    qubits.insert(qubits.end(), group.begin(), group.end());
  }
  std::ranges::sort(qubits);
  return qubits;
}

class RearrangementGenerator {
  const std::map<qc::Qubit, Movement>& movements_;
  const std::vector<std::reference_wrapper<const Atom>>& atoms_;
  NAComputation& code_;
  std::map<qc::Qubit, std::pair<int64_t, int64_t>> shuttling_;

  auto emitMove(const ActiveLines& columns, const ActiveLines& rows) -> void {
    std::vector<const Atom*> atoms;
    std::vector<Location> destinations;
    for (auto& [qubit, current] : shuttling_) {
      const auto& move = movements_.at(qubit);
      const std::pair next{columns.at(move.source.x), rows.at(move.target.y)};
      if (current != next) {
        atoms.emplace_back(&atoms_[qubit].get());
        destinations.emplace_back(
            Location{.x = static_cast<double>(next.first),
                     .y = static_cast<double>(next.second)});
        current = next;
      }
    }
    if (!atoms.empty()) {
      code_.emplaceBack<NAComputationMoveOperation>(atoms, destinations);
    }
  }

  auto emitTransfer(Transfer transfer, const std::vector<qc::Qubit>& qubits)
      -> void {
    std::vector<const Atom*> atoms;
    atoms.reserve(qubits.size());
    for (const auto qubit : qubits) {
      atoms.emplace_back(&atoms_[qubit].get());
      if (transfer == Transfer::Load) {
        const auto& source = movements_.at(qubit).source;
        shuttling_.emplace(qubit, std::pair{source.x, source.y});
      } else {
        shuttling_.erase(qubit);
      }
    }
    if (transfer == Transfer::Load) {
      code_.emplaceBack<NAComputationLoadOperation>(atoms);
    } else {
      code_.emplaceBack<NAComputationStoreOperation>(atoms);
    }
  }

public:
  RearrangementGenerator(
      const std::map<qc::Qubit, Movement>& movements,
      const std::vector<std::reference_wrapper<const Atom>>& atoms,
      NAComputation& code)
      : movements_(movements), atoms_(atoms), code_(code) {}

  auto transfer(Transfer transfer, Axis axis, const SLM& slm,
                int64_t verticalSign, int64_t pairSeparation) -> void {
    const auto lines = collectLines(movements_, transfer, axis);
    const auto spacing = getSpacing(slm, axis, verticalSign, pairSeparation);
    ActiveLines rows;
    ActiveLines columns;
    auto& active = axis == Axis::Row ? rows : columns;
    auto& perpendicular = axis == Axis::Row ? columns : rows;
    const auto alignPerpendicular = [&](qc::Qubit qubit, bool offset) {
      const auto& move = movements_.at(qubit);
      const auto& site = move.position(transfer);
      if (axis == Axis::Column) {
        const auto dy = static_cast<int64_t>(slm.siteSeparation.second);
        perpendicular[move.target.y] =
            site.y + (offset ? verticalSign * dy / 2 : 0);
      } else {
        const auto dx = static_cast<int64_t>(slm.siteSeparation.first);
        auto shift = dx / 2;
        if (site.kind != SiteKind::Storage) {
          shift = site.kind == SiteKind::Left ? -dx / 4 : dx / 4;
        }
        perpendicular[move.source.x] = site.x + (offset ? shift : 0);
      }
    };

    std::map<int64_t, Lines::const_iterator> targets;
    if (transfer == Transfer::Store) {
      for (auto it = lines.begin(); it != lines.end(); ++it) {
        const auto& [target, line] = *it;
        targets.emplace(line.id, it);
        active.emplace(line.id, target + spacing.offset(line.kind));
        for (const auto qubit : line.qubits) {
          alignPerpendicular(qubit, true);
        }
      }
      spreadForStore(active, lines.begin()->second, spacing);
    }

    for (auto it = lines.begin(); it != lines.end(); ++it) {
      const auto& line = it->second;
      // A previous batch may have already transferred this line.
      const bool isActive = active.contains(line.id);
      if (transfer == Transfer::Load ? isActive : !isActive) {
        continue;
      }
      const auto batch = transfer == Transfer::Load
                             ? packLoad(lines, it, active, spacing)
                             : packStore(it, active, targets, spacing);
      emitMove(columns, rows);
      for (const auto qubit : line.qubits) {
        alignPerpendicular(qubit, false);
      }
      if (transfer == Transfer::Store) {
        for (const auto entry : batch) {
          active.at(entry->second.id) = entry->first;
        }
      }
      emitMove(columns, rows);

      const auto qubits = batchedQubits(batch);
      emitTransfer(transfer, qubits);
      for (const auto qubit : qubits) {
        alignPerpendicular(qubit, true);
      }
      if (transfer == Transfer::Store) {
        for (const auto entry : batch) {
          active.erase(entry->second.id);
        }
      }
      emitMove(columns, rows);
    }
  }
};
} // namespace

auto CodeGenerator::appendRearrangement(
    const Placement& startPlacement, const Routing& routing,
    const Placement& targetPlacement,
    const std::vector<std::reference_wrapper<const Atom>>& atoms,
    NAComputation& code) const -> void {
  for (const auto& qubits : routing) {
    if (qubits.empty()) {
      continue;
    }
    std::map<qc::Qubit, Movement> movements;
    std::map<int64_t, int64_t> horizontalMoves;
    std::map<int64_t, int64_t> verticalMoves;
    for (const auto qubit : qubits) {
      const auto source =
          getPosition(architecture_.get(), startPlacement.at(qubit));
      const auto target =
          getPosition(architecture_.get(), targetPlacement.at(qubit));
      movements.emplace(qubit, Movement{.source = source, .target = target});
      horizontalMoves.emplace(source.x, target.x);
      verticalMoves.emplace(source.y, target.y);
      assert(horizontalMoves.at(source.x) == target.x &&
             verticalMoves.at(source.y) == target.y);
    }
    const bool loadColumns =
        horizontalMoves.size() < verticalMoves.size() &&
        std::ranges::is_sorted(verticalMoves | std::views::values);
    const bool storeRows =
        verticalMoves.size() < horizontalMoves.size() &&
        std::ranges::is_sorted(horizontalMoves | std::views::values);
    const auto& [sourceY, targetY] = *verticalMoves.begin();
    const auto sign = sourceY < targetY ? 1 : -1;
    const auto& sourceSlm =
        std::get<0>(startPlacement.at(qubits.front())).get();
    const auto& targetSlm =
        std::get<0>(targetPlacement.at(qubits.front())).get();
    const auto* pair = sourceSlm.isEntanglement() ? sourceSlm.entanglementZone_
                                                  : targetSlm.entanglementZone_;
    assert(pair != nullptr);
    const auto pairSeparation =
        std::abs(static_cast<int64_t>(pair->back().location.first) -
                 static_cast<int64_t>(pair->front().location.first));
    RearrangementGenerator generator(movements, atoms, code);
    generator.transfer(Transfer::Load, loadColumns ? Axis::Column : Axis::Row,
                       sourceSlm, sign, pairSeparation);
    generator.transfer(Transfer::Store, storeRows ? Axis::Row : Axis::Column,
                       targetSlm, -sign, pairSeparation);
  }
}
auto CodeGenerator::generate(
    const std::vector<SingleQubitGateLayer>& singleQubitGateLayers,
    const std::vector<Placement>& placement,
    const std::vector<Routing>& routing) const -> NAComputation {
  NAComputation code;
  std::vector<std::reference_wrapper<const Zone>> rydbergZones;
  rydbergZones.reserve(architecture_.get().rydbergRangeMinX.size());
  for (size_t i = 0; i < architecture_.get().rydbergRangeMinX.size(); ++i) {
    rydbergZones.emplace_back(code.emplaceBackZone(
        "zone_cz" + std::to_string(i),
        Zone::Extent{
            static_cast<double>(architecture_.get().rydbergRangeMinX.at(i)),
            static_cast<double>(architecture_.get().rydbergRangeMinY.at(i)),
            static_cast<double>(architecture_.get().rydbergRangeMaxX.at(i)),
            static_cast<double>(architecture_.get().rydbergRangeMaxY.at(i))}));
  }
  size_t minX = std::numeric_limits<size_t>::max();
  size_t maxX = std::numeric_limits<size_t>::min();
  size_t minY = std::numeric_limits<size_t>::max();
  size_t maxY = std::numeric_limits<size_t>::min();
  for (const auto& zone : architecture_.get().storageZones) {
    minX = std::min(minX, zone->location.first);
    maxX = std::max(maxX, zone->location.first +
                              zone->siteSeparation.first * zone->nCols);
    minY = std::min(minY, zone->location.second);
    maxY = std::max(maxY, zone->location.second +
                              zone->siteSeparation.second * zone->nRows);
  }
  const auto& globalZone = code.emplaceBackZone(
      "global",
      Zone::Extent{static_cast<double>(minX), static_cast<double>(minY),
                   static_cast<double>(maxX), static_cast<double>(maxY)});
  const auto& initialPlacement = placement.front();
  std::vector<std::reference_wrapper<const Atom>> atoms;
  atoms.reserve(initialPlacement.size());
  for (const auto& [slm, r, c] : initialPlacement) {
    atoms.emplace_back(
        code.emplaceBackAtom("atom" + std::to_string(atoms.size())));
    const auto& [x, y] = architecture_.get().exactSLMLocation(slm, r, c);
    code.emplaceInitialLocation(atoms.back(), x, y);
  }
  // early return if no single-qubit gates are given
  if (singleQubitGateLayers.empty()) {
    return code;
  }
  assert(2 * singleQubitGateLayers.size() == placement.size() + 1);
  assert(placement.size() == routing.size() + 1);
  appendSingleQubitGates(singleQubitGateLayers.front(), atoms, globalZone,
                         code);
  for (size_t layer = 0; layer + 1 < singleQubitGateLayers.size(); ++layer) {
    appendTwoQubitGates(placement[2 * layer], routing[2 * layer],
                        placement[(2 * layer) + 1], routing[(2 * layer) + 1],
                        placement[2 * (layer + 1)], atoms, rydbergZones, code);
    appendSingleQubitGates(singleQubitGateLayers[layer + 1], atoms, globalZone,
                           code);
  }
  return code;
}
} // namespace na::zoned
