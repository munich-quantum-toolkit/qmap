//
// This file is part of the MQT QMAP library released under the MIT license.
// See README.md or go to https://github.com/cda-tum/qmap for more information.
//

#include "sc/DataLogger.hpp"
#include "sc/Mapper.hpp"
#include "sc/configuration/Configuration.hpp"
#include "sc/heuristic/UniquePriorityQueue.hpp"
#include "sc/utils.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <ostream>
#include <set>
#include <vector>

#pragma once

class HeuristicMapper : public Mapper {
public:
  using Mapper::Mapper; // import constructors from parent class

  static constexpr double EFFECTIVE_BRANCH_RATE_TOLERANCE = 1e-10;

  /**
   * @brief map the circuit passed at initialization to the architecture
   *
   * @param config the settings for this mapping run (controls e.g. layering
   * methods, pre- and post-optimizations, etc.)
   */
  void map(const Configuration& configuration) override;

  /**
   * @brief struct representing one node in the A* search containing info about
   * swaps, mappings and costs
   */
  struct Node {
    /** gates (pair of logical qubits) currently mapped next to each other */
    std::set<Edge> validMappedTwoQubitGates;
    /** swaps used so far to get from the initial mapping of the current layer
     * to the current mapping in this node */
    std::vector<Exchange> swaps;
    /**
     * containing the logical qubit currently mapped to each physical qubit.
     * `qubits[physical_qubit] = logical_qubit`
     *
     * The inverse of `locations`
     */
    std::vector<std::int16_t> qubits;
    /**
     * containing the logical qubit currently mapped to each physical qubit.
     * `locations[logical_qubit] = physical_qubit`
     *
     * The inverse of `qubits`
     */
    std::vector<std::int16_t> locations;
    /** current fixed cost
     *
     * non-fidelity-aware: cost of all swaps used in the node
     *
     * fidelity-aware: fidelity cost of all swaps used in the node + fidelity
     *    cost of all validly mapped gates at their current position
     */
    double costFixed = 0.;
    /** current fixed cost of reversals (only for non-fidelity-aware mapping
     * and only in goal nodes)*/
    double costFixedReversals = 0.;
    /** heuristic cost (i.e. expected difference from current cost to cost of
     * the best reachable goal node)
     */
    double costHeur = 0.;
    /** heuristic cost expected for future swaps needed in later circuit layers
     * (further layers contribute less) */
    double lookaheadPenalty = 0.;
    /** number of swaps that were shared with another considered qubit such
     * that both qubits got closer to being validly mapped*/
    std::size_t sharedSwaps = 0;
    /** depth in search tree (starting with 0 at the root) */
    std::size_t depth = 0;
    std::size_t parent = 0;
    std::size_t id = 0;
    /** true if all qubit pairs are mapped next to each other on the
     * architecture */
    bool validMapping = true;

    explicit Node(std::uint16_t nqubits, const std::size_t nodeId)
        : id(nodeId) {
      qubits.resize(nqubits, DEFAULT_POSITION);
      locations.resize(nqubits, DEFAULT_POSITION);
    };
    Node(std::size_t nodeId, std::size_t parentId,
         const std::vector<std::int16_t>& q,
         const std::vector<std::int16_t>& loc,
         const std::vector<Exchange>& sw = {},
         const std::set<Edge>& valid2QGates = {},
         const double initCostFixed = 0,
         const double initCostFixedReversals = 0,
         const std::size_t searchDepth = 0,
         const std::size_t initSharedSwaps = 0)
        : validMappedTwoQubitGates(valid2QGates), swaps(sw), qubits(q),
          locations(loc), costFixed(initCostFixed),
          costFixedReversals(initCostFixedReversals),
          sharedSwaps(initSharedSwaps), depth(searchDepth), parent(parentId),
          id(nodeId) {}

    /**
     * @brief returns costFixed + costHeur + lookaheadPenalty
     */
    [[nodiscard]] double getTotalCost() const {
      return costFixed + costFixedReversals + costHeur + lookaheadPenalty;
    }

    /**
     * @brief returns costFixed + lookaheadPenalty
     */
    [[nodiscard]] double getTotalFixedCost() const {
      return costFixed + costFixedReversals + lookaheadPenalty;
    }

    std::ostream& print(std::ostream& out) const {
      out << "{\n";
      out << "\t\"valid_mapping\": " << validMapping << ",\n";
      out << "\t\"cost\": {\n";
      out << "\t\t\"fixed\": " << costFixed << ",\n";
      out << "\t\t\"heuristic\": " << costHeur << ",\n";
      out << "\t\t\"lookahead_penalty\": " << lookaheadPenalty << "\n";
      out << "\t},\n";
      out << "\t\"swaps\": ";
      for (const auto& swap : swaps) {
        out << "(" << swap.first << " " << swap.second << ") ";
      }
      out << "\n}\n";
      return out;
    }
  };

protected:
  UniquePriorityQueue<Node> nodes{};
  std::unique_ptr<DataLogger> dataLogger;
  std::size_t nextNodeId = 0;
  bool principallyAdmissibleHeur = true;
  bool tightHeur = true;
  bool fidelityAwareHeur = false;

  /**
   * @brief check the `results.config` for any invalid settings
   */
  virtual void checkParameters();

  /**
   * @brief creates an initial mapping of logical qubits to physical qubits with
   * different methods depending on `Mapper::results.config.initialLayout`
   */
  virtual void createInitialMapping();

  /**
   * @brief statically creates an initial mapping of logical qubits to physical
   * qubits by considering qubits that share a gate in the first layer and
   * mapping those to any free connected qubit pair in the architecture. The
   * remaining qubits are then just mapped by order of index.
   */
  virtual void staticInitialMapping();

  /**
   * @brief map the logical qubit `target` to a free physical qubit, that is
   * nearest to the physical qubit `source` is mapped to
   *
   * @param source an already mapped logical qubit, which should be mapped near
   * to `target`
   * @param target an unmapped logical qubit
   */
  virtual void mapToMinDistance(std::uint16_t source, std::uint16_t target);

  /**
   * @brief maps any yet unmapped qubits, which are acted on in a given layer,
   * to a physical qubit.
   *
   * @param twoQubitGateMultiplicity number of two qubit gates acting on pairs
   * of logical qubits in the current layer
   */
  virtual void mapUnmappedGates(std::size_t layer);

  /**
   * @brief Routes the input circuit, i.e. inserts SWAPs to meet topology
   * constraints and optimize fidelity if activated
   */
  void routeCircuit();

  /**
   * @brief Performs pseudo-routing on the input circuit, i.e. rearranges the
   * qubit layout layer by layer to meet topology constraints without actually
   * inserting SWAPs (leaves all global data unchanged except for `qubits` and
   * `locations`, which hold the final layout)
   *
   * used for iterative bidirectional routing
   *
   * @param reverse if true, the circuit is routed from the end to the beginning
   */
  void pseudoRouteCircuit(bool reverse = false);

  /**
   * @brief search for an optimal mapping/set of swaps using A*-search and the
   * heuristic specified in `HeuristicMapper::Node::updateHeuristicCost`
   *
   * uses `HeuristicMapper::nodes` as a priority queue for the A*-search,
   * assumed to be empty (or at least containing only nodes compliant with the
   * current layer in their fields `costHeur` and `validMapping`)
   *
   * @param layer index of the current circuit layer
   * @param reverse if true, the circuit is mapped from the end to the beginning
   */
  virtual Node aStarMap(std::size_t layer, bool reverse);

  /**
   * @brief Get all qubits that are acted on by a relevant gate in the given
   * layer
   *
   * @param layer the layer for which to get the considered qubits
   */
  const std::set<std::uint16_t>& getConsideredQubits(std::size_t layer) const {
    if (fidelityAwareHeur) {
      return activeQubits.at(layer);
    }
    return activeQubits2QGates.at(layer);
  }

  /**
   * @brief expand the given node by calling `expand_node_add_one_swap` for all
   * possible swaps, which creates new search nodes and adds them to
   * `HeuristicMapper::nodes`
   *
   * @param node current search node
   * @param layer index of current circuit layer
   */
  void expandNode(Node& node, std::size_t layer);

  /**
   * @brief creates a new node with a swap on the given edge and adds it to
   * `HeuristicMapper::nodes`
   *
   * @param swap edge on which to perform a swap
   * @param node current search node
   * @param layer index of current circuit layer
   */
  void expandNodeAddOneSwap(const Edge& swap, Node& node, std::size_t layer);

  /**
   * @brief applies an in-place swap of 2 virtual qubits in the given node and
   * recalculates all costs accordingly
   *
   * @param swap physical edge on which to perform a swap
   * @param layer index of current circuit layer
   * @param node search node in which to apply the swap
   */
  void applySWAP(const Edge& swap, std::size_t layer, Node& node);

  /**
   * @brief increments `node.sharedSwaps` if the given swap is shared with
   * another qubit such that both qubits get closer to being validly mapped
   *
   * @param swap the swap to check
   * @param layer index of current circuit layer
   * @param node search node in which to update `sharedSwaps`
   */
  void updateSharedSwaps(const Edge& swap, std::size_t layer, Node& node);

  /**
   * @brief recalculates the fixed cost of the node from the current mapping and
   * swaps
   *
   * @param layer index of current circuit layer
   * @param node search node for which to recalculate the fixed cost
   */
  void recalculateFixedCost(std::size_t layer, Node& node);

  /**
   * @brief recalculates the fidelity-aware fixed cost of the node from the
   * current mapping and swaps
   *
   * @param layer index of current circuit layer
   * @param node search node for which to recalculate the fixed cost
   */
  void recalculateFixedCostFidelity(std::size_t layer, Node& node);

  /**
   * @brief recalculates the gate-count-optimizing fixed cost of the node from
   * the current mapping and swaps
   *
   * @param node search node for which to recalculate the fixed cost
   */
  void recalculateFixedCostNonFidelity(Node& node);

  /**
   * @brief recalculates the gate-count-optimizing fixed cost of all reversals
   * in the current mapping of a goal node or sets it to 0 otherwise
   *
   * @param layer index of current circuit layer
   * @param node search node for which to recalculate the fixed cost
   */
  void recalculateFixedCostReversals(std::size_t layer, Node& node);

  /**
   * @brief calculates the heuristic cost of the current mapping in the node
   * for some given layer and writes it to `Node::costHeur`, additionally
   * `Node::validMapping` is set to true if all qubit pairs sharing a gate in
   * the current layer are mapped next to each other
   *
   * @param layer index of current circuit layer
   * @param node search node for which to calculate the heuristic cost
   */
  void updateHeuristicCost(std::size_t layer, Node& node);

  /**
   * @brief calculates the heuristic using `Heuristic::GateCountMaxDistance`
   *
   * @param layer index of current circuit layer
   * @param node search node for which to calculate the heuristic cost
   *
   * @return heuristic cost
   */
  double heuristicGateCountMaxDistance(std::size_t layer, Node& node);

  /**
   * @brief calculates the heuristic using `Heuristic::GateCountSumDistance`
   *
   * @param layer index of current circuit layer
   * @param node search node for which to calculate the heuristic cost
   *
   * @return heuristic cost
   */
  double heuristicGateCountSumDistance(std::size_t layer, Node& node);

  /**
   * @brief calculates the heuristic using
   * `Heuristic::GateCountSumDistanceMinusSharedSwaps`
   *
   * @param layer index of current circuit layer
   * @param node search node for which to calculate the heuristic cost
   *
   * @return heuristic cost
   */
  double heuristicGateCountSumDistanceMinusSharedSwaps(std::size_t layer,
                                                       Node& node);

  /**
   * @brief calculates the heuristic using
   * `Heuristic::GateCountMaxDistanceOrSumDistanceMinusSharedSwaps`
   *
   * @param layer index of current circuit layer
   * @param node search node for which to calculate the heuristic cost
   *
   * @return heuristic cost
   */
  double
  heuristicGateCountMaxDistanceOrSumDistanceMinusSharedSwaps(std::size_t layer,
                                                             Node& node);

  /**
   * @brief calculates the heuristic using
   * `Heuristic::FidelityBestLocation`
   *
   * @param layer index of current circuit layer
   * @param node search node for which to calculate the heuristic cost
   *
   * @return heuristic cost
   */
  double heuristicFidelityBestLocation(std::size_t layer, Node& node);

  /**
   * @brief calculates an estimation of the heuristic cost for the following
   * layers (depreciated by a constant factor growing with each layer) and
   * saves it in the node as `Node::lookaheadPenalty`
   *
   * @param layer index of current circuit layer
   * @param node search node for which to calculate lookahead penalty
   */
  void updateLookaheadPenalty(std::size_t layer, Node& node);

  /**
   * @brief calculates the lookahead penalty for one layer using
   * `LookaheadHeuristic::GateCountMaxDistance`
   *
   * @param layer index of the circuit layer for which to calculate the
   * lookahead penalty
   * @param node search node for which to calculate the heuristic cost
   *
   * @return lookahead penalty
   */
  double lookaheadGateCountMaxDistance(std::size_t layer, Node& node);

  /**
   * @brief calculates the lookahead penalty for one layer using
   * `LookaheadHeuristic::GateCountSumDistance`
   *
   * @param layer index of the circuit layer for which to calculate the
   * lookahead penalty
   * @param node search node for which to calculate the heuristic cost
   *
   * @return lookahead penalty
   */
  double lookaheadGateCountSumDistance(std::size_t layer, Node& node);

  static double computeEffectiveBranchingRate(std::size_t nodesProcessed,
                                              const std::size_t solutionDepth) {
    // N = (b*)^d + (b*)^(d-1) + ... + (b*)^2 + b* + 1
    // no closed-form solution for b*, so we use approximation via binary search
    if (solutionDepth == 0) {
      return 0.;
    }
    --nodesProcessed; // N - 1 = (b*)^d + (b*)^(d-1) + ... + (b*)^2 + b*
    double upper = std::pow(static_cast<double>(nodesProcessed),
                            1.0 / static_cast<double>(solutionDepth));
    double lower = upper / static_cast<double>(solutionDepth);
    while (upper - lower > 2 * EFFECTIVE_BRANCH_RATE_TOLERANCE) {
      const double mid = (lower + upper) / 2.0;
      double sum = 0.0;
      for (std::size_t i = 1; i <= solutionDepth; ++i) {
        sum += std::pow(mid, i);
      }
      if (sum < static_cast<double>(nodesProcessed)) {
        lower = mid;
      } else {
        upper = mid;
      }
    }
    return (lower + upper) / 2.0;
  }
};

inline bool operator<(const HeuristicMapper::Node& x,
                      const HeuristicMapper::Node& y) {
  auto itx = x.qubits.begin(); // NOLINT (readability-qualified-auto)
  auto ity = y.qubits.begin(); // NOLINT (readability-qualified-auto)
  while (itx != x.qubits.end() && ity != y.qubits.end()) {
    if (*itx != *ity) {
      return *itx < *ity;
    }
    ++itx;
    ++ity;
  }
  return false;
}

inline bool operator>(const HeuristicMapper::Node& x,
                      const HeuristicMapper::Node& y) {
  // order nodes by costFixed + costHeur + lookaheadPenalty (increasing)
  // then by validMapping (true before false)
  // then by costHeur + lookaheadPenalty (increasing),
  //          equivalent to ordering by costFixed (decreasing)
  // then by the amount of validly mapped 2q gates (decreasing)
  // then by the qubit mapping (lexicographically) as an arbitrary but
  //          consistent tie-breaker
  const auto xcost = x.getTotalCost();
  const auto ycost = y.getTotalCost();
  if (std::abs(xcost - ycost) > 1e-6) {
    return xcost > ycost;
  }

  if (x.validMapping != y.validMapping) {
    return y.validMapping;
  }

  const auto xheur = x.costHeur + x.lookaheadPenalty;
  const auto yheur = y.costHeur + y.lookaheadPenalty;
  if (std::abs(xheur - yheur) > 1e-6) {
    return xheur > yheur;
  }

  if (x.validMappedTwoQubitGates.size() != y.validMappedTwoQubitGates.size()) {
    return x.validMappedTwoQubitGates.size() <
           y.validMappedTwoQubitGates.size();
  }

  return x < y;
}
