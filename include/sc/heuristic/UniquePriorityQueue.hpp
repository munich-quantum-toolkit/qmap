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
#include <cassert>
#include <cstddef>
#include <functional>
#include <memory>
#include <set>
#include <utility>
#include <vector>

/// Priority queue with one pending winner per FuncCompare equivalence class.
template <class T, class CostCompare = std::greater<T>,
          class FuncCompare = std::less<T>>
class UniquePriorityQueue {
  template <class Compare> struct Dereference {
    template <class L, class R>
    bool operator()(const L& lhs, const R& rhs) const {
      return Compare{}(*lhs, *rhs);
    }
  };

public:
  using size_type = std::size_t;

  bool push(const T& value) { return pushImpl(value); }
  bool push(T&& value) { return pushImpl(std::move(value)); }

  void pop() { extractTop(); }

  [[nodiscard]] T takeTop() { return std::move(*extractTop()); }

  [[nodiscard]] const T& top() const {
    assert(!empty());
    return *queue.front();
  }

  [[nodiscard]] bool empty() const { return membership.empty(); }

  [[nodiscard]] size_type size() const { return membership.size(); }

  void clear() {
    membership.clear();
    queue.clear();
  }

private:
  template <class Value> bool pushImpl(Value&& value) {
    const auto previous = membership.find(std::addressof(value));
    if (previous != membership.end() && !CostCompare{}(**previous, value)) {
      return false;
    }
    if (previous != membership.end()) {
      membership.erase(previous);
    }
    const auto& inserted =
        queue.emplace_back(std::make_unique<T>(std::forward<Value>(value)));
    membership.insert(inserted.get());
    std::ranges::push_heap(queue, Dereference<CostCompare>{});
    removeObsoleteNodes();
    return true;
  }

  std::unique_ptr<T> extractTop() {
    assert(!empty());
    std::ranges::pop_heap(queue, Dereference<CostCompare>{});
    auto node = std::move(queue.back());
    queue.pop_back();
    membership.erase(node.get());
    removeObsoleteNodes();
    return node;
  }

  [[nodiscard]] bool isPending(const T* node) const {
    const auto it = membership.find(node);
    return it != membership.end() && *it == node;
  }

  void removeObsoleteNodes() {
    while (!queue.empty() && !isPending(queue.front().get())) {
      std::ranges::pop_heap(queue, Dereference<CostCompare>{});
      queue.pop_back();
    }
    // Obsolete nodes can stay below the front. Limit them to the live count
    // to bound retention without rebuilding the heap on each replacement.
    if (queue.size() - membership.size() > membership.size()) {
      std::erase_if(
          queue, [this](const auto& node) { return !isPending(node.get()); });
      std::ranges::make_heap(queue, Dereference<CostCompare>{});
    }
  }

  std::vector<std::unique_ptr<T>> queue;
  std::set<const T*, Dereference<FuncCompare>> membership;
};
