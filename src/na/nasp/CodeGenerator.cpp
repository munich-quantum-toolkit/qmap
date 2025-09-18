#include "na/nasp/CodeGenerator.hpp"

#include "circuit_optimizer/CircuitOptimizer.hpp"
#include "datastructures/Layer.hpp"
#include "ir/Definitions.hpp"
#include "ir/QuantumComputation.hpp"
#include "ir/operations/OpType.hpp"
#include "na/NAComputation.hpp"
#include "na/entities/Atom.hpp"
#include "na/nasp/Solver.hpp"
#include "na/operations/GlobalCZOp.hpp"
#include "na/operations/GlobalRYOp.hpp"
#include "na/operations/LoadOp.hpp"
#include "na/operations/LocalRZOp.hpp"
#include "na/operations/MoveOp.hpp"
#include "na/operations/StoreOp.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace na {
using namespace qc;

auto CodeGenerator::coordFromDiscrete(
    const NASolver::Result::Qubit q, const int64_t maxHOffset,
    const int64_t maxVOffset, const int64_t minEntanglingY,
    const int64_t maxEntanglingY, const int64_t minAtomDist,
    const int64_t noInteractionRadius, const int64_t zoneDist) -> Location {
  const auto dx = noInteractionRadius + (2LL * maxHOffset * minAtomDist);
  const auto dy = noInteractionRadius + (2LL * maxVOffset * minAtomDist);
  const auto x = q.x;
  const auto y = q.y;
  const auto h = q.h;
  const auto v = q.v;
  if (minEntanglingY == 0) {
    // no top storage zone
    if (y <= maxEntanglingY) {
      return {static_cast<double>(x * dx + h * minAtomDist),
              static_cast<double>(y * dy + v * minAtomDist)};
    }
    return {static_cast<double>(x * dx + h * minAtomDist),
            static_cast<double>(zoneDist + (y - 1) * dy + v * minAtomDist)};
  }
  // top storage zone
  if (y < minEntanglingY) {
    return {static_cast<double>(x * dx + h * minAtomDist),
            static_cast<double>(y * dy + v * minAtomDist)};
  }
  if (y <= maxEntanglingY) {
    return {static_cast<double>(x * dx + h * minAtomDist),
            static_cast<double>(zoneDist + (y - 1) * dy + v * minAtomDist)};
  }
  return {static_cast<double>(x * dx + h * minAtomDist),
          static_cast<double>(2LL * zoneDist + (y - 2) * dy + v * minAtomDist)};
}

auto CodeGenerator::generate(const QuantumComputation& input,
                             const NASolver::Result& result,
                             const uint16_t minAtomDist,
                             const uint16_t noInteractionRadius,
                             const uint16_t zoneDist) -> NAComputation {
  auto minEntanglingY = result.minEntanglingY;
  auto maxEntanglingY = result.maxEntanglingY;
  auto maxHOffset = result.maxHOffset;
  auto maxVOffset = result.maxVOffset;
  auto flattened = input;
  CircuitOptimizer::flattenOperations(flattened);
  const Layer layer(flattened);
  NAComputation code;
  const auto& globalZone = code.emplaceBackZone("global");
  const auto& interactionZone = code.emplaceBackZone("zone_cz0");
  std::vector<const Atom*> atoms;
  std::vector<bool> wasAOD;
  atoms.reserve(result.stages.front().qubits.size());
  wasAOD.reserve(result.stages.front().qubits.size());
  // initialize atoms in SLM and load required ones into AOD
  {
    std::unordered_map<std::size_t, std::set<std::size_t>> hAODLines{};
    std::unordered_map<std::size_t, std::set<std::size_t>> vAODLines{};
    for (const auto& q : result.stages.front().qubits) {
      if (q.a) {
        hAODLines[static_cast<std::size_t>(q.y)].emplace(q.r);
        vAODLines[static_cast<std::size_t>(q.x)].emplace(q.c);
      }
    }
    std::vector<const Atom*> loadAtoms;
    std::size_t c = 0;
    for (const auto& q : result.stages.front().qubits) {
      const auto& pos = coordFromDiscrete(
          q, maxHOffset, maxVOffset, minEntanglingY, maxEntanglingY,
          minAtomDist, noInteractionRadius, zoneDist);
      wasAOD.emplace_back(q.a);
      const auto& atom = *atoms.emplace_back(
          &code.emplaceBackAtom("atom" + std::to_string(c++)));
      if (q.a) {
        loadAtoms.emplace_back(&atom);
      }
      code.emplaceInitialLocation(atom, pos);
    }
    if (!loadAtoms.empty()) {
      code.emplaceBack<LoadOp>(loadAtoms);
    }
  }
  const auto ops = layer.getExecutablesOfType(H, 0);
  std::unordered_set<Qubit> affectedQubits;
  std::transform(
      ops.cbegin(), ops.cend(),
      std::inserter(affectedQubits, affectedQubits.end()),
      [](const auto& v) { return v->getOperation()->getTargets().front(); });
  if (affectedQubits.size() != flattened.getNqubits() ||
      ops.size() != affectedQubits.size()) {
    throw std::invalid_argument("Not all atoms are initialized to plus state.");
  }
  // initialize atoms in to |+> state starting in |0> state
  code.emplaceBack<GlobalRYOp>(globalZone, PI_2);
  std::for_each(ops.cbegin(), ops.cend(), [](const auto& v) { v->execute(); });
  // Reference to the executable set of the input circuit
  const auto& executableSet = layer.getExecutableSet();
  if (result.stages.front().rydberg) {
    code.emplaceBack<GlobalCZOp>(interactionZone);
    // find and execute corresponding gates in input circuit
    for (const auto& g : result.stages.front().gates) {
      const auto& it = std::find_if(
          executableSet.begin(), executableSet.end(), [g](const auto& v) {
            if (v->getOperation()->getType() == Z &&
                v->getOperation()->getNcontrols() == 1) {
              const auto& usedQubits = v->getOperation()->getUsedQubits();
              const auto [first, second] = g.qubits;
              return std::set{first, second} == usedQubits;
            }
            return false;
          });
      if (it == executableSet.end()) {
        throw std::invalid_argument(
            "Gate in input circuit has no correspondence in solution.");
      }
      (*it)->execute();
    }
  }
  for (uint16_t t = 1; t < static_cast<uint16_t>(result.stages.size()); ++t) {
    std::vector<const Atom*> moveAtoms;
    std::vector<Location> targetLocations;
    std::vector<const Atom*> loadAtoms;
    std::vector<const Atom*> storeAtoms;
    for (uint16_t i = 0;
         static_cast<size_t>(i) < result.stages.at(t).qubits.size(); ++i) {
      const auto& q = result.stages.at(t).qubits.at(i);
      auto pos = coordFromDiscrete(q, maxHOffset, maxVOffset, minEntanglingY,
                                   maxEntanglingY, minAtomDist,
                                   noInteractionRadius, zoneDist);
      if (wasAOD[i] && q.a) {
        moveAtoms.emplace_back(atoms[i]);
        targetLocations.emplace_back(pos);
      } else if (wasAOD[i] && !q.a) {
        storeAtoms.emplace_back(atoms[i]);
      } else if (!wasAOD[i] && q.a) {
        loadAtoms.emplace_back(atoms[i]);
        moveAtoms.emplace_back(atoms[i]);
        targetLocations.emplace_back(pos);
      }
      wasAOD[i] = q.a;
    }
    if (!storeAtoms.empty()) {
      code.emplaceBack<StoreOp>(storeAtoms);
    }
    if (!loadAtoms.empty()) {
      code.emplaceBack<LoadOp>(loadAtoms);
    }
    if (!moveAtoms.empty()) {
      code.emplaceBack<MoveOp>(moveAtoms, targetLocations);
    }
    if (result.stages.at(t).rydberg) {
      code.emplaceBack<GlobalCZOp>(interactionZone);
    }
    // find and execute corresponding gates in input circuit
    for (const auto& g : result.stages.at(t).gates) {
      const auto& it = std::find_if(
          executableSet.begin(), executableSet.end(), [g](const auto& v) {
            if (v->getOperation()->getType() == Z &&
                v->getOperation()->getNcontrols() == 1) {
              const auto& usedQubits = v->getOperation()->getUsedQubits();
              const auto [first, second] = g.qubits;
              return std::set{first, second} == usedQubits;
            }
            return false;
          });
      if (it == executableSet.end()) {
        throw std::invalid_argument(
            "Gate in input circuit has no correspondence in solution.");
      }
      (*it)->execute();
    }
  }
  if (!executableSet.empty()) {
    code.emplaceBack<GlobalRYOp>(globalZone, -PI_4);
    while (!executableSet.empty()) {
      const auto& v = (*executableSet.cbegin());
      if (v->getOperation()->getType() != H) {
        throw std::invalid_argument(
            "Not all non CZ-gates in input circuit are executed.");
      }
      const auto q = v->getOperation()->getTargets().front();
      code.emplaceBack<LocalRZOp>(*atoms[q], PI);
      v->execute();
    }
    code.emplaceBack<GlobalRYOp>(globalZone, PI_4);
  }
  return code;
}
} // namespace na
