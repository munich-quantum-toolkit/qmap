/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "na/computation/NAComputation.hpp"

#include "na/computation/entities/Atom.hpp"
#include "na/computation/entities/Location.hpp"
#include "na/computation/operations/LoadOp.hpp"
#include "na/computation/operations/LocalOp.hpp"
#include "na/computation/operations/Op.hpp"
#include "na/computation/operations/ShuttlingOp.hpp"
#include "na/computation/operations/StoreOp.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
namespace na {
auto NAComputation::getLocationOfAtomAfterOperation(const Atom& atom,
                                                    const Op& op) const
    -> Location {
  auto currentLocation = initialLocations_.at(&atom);
  for (const auto& opUniquePtr : operations_) {
    if (opUniquePtr->is<ShuttlingOp>()) {
      if (const auto& shuttlingOp = opUniquePtr->as<ShuttlingOp>();
          shuttlingOp.hasTargetLocations()) {
        const auto& opAtoms = shuttlingOp.getAtoms();
        const auto& targetLocations = shuttlingOp.getTargetLocations();
        for (std::size_t k = 0; k < opAtoms.size(); ++k) {
          if (opAtoms[k] == &atom) {
            currentLocation = targetLocations[k];
            break;
          }
        }
      }
    }
    if (opUniquePtr.get() == &op) {
      break;
    }
  }
  return currentLocation;
}
auto NAComputation::toString() const -> std::string {
  std::stringstream ss;
  std::map<Location, const Atom*> initialLocationsAsc;
  for (const auto& [atom, loc] : initialLocations_) {
    initialLocationsAsc.emplace(loc, atom);
  }
  for (const auto& [loc, atom] : initialLocationsAsc) {
    ss << "atom " << loc << " " << *atom << "\n";
  }
  for (const auto& op : operations_) {
    ss << *op << "\n";
  }
  return ss.str();
}
auto NAComputation::validate() const -> std::pair<bool, std::string> {
  // This counter is used to display the operation number where an error
  // occurred.
  // As every operation might not correspond to one line in the output,
  // this may not be identical with the line number in the output.
  // However, the first operation initializes the atom and because of that, the
  // counter starts with 1.
  std::size_t counter = 1;
  std::stringstream ss;
  if (atoms_.size() != initialLocations_.size()) {
    ss << "Number of atoms and initial locations must be equal\n";
    return {false, ss.str()};
  }
  // This map is used to keep track of each atom's current location to check
  // the constraints when shuttling atoms.
  std::unordered_map<const Atom*, Location> currentLocations =
      initialLocations_;
  // This set is used to keep track of the atoms that are currently shuttling,
  // i.e., they are loaded but not yet stored again.
  std::unordered_set<const Atom*> currentlyShuttling{};
  for (const auto& op : operations_) {
    ++counter;
    if (op->is<ShuttlingOp>()) {
      //===----------------------------------------------------------------===//
      // Shuttling Operations
      //===----------------------------------------------------------------===//
      const auto& shuttlingOp = op->as<ShuttlingOp>();
      const auto& opAtoms = shuttlingOp.getAtoms();
      if (shuttlingOp.is<LoadOp>()) {
        //===-----------------------------------------------------------------//
        // Load Operations
        //-----------------------------------------------------------------===//
        if (std::ranges::any_of(opAtoms,
                                [&currentlyShuttling](const auto* atom) {
                                  return currentlyShuttling.contains(atom);
                                })) {
          ss << "Error in op number " << counter << " (atom already loaded)\n";
          return {false, ss.str()};
        }
        for (const auto* atom : opAtoms) {
          currentlyShuttling.emplace(atom);
        }
      } else {
        //===-----------------------------------------------------------------//
        // Move and Store Operations
        //-----------------------------------------------------------------===//
        if (std::ranges::any_of(opAtoms,
                                [&currentlyShuttling](const auto* atom) {
                                  return !currentlyShuttling.contains(atom);
                                })) {
          ss << "Error in op number " << counter << " (atom not loaded)\n";
          return {false, ss.str()};
        }
      }
      //===----------------------------------------------------------------===//
      // All Shuttling Operations that move atoms
      //===----------------------------------------------------------------===//
      if (shuttlingOp.hasTargetLocations()) {
        const auto& targetLocations = shuttlingOp.getTargetLocations();
        // 1) Guard: one-to-one mapping between atoms and targets
        if (opAtoms.size() != targetLocations.size()) {
          ss << "Error in op number " << counter
             << " (atoms/targets size mismatch)\n";
          return {false, ss.str()};
        }
        // 2) Precompute end map and detect duplicates once
        std::unordered_set<const Atom*> seen;
        seen.reserve(opAtoms.size());
        std::unordered_map<const Atom*, Location> endOf;
        endOf.reserve(opAtoms.size());
        for (std::size_t i = 0; i < opAtoms.size(); ++i) {
          const auto* b = opAtoms.at(i);
          if (!seen.emplace(b).second) {
            ss << "Error in op number " << counter
               << " (two atoms identical)\n";
            return {false, ss.str()};
          }
          endOf.emplace(b, targetLocations.at(i));
        }
        std::unordered_map<const Atom*, size_t> opAtomToIndex;
        opAtomToIndex.reserve(opAtoms.size());
        for (std::size_t i = 0; i < opAtoms.size(); ++i) {
          opAtomToIndex.emplace(opAtoms[i], i);
        }
        // 3) Validate against all loaded atoms, including non-moving ones
        for (const auto& atom : atoms_) {
          if (const auto* a = atom.get(); currentlyShuttling.contains(a)) {
            const auto& s1 = currentLocations.at(a);
            const auto it1 = endOf.find(a);
            const Location& e1 = (it1 != endOf.end()) ? it1->second : s1;
            const auto it2 = opAtomToIndex.find(a);
            for (std::size_t i = it2 == opAtomToIndex.end() ? 0
                                                            : it2->second + 1;
                 i < opAtoms.size(); ++i) {
              const auto* b = opAtoms.at(i);
              assert(a != b);
              const auto& s2 = currentLocations.at(b);
              const Location& e2 = targetLocations.at(i);

              if (e1 == e2) {
                ss << "Error in op number " << counter
                   << " (two end points identical)\n";
                return {false, ss.str()};
              }
              // Exp.:
              //  o -----> o
              //  o --> o
              if (s1.x == s2.x && e1.x != e2.x) {
                ss << "Error in op number " << counter
                   << " (columns not preserved)\n";
                return {false, ss.str()};
              }
              // Exp.:
              // o   o
              // |   |
              // v   |
              // o   v
              //     o
              if (s1.y == s2.y && e1.y != e2.y) {
                ss << "Error in op number " << counter
                   << " (rows not preserved)\n";
                return {false, ss.str()};
              }
              // Exp.:
              // o -------> o
              //    o--> o
              if (s1.x < s2.x && e1.x >= e2.x) {
                ss << "Error in op number " << counter
                   << " (column order not preserved)\n";
                return {false, ss.str()};
              }
              // Exp.:
              // o
              // |  o
              // |  |
              // |  v
              // v  o
              // o
              if (s1.y < s2.y && e1.y >= e2.y) {
                ss << "Error in op number " << counter
                   << " (row order not preserved)\n";
                return {false, ss.str()};
              }
              // Exp.:
              //    o--> o
              // o -------> o
              if (s1.x > s2.x && e1.x <= e2.x) {
                ss << "Error in op number " << counter
                   << " (column order not preserved)\n";
                return {false, ss.str()};
              }
              // Exp.:
              //   o
              // o |
              // | |
              // v |
              // o v
              //   o
              if (s1.y > s2.y && e1.y <= e2.y) {
                ss << "Error in op number " << counter
                   << " (row order not preserved)\n";
                return {false, ss.str()};
              }
            }
          }
        }
        // 4) Update current locations
        for (std::size_t i = 0; i < opAtoms.size(); ++i) {
          currentLocations.at(opAtoms.at(i)) = targetLocations.at(i);
        }
      }
      if (shuttlingOp.is<StoreOp>()) {
        //===-----------------------------------------------------------------//
        // Store Operations
        //-----------------------------------------------------------------===//
        for (const auto& atom : opAtoms) {
          currentlyShuttling.erase(atom);
        }
      }
    } else if (op->is<LocalOp>()) {
      //===----------------------------------------------------------------===//
      // Local Operations
      //===----------------------------------------------------------------===//
      const auto& opAtoms = op->as<LocalOp>().getAtoms();
      std::unordered_set<const Atom*> usedAtoms;
      for (const auto* const atom : opAtoms) {
        if (!usedAtoms.emplace(atom).second) {
          ss << "Error in op number " << counter << " (two atoms identical)\n";
          return {false, ss.str()};
        }
      }
    }
  }
  return {true, ""};
}
} // namespace na
