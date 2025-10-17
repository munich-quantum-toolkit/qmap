/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
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
#include <stack>
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
 * @brief The A* placer is a class that provides a method to determine the
 * placement of the atoms in each layer using the A* search algorithm.
 */
class DFSPlacer : public PlacerBase {
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
  /// The configuration of the A* placer
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
    size_t windowMinWidth = 8;
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
    double windowShare = 0.6;
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
    float deepeningFactor = 0.8F;
    /**
     * @brief Before the sum of standard deviations is multiplied with the
     * number of unplaced nodes and @ref deepeningFactor_, this value is added
     * to the sum to amplify the influence of the unplaced nodes count.
     * @see deepeningFactor_
     */
    float deepeningValue = 0.2F;
    /**
     * @brief The cost function can consider the distance of atoms to their
     * interaction partner in the next layer.
     * @details This factor is multiplied with the distance to adjust the
     * influence of this term. Setting it to 0.0 disables the lookahead
     * entirely. A factor of 1.0 implies that the lookahead is as important as
     * the distance to the target site, which is usually not desired.
     */
    float lookaheadFactor = 0.2F;
    /**
     * @brief The reuse level corresponds to the estimated extra fidelity loss
     * due to the extra trap transfers when the atom is not reused and instead
     * moved to the storage zone and back to the entanglement zone.
     * @details It is subtracted from the cost for the reuse option to favor
     * this option over the non-reuse options.
     */
    float reuseLevel = 5.0F;
    // todo(yannick): add docstring
    size_t trials = 10;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Config, useWindow,
                                                windowMinWidth, windowRatio,
                                                windowShare, deepeningFactor,
                                                deepeningValue, lookaheadFactor,
                                                reuseLevel, trials);
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
    uint8_t level = 0;
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
    uint8_t level = 0;
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
  DFSPlacer(const Architecture& architecture, const Config& config);

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
  template <class T, class Compare = std::less<T>> class BoundedQueueStack {
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
    };
    std::vector<std::unique_ptr<Node>> minHeap_;
    std::vector<Node*> maxHeap_;
    SizeType heapCapacity_;
    std::stack<ValueType> stack_;

    auto heapifyMinHeapUp(SizeType i) -> void {
      assert(i < minHeap_.size());
      while (i > 0) {
        size_t parent = (i - 1) / 2;
        if (PriorityCompare{}(minHeap_[i]->value, minHeap_[parent]->value)) {
          std::swap(minHeap_[i], minHeap_[parent]);
          minHeap_[i]->minHeapIndex = i;
          minHeap_[parent]->minHeapIndex = parent;
          i = parent;
        } else {
          break;
        }
      }
    }

    auto heapifyMaxHeapUp(SizeType i) -> void {
      assert(i < maxHeap_.size());
      while (i > 0) {
        size_t parent = (i - 1) / 2;
        if (PriorityCompare{}(maxHeap_[parent]->value, maxHeap_[i]->value)) {
          std::swap(maxHeap_[i], maxHeap_[parent]);
          maxHeap_[i]->maxHeapIndex = i;
          maxHeap_[parent]->maxHeapIndex = parent;
          i = parent;
        } else {
          break;
        }
      }
    }

    auto heapifyMinHeapDown(SizeType i) -> void {
      while (true) {
        size_t leftChild = (2 * i) + 1;
        size_t rightChild = (2 * i) + 2;
        size_t smallest = i;

        if (leftChild < minHeap_.size() &&
            PriorityCompare{}(minHeap_[leftChild]->value,
                              minHeap_[smallest]->value)) {
          smallest = leftChild;
        }
        if (rightChild < minHeap_.size() &&
            PriorityCompare{}(minHeap_[rightChild]->value,
                              minHeap_[smallest]->value)) {
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

    auto heapifyMaxHeapDown(SizeType i) -> void {
      while (true) {
        size_t leftChild = (2 * i) + 1;
        size_t rightChild = (2 * i) + 2;
        size_t largest = i;

        if (leftChild < maxHeap_.size() &&
            PriorityCompare{}(maxHeap_[largest]->value,
                              maxHeap_[leftChild]->value)) {
          largest = leftChild;
        }
        if (rightChild < maxHeap_.size() &&
            PriorityCompare{}(maxHeap_[largest]->value,
                              maxHeap_[rightChild]->value)) {
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
    explicit BoundedQueueStack(const SizeType maxQueueSize)
        : heapCapacity_(maxQueueSize) {
      minHeap_.reserve(heapCapacity_);
      maxHeap_.reserve(heapCapacity_);
    }
    /**
     * @returns the top element of the stack.
     * @note If @ref stackEmpty returns `true`, calling this function is
     * undefined behavior.
     */
    [[nodiscard]] auto top() const -> const ValueType& {
      assert(!stack_.empty());
      return stack_.top();
    }
    [[nodiscard]] auto top() -> ValueType& {
      assert(!stack_.empty());
      return stack_.top();
    }
    /**
     * @brief Removes the top element.
     * @note If @ref stackEmpty returns `true`, calling this function is
     * undefined behavior.
     */
    auto pop() -> void {
      assert(!stack_.empty());
      stack_.pop();
    }
    /// @returns `true` if the stack is empty.
    [[nodiscard]] auto empty() const -> bool { return stack_.empty(); }
    /// @brief Inserts an element at the top.
    auto push(ValueType&& value) -> void { emplace(std::move(value)); }
    /// @brief Constructs element in-place at the top
    template <class... Args> auto emplace(Args&&... args) -> Reference {
      return stack_.emplace(std::forward<Args>(args)...);
    }
    /**
     * @brief Clears the stack and initializes with the minimal element from
     * the queue.
     * @details This function performs the following steps:
     * (1) It pops all elements from the stack until the stack is empty and
     *     pushes them to the queue respecting the maximum capacity of the
     *     queue.
     * (2) After the stack is cleared, it pops the first (minimal) element from
     *     the queue and pushes it onto the stack.
     * (3) It reduces the maximum capacity by one.
     * @see emptyQueue
     */
    auto clearAndSeedStackFromQueue() -> void {
      while (!stack_.empty()) {
        assert(minHeap_.size() == maxHeap_.size());
        if (heapCapacity_ > 0) {
          if (minHeap_.size() < heapCapacity_) {
            minHeap_.emplace_back(std::make_unique<Node>(
                minHeap_.size(), maxHeap_.size(), std::move(top())));
            maxHeap_.emplace_back(minHeap_.back().get());
            heapifyMinHeapUp(minHeap_.size() - 1);
            heapifyMaxHeapUp(maxHeap_.size() - 1);
          } else {
            assert(minHeap_.size() == heapCapacity_);
            // if capacity is reached, only insert the value if smaller than max
            const auto& value = top();
            if (PriorityCompare{}(value, maxHeap_.front()->value)) {
              const auto i = maxHeap_.front()->minHeapIndex;
              assert(i < minHeap_.size());
              minHeap_[i] = std::make_unique<Node>(i, 0, value);
              maxHeap_.front() = minHeap_[i].get();
              heapifyMinHeapUp(i);
              heapifyMaxHeapDown(0);
            }
          }
        }
        pop();
      }
      if (!minHeap_.empty()) {
        assert(minHeap_.size() == maxHeap_.size());
        assert(minHeap_.size() <= heapCapacity_);
        push(std::move(minHeap_.front()->value));
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
            heapifyMaxHeapDown(i);
          }
        }
      }
      if (heapCapacity_ > 0) {
        --heapCapacity_;
      }
      assert(minHeap_.size() == maxHeap_.size());
      assert(minHeap_.size() <= heapCapacity_);
    }
  };

  // todo(yannick): add docstring
  template <class Node>
  [[nodiscard]] static auto dfsTreeSearch(std::shared_ptr<const Node> start,
                const std::function<std::vector<std::shared_ptr<const Node>>(
                    std::shared_ptr<const Node>)>& getNeighbors,
                const std::function<bool(const Node&)>& isGoal,
                const std::function<double(const Node&)>& getCost,
                const std::function<double(const Node&)>& getHeuristic,
                size_t trials) -> std::shared_ptr<const Node>;

  /**
   * @brief This function takes a list of atoms together with their current
   * placement and returns two maps from concrete columns and rows to their
   * discrete indices.
   * @param placement is a list of atoms together with their current placement
   * @param atoms is a list of all atoms that must be placed
   * @return a pair of two maps, the first one maps rows to their discrete
   * indices and the second one maps columns to their discrete indices
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
   * @param nodes is the list of all nodes created so far with permanent memory
   * allocation
   * @param atomJobs are the atoms to be placed
   * @param node is the node to be expanded
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
   * @param nodes is the list of all nodes created so far with permanent memory
   * allocation
   * @param gateJobs are the gates to be placed
   * @param node is the node to be expanded
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
   * group, if it already exists or to the element directly following the
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
} // namespace na::zoned
