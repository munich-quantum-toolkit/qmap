/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "ir/QuantumComputation.hpp"
#include "qasm3/Importer.hpp"
#include "sc/Architecture.hpp"
#include "sc/Mapper.hpp"
#include "sc/MappingResults.hpp"
#include "sc/configuration/AvailableArchitecture.hpp"
#include "sc/configuration/CommanderGrouping.hpp"
#include "sc/configuration/Configuration.hpp"
#include "sc/configuration/EarlyTermination.hpp"
#include "sc/configuration/Encoding.hpp"
#include "sc/configuration/Heuristic.hpp"
#include "sc/configuration/InitialLayout.hpp"
#include "sc/configuration/Layering.hpp"
#include "sc/configuration/LookaheadHeuristic.hpp"
#include "sc/configuration/Method.hpp"
#include "sc/configuration/SwapReduction.hpp"
#include "sc/exact/ExactMapper.hpp"
#include "sc/heuristic/HeuristicMapper.hpp"
#include "sc/utils.hpp"

#include <cstdint>
#include <exception>
#include <memory>
#include <nanobind/nanobind.h>
#include <nanobind/stl/pair.h>   // NOLINT(misc-include-cleaner)
#include <nanobind/stl/set.h>    // NOLINT(misc-include-cleaner)
#include <nanobind/stl/string.h> // NOLINT(misc-include-cleaner)
#include <nanobind/stl/vector.h> // NOLINT(misc-include-cleaner)
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace nb = nanobind;
using namespace nb::literals;

namespace {
// c++ binding function
std::pair<qc::QuantumComputation, MappingResults>
map(const qc::QuantumComputation& circ, Architecture& arch,
    Configuration& config) {
  std::unique_ptr<Mapper> mapper;
  try {
    if (config.method == Method::Heuristic) {
      mapper = std::make_unique<HeuristicMapper>(circ, arch);
    } else if (config.method == Method::Exact) {
      mapper = std::make_unique<ExactMapper>(circ, arch);
    }
  } catch (const std::exception& e) {
    std::stringstream ss{};
    ss << "Could not construct mapper: " << e.what();
    throw std::invalid_argument(ss.str());
  }

  try {
    mapper->map(config);
  } catch (const std::exception& e) {
    std::stringstream ss{};
    ss << "Error during mapping: " << e.what();
    throw std::invalid_argument(ss.str());
  }

  auto& results = mapper->getResults();
  auto&& qcMapped = mapper->moveMappedCircuit();

  return {std::move(qcMapped), results};
}
} // namespace

NB_MODULE(MQT_QMAP_MODULE_NAME, m) {
  nb::module_::import_("mqt.core.ir");

  // Pre-defined architecture available within QMAP
  nb::enum_<AvailableArchitecture>(m, "Arch")
      .value("IBM_QX4", AvailableArchitecture::IbmQx4,
             "5 qubit, directed bow tie layout")
      .value("IBM_QX5", AvailableArchitecture::IbmQx5,
             "16 qubit, directed ladder layout")
      .value("IBMQ_Yorktown", AvailableArchitecture::IbmqYorktown,
             "5 qubit, undirected bow tie layout")
      .value("IBMQ_London", AvailableArchitecture::IbmqLondon,
             "5 qubit, undirected T-shape layout")
      .value("IBMQ_Bogota", AvailableArchitecture::IbmqBogota,
             "5 qubit, undirected linear chain layout")
      .value("IBMQ_Casablanca", AvailableArchitecture::IbmqCasablanca,
             "7 qubit, undirected H-shape layout")
      .value("IBMQ_Tokyo", AvailableArchitecture::IbmqTokyo,
             "20 qubit, undirected brick-like layout")
      .value("Rigetti_Agave", AvailableArchitecture::RigettiAgave,
             "8 qubit, undirected ring layout")
      .value("Rigetti_Aspen", AvailableArchitecture::RigettiAspen,
             "16 qubit, undirected dumbbell layout");

  // Mapping methodology to use
  nb::enum_<Method>(m, "Method")
      .value("heuristic", Method::Heuristic)
      .value("exact", Method::Exact);

  // Initial layout strategy
  nb::enum_<InitialLayout>(m, "InitialLayout")
      .value("identity", InitialLayout::Identity)
      .value("static", InitialLayout::Static)
      .value("dynamic", InitialLayout::Dynamic);

  // Heuristic function
  nb::enum_<Heuristic>(m, "Heuristic")
      .value("gate_count_max_distance", Heuristic::GateCountMaxDistance)
      .value("gate_count_sum_distance", Heuristic::GateCountSumDistance)
      .value("gate_count_sum_distance_minus_shared_swaps",
             Heuristic::GateCountSumDistanceMinusSharedSwaps)
      .value("gate_count_max_distance_or_sum_distance_minus_shared_swaps",
             Heuristic::GateCountMaxDistanceOrSumDistanceMinusSharedSwaps)
      .value("fidelity_best_location", Heuristic::FidelityBestLocation);

  // Lookahead heuristic function
  nb::enum_<LookaheadHeuristic>(m, "LookaheadHeuristic")
      .value("none", LookaheadHeuristic::None)
      .value("gate_count_max_distance",
             LookaheadHeuristic::GateCountMaxDistance)
      .value("gate_count_sum_distance",
             LookaheadHeuristic::GateCountSumDistance);

  // Gate clustering / layering strategy
  nb::enum_<Layering>(m, "Layering")
      .value("individual_gates", Layering::IndividualGates)
      .value("disjoint_qubits", Layering::DisjointQubits)
      .value("odd_gates", Layering::OddGates)
      .value("qubit_triangle", Layering::QubitTriangle)
      .value("disjoint_2q_blocks", Layering::Disjoint2qBlocks);

  // Early termination strategy in heuristic mapper
  nb::enum_<EarlyTermination>(m, "EarlyTermination")
      .value("none", EarlyTermination::None)
      .value("expanded_nodes", EarlyTermination::ExpandedNodes)
      .value("expanded_nodes_after_first_solution",
             EarlyTermination::ExpandedNodesAfterFirstSolution)
      .value("expanded_nodes_after_current_optimal_solution",
             EarlyTermination::ExpandedNodesAfterCurrentOptimalSolution)
      .value("solution_nodes", EarlyTermination::SolutionNodes)
      .value("solution_nodes_after_current_optimal_solution",
             EarlyTermination::SolutionNodesAfterCurrentOptimalSolution);

  // Encoding settings for at-most-one and exactly-one constraints
  nb::enum_<Encoding>(m, "Encoding")
      .value("naive", Encoding::Naive)
      .value("commander", Encoding::Commander)
      .value("bimander", Encoding::Bimander);

  // Grouping settings if using the commander encoding
  nb::enum_<CommanderGrouping>(m, "CommanderGrouping")
      .value("fixed2", CommanderGrouping::Fixed2)
      .value("fixed3", CommanderGrouping::Fixed3)
      .value("halves", CommanderGrouping::Halves)
      .value("logarithm", CommanderGrouping::Logarithm);

  // Strategy for reducing the number of permutations/swaps considered in front
  // of every gate
  nb::enum_<SwapReduction>(m, "SwapReduction")
      .value("none", SwapReduction::None)
      .value("coupling_limit", SwapReduction::CouplingLimit)
      .value("custom", SwapReduction::Custom)
      .value("increasing", SwapReduction::Increasing);

  // All configuration options for QMAP
  nb::class_<Configuration>(
      m, "Configuration",
      "Configuration options for the MQT QMAP quantum circuit mapping tool")
      .def(nb::init<>())
      .def_rw("method", &Configuration::method)
      .def_rw("heuristic", &Configuration::heuristic)
      .def_rw("verbose", &Configuration::verbose)
      .def_rw("debug", &Configuration::debug)
      .def_rw("data_logging_path", &Configuration::dataLoggingPath)
      .def_rw("layering", &Configuration::layering)
      .def_rw("automatic_layer_splits", &Configuration::automaticLayerSplits)
      .def_rw("automatic_layer_splits_node_limit",
              &Configuration::automaticLayerSplitsNodeLimit)
      .def_rw("early_termination", &Configuration::earlyTermination)
      .def_rw("early_termination_limit", &Configuration::earlyTerminationLimit)
      .def_rw("initial_layout", &Configuration::initialLayout)
      .def_rw("iterative_bidirectional_routing",
              &Configuration::iterativeBidirectionalRouting)
      .def_rw("iterative_bidirectional_routing_passes",
              &Configuration::iterativeBidirectionalRoutingPasses)
      .def_rw("lookahead_heuristic", &Configuration::lookaheadHeuristic)
      .def_rw("lookaheads", &Configuration::nrLookaheads)
      .def_rw("first_lookahead_factor", &Configuration::firstLookaheadFactor)
      .def_rw("lookahead_factor", &Configuration::lookaheadFactor)
      .def_rw("timeout", &Configuration::timeout)
      .def_rw("encoding", &Configuration::encoding)
      .def_rw("commander_grouping", &Configuration::commanderGrouping)
      .def_rw("use_subsets", &Configuration::useSubsets)
      .def_rw("include_WCNF", &Configuration::includeWCNF)
      .def_rw("enable_limits", &Configuration::enableSwapLimits)
      .def_rw("swap_reduction", &Configuration::swapReduction)
      .def_rw("swap_limit", &Configuration::swapLimit)
      .def_rw("subgraph", &Configuration::subgraph)
      .def_rw("pre_mapping_optimizations",
              &Configuration::preMappingOptimizations)
      .def_rw("post_mapping_optimizations",
              &Configuration::postMappingOptimizations)
      .def_rw("add_measurements_to_mapped_circuit",
              &Configuration::addMeasurementsToMappedCircuit)
      .def_rw("add_barriers_between_layers",
              &Configuration::addBarriersBetweenLayers)
      .def("json",
           [](const Configuration& config) {
             const nb::module_ json = nb::module_::import_("json");
             const nb::object loads = json.attr("loads");
             return loads(config.json().dump());
           })
      .def("__repr__", &Configuration::toString);

  // Results of the mapping process
  nb::class_<MappingResults>(
      m, "MappingResults",
      "Results of the MQT QMAP quantum circuit mapping tool")
      .def(nb::init<>())
      .def_rw("input", &MappingResults::input)
      .def_rw("output", &MappingResults::output)
      .def_rw("configuration", &MappingResults::config)
      .def_rw("time", &MappingResults::time)
      .def_rw("timeout", &MappingResults::timeout)
      .def_rw("mapped_circuit", &MappingResults::mappedCircuit)
      .def_rw("heuristic_benchmark", &MappingResults::heuristicBenchmark)
      .def_rw("layer_heuristic_benchmark",
              &MappingResults::layerHeuristicBenchmark)
      .def_rw("wcnf", &MappingResults::wcnf)
      .def("json",
           [](const MappingResults& results) {
             const nb::module_ json = nb::module_::import_("json");
             const nb::object loads = json.attr("loads");
             return loads(results.json().dump());
           })
      .def("__repr__", &MappingResults::toString);

  // Main class for storing circuit information
  nb::class_<MappingResults::CircuitInfo>(m, "CircuitInfo",
                                          "Circuit information")
      .def(nb::init<>())
      .def_rw("name", &MappingResults::CircuitInfo::name)
      .def_rw("qubits", &MappingResults::CircuitInfo::qubits)
      .def_rw("gates", &MappingResults::CircuitInfo::gates)
      .def_rw("single_qubit_gates",
              &MappingResults::CircuitInfo::singleQubitGates)
      .def_rw("cnots", &MappingResults::CircuitInfo::cnots)
      .def_rw("layers", &MappingResults::CircuitInfo::layers)
      .def_rw("total_fidelity", &MappingResults::CircuitInfo::totalFidelity)
      .def_rw("total_log_fidelity",
              &MappingResults::CircuitInfo::totalLogFidelity)
      .def_rw("swaps", &MappingResults::CircuitInfo::swaps)
      .def_rw("direction_reverse",
              &MappingResults::CircuitInfo::directionReverse);

  // Heuristic benchmark information
  nb::class_<MappingResults::HeuristicBenchmarkInfo>(
      m, "HeuristicBenchmarkInfo", "Heuristic benchmark information")
      .def(nb::init<>())
      .def_rw("expanded_nodes",
              &MappingResults::HeuristicBenchmarkInfo::expandedNodes)
      .def_rw("generated_nodes",
              &MappingResults::HeuristicBenchmarkInfo::generatedNodes)
      .def_rw("time_per_node",
              &MappingResults::HeuristicBenchmarkInfo::secondsPerNode)
      .def_rw("average_branching_factor",
              &MappingResults::HeuristicBenchmarkInfo::averageBranchingFactor)
      .def_rw("effective_branching_factor",
              &MappingResults::HeuristicBenchmarkInfo::effectiveBranchingFactor)
      .def("json", [](const MappingResults::HeuristicBenchmarkInfo& info) {
        const nb::module_ json = nb::module_::import_("json");
        const nb::object loads = json.attr("loads");
        return loads(info.json().dump());
      });

  // Heuristic benchmark information for individual layers
  nb::class_<MappingResults::LayerHeuristicBenchmarkInfo>(
      m, "LayerHeuristicBenchmarkInfo", "Heuristic benchmark information")
      .def(nb::init<>())
      .def_rw("expanded_nodes",
              &MappingResults::LayerHeuristicBenchmarkInfo::expandedNodes)
      .def_rw("generated_nodes",
              &MappingResults::LayerHeuristicBenchmarkInfo::generatedNodes)
      .def_rw("expanded_nodes_after_first_solution",
              &MappingResults::LayerHeuristicBenchmarkInfo::
                  expandedNodesAfterFirstSolution)
      .def_rw("expanded_nodes_after_optimal_solution",
              &MappingResults::LayerHeuristicBenchmarkInfo::
                  expandedNodesAfterOptimalSolution)
      .def_rw("solution_nodes",
              &MappingResults::LayerHeuristicBenchmarkInfo::solutionNodes)
      .def_rw("solution_nodes_after_optimal_solution",
              &MappingResults::LayerHeuristicBenchmarkInfo::
                  solutionNodesAfterOptimalSolution)
      .def_rw("solution_depth",
              &MappingResults::LayerHeuristicBenchmarkInfo::solutionDepth)
      .def_rw("time_per_node",
              &MappingResults::LayerHeuristicBenchmarkInfo::secondsPerNode)
      .def_rw(
          "average_branching_factor",
          &MappingResults::LayerHeuristicBenchmarkInfo::averageBranchingFactor)
      .def_rw("effective_branching_factor",
              &MappingResults::LayerHeuristicBenchmarkInfo::
                  effectiveBranchingFactor)
      .def_rw("early_termination",
              &MappingResults::LayerHeuristicBenchmarkInfo::earlyTermination)
      .def("json", [](const MappingResults::LayerHeuristicBenchmarkInfo& info) {
        const nb::module_ json = nb::module_::import_("json");
        const nb::object loads = json.attr("loads");
        return loads(info.json().dump());
      });

  auto arch = nb::class_<Architecture>(
      m, "Architecture", "Class representing device/backend information");
  auto properties = nb::class_<Architecture::Properties>(
      arch, "Properties", "Class representing properties of an architecture");

  // Properties of an architecture (e.g. number of qubits, connectivity, error
  // rates, ...)
  properties.def(nb::init<>())
      .def_prop_rw("name", &Architecture::Properties::getName,
                   &Architecture::Properties::setName)
      .def_prop_rw("num_qubits", &Architecture::Properties::getNqubits,
                   &Architecture::Properties::setNqubits)
      .def("get_single_qubit_error",
           &Architecture::Properties::getSingleQubitErrorRate, "qubit"_a,
           "operation"_a)
      .def("set_single_qubit_error",
           &Architecture::Properties::setSingleQubitErrorRate, "qubit"_a,
           "operation"_a, "error_rate"_a)
      .def("get_two_qubit_error",
           &Architecture::Properties::getTwoQubitErrorRate, "control"_a,
           "target"_a, "operation"_a = "cx")
      .def("set_two_qubit_error",
           &Architecture::Properties::setTwoQubitErrorRate, "control"_a,
           "target"_a, "error_rate"_a, "operation"_a = "cx")
      .def(
          "get_readout_error",
          [](const Architecture::Properties& props, std::uint16_t qubit) {
            return props.readoutErrorRate.get(qubit);
          },
          "qubit"_a)
      .def(
          "set_readout_error",
          [](Architecture::Properties& props, std::uint16_t qubit,
             double rate) { props.readoutErrorRate.set(qubit, rate); },
          "qubit"_a, "readout_error_rate"_a)
      .def(
          "get_t1",
          [](const Architecture::Properties& props, std::uint16_t qubit) {
            return props.t1Time.get(qubit);
          },
          "qubit"_a)
      .def(
          "set_t1",
          [](Architecture::Properties& props, std::uint16_t qubit, double t1) {
            props.t1Time.set(qubit, t1);
          },
          "qubit"_a, "t1"_a)
      .def(
          "get_t2",
          [](const Architecture::Properties& props, std::uint16_t qubit) {
            return props.t2Time.get(qubit);
          },
          "qubit"_a)
      .def(
          "set_t2",
          [](Architecture::Properties& props, std::uint16_t qubit, double t2) {
            props.t2Time.set(qubit, t2);
          },
          "qubit"_a, "t2"_a)
      .def(
          "get_frequency",
          [](const Architecture::Properties& props, std::uint16_t qubit) {
            return props.qubitFrequency.get(qubit);
          },
          "qubit"_a)
      .def(
          "set_frequency",
          [](Architecture::Properties& props, std::uint16_t qubit,
             double freq) { props.qubitFrequency.set(qubit, freq); },
          "qubit"_a, "qubit_frequency"_a)
      .def(
          "get_calibration_date",
          [](const Architecture::Properties& props, std::uint16_t qubit) {
            return props.calibrationDate.get(qubit);
          },
          "qubit"_a)
      .def(
          "set_calibration_date",
          [](Architecture::Properties& props, std::uint16_t qubit,
             const std::string& date) {
            props.calibrationDate.set(qubit, date);
          },
          "qubit"_a, "calibration_date"_a)
      .def(
          "json",
          [](const Architecture::Properties& props) {
            const nb::module_ json = nb::module_::import_("json");
            const nb::object loads = json.attr("loads");
            return loads(props.json().dump());
          },
          "Returns a JSON-style dictionary of all the information present in "
          "the :class:`.Properties`")
      .def("__repr__", &Architecture::Properties::toString,
           "Prints a JSON-formatted representation of all the information "
           "present in the :class:`.Properties`");

  // Interface to the QMAP internal architecture class
  arch.def(nb::init<>())
      .def(nb::init<std::uint16_t, const CouplingMap&>(), "num_qubits"_a,
           "coupling_map"_a)
      .def(nb::init<std::uint16_t, const CouplingMap&,
                    const Architecture::Properties&>(),
           "num_qubits"_a, "coupling_map"_a, "properties"_a)
      .def_prop_rw("name", &Architecture::getName, &Architecture::setName)
      .def_prop_rw("num_qubits", &Architecture::getNqubits,
                   &Architecture::setNqubits)
      .def_prop_rw(
          "coupling_map", nb::overload_cast<>(&Architecture::getCouplingMap),
          &Architecture::setCouplingMap, nb::rv_policy::reference_internal)
      .def_prop_rw(
          "properties", nb::overload_cast<>(&Architecture::getProperties),
          &Architecture::setProperties, nb::rv_policy::reference_internal)
      .def("load_coupling_map",
           nb::overload_cast<AvailableArchitecture>(
               &Architecture::loadCouplingMap),
           "available_architecture"_a)
      .def(
          "load_coupling_map",
          nb::overload_cast<const std::string&>(&Architecture::loadCouplingMap),
          "coupling_map_file"_a)
      .def("load_properties",
           nb::overload_cast<const Architecture::Properties&>(
               &Architecture::loadProperties),
           "properties"_a)
      .def("load_properties",
           nb::overload_cast<const std::string&>(&Architecture::loadProperties),
           "properties"_a);

  // Main mapping function
  m.def("map", &map, "map a quantum circuit", "circ"_a, "arch"_a, "config"_a);
}
