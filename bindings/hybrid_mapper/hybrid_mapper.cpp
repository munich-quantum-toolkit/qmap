/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "hybridmap/HybridNeutralAtomMapper.hpp"
#include "hybridmap/NeutralAtomArchitecture.hpp"
#include "hybridmap/NeutralAtomScheduler.hpp"
#include "hybridmap/NeutralAtomUtils.hpp"
#include "qasm3/Importer.hpp"

#include <cstdint>
#include <pybind11/attr.h>
#include <pybind11/cast.h>
#include <pybind11/detail/common.h>
#include <pybind11/pybind11.h>
#include <pybind11/pytypes.h>
// NOLINTNEXTLINE(misc-include-cleaner)
#include <pybind11/stl.h>
#include <string>

namespace py = pybind11;
using namespace pybind11::literals;

PYBIND11_MODULE(MQT_QMAP_MODULE_NAME, m, py::mod_gil_not_used()) {
  // Neutral Atom Hybrid Mapper
  py::enum_<na::InitialCoordinateMapping>(
      m, "InitialCoordinateMapping",
      "Initial mapping between hardware qubits hardware coordinates.")
      .value("trivial", na::InitialCoordinateMapping::Trivial,
             "Trivial identity mapping.")
      .value("random", na::InitialCoordinateMapping::Random, "Random mapping.")
      .export_values()
      .def(py::init([](const std::string& name) {
        return na::initialCoordinateMappingFromString(name);
      }));
  py::enum_<na::InitialMapping>(
      m, "InitialCircuitMapping",
      "Initial mapping between circuit qubits and hardware qubits.")
      .value("identity", na::InitialMapping::Identity, "Identity mapping.")
      .export_values()
      .def(py::init([](const std::string& name) {
        return na::initialMappingFromString(name);
      }));

  py::class_<na::MapperParameters>(
      m, "HybridMapperParameters",
      "Parameters for the Neutral Atom Hybrid Mapper.")
      .def(py::init<>())
      .def(py::init<double, double, double, double, double, double>(),
           "lookahead_weight_swaps"_a = 0.1, "lookahead_weight_moves"_a = 0.1,
           "decay"_a = 0.1, "shuttling_time_weight"_a = 1, "gate_weight"_a = 1,
           "shuttling_weight"_a = 1)
      .def_readwrite("lookahead_weight_swaps",
                     &na::MapperParameters::lookaheadWeightSwaps,
                     "Weight for the lookahead for the SWAP gates. 0 means no "
                     "lookahead is considered.")
      .def_readwrite("lookahead_weight_moves",
                     &na::MapperParameters::lookaheadWeightMoves,
                     "Weight for the lookahead for the MOVE gates. 0 means no "
                     "lookahead is considered.")
      .def_readwrite("decay", &na::MapperParameters::decay,
                     "Decay factor for the blocking constraint to avoid SWAPs "
                     "that block each other. 0 means no decay.")
      .def_readwrite(
          "shuttling_time_weight", &na::MapperParameters::shuttlingTimeWeight,
          "Weight how much the shuttling Times should be considered.")
      .def_readwrite(
          "gate_weight", &na::MapperParameters::gateWeight,
          "Weight for the SWAP gates. Higher means mapper will prefer SWAP "
          "gates over shuttling. 0 means only shuttling is used.")
      .def_readwrite(
          "shuttling_weight", &na::MapperParameters::shuttlingWeight,
          "Weight for the shuttling. Higher means mapper will prefer "
          "shuttling over SWAP gates. 0 means only SWAP gates are "
          "used.")
      .def_readwrite(
          "seed", &na::MapperParameters::seed,
          "Seed for the random number generator. 0 means random seed.")
      .def_readwrite("verbose", &na::MapperParameters::verbose,
                     "Print additional information during the mapping process.")
      .def_readwrite("initial_mapping", &na::MapperParameters::initialMapping,
                     "Initial mapping between circuit qubits and hardware "
                     "qubits.");

  py::class_<na::NeutralAtomArchitecture>(m, "NeutralAtomHybridArchitecture")
      .def(py::init<const std::string&>(), "filename"_a)
      .def("load_json", &na::NeutralAtomArchitecture::loadJson,
           "json_filename"_a)
      .def_readwrite("name", &na::NeutralAtomArchitecture::name,
                     "Name of the "
                     "architecture")
      .def_property_readonly(
          "nrows", &na::NeutralAtomArchitecture::getNrows,
          "Number of rows in a rectangular grid SLM arrangement")
      .def_property_readonly(
          "ncolumns", &na::NeutralAtomArchitecture::getNcolumns,
          "Number of columns in a rectangular grid SLM arrangement")
      .def_property_readonly(
          "npositions", &na::NeutralAtomArchitecture::getNpositions,
          "Total number of positions in a rectangular grid SLM arrangement")
      .def_property_readonly(
          "naods", &na::NeutralAtomArchitecture::getNAods,
          "Number of independent 2D acousto-optic deflectors")
      .def_property_readonly("naod_coordinates",
                             &na::NeutralAtomArchitecture::getNAodCoordinates,
                             "Maximal number of AOD rows/columns (NOT USED)")
      .def_property_readonly("nqubits",
                             &na::NeutralAtomArchitecture::getNqubits,
                             "Number of atoms in the neutral atom quantum "
                             "computer that can be used as qubits")
      .def_property_readonly(
          "inter_qubit_distance",
          &na::NeutralAtomArchitecture::getInterQubitDistance,
          "Distance "
          "between "
          "SLM traps in "
          "micrometers")
      .def_property_readonly("interaction_radius",
                             &na::NeutralAtomArchitecture::getInteractionRadius,
                             "Interaction radius in inter-qubit distances")
      .def_property_readonly("blocking_factor",
                             &na::NeutralAtomArchitecture::getBlockingFactor,
                             "Blocking factor for parallel Rydberg gates")
      .def_property_readonly(
          "naod_intermediate_levels",
          &na::NeutralAtomArchitecture::getNAodIntermediateLevels,
          "Number of possible AOD positions between two SLM traps")
      .def_property_readonly("decoherence_time",
                             &na::NeutralAtomArchitecture::getDecoherenceTime,
                             "Decoherence time in microseconds")
      .def("compute_swap_distance",
           static_cast<int (na::NeutralAtomArchitecture::*)(
               std::uint32_t, std::uint32_t) const>(
               &na::NeutralAtomArchitecture::getSwapDistance),
           "Number of SWAP gates required between two positions",
           py::arg("idx1"), py::arg("idx2"))
      .def("get_gate_time", &na::NeutralAtomArchitecture::getGateTime,
           "Execution time of certain gate in microseconds", "s"_a)
      .def("get_gate_average_fidelity",
           &na::NeutralAtomArchitecture::getGateAverageFidelity,
           "Average gate fidelity from [0,1]", "s"_a)
      .def("get_nearby_coordinates",
           &na::NeutralAtomArchitecture::getNearbyCoordinates,
           "Positions that are within the interaction radius of the passed "
           "position",
           "idx"_a)
      .def("get_animation_csv", &na::NeutralAtomArchitecture::getAnimationCsv,
           "Returns string representation of the architecture used for "
           "animation")
      .def("save_animation_csv", &na::NeutralAtomArchitecture::saveAnimationCsv,
           "filename"_a, "Saves the animation csv string to a file");

  py::class_<na::NeutralAtomMapper>(
      m, "HybridNAMapper",
      "Neutral Atom Hybrid Mapper that can use both SWAP gates and AOD "
      "movements to map a quantum circuit to a neutral atom quantum computer.")
      .def(py::init<const na::NeutralAtomArchitecture&, na::MapperParameters>(),
           "Create Hybrid NA Mapper with mapper parameters",
           py::keep_alive<1, 2>(), py::keep_alive<1, 3>(), "arch"_a,
           "params"_a = na::MapperParameters())
      .def("set_parameters", &na::NeutralAtomMapper::setParameters,
           "Set the parameters for the Hybrid NA Mapper", "params"_a)
      .def(
          "get_init_hw_pos", &na::NeutralAtomMapper::getInitHwPos,
          "Get the initial hardware positions, required to create an animation")
      .def("map", &na::NeutralAtomMapper::mapAndConvert,
           "Map a quantum circuit to the neutral atom quantum computer",
           "circ"_a, "initial_mapping"_a = na::InitialMapping::Identity,
           "verbose"_a = false)
      .def(
          "map_qasm_file",
          [](na::NeutralAtomMapper& mapper, const std::string& filename,
             const na::InitialMapping initialMapping) {
            auto qc = qasm3::Importer::importf(filename);
            mapper.map(qc, initialMapping);
          },
          "Map a quantum circuit to the neutral atom quantum computer",
          "filename"_a, "initial_mapping"_a = na::InitialMapping::Identity)
      .def("get_mapped_qc", &na::NeutralAtomMapper::getMappedQc,
           "Returns the mapped circuit as an extended qasm2 string")
      .def("save_mapped_qc", &na::NeutralAtomMapper::saveMappedQc,
           "Saves the mapped circuit as an extended qasm2 string to a file",
           "filename"_a)
      .def("get_mapped_qc_aod", &na::NeutralAtomMapper::getMappedQcAOD,
           "Returns the mapped circuit as an extended qasm2 string with native "
           "AOD movements")
      .def("save_mapped_qc_aod", &na::NeutralAtomMapper::saveMappedQcAOD,
           "Saves the mapped circuit as an extended qasm2 string with native "
           "AOD movements to a file",
           "filename"_a)
      .def(
          "schedule",
          [](na::NeutralAtomMapper& mapper, const bool verbose,
             const bool createAnimationCsv, const double shuttlingSpeedFactor) {
            auto results = mapper.schedule(verbose, createAnimationCsv,
                                           shuttlingSpeedFactor);
            return results.toMap();
          },
          "Schedule the mapped circuit", "verbose"_a = false,
          "create_animation_csv"_a = false, "shuttling_speed_factor"_a = 1.0)
      .def("get_animation_csv", &na::NeutralAtomMapper::getAnimationCsv,
           "Returns the animation csv string")
      .def("save_animation_csv", &na::NeutralAtomMapper::saveAnimationCsv,
           "Saves the animation csv string to a file", "filename"_a);
}
