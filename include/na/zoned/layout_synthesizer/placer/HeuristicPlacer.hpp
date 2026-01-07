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

#include "ir/Definitions.hpp"
#include "na/zoned/Architecture.hpp"
#include "na/zoned/Types.hpp"
#include "na/zoned/layout_synthesizer/placer/PlacerBase.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <queue>
#include <spdlog/spdlog.h>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace na::zoned {
/**
 * An unordered map from a row or columns of an SLM to a value of type T.
 * @tparam T the type of the value
 */
template <class T>
using RowColumnMap =
    std::unordered_map<std::pair<std::reference_wrapper<const SLM>, size_t>, T>;
/// An unordered set of rows or columns of an SLM.
using RowColumnSet =
    std::unordered_set<std::pair<std::reference_wrapper<const SLM>, size_t>>;
/**
 * @brief The heuristic placer is a class that provides a method to determine
 * the placement of the atoms in each layer using a heuristic search algorithm.
 */
class HeuristicPlacer : public PlacerBase {
  friend class HeuristicPlacerTest_AStarSearch_Test;
  using DiscreteSite = std::array<uint8_t, 2>;
  using CompatibilityGroup = std::array<std::map<uint8_t, uint8_t>, 2>;

  std::reference_wrapper<const Architecture> architecture_;
  /**
   * @brief If true, during the initial placement, the atoms are placed starting
   * in the last row instead of the first row.
   * @details This flag is computed automatically based on the given
   * architecture. If the (first) entanglement zone is closer to the bottom of
   * the storage zone, this flag is set to true.
   * @note Is automatically set in the constructor.
   */
  bool reverseInitialPlacement_ = false;
  /**
   * @brief If the window is used, this denotes the minimum height in terms of
   * columns. The window is centered at the nearest site. This value is
   * computed based on the @ref windowMinWidth_ and the @ref windowRatio_.
   * @note Is automatically computed in the constructor.
   */
  size_t windowMinHeight_;

public:
  /// The configuration of the heuristic placer
  struct Config {
    /**
     * @brief This flag indicates whether the placement should use a window when
     * selecting potential free sites.
     * @note Specified by the user in the configuration file.
     */
    bool useWindow = true;
    /**
     * @brief If the window is used, this denotes the minimum width in terms of
     * rows. The window is centered at the nearest site.
     * @note Specified by the user in the configuration file.
     */
    size_t windowMinWidth = 16;
    /**
     * @brief If the window is used, this denotes the ratio between the height
     * and the width of the window.
     * @details A value greater than 1 means that the window is
     * higher than wide (portrait). A value of exactly 1 means that the window
     * is square. A value smaller than 1 means that the window is wider than
     * high (landscape).
     * @note Specified by the user in the configuration file.
     */
    double windowRatio = 1.0;
    /**
     * @brief If the window is used, this denotes the share of free sites in the
     * window in relation to the number of atoms to be moved in this step.
     * @details The window is extended according to the ratio as long as the
     * share of free sites is smaller than this value. A value of one ensures
     * that there are at least as many free sites in the window of every atom as
     * atoms that need to be moved. Hence, a value greater or equal to 1 ensures
     * that there exists a solution. However, a smaller value might be a
     * reasonable good guess since it is almost certain that not all atoms to be
     * moved will end in the same window.
     * @note Specified by the user in the configuration file.
     */
    double windowShare = 0.8;
    /// Enum of available heuristic methods used for the search.
    enum class Method : uint8_t {
      /// A-star algorithm
      ASTAR,
      /// Iterative diving search
      IDS
    };
    /// The heuristic method used for the search (default: IDS).
    Method method = Method::IDS;
    /**
     * @brief The heuristic used in the A* search contains a term that resembles
     * the standard deviation of the differences between the current and target
     * sites of the atoms to be moved in every orientation.
     * @details This factor is multiplied with the sum of standard deviations to
     * adjust the influence of this term. Setting it to 0.0 disables this term
     * and, if the lookahead is also disabled, resulting in an admissible
     * heuristic. However, this leads to a vast exploration of the search tree
     * and usually results in a huge number of nodes visited.
     */
    float deepeningFactor = 0.01F;
    /**
     * @brief Before the sum of standard deviations is multiplied with the
     * number of unplaced nodes and @ref deepeningFactor_, this value is added
     * to the sum to amplify the influence of the unplaced nodes count.
     * @see deepeningFactor_
     */
    float deepeningValue = 0.0F;
    /**
     * @brief The cost function can consider the distance of atoms to their
     * interaction partner in the next layer.
     * @details This factor is multiplied with the distance to adjust the
     * influence of this term. Setting it to 0.0 disables the lookahead
     * entirely. A factor of 1.0 implies that the lookahead is as important as
     * the distance to the target site, which is usually not desired.
     */
    float lookaheadFactor = 0.4F;
    /**
     * @brief The reuse level corresponds to the estimated extra fidelity loss
     * due to the extra trap transfers when the atom is not reused and instead
     * moved to the storage zone and back to the entanglement zone.
     * @details It is subtracted from the cost for the reuse option to favor
     * this option over the non-reuse options.
     */
    float reuseLevel = 5.0F;
    /**
     * @brief The maximum number of nodes that are allowed to be visited in the
     * A* search tree.
     * @detsils If this number is exceeded, the search is aborted and an error
     * is raised.
     */
    size_t maxNodes = 10'000'000;
    /**
     * @brief The number of trials determines the number of restarts during IDS.
     * @note This option is only relevant if the IDS heuristic method is used.
     */
    size_t trials = 4;
    /**
     * @brief The maximum capacity of the priority queue used during IDS.
     * @note This option is only relevant if the IDS heuristic method is used.
     */
    size_t queueCapacity = 100;

  private:
    /// @returns the default configuration for the A* method
    [[nodiscard]] static auto createWithAStarDefaults() -> Config;

    /// @returns the default configuration for the IDS method
    [[nodiscard]] static auto createWithIDSDefaults() -> Config;

  public:
    /**
     * @param method the method to use for the placer
     * @returns the default configuration for a given method
     */
    [[nodiscard]] static auto createForMethod(Method method) -> Config;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(
        Config, useWindow, windowMinWidth, windowRatio, windowShare, method,
        deepeningFactor, deepeningValue, lookaheadFactor, reuseLevel, maxNodes,
        trials, queueCapacity);
  };

private:
  /// The configuration of the A* placer
  Config config_;
  /**
   * @brief When placing atoms after a rydberg layer back in the storage zone,
   * this struct stores for every such atom all required information, i.e., the
   * current site and potential target sites ordered by distance (ascending).
   */
  struct AtomJob {
    /// The atom to be placed
    qc::Qubit atom;
    /// The current site of the atom
    DiscreteSite currentSite;
    /// The minimum lookahead distance
    float meanLookaheadCost = 0.0F;
    /// A struct describing one potential target site
    struct Option {
      /// The target site
      DiscreteSite site;
      /**
       * When this flag is set to true, it indicates that the atom should not
       * move at all and remain in the entanglement zone. Then the attribute
       * `site` is ignored.
       */
      bool reuse;
      /// The distance the atom must travel to reach the target site
      float distance;
      /// Additional lookahead distance to next interaction partner
      float lookaheadCost = 0.0F;
    };
    /// A list of all potential target sites ordered by distance (ascending)
    std::vector<Option> options;
  };

  /**
   * @brief When placing gates in the entanglement zone before a rydberg layer,
   * this struct stores for every such gate all required information, i.e., the
   * current sites of the corresponding atoms and potential target sites
   * ordered by distance (ascending).
   */
  struct GateJob {
    /// The two atoms belonging to that gate
    std::array<qc::Qubit, 2> qubits;
    /// The current sites of the two atoms
    std::array<DiscreteSite, 2> currentSites;
    /// The minimum lookahead distance
    float meanLookaheadCost = 0.0F;
    /// A struct describing one potential target site for each atom
    struct Option {
      /// The target sites for the two atoms
      std::array<DiscreteSite, 2> sites;
      /// The max distance the atoms must travel to reach the target sites
      std::array<float, 2> distance;
      /// The additional lookahead distance to next interaction partner
      float lookaheadCost = 0.0F;
    };
    /// A list of all potential target sites ordered by distance (ascending)
    std::vector<Option> options;
  };

  /**
   * @brief A node representing one stage in the process of placing all atoms
   * that must be moved for the next stage starting from the last mapping
   * until a new mapping is found satisfying all constraints of the next
   * stage
   */
  struct AtomNode {
    /// The parent node.
    std::shared_ptr<const AtomNode> parent = nullptr;
    /**
     * The current level in the search tree. A level equal to the number of
     * atoms to be placed indicates that all atoms have been placed.
     */
    uint16_t level = 0;
    /**
     * The index of the chosen option for the current atom instead of a pointer
     * to that option to save memory
     */
    uint16_t option = 0;
    /// The accumulated lookahead cost
    float lookaheadCost = 0.0F;
    /**
     * A set of all sites that are already occupied by an atom due to the
     * current placement
     */
    std::unordered_set<DiscreteSite> consumedFreeSites;
    /**
     * A binary search tree representing the horizontal and vertical group,
     * @see getNeighbors for more details
     */
    std::vector<CompatibilityGroup> groups;
    /**
     * The maximum distance of placed atoms in every group to their
     * target location
     */
    std::vector<float> maxDistancesOfPlacedAtomsPerGroup;
  };

  /**
   * @brief A node representing one stage in the process of placing all atoms
   * that must be moved for the next stage starting from the last mapping
   * until a new mapping is found satisfying all constraints of the next
   * stage.
   */
  struct GateNode {
    /// The parent node.
    std::shared_ptr<const GateNode> parent = nullptr;
    /**
     * The current level in the search tree. A level equal to the number of
     * gates to be placed indicates that all gates have been placed.
     */
    uint16_t level = 0;
    /**
     * The index of the chosen option for the current gate instead of a pointer
     * to that option to save memory
     */
    uint16_t option = 0;
    /// The accumulated lookahead cost
    float lookaheadCost = 0.0F;
    /**
     * A set of all sites that are already occupied by an atom due to the
     * current placement
     */
    std::unordered_set<DiscreteSite> consumedFreeSites;
    /**
     * A binary search tree representing the horizontal and vertical group,
     * @see getNeighbors for more details
     */
    std::vector<CompatibilityGroup> groups;
    /**
     * The maximum distance of placed atoms in every group to their
     * target location
     */
    std::vector<float> maxDistancesOfPlacedAtomsPerGroup;
  };

public:
  /// Constructs an A* placer for the given architecture and configuration.
  HeuristicPlacer(const Architecture& architecture, const Config& config);

  /**
   * This function defines the interface of the placer and delegates the
   * placement of the qubits to the respective functions.
   * @param nQubits denotes the number of qubits to be placed
   * @param twoQubitGateLayers are the qubits that must be placed for each layer
   * @param reuseQubits are the qubits that are reused in the next stage
   */
  [[nodiscard]] auto
  place(size_t nQubits,
        const std::vector<TwoQubitGateLayer>& twoQubitGateLayers,
        const std::vector<std::unordered_set<qc::Qubit>>& reuseQubits)
      -> std::vector<Placement> override;

private:
  /**
   * @brief This class implements a bounded priority queue.
   * @details It uses two heaps to allow for efficient retrieval and removal of
   * both the minimum and maximum elements. If the maximum capacity is reached,
   * either the new element or the maximum element is discarded on insertion
   * depending on which has a higher priority.
   * @tparam T is the type of elements stored in the queue.
   * @tparam Compare is the comparison functor to determine the priority of the
   * elements.
   */
  template <class T, class Compare = std::less<T>> class BoundedPriorityQueue {
  public:
    using ValueType = T;
    using SizeType = size_t;
    using PriorityCompare = Compare;
    using Reference = ValueType&;

  private:
    struct Node {
      SizeType minHeapIndex;
      SizeType maxHeapIndex;
      ValueType value;
      Node(const SizeType minHeapIndex, const SizeType maxHeapIndex,
           ValueType&& value)
          : minHeapIndex(minHeapIndex), maxHeapIndex(maxHeapIndex),
            value(std::move(value)) {}
    };
    /// Vector of heap elements satisfying the min-heap property.
    std::vector<std::unique_ptr<Node>> minHeap_;
    /// Vector of heap elements satisfying the max-heap property.
    std::vector<Node*> maxHeap_;
    /// The maximum number of elements in the heap.
    SizeType heapCapacity_;
    /// The comparison functor used to determine priority.
    PriorityCompare compare_;

    /**
     * Starts to establish the min-heap property from the given index upwards.
     * @param i is the index in the min-heap.
     */
    auto heapifyMinHeapUp(SizeType i) -> void {
      assert(i < minHeap_.size());
      while (i > 0) {
        size_t parent = (i - 1) / 2;
        if (compare_(minHeap_[i]->value, minHeap_[parent]->value)) {
          std::swap(minHeap_[i], minHeap_[parent]);
          minHeap_[i]->minHeapIndex = i;
          minHeap_[parent]->minHeapIndex = parent;
          i = parent;
        } else {
          break;
        }
      }
    }

    /**
     * Starts to establish the max-heap property from the given index upwards.
     * @param i is the index in the max-heap.
     */
    auto heapifyMaxHeapUp(SizeType i) -> void {
      assert(i < maxHeap_.size());
      while (i > 0) {
        size_t parent = (i - 1) / 2;
        if (compare_(maxHeap_[parent]->value, maxHeap_[i]->value)) {
          std::swap(maxHeap_[i], maxHeap_[parent]);
          maxHeap_[i]->maxHeapIndex = i;
          maxHeap_[parent]->maxHeapIndex = parent;
          i = parent;
        } else {
          break;
        }
      }
    }

    /**
     * Starts to establish the min-heap property from the given index downwards.
     * @param i is the index in the min-heap.
     */
    auto heapifyMinHeapDown(SizeType i) -> void {
      while (true) {
        size_t leftChild = (2 * i) + 1;
        size_t rightChild = (2 * i) + 2;
        size_t smallest = i;

        if (leftChild < minHeap_.size() &&
            compare_(minHeap_[leftChild]->value, minHeap_[smallest]->value)) {
          smallest = leftChild;
        }
        if (rightChild < minHeap_.size() &&
            compare_(minHeap_[rightChild]->value, minHeap_[smallest]->value)) {
          smallest = rightChild;
        }
        if (smallest != i) {
          std::swap(minHeap_[i], minHeap_[smallest]);
          minHeap_[i]->minHeapIndex = i;
          minHeap_[smallest]->minHeapIndex = smallest;
          i = smallest;
        } else {
          break;
        }
      }
    }

    /**
     * Starts to establish the max-heap property from the given index downwards.
     * @param i is the index in the max-heap.
     */
    auto heapifyMaxHeapDown(SizeType i) -> void {
      while (true) {
        size_t leftChild = (2 * i) + 1;
        size_t rightChild = (2 * i) + 2;
        size_t largest = i;

        if (leftChild < maxHeap_.size() &&
            compare_(maxHeap_[largest]->value, maxHeap_[leftChild]->value)) {
          largest = leftChild;
        }
        if (rightChild < maxHeap_.size() &&
            compare_(maxHeap_[largest]->value, maxHeap_[rightChild]->value)) {
          largest = rightChild;
        }
        if (largest != i) {
          std::swap(maxHeap_[i], maxHeap_[largest]);
          maxHeap_[i]->maxHeapIndex = i;
          maxHeap_[largest]->maxHeapIndex = largest;
          i = largest;
        } else {
          break;
        }
      }
    }

  public:
    /**
     * Constructs a new instance of the bounded priority queue.
     * @param maxQueueSize is the maximum number of elements that can be stored
     * in the queue.
     * @note A capacity of 0 results in a no-op queue where push() discards all
     * elements
     */
    explicit BoundedPriorityQueue(const SizeType maxQueueSize)
        : heapCapacity_(maxQueueSize) {
      minHeap_.reserve(heapCapacity_);
      maxHeap_.reserve(heapCapacity_);
    }
    /**
     * @returns the top element of the priority queue.
     * @note If @ref empty returns `true`, calling this function is
     * undefined behavior.
     */
    [[nodiscard]] auto top() const -> const ValueType& {
      assert(!minHeap_.empty());
      return minHeap_.front()->value;
    }
    /**
     * @returns the top element of the priority queue.
     * @note If @ref empty returns `true`, calling this function is
     * undefined behavior.
     */
    [[nodiscard]] auto top() -> ValueType& {
      assert(!minHeap_.empty());
      return minHeap_.front()->value;
    }
    /**
     * @brief Removes the top element.
     * @note If @ref empty returns `true`, calling this function is
     * undefined behavior.
     */
    auto pop() -> void {
      assert(!minHeap_.empty());
      assert(minHeap_.size() == maxHeap_.size());
      assert(minHeap_.size() <= heapCapacity_);
      if (minHeap_.size() == 1) {
        minHeap_.pop_back();
        maxHeap_.pop_back();
      } else {
        assert(minHeap_.size() > 1);
        const auto i = minHeap_.front()->maxHeapIndex;
        std::swap(minHeap_.front(), minHeap_.back());
        minHeap_.pop_back();
        minHeap_.front()->minHeapIndex = 0;
        heapifyMinHeapDown(0);
        if (i == maxHeap_.size() - 1) {
          maxHeap_.pop_back();
        } else {
          std::swap(maxHeap_[i], maxHeap_.back());
          maxHeap_.pop_back();
          maxHeap_[i]->maxHeapIndex = i;
          // Restoring heap property may require moving the swapped element up
          // or down.
          if (i > 0) {
            const size_t parent = (i - 1) / 2;
            if (compare_(maxHeap_[parent]->value, maxHeap_[i]->value)) {
              heapifyMaxHeapUp(i);
            } else {
              heapifyMaxHeapDown(i);
            }
          } else {
            heapifyMaxHeapDown(i);
          }
        }
      }
      assert(minHeap_.size() == maxHeap_.size());
      assert(minHeap_.size() <= heapCapacity_);
    }
    /// @returns `true` if the queue is empty.
    [[nodiscard]] auto empty() const -> bool { return minHeap_.empty(); }
    /// @brief Inserts an element into the priority queue.
    auto push(ValueType&& value) -> void {
      assert(minHeap_.size() == maxHeap_.size());
      if (heapCapacity_ > 0) {
        if (minHeap_.size() < heapCapacity_) {
          minHeap_.emplace_back(std::make_unique<Node>(
              minHeap_.size(), maxHeap_.size(), std::move(value)));
          maxHeap_.emplace_back(minHeap_.back().get());
          heapifyMinHeapUp(minHeap_.size() - 1);
          heapifyMaxHeapUp(maxHeap_.size() - 1);
        } else {
          assert(minHeap_.size() == heapCapacity_);
          // if capacity is reached, only insert the value if smaller than max
          if (compare_(value, maxHeap_.front()->value)) {
            const auto i = maxHeap_.front()->minHeapIndex;
            assert(i < minHeap_.size());
            minHeap_[i] = std::make_unique<Node>(i, 0, std::move(value));
            maxHeap_.front() = minHeap_[i].get();
            heapifyMinHeapUp(i);
            heapifyMaxHeapDown(0);
          }
        }
      }
      assert(minHeap_.size() == maxHeap_.size());
      assert(minHeap_.size() <= heapCapacity_);
    }
    /**
     * @brief Removes the top element and decrements the maximum capacity by
     * one.
     * @note If @ref empty returns `true`, calling this function is
     * undefined behavior.
     */
    auto popAndShrink() -> void {
      pop();
      if (heapCapacity_ > 0) {
        --heapCapacity_;
      }
      assert(minHeap_.size() == maxHeap_.size());
      assert(minHeap_.size() <= heapCapacity_);
    }
  };

  /**
   * @brief Performs an iterative diving search (IDS) on the defined graph.
   * @tparam Node is the type of nodes in the graph.
   * @param start is the start node to start the search from.
   * @param getNeighbors is a function to retrieve the neighbor reachable from a
   * given node.
   * @param isGoal is a function to determine whether a given node is a goal
   * node.
   * @param getCost is a function returning the cost of a node in the graph. The
   * cost of the start node must be 0.
   * @param getHeuristic is a function returning the heuristic value of a given
   * node.
   * @param trials is the number of attempts to find a goal node that are
   * performed at most. This parameter must be greater than 0.
   * @param queueCapacity is the capacity of the queue used for the iterative
   * diving search. For the actual capacity, the current value of trial is
   * added.
   * @return a shared pointer to the final node of the search.
   */
  template <class Node>
  [[nodiscard]] static auto iterativeDivingSearch(
      std::shared_ptr<const Node> start,
      const std::function<std::vector<std::shared_ptr<const Node>>(
          std::shared_ptr<const Node>)>& getNeighbors,
      const std::function<bool(const Node&)>& isGoal,
      const std::function<double(const Node&)>& getCost,
      const std::function<double(const Node&)>& getHeuristic, size_t trials,
      const size_t queueCapacity) -> std::shared_ptr<const Node> {
    if (trials == 0) {
      throw std::invalid_argument("IDS requires trials >= 1");
    }
    struct Item {
      double priority;                  //< sum of cost and heuristic
      std::shared_ptr<const Node> node; //< pointer to the node

      Item(const double priority, std::shared_ptr<const Node> node)
          : priority(priority), node(node) {
        assert(!std::isnan(priority));
      }
    };
    struct ItemCompare {
      auto operator()(const Item& a, const Item& b) const -> bool {
        return a.priority < b.priority;
      }
    };
    // Initial capacity accounts for shrinking: popAndShrink() decrements
    // capacity on each trial
    BoundedPriorityQueue<Item, ItemCompare> queue(queueCapacity + trials);
    std::optional<Item> goal;
    Item currentItem{getHeuristic(*start), start};
    while (true) {
      if (isGoal(*currentItem.node)) {
        SPDLOG_TRACE("Goal node found with priority {}", currentItem.priority);
        trials--;
        if (!goal.has_value() || currentItem.priority < goal->priority) {
          goal = std::move(currentItem);
        }
        if (trials > 0 && !queue.empty()) {
          SPDLOG_TRACE("Restart search with priority {}", goal->priority);
          currentItem = std::move(queue.top());
          queue.popAndShrink();
          continue;
        }
        break;
      }
      // Expand the current node by adding all neighbors to the open set
      std::optional<Item> minItem = std::nullopt;
      for (auto& neighbor : getNeighbors(currentItem.node)) {
        const auto cost = getCost(*neighbor);
        const auto heuristic = getHeuristic(*neighbor);
        Item item{cost + heuristic, std::move(neighbor)};
        if (!minItem) {
          minItem = std::move(item);
        } else if (item.priority < minItem->priority) {
          queue.push(std::move(*minItem));
          minItem = std::move(item);
        } else {
          queue.push(std::move(item));
        }
      }
      if (minItem) {
        currentItem = std::move(*minItem);
      } else {
        assert(trials > 0);
        if (!queue.empty()) {
          SPDLOG_TRACE("No neighbors found, restarting search");
          currentItem = std::move(queue.top());
          queue.pop();
        } else {
          break;
        }
      }
    }
    if (!goal) {
      throw std::runtime_error(
          "No path from start to any goal found. This may be caused by a too "
          "narrow window size. Try adjusting the window_share compiler "
          "configuration option to a higher value, such as 1.0.");
    }
    return goal->node;
  }

  /**
   * @brief A* search algorithm for trees
   * @details A* is a graph traversal and path search algorithm that finds the
   * shortest path between a start node and a goal node. It evaluates nodes by
   * combining the cost to reach the node and the cost to get from the node to
   * the goal estimated by a heuristic function.
   * @par
   * This implementation of the A* search algorithm has some particularities:
   * - To increase performance for the special case of a tree, where there
   * cannot be any cycles and a node can only be reached by one path, it does
   * not keep visited nodes. This would require a hash set or similar data
   * structure to store visited nodes and check if a node has already been
   * visited. This check would take at least O(log(n)) time for a hash set and
   * is superfluous for trees.
   * - As a consequence of the first point, this implementation also does not
   * check whether a node is already in the open set. This would also require an
   * O(log(n)) check operation, which is not necessary for trees as one path can
   * only reach a node.
   * @note This implementation of A* search can only handle trees and not
   * general graphs. This is because it does not keep track of visited nodes and
   * therefore cannot detect cycles. Also, for DAGs it may expand nodes multiple
   * times when they can be reached by different paths from the start node.
   * @note The calling program has to make sure that the pointers passed to this
   * function are valid and that the iterators are not invalidated during the
   * search, e.g., by calling one of the passed functions like @p getNeighbors.
   * @param start is a reference to the start node
   * @param getNeighbors is a function that returns the neighbors of a node as
   * references
   * @param isGoal is a function that returns true if a node is one of
   * potentially multiple goals
   * @param getCost is a function that returns the total cost to reach that
   * particular node from the start node. The cost of the start node must be 0.
   * @param getHeuristic is a function that returns the heuristic cost from the
   * node to any goal.
   * @param maxNodes is the maximum number of nodes held in the priority queue
   * before the search is aborted. This parameter must be greater than 0.
   * @return a vector of node references representing the path from the start to
   * a goal
   */
  template <class Node>
  [[nodiscard]] static auto
  aStarTreeSearch(std::shared_ptr<const Node> start,
                  const std::function<std::vector<std::shared_ptr<const Node>>(
                      std::shared_ptr<const Node>)>& getNeighbors,
                  const std::function<bool(const Node&)>& isGoal,
                  const std::function<double(const Node&)>& getCost,
                  const std::function<double(const Node&)>& getHeuristic,
                  size_t maxNodes) -> std::shared_ptr<const Node> {
    if (maxNodes == 0) {
      throw std::invalid_argument("`maxNodes` must be greater than 0");
    }
    //===--------------------------------------------------------------------===//
    // Setup open set structure
    //===--------------------------------------------------------------------===//
    // struct for items in the open set
    struct Item {
      double priority;                  //< sum of cost and heuristic
      std::shared_ptr<const Node> node; //< pointer to the node

      Item(const double priority, std::shared_ptr<const Node> node)
          : priority(priority), node(node) {
        assert(!std::isnan(priority));
      }
    };
    // compare function for the open set
    struct ItemCompare {
      auto operator()(const Item& a, const Item& b) const -> bool {
        // this way, the item with the lowest priority is on top of the heap
        return a.priority > b.priority;
      }
    };
    // open list of nodes to be evaluated as a minimum heap based on the
    // priority.
    std::priority_queue<Item, std::vector<Item>, ItemCompare> openSet;
    openSet.emplace(getHeuristic(*start), start);
    //===--------------------------------------------------------------------===//
    // Perform A* search
    //===--------------------------------------------------------------------===//
    while (!openSet.empty()) {
      auto itm = openSet.top();
      openSet.pop();
      // if a goal is reached, that is the shortest path to a goal under the
      // assumption that the heuristic is admissible
      if (isGoal(*itm.node)) {
        return itm.node;
      }
      // expand the current node by adding all neighbors to the open set
      const auto& neighbors = getNeighbors(itm.node);
      if (!neighbors.empty()) {
        for (const auto& neighbor : neighbors) {
          // getCost returns the total cost to reach the current node
          const auto cost = getCost(*neighbor);
          const auto heuristic = getHeuristic(*neighbor);
          openSet.emplace(cost + heuristic, neighbor);
        }
      }
      if (openSet.size() >= maxNodes) {
        throw std::runtime_error(
            "Maximum number of nodes reached. Increase max_nodes or increase "
            "deepening_value and deepening_factor to reduce the number of "
            "explored nodes.");
      }
    }
    throw std::runtime_error(
        "No path from start to any goal found. This may be caused by a too "
        "narrow window size. Try adjusting the window_share compiler "
        "configuration option to a higher value, such as 1.0.");
  }

  /**
   * @brief This function takes a list of atoms together with their current
   * placement and returns two maps from concrete columns and rows to their
   * discrete indices.
   * @param placement is a list of atoms together with their current placement
   * @param atoms is a list of all atoms that must be placed
   * @return a pair of two maps, the first one maps rows to their discrete
   * indices, and the second one maps columns to their discrete indices
   */
  [[nodiscard]] auto
  discretizePlacementOfAtoms(const Placement& placement,
                             const std::vector<qc::Qubit>& atoms) const
      -> std::pair<RowColumnMap<uint8_t>, RowColumnMap<uint8_t>>;

  /**
   * @brief This function discretizes the storage zone of the architecture and
   * returns two maps from concrete columns and rows to their discrete indices.
   * @param occupiedSites is a set of occupied sites in the storage zone
   * @return a pair of two maps, the first one maps rows to their discrete
   * indices and the second one maps columns to their discrete indices
   */
  [[nodiscard]] auto
  discretizeNonOccupiedStorageSites(const SiteSet& occupiedSites) const
      -> std::pair<RowColumnMap<uint8_t>, RowColumnMap<uint8_t>>;

  /**
   * @brief This function discretizes the entanglement zone of the architecture
   * and returns two maps from concrete columns and rows to their discrete
   * indices.
   * @param occupiedSites is a set of occupied sites in the storage zone
   * @return a pair of two maps, the first one maps rows to their discrete
   * indices and the second one maps columns to their discrete indices
   */
  [[nodiscard]] auto
  discretizeNonOccupiedEntanglementSites(const SiteSet& occupiedSites) const
      -> std::pair<RowColumnMap<uint8_t>, RowColumnMap<uint8_t>>;

  /**
   * @brief This function generates a trivial initial placement for the qubits
   * and fills up the storage zone row by row in the order of the atoms.
   * @param nQubits is the total number of qubits in the quantum computation
   * @return a list of tuples containing the SLM, row, and column of the atom's
   * initial placement
   */
  [[nodiscard]] auto makeInitialPlacement(size_t nQubits) const -> Placement;

  /**
   * @brief Generates the placements for the next two-qubit and single-qubit
   * layers.
   * @details This function takes the placement of the last single-qubit layer
   * where some atoms may have remained in the entanglement zone due to their
   * reuse. It then generates the placement for the next two-qubit layer by
   * and the next single-qubit layer considering the reuse of the atoms.
   * @param previousPlacement is a reference to the previous placement of the
   * atoms
   * @param previousReuseQubits is a reference to the atoms that may have
   * remained in the entanglement zone during the previous placement, i.e., they
   * do not need to be considered for the current placement. Since, the reuse is
   * optional and not mandatory, not all atoms may have been reused.
   * @param reuseQubits is a reference to the atoms that can be reused for the
   * next layer and must be considered during the placement for the single-qubit
   * gate layer.
   * @param twoQubitGates is a list of all two-qubit gates that must be placed
   * in the current layer.
   * @param nextTwoQubitGates is a list of all two-qubit gates that must be
   * placed in the current layer.
   * @return a pair of two lists, the first one contains the placement for the
   * two-qubit gates and the second one contains the placement for the
   * single-qubit gates.
   * @see placeGatesInEntanglementZone
   * @see placeAtomsInStorageZone
   */
  [[nodiscard]] auto makeIntermediatePlacement(
      const Placement& previousPlacement,
      const std::unordered_set<qc::Qubit>& previousReuseQubits,
      const std::unordered_set<qc::Qubit>& reuseQubits,
      const TwoQubitGateLayer& twoQubitGates,
      const TwoQubitGateLayer& nextTwoQubitGates)
      -> std::pair<Placement, Placement>;

  /**
   * @brief This function places the atoms corresponding to gates in the
   * entanglement zone.
   * @details After this placement has been performed, the activation of the
   * Rydberg beam will execute the gates in the given layer. Afterward, the
   * next placement for moving (non-reuse) qubits back to the storage zone is
   * determined by @ref placeAtomsInStorageZone.
   * @param previousPlacement is a reference to the previous placement of the
   * atoms
   * @param reuseQubits is a reference to the atoms that may have been reused,
   * i.e., they have remained in the entanglement zone and do not need to be
   * considered for the current placement.
   * @param twoQubitGates is a list of all two-qubit gates that must be placed
   * in the current layer.
   * @param nextReuseQubits is a reference to the atoms that can be reused for
   * the next layer. Those atoms are taken into account when calculating the
   * lookahead for gates with reuse atoms.
   * @param nextTwoQubitGates is a list of all two-qubit gates in the next
   * two-qubit gate layer used to calculate the lookahead.
   * @return the placement of the atoms for the current two-qubit gate layer.
   */
  [[nodiscard]] auto placeGatesInEntanglementZone(
      const Placement& previousPlacement,
      const std::unordered_set<qc::Qubit>& reuseQubits,
      const TwoQubitGateLayer& twoQubitGates,
      const std::unordered_set<qc::Qubit>& nextReuseQubits,
      const TwoQubitGateLayer& nextTwoQubitGates) -> Placement;

  /**
   * @brief This function places qubits from the entanglement zone in the
   * storage zone after a rydberg gate has been performed.
   * @param previousPlacement is a reference to the previous placement of the
   * atoms.
   * @param reuseQubits is a reference to the atoms that can be reused, i.e.,
   * remain in the entanglement zone for the next two-qubit gate layer.
   * @param twoQubitGates is a list of all two-qubit gates that have been
   * executed in the previous two-qubit gate layer. Those atoms are located in
   * the entanglement zone and must be moved back to the storage zone except
   * those that are reused.
   * @param nextTwoQubitGates is a list of all two-qubit gates in the next
   * two-qubit gate layer used to calculate the lookahead.
   * @return the placement of the atoms for the current single-qubit gate layer.
   */
  auto placeAtomsInStorageZone(const Placement& previousPlacement,
                               const std::unordered_set<qc::Qubit>& reuseQubits,
                               const TwoQubitGateLayer& twoQubitGates,
                               const TwoQubitGateLayer& nextTwoQubitGates)
      -> Placement;

  /**
   * @param nGates is the number of gates to be placed
   * @param node is the node to be checked
   * @return true if the given node is a goal node
   */
  [[nodiscard]] static auto isGoal(size_t nGates, const GateNode& node) -> bool;
  /**
   * @param nAtoms is the number of atoms to be placed
   * @param node is the node to be checked
   * @return true if the given node is a goal node
   */
  [[nodiscard]] static auto isGoal(size_t nAtoms, const AtomNode& node) -> bool;

  /**
   * @brief Returns the cost of a node, i.e., the total cost to reach that node
   * from the start node.
   * @details The cost of a node is the sum of the distances of all atoms to
   * their target sites. Additionally, the cost of the lookahead is added to the
   * total cost.
   * @param node is the node to be checked
   * @return the cost of the node
   */
  [[nodiscard]] static auto getCost(const GateNode& node) -> float;
  /**
   * @brief Returns the cost of a node, i.e., the total cost to reach that node
   * from the start node.
   * @details The cost of a node is the sum of the distances of all atoms to
   * their target sites. Additionally, the cost of the lookahead is added to the
   * total cost.
   * @param node is the node to be checked
   * @return the cost of the node
   */
  [[nodiscard]] static auto getCost(const AtomNode& node) -> float;

  /**
   * @brief Calculates the standard deviation of the differences value - key
   * and sums them up over all horizontal and vertical groups.
   * @details To compensate for different sizing of the source and target area,
   * the keys are scaled by the respective scale factors. E.g., for the
   * horizontal group, if the target area features a wider distances of the
   * sites than the source area, then, a respective scale factor smaller than 1
   * should be used. The goal is that if the standard deviation is 0, all atoms
   * are moved without changing their relative distances.
   * @param scaleFactors are the scale factors for the horizontal and vertical
   * groups
   * @param groups are the groups to be considered
   * @return the sum of standard deviations
   */
  [[nodiscard]] static auto
  sumStdDeviationForGroups(const std::array<float, 2>& scaleFactors,
                           const std::vector<CompatibilityGroup>& groups)
      -> float;

  /**
   * @brief Return the estimated cost still required to reach a goal node.
   * @param atomJobs are the atoms to be placed
   * @param deepeningFactor is the factor to adjust the influence of the
   * standard deviation term
   * @param deepeningValue is the value added to the sum of standard deviations
   * @param scaleFactors are the scale factors for the horizontal and vertical
   * groups
   * @param node is the node to be checked
   * @return the heuristic cost
   */
  [[nodiscard]] static auto
  getHeuristic(const std::vector<AtomJob>& atomJobs, float deepeningFactor,
               float deepeningValue, const std::array<float, 2>& scaleFactors,
               const AtomNode& node) -> float;

  /**
   * @brief Return the estimated cost still required to reach a goal node.
   * @param gateJobs are the gates to be placed
   * @param deepeningFactor is the factor to adjust the influence of the
   * standard deviation term
   * @param deepeningValue is the value added to the sum of standard deviations
   * @param scaleFactors are the scale factors for the horizontal and vertical
   * groups
   * @param node is the node to be checked
   * @return the heuristic cost
   */
  [[nodiscard]] static auto
  getHeuristic(const std::vector<GateJob>& gateJobs, float deepeningFactor,
               float deepeningValue, const std::array<float, 2>& scaleFactors,
               const GateNode& node) -> float;

  /**
   * @brief Return references to all neighbors of the given node.
   * @details When calling this function, the neighbors are allocated
   * permanently such that (1) the returned references remain valid when the
   * execution returned from this function and (2) not all nodes in the tree
   * have to be created before they are needed. Hence, nodes are only created on
   * demand in this function. Consequently, this function must only be called
   * once per node. Otherwise, neighbors for the same node are created twice.
   * @par
   * When creating a new node, the horizontal and vertical groups are checked
   * whether the new corresponding placement is compatible with any of the
   * existing groups. If yes, the new placement is added to the respective group
   * and otherwise, a new group is formed with the new placement.
   * @param atomJobs are the atoms to be placed.
   * @param node is the node to be expanded.
   * @return a list of references to the neighbors of the given node
   */
  [[nodiscard]] static auto
  getNeighbors(const std::vector<AtomJob>& atomJobs,
               const std::shared_ptr<const AtomNode>& node)
      -> std::vector<std::shared_ptr<const AtomNode>>;

  /**
   * @brief Return references to all neighbors of the given node.
   * @details When calling this function, the neighbors are allocated
   * permanently such that (1) the returned references remain valid when the
   * execution returned from this function and (2) not all nodes in the tree
   * have to be created before they are needed. Hence, nodes are only created on
   * demand in this function. Consequently, this function must only be called
   * once per node. Otherwise, neighbors for the same node are created twice.
   * @par
   * When creating a new node, the horizontal and vertical groups are checked
   * whether the new corresponding placement is compatible with any of the
   * existing groups. If yes, the new placement is added to the respective group
   * and otherwise, a new group is formed with the new placement.
   * @param gateJobs are the gates to be placed.
   * @param node is the node to be expanded.
   * @return a list of references to the neighbors of the given node
   */
  [[nodiscard]] static auto
  getNeighbors(const std::vector<GateJob>& gateJobs,
               const std::shared_ptr<const GateNode>& node)
      -> std::vector<std::shared_ptr<const GateNode>>;

  /**
   * Checks the compatibility with a new assignment, i.e., a key-value pair,
   * whether it is compatible with an existing group. The group can either be
   * a horizontal or vertical group. In case the new assignment is compatible
   * with the group, an iterator is returned pointing to the assignment in the
   * group if it already exists or to the element directly following the
   * new key. Additionally, a boolean is returned indicating whether the new
   * exists in the group.
   * @param key the key of the new assignment
   * @param value the value of the new assignment
   * @param group the group to which the new assignment is compared
   * @return an optional pair of an iterator pointing to the key equal or
   * directly following the new key in the group and a boolean indicating
   * whether the new assignment exists in the group. If the new assignment is
   * not compatible with the group, an empty optional is returned.
   */
  [[nodiscard]] static auto
  checkCompatibilityWithGroup(uint8_t key, uint8_t value,
                              const std::map<uint8_t, uint8_t>& group)
      -> std::optional<
          std::pair<std::map<uint8_t, uint8_t>::const_iterator, bool>>;

  /**
   * Checks for the new placement of the atom whether it is compatible with
   * one of the existing groups. If yes, the new placement is added to the
   * respective group. Otherwise, a new group is formed with the new placement.
   * @param hKey the horizontal start index of the new placement
   * @param hValue the horizontal target index of the new placement
   * @param vKey the vertical start index of the new placement
   * @param vValue the vertical target index of the new placement
   * @param distance the distance the atom must travel to reach the target site
   * @param groups the groups to which the new placement can be added
   * @param maxDistances the maximum distances of placed atoms in each group
   * @return true if the new placement could be added to an existing group
   */
  static auto
  checkCompatibilityAndAddPlacement(uint8_t hKey, uint8_t hValue, uint8_t vKey,
                                    uint8_t vValue, float distance,
                                    std::vector<CompatibilityGroup>& groups,
                                    std::vector<float>& maxDistances) -> bool;

  /**
   * @brief This function creates a new GateJob for the given parameters and
   * initializes it accordingly.
   * @param discreteTargetRows is a map from concrete rows to their discrete
   * indices
   * @param discreteTargetColumns is a map from concrete columns to their
   * discrete indices
   * @param leftSLM is the SLM of the left atom
   * @param leftRow is the row of the left atom
   * @param leftCol is the column of the left atom
   * @param rightSLM is the SLM of the right atom
   * @param rightRow is the row of the right atom
   * @param rightCol is the column of the right atom
   * @param nearestSLM is the SLM of the nearest atom
   * @param r is the row of the nearest atom
   * @param c is the column of the nearest atom
   * @param job is the GateJob to be initialized
   */
  auto addGateOption(const RowColumnMap<uint8_t>& discreteTargetRows,
                     const RowColumnMap<uint8_t>& discreteTargetColumns,
                     const SLM& leftSLM, size_t leftRow, size_t leftCol,
                     const SLM& rightSLM, size_t rightRow, size_t rightCol,
                     const SLM& nearestSLM, size_t r, size_t c,
                     GateJob& job) const -> void;
};
NLOHMANN_JSON_SERIALIZE_ENUM(HeuristicPlacer::Config::Method,
                             {
                                 {HeuristicPlacer::Config::Method::ASTAR,
                                  "astar"},
                                 {HeuristicPlacer::Config::Method::IDS, "ids"},
                             })
} // namespace na::zoned
