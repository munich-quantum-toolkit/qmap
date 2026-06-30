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

#include <algorithm>
#include <cstddef>
#include <unordered_map>

namespace qc {
template <class T> struct DisjointSet {
  std::unordered_map<T, T> parent;
  std::unordered_map<T, std::size_t> rank;

  template <class Iterator>
  explicit DisjointSet(const Iterator& begin, const Iterator& end) {
    std::for_each(begin, end, [&](const auto& element) {
      parent[element] = element;
      rank[element] = 0;
    });
  }

  T findSet(const T& v) {
    if (parent[v] != v) {
      parent[v] = findSet(parent[v]);
    }
    return parent[v];
  }

  void unionSet(const T& x, const T& y) {
    const auto& xSet = findSet(x);
    const auto& ySet = findSet(y);
    if (rank[xSet] > rank[ySet]) {
      parent[ySet] = xSet;
    } else {
      parent[xSet] = ySet;
      if (rank[xSet] == rank[ySet]) {
        ++rank[ySet];
      }
    }
  }
};
} // namespace qc
