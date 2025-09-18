/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include <algorithm>
#include <array>
#include <cassert>
#include <functional>
#include <iostream>
#include <queue>
#include <set>
#include <vector>

#pragma once

constexpr int MAX_QUEUE_SIZE = 6000000;
constexpr int MAX_QUEUE_COPY_LENGTH = 1000000;
constexpr double QUEUE_COPY_LENGTH_PERCENTAGE = 1. / 6;

template <class T> struct DoNothing {
  void operator()(const T& /*unused*/) {
    // intentionally left blank
  }
};

template <class T, class Container = std::vector<T>,
          class Compare = std::less<typename Container::value_type>>
class OwnPriorityQueue : public std::priority_queue<T, Container, Compare> {
public:
  Container& getContainer() { return this->c; }
};

/**
 * Priority queue with unique (according to FuncCompare) elements of type T
 * where the sorting is based on CostCompare. If NDEBUG is *not* defined, there
 * are some assertions that help catching errors in the provided comparison
 * functions.
 */
template <class T, class CostCompare = std::greater<T>,
          class FuncCompare = std::less<T>,
          class CleanObsoleteElement = DoNothing<T>>
class UniquePriorityQueue {
public:
  using size_type =
      typename OwnPriorityQueue<T, std::vector<T>, CostCompare>::size_type;

  /**
   * Return true if the element was inserted into the queue.
   * This happens if equivalent element is present or if the new element has a
   * lower cost associated to it. False is returned if no insertion into the
   * queue took place.
   */
  bool push(const T& v) {
    const auto& insertionPair = membership.insert(v);
    if (insertionPair.second) {
      queue.push(v);
    } else if (CostCompare()(*(insertionPair.first), v)) {
      CleanObsoleteElement()(*(insertionPair.first));
      membership.erase(insertionPair.first);

      [[maybe_unused]] const auto inserted = membership.insert(v);
      assert(inserted.second);

      queue = OwnPriorityQueue<T, std::vector<T>, CostCompare>();
      for (const auto& element : membership) {
        queue.push(element);
      }
      assert(queue.size() == membership.size());

      return true;
    } else {
      CleanObsoleteElement()(v);
    }
    assert(queue.size() == membership.size());
    return insertionPair.second;
  }

  void pop() {
    assert(!queue.empty() && queue.size() == membership.size());

    const auto& topElement = queue.top();
    [[maybe_unused]] const auto numberErased = membership.erase(topElement);
    assert(numberErased == 1);

    queue.pop();
    assert(queue.size() == membership.size());
  }

  const T& top() const {
    assert(!queue.empty());
    return queue.top();
  }

  [[nodiscard]] bool empty() const {
    assert(queue.size() == membership.size());
    return queue.empty();
  }

  size_type size() const { return queue.size(); }

  std::vector<T>& getContainer() { return queue.get_container(); }

  void deleteQueue() {
    std::vector<T>& v = getContainer();
    for (typename std::vector<T>::const_iterator it = v.begin(); it != v.end();
         it++) {
      CleanObsoleteElement()(*it);
    }
    queue = OwnPriorityQueue<T, std::vector<T>, CostCompare>();
    membership = std::set<T, FuncCompare>();
  }

  // clears the queue until a certain length is reached
  void update() {
    std::array<T, MAX_QUEUE_SIZE> tempQueue;
    unsigned int length =
        std::min(static_cast<int>(queue.size() * QUEUE_COPY_LENGTH_PERCENTAGE),
                 MAX_QUEUE_COPY_LENGTH);

    for (unsigned int i = 0; i < length; i++) {
      tempQueue[i] = queue.top();
      queue.pop();

      lastNodeCopied = i;
    }
    deleteQueue();
    for (unsigned int i = 0; i < length; i++) {
      queue.push(tempQueue[i]);
    }

    std::cout << "RESULTING SIZE: " << queue.size() << std::endl;
  }

  void restart(T& n) {
    deleteQueue();
    push(n);
  }

private:
  OwnPriorityQueue<T, std::vector<T>, CostCompare> queue;
  std::set<T, FuncCompare> membership;
  unsigned int lastNodeCopied = 0;
};
