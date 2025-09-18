/*
 * Copyright (c) 2025 Chair for Design Automation, TUM
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "na/nalac/datastructures/NAComputation.hpp"

#include "na/nalac/datastructures/operations/NALocalOperation.hpp"
#include "na/nalac/datastructures/operations/NAShuttlingOperation.hpp"

#include <cstddef>
#include <ios>
#include <iostream>
#include <sstream>
#include <string>

namespace na::nalac {
auto NAComputation::toString() const -> std::string {
  std::stringstream ss;
  ss << "init at ";
  for (const auto& p : initialPositions) {
    ss << *p << ", ";
  }
  if (ss.tellp() == 8) {
    ss.seekp(-1, std::ios_base::end);
  } else {
    ss.seekp(-2, std::ios_base::end);
  }
  ss << ";\n";
  for (const auto& op : operations) {
    ss << *op;
  }
  return ss.str();
}
auto NAComputation::validateAODConstraints() const -> bool {
  std::size_t counter = 1; // the first operation is `init at ...;`
  for (const auto& naOp : operations) {
    ++counter;
    if (naOp->isShuttlingOperation()) {
      const auto& shuttlingOp =
          dynamic_cast<const NAShuttlingOperation&>(*naOp);
      if (shuttlingOp.getStart().size() != shuttlingOp.getEnd().size()) {
        return false;
      }
      for (std::size_t i = 0; i < shuttlingOp.getStart().size(); ++i) {
        for (std::size_t j = i + 1; j < shuttlingOp.getStart().size(); ++j) {
          const auto& s1 = shuttlingOp.getStart()[i];
          const auto& s2 = shuttlingOp.getStart()[j];
          const auto& e1 = shuttlingOp.getEnd()[i];
          const auto& e2 = shuttlingOp.getEnd()[j];
          if (*s1 == *s2) {
            std::cout << "Error in op number " << counter
                      << " (two start points identical)\n";
            return false;
          }
          if (*e1 == *e2) {
            std::cout << "Error in op number " << counter
                      << " (two end points identical)\n";
            return false;
          }
          if (s1->x == s2->x && e1->x != e2->x) {
            std::cout << "Error in op number " << counter
                      << " (columns not preserved)\n";
            return false;
          }
          if (s1->y == s2->y && e1->y != e2->y) {
            std::cout << "Error in op number " << counter
                      << " (rows not preserved)\n";
            return false;
          }
          if (s1->x < s2->x && e1->x >= e2->x) {
            std::cout << "Error in op number " << counter
                      << " (column order not preserved)\n";
            return false;
          }
          if (s1->y < s2->y && e1->y >= e2->y) {
            std::cout << "Error in op number " << counter
                      << " (row order not preserved)\n";
            return false;
          }
          if (s1->x > s2->x && e1->x <= e2->x) {
            std::cout << "Error in op number " << counter
                      << " (column order not preserved)\n";
            return false;
          }
          if (s1->y > s2->y && e1->y <= e2->y) {
            std::cout << "Error in op number " << counter
                      << " (row order not preserved)\n";
            return false;
          }
        }
      }
    } else if (naOp->isLocalOperation()) {
      const auto& localOp = dynamic_cast<const NALocalOperation&>(*naOp);
      for (std::size_t i = 0; i < localOp.getPositions().size(); ++i) {
        for (std::size_t j = i + 1; j < localOp.getPositions().size(); ++j) {
          const auto& a = localOp.getPositions()[i];
          const auto& b = localOp.getPositions()[j];
          if (*a == *b) {
            std::cout << "Error in op number " << counter
                      << " (identical positions)\n";
            return false;
          }
        }
      }
    }
  }
  return true;
}
} // namespace na::nalac
