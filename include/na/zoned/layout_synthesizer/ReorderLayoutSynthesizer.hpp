#pragma once

#include "na/zoned/layout_synthesizer/LayoutSynthesizerBase.hpp"

#pragma once

#include <cstddef>
#include <functional>
#include <unordered_map>
#include <utility>
#include <vector>


namespace na::zoned {
namespace {
template <class Priority, class T, class Compare = std::less<Priority>>
/// A class that implements a heap data structure.
/// @details The heap is a container that provides constant time lookup of the
/// largest (by default) element, at the expense of logarithmic insertion and
/// extraction. A user-provided `Compare` can be supplied to change the
/// ordering, e.g., using `std::greater<T>` would cause the smallest element to
/// appear as the top(). Opposed to `std::priority_queue`, this heap allows for
/// updating the priority of an element in O(log(n)) time. Additionally, it
/// allows for erasing an element in O(log(n)) time and elements are unique.
/// This means, if an element is pushed that is already in the heap, the
/// priority of the existing element is updated.
/// @tparam Priority The type of the priority of the elements.
/// @tparam T The type of the elements.
/// @tparam Compare The type of the comparison function.
class Heap {
public:
  using PriorityType = Priority;
  using ElementType = T;
  using ValueType = std::pair<PriorityType, ElementType>;
  using SizeType = typename std::vector<ValueType>::size_type;
  using PriorityCompare = Compare;
  using ConstReference = const ValueType&;
  using Reference = ValueType&;

private:
  std::vector<ValueType> heap_;
  std::unordered_map<ElementType, SizeType> keyToIndex_;

  /// Moves the element at index i up the heap until the heap property is
  /// satisfied.
  /// @param i The index of the element to move up.
  void heapifyUp(size_t i) {
    while (i > 0) {
      size_t parent = (i - 1) / 2;
      if (PriorityCompare{}(heap_[i].first, heap_[parent].first)) {
        std::swap(heap_[i], heap_[parent]);
        keyToIndex_[heap_[i]] = i;
        keyToIndex_[heap_[parent]] = parent;
        i = parent;
      } else {
        break;
      }
    }
  }

  /// Moves the element at index i down the heap until the heap property is
  /// satisfied.
  /// @param i The index of the element to move down.
  void heapifyDown(size_t i) {
    while (true) {
      size_t leftChild = (2 * i) + 1;
      size_t rightChild = (2 * i) + 2;
      size_t smallest = i;

      if (leftChild < heap_.size() &&
          PriorityCompare{}(heap_[leftChild], heap_[smallest])) {
        smallest = leftChild;
      }
      if (rightChild < heap_.size() &&
          PriorityCompare{}(heap_[rightChild], heap_[smallest])) {
        smallest = rightChild;
      }
      if (smallest != i) {
        std::swap(heap_[i], heap_[smallest]);
        keyToIndex_[heap_[i]] = i;
        keyToIndex_[heap_[smallest]] = smallest;
        i = smallest;
      } else {
        break;
      }
    }
  }

public:
  /// Returns a reference to the top element of the heap.
  /// @details Takes O(1) time.
  /// @return A reference to the top element of the heap.
  [[nodiscard]] auto top() const -> ConstReference { return heap_.front(); }
  /// Removes the top element from the heap.
  /// @details Takes O(log(n)) time.
  auto pop() -> void {
    keyToIndex_.erase(heap_.front().second);
    std::swap(heap_.front(), heap_.back());
    heap_.pop_back();
    keyToIndex_.at(heap_.front().second) = 0;
    heapifyDown(0);
  }
  /// Checks if the heap is empty.
  /// @return True if the heap is empty, false otherwise.
  [[nodiscard]] auto empty() const -> bool { return heap_.empty(); }
  /// Returns the number of elements in the heap.
  /// @return The number of elements in the heap.
  [[nodiscard]] auto size() const -> SizeType { return heap_.size(); }
  /// Adds an element to the heap.
  /// @details Takes O(log(n)) time.
  /// @param value The priority-element pair to add to the heap.
  auto push(const ValueType& value) -> void {
    if (keyToIndex_.find(value.second) != keyToIndex_.end()) {
      update(value.second, value.first);
    }
    heap_.push_back(value);
    keyToIndex_.at(value.second) = heap_.size() - 1;
    heapifyUp(heap_.size() - 1);
  }
  /// Constructs a new element in place in the heap.
  /// @details Takes O(log(n)) time.
  /// @param args The arguments to construct the priority-element pair.
  /// @return A reference to the newly constructed priority-element pair.
  template <class... Args> auto emplace(Args&&... args) -> ConstReference {
    auto value = value_type(std::forward<Args>(args)...);
    if (keyToIndex_.find(value.second) != keyToIndex_.end()) {
      return update(std::move(value));
    }
    push(std::move(value));
    return heap_[keyToIndex_.at(value.second)];
  }
  /// Updates the priority of an element in the heap.
  /// @details Takes O(log(n)) time.
  /// @param value The priority-element pair with the new priority for the
  /// element to be updated.
  auto update(const ValueType& value) -> ConstReference {
    const auto i = keyToIndex_.find(value.second)->second;
    heap_[i] = value;
    keyToIndex_.at(value.second) = i;
    // for the case that the priority is increased
    heapifyUp(i);
    // for the case that the priority is decreased
    heapifyDown(i);
    return heap_[keyToIndex_.at(value.second)];
  }
  /// Erase an element from the heap.
  /// @details Takes O(log(n)) time.
  /// @param element The element to erase.
  auto erase(const ElementType& element) -> void {
    const auto i = keyToIndex_.find(element)->second;
    keyToIndex_.erase(element);
    std::swap(heap_[i], heap_.back());
    heap_.pop_back();
    keyToIndex_.at(heap_[i].second) = i;
    heapifyDown(i);
  }
};
} // namespace
class ReorderLayoutSynthesizer : public LayoutSynthesizerBase {

};
} // namespace na
