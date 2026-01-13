/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "hybridmap/HybridNeutralAtomMapper.hpp"
#include "hybridmap/HybridSynthesisMapper.hpp"
#include "hybridmap/NeutralAtomArchitecture.hpp"
#include "hybridmap/NeutralAtomScheduler.hpp"
#include "hybridmap/NeutralAtomUtils.hpp"
#include "qasm3/Importer.hpp"

#include <cstddef>
#include <cstdint>
#include <nanobind/nanobind.h>
#include <nanobind/stl/map.h>           // NOLINT(misc-include-cleaner)
#include <nanobind/stl/set.h>           // NOLINT(misc-include-cleaner)
#include <nanobind/stl/string.h>        // NOLINT(misc-include-cleaner)
#include <nanobind/stl/unordered_map.h> // NOLINT(misc-include-cleaner)
#include <nanobind/stl/vector.h>        // NOLINT(misc-include-cleaner)
#include <string>
#include <vector>

namespace nb = nanobind;
using namespace nb::literals;

// NOLINTNEXTLINE(performance-unnecessary-value-param)
NB_MODULE(MQT_QMAP_MODULE_NAME, m) {
  nb::module_::import_("mqt.core.ir");

  // Neutral Atom Hybrid Mapper
  nb::enum_<na::InitialCoordinateMapping>(
      m, "InitialCoordinateMapping",
      "Initial mapping between hardware qubits hardware coordinates.")
      .value("trivial", na::InitialCoordinateMapping::Trivial,
             "Trivial identity mapping.")
      .value("random", na::InitialCoordinateMapping::Random, "Random mapping.");

  nb::enum_<na::InitialMapping>(
      m, "InitialCircuitMapping",
      "Initial mapping between circuit qubits and hardware qubits.")
      .value("identity", na::InitialMapping::Identity, "Identity mapping.")
      .value("graph", na::InitialMapping::Graph, "Graph matching mapping.");

  nb::class_<na::MapperParameters>(
      m, "MapperParameters", "Parameters controlling the mapper behavior.")
      .def(nb::init<>(),
           "Create a MapperParameters instance with default values.")
      .def_rw("lookahead_depth", &na::MapperParameters::lookaheadDepth,
              "Depth of lookahead for mapping decisions.")
      .def_rw("lookahead_weight_swaps",
              &na::MapperParameters::lookaheadWeightSwaps,
              "Weight assigned to swap operations during lookahead.")
      .def_rw("lookahead_weight_moves",
              &na::MapperParameters::lookaheadWeightMoves,
              "Weight assigned to move operations during lookahead.")
      .def_rw("decay", &na::MapperParameters::decay,
              "Decay factor for gate blocking.")
      .def_rw("shuttling_time_weight",
              &na::MapperParameters::shuttlingTimeWeight,
              "Weight for shuttling time in cost evaluation.")
      .def_rw(
          "dynamic_mapping_weight", &na::MapperParameters::dynamicMappingWeight,
          "Weight for dynamic remapping (SWAPs or MOVEs) in cost evaluation.")
      .def_rw("gate_weight", &na::MapperParameters::gateWeight,
              "Weight for gate execution in cost evaluation.")
      .def_rw("shuttling_weight", &na::MapperParameters::shuttlingWeight,
              "Weight for shuttling operations in cost evaluation.")
      .def_rw("seed", &na::MapperParameters::seed,
              "Random seed for stochastic decisions (initial mapping, etc.).")
      .def_rw("num_flying_ancillas", &na::MapperParameters::numFlyingAncillas,
              "Number of ancilla qubits to be used (0 or 1 for now).")
      .def_rw("limit_shuttling_layer",
              &na::MapperParameters::limitShuttlingLayer,
              "Maximum allowed shuttling layer (default: 10).")
      .def_rw("max_bridge_distance", &na::MapperParameters::maxBridgeDistance,
              "Maximum distance for bridge operations.")
      .def_rw("use_pass_by", &na::MapperParameters::usePassBy,
              "Enable or disable pass-by operations.")
      .def_rw("verbose", &na::MapperParameters::verbose,
              "Enable verbose logging for debugging.")
      .def_rw("initial_coord_mapping",
              &na::MapperParameters::initialCoordMapping,
              "Strategy for initial coordinate mapping.");

  nb::class_<na::MapperStats>(m, "MapperStats")
      .def(nb::init<>())
      .def_rw("num_swaps", &na::MapperStats::nSwaps,
              "Number of swap operations performed.")
      .def_rw("num_bridges", &na::MapperStats::nBridges,
              "Number of bridge operations performed.")
      .def_rw("num_f_ancillas", &na::MapperStats::nFAncillas,
              "Number of fresh ancilla qubits used.")
      .def_rw("num_moves", &na::MapperStats::nMoves,
              "Number of move operations performed.")
      .def_rw("num_pass_by", &na::MapperStats::nPassBy,
              "Number of pass-by operations performed.");

  nb::class_<na::NeutralAtomArchitecture>(m, "NeutralAtomHybridArchitecture")
      .def(nb::init<const std::string&>(), "filename"_a)
      .def("load_json", &na::NeutralAtomArchitecture::loadJson,
           "json_filename"_a)
      .def_rw("name", &na::NeutralAtomArchitecture::name,
              "Name of the architecture.")
      .def_prop_ro("num_rows", &na::NeutralAtomArchitecture::getNrows,
                   "Number of rows in a rectangular grid SLM arrangement.")
      .def_prop_ro("num_columns", &na::NeutralAtomArchitecture::getNcolumns,
                   "Number of columns in a rectangular grid SLM arrangement.")
      .def_prop_ro(
          "num_positions", &na::NeutralAtomArchitecture::getNpositions,
          "Total number of positions in a rectangular grid SLM arrangement.")
      .def_prop_ro("num_aods", &na::NeutralAtomArchitecture::getNAods,
                   "Number of independent 2D acousto-optic deflectors.")
      .def_prop_ro("num_qubits", &na::NeutralAtomArchitecture::getNqubits,
                   "Number of atoms in the neutral atom quantum "
                   "computer that can be used as qubits.")
      .def_prop_ro("inter_qubit_distance",
                   &na::NeutralAtomArchitecture::getInterQubitDistance,
                   "Distance between SLM traps in micrometers.")
      .def_prop_ro("interaction_radius",
                   &na::NeutralAtomArchitecture::getInteractionRadius,
                   "Interaction radius in inter-qubit distances.")
      .def_prop_ro("blocking_factor",
                   &na::NeutralAtomArchitecture::getBlockingFactor,
                   "Blocking factor for parallel Rydberg gates.")
      .def_prop_ro("naod_intermediate_levels",
                   &na::NeutralAtomArchitecture::getNAodIntermediateLevels,
                   "Number of possible AOD positions between two SLM traps.")
      .def_prop_ro("decoherence_time",
                   &na::NeutralAtomArchitecture::getDecoherenceTime,
                   "Decoherence time in microseconds.")
      .def("compute_swap_distance",
           static_cast<int (na::NeutralAtomArchitecture::*)(
               std::uint32_t, std::uint32_t) const>(
               &na::NeutralAtomArchitecture::getSwapDistance),
           "Number of SWAP gates required between two positions.",
           nb::arg("idx1"), nb::arg("idx2"))
      .def("get_gate_time", &na::NeutralAtomArchitecture::getGateTime,
           "Execution time of certain gate in microseconds.", "s"_a)
      .def("get_gate_average_fidelity",
           &na::NeutralAtomArchitecture::getGateAverageFidelity,
           "Average gate fidelity from [0,1].", "s"_a)
      .def("get_nearby_coordinates",
           &na::NeutralAtomArchitecture::getNearbyCoordinates,
           "Positions that are within the interaction radius of the passed "
           "position.",
           "idx"_a);

  nb::class_<na::NeutralAtomMapper>(
      m, "HybridNAMapper",
      "Neutral Atom Hybrid Mapper that can use both SWAP gates and AOD "
      "movements to map a quantum circuit to a neutral atom quantum "
      "computer.")
      .def(
          nb::init<const na::NeutralAtomArchitecture&, na::MapperParameters&>(),
          "Create Hybrid NA Mapper with mapper parameters.",
          nb::keep_alive<1, 2>(), nb::keep_alive<1, 3>(), "arch"_a, "params"_a)
      .def("set_parameters", &na::NeutralAtomMapper::setParameters,
           "Set the parameters for the Hybrid NA Mapper.", "params"_a,
           nb::keep_alive<1, 2>())
      .def("get_init_hw_pos", &na::NeutralAtomMapper::getInitHwPos,
           "Get the initial hardware positions, required to create an "
           "animation.")
      .def(
          "map",
          [](na::NeutralAtomMapper& mapper, qc::QuantumComputation& circ,
             const na::InitialMapping initialMapping) {
            mapper.map(circ, initialMapping);
          },
          "Map a quantum circuit object to the neutral atom quantum computer.",
          "qc"_a, "initial_mapping"_a = na::InitialMapping::Identity)
      .def(
          "map_qasm_file",
          [](na::NeutralAtomMapper& mapper, const std::string& filename,
             const na::InitialMapping initialMapping) {
            auto circ = qasm3::Importer::importf(filename);
            mapper.map(circ, initialMapping);
          },
          "Map a quantum circuit to the neutral atom quantum computer.",
          "filename"_a, "initial_mapping"_a = na::InitialMapping::Identity)
      .def("get_stats", &na::NeutralAtomMapper::getStatsMap,
           "Returns the statistics of the mapping.")
      .def("get_mapped_qc_qasm", &na::NeutralAtomMapper::getMappedQcQasm,
           "Returns the mapped circuit as an extended qasm2 string.")
      .def("get_mapped_qc_aod_qasm", &na::NeutralAtomMapper::getMappedQcAodQasm,
           "Returns the mapped circuit with AOD operations as an extended "
           "qasm2 string.")
      .def("save_mapped_qc_aod_qasm",
           &na::NeutralAtomMapper::saveMappedQcAodQasm,
           "Saves the mapped circuit with AOD operations as an extended qasm2 "
           "string.",
           "filename"_a)
      .def(
          "schedule",
          [](na::NeutralAtomMapper& mapper, const bool verbose,
             const bool createAnimationCsv, const double shuttlingSpeedFactor) {
            auto results = mapper.schedule(verbose, createAnimationCsv,
                                           shuttlingSpeedFactor);
            return results.toMap();
          },
          "Schedule the mapped circuit.", "verbose"_a = false,
          "create_animation_csv"_a = false, "shuttling_speed_factor"_a = 1.0)
      .def("save_animation_files", &na::NeutralAtomMapper::saveAnimationFiles,
           "Saves the animation files (.naviz and .namachine) for the "
           "scheduling.",
           "filename"_a)
      .def("get_animation_viz", &na::NeutralAtomMapper::getAnimationViz,
           "Returns the .naviz event-log content for the last scheduling.");

  nb::class_<na::HybridSynthesisMapper>(
      m, "HybridSynthesisMapper",
      "Neutral Atom Mapper that can evaluate different synthesis steps "
      "to choose the best one.")
      .def(nb::init<const na::NeutralAtomArchitecture&,
                    const na::MapperParameters&, uint32_t>(),
           "Create Hybrid Synthesis Mapper with mapper parameters.",
           nb::keep_alive<1, 2>(), nb::keep_alive<1, 3>(), "arch"_a,
           "params"_a = na::MapperParameters(), "buffer_size"_a = 0)
      .def("set_parameters", &na::HybridSynthesisMapper::setParameters,
           "Set the parameters for the Hybrid Synthesis Mapper.", "params"_a,
           nb::keep_alive<1, 2>())
      .def("init_mapping", &na::HybridSynthesisMapper::initMapping,
           "Initializes the synthesized and mapped circuits and mapping "
           "structures for the given number of qubits.",
           "n_qubits"_a)
      .def("get_mapped_qc_qasm", &na::HybridSynthesisMapper::getMappedQcQasm,
           "Returns the mapped circuit as an extended qasm2 string.")
      .def("save_mapped_qc_qasm", &na::HybridSynthesisMapper::saveMappedQcQasm,
           "Saves the mapped circuit as an extended qasm2 to a file.",
           "filename"_a)
      .def("convert_to_aod", &na::HybridSynthesisMapper::convertToAod,
           "Converts the mapped circuit to "
           "native AOD movements.")
      .def(
          "get_mapped_qc_aod_qasm",
          &na::HybridSynthesisMapper::getMappedQcAodQasm,
          "Returns the mapped circuit with native AOD movements as an extended "
          "qasm2 string.")
      .def("save_mapped_qc_aod_qasm",
           &na::HybridSynthesisMapper::saveMappedQcAodQasm,
           "Saves the mapped circuit with native AOD movements as an extended "
           "qasm2 to a "
           "file.",
           "filename"_a)
      .def("get_synthesized_qc_qasm",
           &na::HybridSynthesisMapper::getSynthesizedQcQASM,
           "Returns the synthesized circuit with all gates but not "
           "mapped to the hardware as a qasm2 string.")
      .def("save_synthesized_qc_qasm",
           &na::HybridSynthesisMapper::saveSynthesizedQc,
           "Saves the synthesized circuit with all gates but not "
           "mapped to the hardware as qasm2 to a file.",
           "filename"_a)
      .def(
          "append_with_mapping",
          [](na::HybridSynthesisMapper& mapper, qc::QuantumComputation& qc,
             bool completeRemap) {
            mapper.appendWithMapping(qc, completeRemap);
          },
          "Appends the given QuantumComputation to the synthesized "
          "QuantumComputation and maps the gates to the hardware.",
          "qc"_a, "complete_remap"_a = false)
      .def(
          "get_circuit_adjacency_matrix",
          [](const na::HybridSynthesisMapper& mapper) {
            const auto symAdjMatrix = mapper.getCircuitAdjacencyMatrix();
            const auto n = symAdjMatrix.size();
            std::vector<std::vector<int>> adjMatrix(n, std::vector<int>(n));
            for (size_t i = 0; i < n; ++i) {
              for (size_t j = 0; j < n; ++j) {
                adjMatrix[i][j] = symAdjMatrix(i, j);
              }
            }
            return adjMatrix;
          },
          "Returns the current circuit-qubit adjacency matrix used for "
          "mapping.")
      .def(
          "evaluate_synthesis_steps",
          [](na::HybridSynthesisMapper& mapper,
             std::vector<qc::QuantumComputation>& qcs, bool completeRemap,
             bool alsoMap) {
            return mapper.evaluateSynthesisSteps(qcs, completeRemap, alsoMap);
          },
          "Evaluates the synthesis steps proposed by the ZX extraction. "
          "Returns a list of fidelities of the mapped synthesis steps.",
          "synthesis_steps"_a, "complete_remap"_a = false, "also_map"_a = false)
      .def(
          "complete_remap",
          [](na::HybridSynthesisMapper& mapper, bool includeBuffer) {
            mapper.completeRemap(includeBuffer);
          },
          "Remaps the QuantumComputation to the hardware.",
          "include_buffer"_a = true)
      .def(
          "schedule",
          [](na::HybridSynthesisMapper& mapper, const bool verbose,
             const bool createAnimationCsv, const double shuttlingSpeedFactor) {
            const auto results = mapper.schedule(verbose, createAnimationCsv,
                                                 shuttlingSpeedFactor);
            return results.toMap();
          },
          "Schedule the mapped circuit.", "verbose"_a = false,
          "create_animation_csv"_a = false, "shuttling_speed_factor"_a = 1.0);
}
