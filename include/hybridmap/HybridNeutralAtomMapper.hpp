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

#include "circuit_optimizer/CircuitOptimizer.hpp"
#include "hybridmap/HardwareQubits.hpp"
#include "hybridmap/Mapping.hpp"
#include "hybridmap/NeutralAtomArchitecture.hpp"
#include "hybridmap/NeutralAtomDefinitions.hpp"
#include "hybridmap/NeutralAtomLayer.hpp"
#include "hybridmap/NeutralAtomScheduler.hpp"
#include "hybridmap/NeutralAtomUtils.hpp"
#include "ir/Definitions.hpp"
#include "ir/QuantumComputation.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace na {

/**
 * @brief Runtime configuration parameters for the neutral atom mapper.
 * @details Tunable weights and limits guiding swap vs. shuttling vs. ancilla
 * decisions, lookahead, and stochastic initialization.
 */
struct MapperParameters {
  uint32_t lookaheadDepth = 1;
  qc::fp lookaheadWeightSwaps = 0.1;
  qc::fp lookaheadWeightMoves = 0.1;
  qc::fp decay = 0.1;
  qc::fp shuttlingTimeWeight = 0.1;
  qc::fp dynamicMappingWeight = 1;
  qc::fp gateWeight = 1;
  qc::fp shuttlingWeight = 1;
  uint32_t seed = 42;
  uint32_t numFlyingAncillas = 0;
  uint32_t limitShuttlingLayer = 10;
  uint32_t maxBridgeDistance = 1;
  bool usePassBy = true;
  bool verbose = false;
  InitialCoordinateMapping initialCoordMapping = Trivial;
};

/**
 * @brief Aggregated counters collected during mapping.
 */
struct MapperStats {
  uint32_t nSwaps = 0;     ///< Number of executed SWAP gates.
  uint32_t nBridges = 0;   ///< Number of bridge operations.
  uint32_t nFAncillas = 0; ///< Number of flying ancilla usages.
  uint32_t nMoves = 0;     ///< Number of MOVE operations.
  uint32_t nPassBy = 0;    ///< Number of pass-by combinations.
};

/**
 * @brief Maps a quantum circuit onto a neutral atom architecture using hybrid
 * routing.
 * @details Combines gate-based (swap/bridge) and shuttling-based
 * (move/pass-by/flying ancilla) strategies. Workflow:
 * 1. Initialize mapping and hardware placement.
 * 2. Build front/lookahead layers using commutation rules.
 * 3. Estimate routing costs (swap vs. move vs. bridge/flying ancilla).
 * 4. Select best primitive via weighted cost functions (SABRE-inspired
 * heuristic).
 * 5. Handle multi-qubit gate positioning.
 * 6. Produce circuit with abstract SWAP and MOVE operations (later decomposed
 * to AOD level).
 */
class NeutralAtomMapper {
protected:
  MapperStats stats;
  // The considered architecture
  const NeutralAtomArchitecture* arch;
  // The mapped quantum circuit
  qc::QuantumComputation mappedQc;
  // The mapped quantum circuit converted to AOD movements
  qc::QuantumComputation mappedQcAOD;
  // The scheduler to schedule the mapped quantum circuit
  NeutralAtomScheduler scheduler;
  // Gates in the front layer to be executed with swap gates
  GateList frontLayerGate;
  // Gates in the front layer to be executed with move operations
  GateList frontLayerShuttling;
  // Gates in the lookahead layer to be executed with swap gates
  GateList lookaheadLayerGate;
  // Gates in the lookahead layer to be executed with move operations
  GateList lookaheadLayerShuttling;
  // The minimal weight for any multi-qubit gate
  qc::fp twoQubitSwapWeight = 1;
  // The runtime parameters of the mapper
  MapperParameters parameters;
  // The qubits that are blocked by the last swap
  std::deque<std::set<HwQubit>> lastBlockedQubits;
  // The last swap that has been executed
  Swap lastSwap = {0, 0};
  // The last moves that have been executed
  std::deque<AtomMove> lastMoves;
  // Precomputed decay weights
  std::vector<qc::fp> decayWeights;
  // indicates if multi-qubit gates are in the circuit
  bool multiQubitGates = false;

  // The current placement of the hardware qubits onto the coordinates
  HardwareQubits hardwareQubits;
  HardwareQubits flyingAncillas;
  // The current mapping between circuit qubits and hardware qubits
  Mapping mapping;

  // Methods for mapping

  /**
   * @brief Append a single operation to the mapped circuit applying current
   * qubit mapping.
   * @param op Operation (from original circuit) to map.
   */
  void mapGate(const qc::Operation* op);
  /**
   * @brief Iteratively map all executable gates until front layer stalls.
   * @param frontLayer Current front layer container.
   * @param lookaheadLayer Lookahead layer container.
   */
  void mapAllPossibleGates(NeutralAtomLayer& frontLayer,
                           NeutralAtomLayer& lookaheadLayer);
  /**
   * @brief Filter a list to gates executable under current mapping.
   * @param gates Candidate gates.
   * @return Subset of executable gates.
   */
  GateList getExecutableGates(const GateList& gates);
  /**
   * @brief Check gate executability given current mapping and placement.
   * @param opPointer Gate to test.
   * @return True if gate can be applied now; false otherwise.
   */
  bool isExecutable(const qc::Operation* opPointer);

  void updateBlockedQubits(const Swap& swap) {
    const HwQubits qubits = {swap.first, swap.second};
    updateBlockedQubits(qubits);
  }
  void updateBlockedQubits(const HwQubits& qubits);

  /**
   * @brief Apply a SWAP to update logical↔hardware mapping state.
   * @param swap Hardware qubit pair.
   */
  void applySwap(const Swap& swap);
  /**
   * @brief Apply a MOVE (shuttling) operation updating placement & mapping.
   * @param move Move descriptor.
   */
  void applyMove(AtomMove move);

  void applyBridge(NeutralAtomLayer& frontLayer, const Bridge& bridge);
  void applyFlyingAncilla(NeutralAtomLayer& frontLayer,
                          const FlyingAncillaComb& faComb);
  void applyPassBy(NeutralAtomLayer& frontLayer, const PassByComb& pbComb);

  // Methods for gate vs. shuttling
  /**
   * @brief Partition gates into gate-routing vs. shuttling lists for each
   * layer.
   * @param frontGates Gates in current front layer.
   * @param lookaheadGates Gates in lookahead layer.
   */
  void reassignGatesToLayers(const GateList& frontGates,
                             const GateList& lookaheadGates);

  /**
   * @brief Advance one step using gate-based routing (swaps/bridges).
   * @param frontLayer Front layer container.
   * @param lookaheadLayer Lookahead layer container.
   * @param i Index of the considered operation in the front layer.
   * @return Number of mapped operations performed.
   */
  size_t gateBasedMapping(NeutralAtomLayer& frontLayer,
                          NeutralAtomLayer& lookaheadLayer, size_t i);
  /**
   * @brief Advance one step using shuttling primitives (moves/pass-by/flying
   * ancilla).
   * @param frontLayer Front layer container.
   * @param lookaheadLayer Lookahead layer container.
   * @param i Index of the considered operation in the front layer.
   * @return Number of mapped operations performed.
   */
  size_t shuttlingBasedMapping(NeutralAtomLayer& frontLayer,
                               NeutralAtomLayer& lookaheadLayer, size_t i);

  /**
   * @brief Estimate swaps and time required to execute a gate via swapping.
   * @param opPointer Gate under consideration.
   * @return Pair (#swaps, estimated time cost).
   */
  std::pair<uint32_t, qc::fp>
  estimateNumSwapGates(const qc::Operation* opPointer);
  /**
   * @brief Estimate MOVE count and time for executing a gate via shuttling.
   * @param opPointer Gate under consideration.
   * @return Pair (#moves, estimated time cost).
   */
  std::pair<uint32_t, qc::fp>
  estimateNumMove(const qc::Operation* opPointer) const;
  /**
   * @brief Compare swap vs. move estimates to choose routing primitive.
   * @param opPointer Target gate.
   * @return True if swap-based routing chosen; false for move-based.
   */
  bool swapGateBetter(const qc::Operation* opPointer);

  // Methods for swap gates mapping
  /**
   * @brief Select best swap minimizing composite cost (distance + decay).
   * @param lastSwapUsed Previously applied swap (for decay context).
   * @return Best swap candidate.
   */
  Swap findBestSwap(const Swap& lastSwapUsed);
  /**
   * @brief Enumerate candidate swaps derived from front layer proximity.
   * @param swapsFront Pair of (close-by swaps, weighted exact swaps).
   * @return Set of swap candidates.
   */
  [[nodiscard]] std::set<Swap>
  getAllPossibleSwaps(const std::pair<Swaps, WeightedSwaps>& swapsFront) const;

  // Methods for bridge operations mapping

  /**
   * @brief Choose best bridge operation given current best swap.
   * @param bestSwap Candidate swap used for context.
   * @return Selected bridge descriptor.
   */
  [[nodiscard]] Bridge findBestBridge(const Swap& bestSwap);
  /**
   * @brief Compute shortest bridge circuits compatible with a swap candidate.
   * @param bestSwap Swap context.
   * @return List of shortest bridges.
   */
  [[nodiscard]] Bridges getShortestBridges(const Swap& bestSwap);

  /**
   * @brief Current coordinate usage (occupied indices) snapshot.
   * @return Set of occupied coordinate indices.
   */
  [[nodiscard]] CoordIndices computeCurrentCoordUsages() const;

  /**
   * @brief Convert a MOVE combination to a flying ancilla combination if
   * suitable.
   * @param moveComb MOVE combination candidate.
   * @return Equivalent flying-ancilla combination.
   */
  [[nodiscard]] FlyingAncillaComb
  convertMoveCombToFlyingAncillaComb(const MoveComb& moveComb) const;
  /**
   * @brief Convert a MOVE combination to a pass-by combination if suitable.
   * @param moveComb MOVE combination candidate.
   * @return Equivalent pass-by combination.
   */
  [[nodiscard]] PassByComb
  convertMoveCombToPassByComb(const MoveComb& moveComb) const;

  // Methods for shuttling operations mapping
  /**
   * @brief Select best MOVE combination using cost (distance reduction +
   * parallelization).
   * @return Best move combination descriptor.
   */
  MoveComb findBestAtomMove();
  // std::pair<MoveComb, MoveInfo> findBestAtomMoveWithOp();
  /**
   * @brief Generate all minimal MOVE combinations (direct/away/multi-qubit).
   * @return List of viable combinations.
   */
  MoveCombs getAllMoveCombinations();
  /**
   * @brief Enumerate staged move-away then move-to-target combinations.
   * @param startCoord Origin coordinate of final move.
   * @param targetCoord Destination coordinate.
   * @param excludedCoords Coordinates disallowed for interim moves.
   * @return Candidate move-away sequences.
   */
  [[nodiscard]] MoveCombs
  getMoveAwayCombinations(CoordIndex startCoord, CoordIndex targetCoord,
                          const CoordIndices& excludedCoords) const;

  // Methods for flying ancilla operations mapping
  /**
   * @brief Compare swap vs. bridge and select routing method.
   * @param bestSwap Swap candidate.
   * @param bestBridge Bridge candidate.
   * @return Chosen mapping method.
   */
  [[nodiscard]] MappingMethod compareSwapAndBridge(const Swap& bestSwap,
                                                   const Bridge& bestBridge);
  /**
   * @brief Compare shuttling vs. flying ancilla vs. pass-by for a move
   * candidate.
   * @param bestMoveComb Best move combination.
   * @param bestFaComb Best flying ancilla combination.
   * @param bestPbComb Best pass-by combination.
   * @return Chosen mapping method.
   */
  [[nodiscard]] MappingMethod
  compareShuttlingAndFlyingAncilla(const MoveComb& bestMoveComb,
                                   const FlyingAncillaComb& bestFaComb,
                                   const PassByComb& bestPbComb) const;

  // Helper methods
  /**
   * @brief Classify swaps into close-by (2-qubit) vs. exact (multi-qubit
   * positioning).
   * @param layer Gate list (front or lookahead).
   * @return Pair (close-by swaps, weighted exact swaps).
   */
  std::pair<Swaps, WeightedSwaps> initSwaps(const GateList& layer);
  /**
   * @brief Set base two-qubit swap weight to min multi-qubit exact weight or 1.
   * @param swapExact Weighted exact swaps.
   */
  void setTwoQubitSwapWeight(const WeightedSwaps& swapExact);

  /**
   * @brief Compute best coordinate aggregation for gate qubits.
   * @param gateCoords Current coordinate indices of gate's qubits.
   * @return Selected position indices candidate set.
   */
  CoordIndices getBestMovePos(const CoordIndices& gateCoords);
  MultiQubitMovePos getMovePositionRec(MultiQubitMovePos currentPos,
                                       const CoordIndices& gateCoords,
                                       const size_t& maxNMoves);
  /**
   * @brief Enumerate move combinations relocating gate qubits to target
   * coordinates.
   * @param gateQubits Hardware qubits participating in gate.
   * @param position Target coordinate set.
   * @return List of move combination candidates.
   */
  [[nodiscard]] MoveCombs
  getMoveCombinationsToPosition(const HwQubits& gateQubits,
                                const CoordIndices& position) const;

  // Multi-qubit gate based methods
  /**
   * @brief Determine optimal convergence position for a multi-qubit gate.
   * @param opPointer Multi-qubit operation.
   * @return Hardware qubit set representing chosen position.
   */
  HwQubits getBestMultiQubitPosition(const qc::Operation* opPointer);
  /**
   * @brief Recursive helper to search for optimal multi-qubit convergence
   * position.
   * @param remainingGateQubits Remaining gate participants.
   * @param selectedQubits Already selected qubits (path state).
   * @param remainingNearbyQubits Candidate nearby qubits.
   * @return Selected hardware qubit set.
   */
  HwQubits getBestMultiQubitPositionRec(HwQubits remainingGateQubits,
                                        std::vector<HwQubit> selectedQubits,
                                        HwQubits remainingNearbyQubits);
  /**
   * @brief Compute exact swaps required to align qubits to target multi-qubit
   * position.
   * @param op Multi-qubit operation.
   * @param position Target hardware qubit set.
   * @return Weighted swap list for alignment.
   */
  WeightedSwaps getExactSwapsToPosition(const qc::Operation* op,
                                        HwQubits position);

  // Cost function calculation
  /**
   * @brief Compute distance reduction contribution for a swap.
   * @param swap Candidate swap.
   * @param swapCloseBy Close-by (2-qubit) swaps.
   * @param swapExact Weighted exact swaps (multi-qubit alignment).
   * @return Reduction metric (higher means better improvement).
   */
  qc::fp swapCostPerLayer(const Swap& swap, const Swaps& swapCloseBy,
                          const WeightedSwaps& swapExact);
  /**
   * @brief Aggregate total cost for a swap (distance reduction + decay
   * penalties).
   * @param swap Candidate swap.
   * @param swapsFront Pair (close-by, exact) for front layer.
   * @param swapsLookahead Pair (close-by, exact) for lookahead layer.
   * @return Total cost (lower preferred if negative convention, or compared
   * relatively).
   */
  qc::fp swapCost(const Swap& swap,
                  const std::pair<Swaps, WeightedSwaps>& swapsFront,
                  const std::pair<Swaps, WeightedSwaps>& swapsLookahead);
  /**
   * @brief Distance reduction from a move combination for a given layer.
   * @param moveComb Move combo.
   * @param layer Target layer.
   * @return Distance reduction score.
   */
  [[nodiscard]] qc::fp moveCombDistanceReduction(const MoveComb& moveComb,
                                                 const GateList& layer) const;
  /**
   * @brief Distance reduction from a swap for a given layer.
   * @param swap Swap candidate.
   * @param layer Target layer.
   * @return Distance reduction score.
   */
  qc::fp swapDistanceReduction(const Swap& swap, const GateList& layer);

  /**
   * @brief Compute bonus/penalty for parallelizing a move with recent moves.
   * @param moveComb Move combination candidate.
   * @return Parallelization cost component.
   */
  [[nodiscard]] qc::fp parallelMoveCost(const MoveComb& moveComb) const;
  /**
   * @brief Total cost for a move combination (distance reduction +
   * parallelization).
   * @param moveComb Candidate combination.
   * @return Aggregate cost value.
   */
  [[nodiscard]] qc::fp moveCostComb(const MoveComb& moveComb) const;

  /**
   * @brief Debug print of current front/lookahead layers.
   */
  void printLayers() const;

public:
  // Constructors
  NeutralAtomMapper() = delete;
  explicit NeutralAtomMapper(const NeutralAtomArchitecture* architecture,
                             const MapperParameters& p)
      : arch(architecture), scheduler(*architecture), parameters(p),
        hardwareQubits(*arch, arch->getNqubits() - p.numFlyingAncillas,
                       p.initialCoordMapping, p.seed),
        flyingAncillas(*arch, p.numFlyingAncillas, Trivial, p.seed) {
    const auto nPositions = static_cast<int>(arch->getNpositions());
    const auto nQubits = static_cast<int>(arch->getNqubits());
    if (nPositions - nQubits < 1 && p.shuttlingWeight > 0) {
      throw std::runtime_error(
          "No free coordinates for shuttling but shuttling "
          "weight is greater than 0.");
    }
    if (parameters.numFlyingAncillas > 1) {
      throw std::runtime_error("Only one flying ancilla is supported for now.");
    }
    //   precompute exponential decay weights
    decayWeights.reserve(arch->getNcolumns());
    for (uint32_t i = arch->getNcolumns(); i > 0; --i) {
      decayWeights.emplace_back(std::exp(-parameters.decay * i));
    }
  }
  explicit NeutralAtomMapper(const NeutralAtomArchitecture& architecture,
                             const MapperParameters& p = MapperParameters())
      : NeutralAtomMapper(&architecture, p) {}

  /**
   * @brief Set/replace runtime parameters and reset internal state.
   * @param p New parameter set.
   * @throw std::runtime_error If shuttling weight >0 but no free coordinates or
   * unsupported number of flying ancillas.
   */
  void setParameters(const MapperParameters& p) {
    parameters = p;
    const auto nPositions = static_cast<int>(arch->getNpositions());
    const auto nQubits = static_cast<int>(arch->getNqubits());
    if (nPositions - nQubits < 1 && p.shuttlingWeight > 0) {
      throw std::runtime_error(
          "No free coordinates for shuttling but shuttling "
          "weight is greater than 0.");
    }
    if (parameters.numFlyingAncillas > 1) {
      throw std::runtime_error("Only one flying ancilla is supported for now.");
    }
    reset();
  }

  /**
   * @brief Shallow copy of architecture, parameters, mapping, placement and
   * scheduler state.
   * @param mapper Source mapper.
   */
  void copyStateFrom(const NeutralAtomMapper& mapper) {
    arch = mapper.arch;
    parameters = mapper.parameters;
    mapping = mapper.mapping;
    hardwareQubits = mapper.hardwareQubits;
    lastMoves = mapper.lastMoves;
    lastBlockedQubits = mapper.lastBlockedQubits;
    scheduler = mapper.scheduler;
    decayWeights = mapper.decayWeights;
    flyingAncillas = mapper.flyingAncillas;
  }

  /**
   * @brief Reset mapping and hardware placement (reinitialize qubits &
   * ancillas).
   */
  void reset() {
    hardwareQubits =
        HardwareQubits(*arch, arch->getNqubits() - parameters.numFlyingAncillas,
                       parameters.initialCoordMapping, parameters.seed);
    flyingAncillas = HardwareQubits(*arch, parameters.numFlyingAncillas,
                                    Trivial, parameters.seed);
  }

  // Methods
  /**
   * @brief Maps the given quantum circuit to the given architecture.
   * @details The mapping has following important parts:
   * - initial mapping: The initial mapping of the circuit qubits to the
   * hardware qubits.
   * - layer creation: The creation of the front and lookahead layers, done one
   * the fly and taking into account basic commutation rules.
   * - estimation: The estimation of the number of swap gates and moves needed
   * to execute a given gate and the decision which technique is better.
   * - gate based mapping: SABRE based algorithm to choose the bast swap for the
   * given layers.
   * - shuttling based mapping: Computing and evaluation of possible moves and
   * choosing best.
   * - multi-qubit-gates: Additional steps and checks to bring multiple qubits
   * together.
   * -> Final circuit contains abstract SWAP gates and MOVE operations, which
   * need to be decomposed using convertToAod method.
   *
   * @param qc The quantum circuit to be mapped
   * @param initialMapping The initial mapping of the circuit qubits to the
   * hardware qubits
   * @return The mapped quantum circuit with abstract SWAP gates and MOVE
   * operations
   */
  qc::QuantumComputation map(qc::QuantumComputation& qc,
                             const Mapping& initialMapping) {
    stats = MapperStats();
    mappedQc = qc::QuantumComputation(arch->getNpositions());
    mappedQcAOD = qc::QuantumComputation(arch->getNpositions());
    mapAppend(qc, initialMapping);
    return mappedQc;
  }

  qc::QuantumComputation map(qc::QuantumComputation& qc,
                             const InitialMapping initialMapping) {
    const auto actualMapping =
        Mapping(qc.getNqubits(), initialMapping, qc, hardwareQubits);
    return map(qc, actualMapping);
  }

  /**
   * @brief Append and map additional circuit portion using given initial
   * mapping.
   * @param qc Circuit to extend mapping with.
   * @param initialMapping Initial mapping state.
   */
  void mapAppend(qc::QuantumComputation& qc, const Mapping& initialMapping);

  /**
   * @brief Retrieve accumulated mapping statistics.
   * @return Stats struct.
   */
  [[nodiscard]] MapperStats getStats() const { return stats; }

  [[maybe_unused]] [[nodiscard]] std::unordered_map<std::string, qc::fp>
  getStatsMap() const {
    std::unordered_map<std::string, qc::fp> result;
    result["nSwaps"] = stats.nSwaps;
    result["nBridges"] = stats.nBridges;
    result["nFAncillas"] = stats.nFAncillas;
    result["nMoves"] = stats.nMoves;
    result["nPassBy"] = stats.nPassBy;
    return result;
  }

  /**
   * @brief Get current mapped circuit (abstract operations form).
   * @return Mapped circuit object.
   */
  [[nodiscard]] const qc::QuantumComputation& getMappedQc() const {
    return mappedQc;
  }

  /**
   * @brief Serialize mapped circuit (abstract operations) to extended OpenQASM.
   * @return OpenQASM string.
   */
  [[nodiscard]] [[maybe_unused]] std::string getMappedQcQasm() const {
    std::stringstream ss;
    mappedQc.dumpOpenQASM(ss, false);
    return ss.str();
  }

  /**
   * @brief Save mapped abstract circuit (SWAP/MOVE) to file in OpenQASM.
   * @param filename Output file path.
   */
  [[maybe_unused]] void saveMappedQcQasm(const std::string& filename) const {
    std::ofstream ofs(filename);
    mappedQc.dumpOpenQASM(ofs, false);
  }

  /**
   * @brief Get mapped circuit serialized at native AOD (movement + CZ) level.
   * @return OpenQASM string (AOD-native).
   */
  [[maybe_unused]] std::string getMappedQcAodQasm() {
    if (mappedQcAOD.empty()) {
      convertToAod();
    }
    std::stringstream ss;
    mappedQcAOD.dumpOpenQASM(ss, false);
    return ss.str();
  }

  /**
   * @brief Save AOD-native mapped circuit to file.
   * @param filename Output file path.
   */
  [[maybe_unused]] void saveMappedQcAodQasm(const std::string& filename) {
    if (mappedQcAOD.empty()) {
      convertToAod();
    }
    std::ofstream ofs(filename);
    if (!ofs) {
      throw std::runtime_error("Failed to open file: " + filename);
    }
    mappedQcAOD.dumpOpenQASM(ofs, false);
  }

  /**
   * @brief Schedule mapped circuit (AOD level) on architecture timeline.
   * @details Lazily converts to AOD if needed, then computes start times with
   * blocking, timing and optional animation.
   * @param verboseArg Enable verbose scheduler output.
   * @param createAnimationCsv Generate animation CSV artifacts.
   * @param shuttlingSpeedFactor Factor to scale shuttling durations.
   * @return Scheduler results (timings, animation, metrics).
   */
  [[maybe_unused]] SchedulerResults
  schedule(const bool verboseArg = false, const bool createAnimationCsv = false,
           const qc::fp shuttlingSpeedFactor = 1.0) {
    if (mappedQcAOD.empty()) {
      convertToAod();
    }
    return scheduler.schedule(mappedQcAOD, hardwareQubits.getInitHwPos(),
                              flyingAncillas.getInitHwPos(), verboseArg,
                              createAnimationCsv, shuttlingSpeedFactor);
  }

  /**
   * @brief Retrieve animation CSV content produced by scheduler.
   * @return CSV string.
   */
  [[maybe_unused]] [[nodiscard]] std::string getAnimationViz() const {
    return scheduler.getAnimationViz();
  }

  /**
   * @brief Persist animation CSV assets to disk.
   * @param filename Base filename for output.
   */
  [[maybe_unused]] void saveAnimationFiles(const std::string& filename) const {
    scheduler.saveAnimationFiles(filename);
  }

  void decomposeBridgeGates(qc::QuantumComputation& qc) const;

  /**
   * @brief Convert abstract mapped circuit to native AOD (movement & CZ) level.
   * @details Decompose SWAP to CX sequence, multi-controlled X to CZ form,
   * merge consecutive MOVE operations, and translate to AOD-native
   * instructions.
   * @return Converted quantum computation object.
   */
  qc::QuantumComputation convertToAod();

  /**
   * @brief Initial hardware placement map (delegated from HardwareQubits).
   * @return Map: hardware qubit -> initial coordinate index.
   */
  [[maybe_unused]] [[nodiscard]] std::map<HwQubit, HwQubit>
  getInitHwPos() const {
    return hardwareQubits.getInitHwPos();
  }
};

} // namespace na
