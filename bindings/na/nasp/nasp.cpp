/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "ir/QuantumComputation.hpp"
#include "ir/operations/OpType.hpp"
#include "na/nasp/CodeGenerator.hpp"
#include "na/nasp/Solver.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <pybind11/cast.h>
#include <pybind11/detail/common.h>
#include <pybind11/pybind11.h>
// NOLINTNEXTLINE(misc-include-cleaner)
#include <pybind11/stl.h>
// NOLINTNEXTLINE(misc-include-cleaner)
#include <pybind11_json/pybind11_json.hpp>
#include <string>

namespace py = pybind11;
using namespace pybind11::literals;

PYBIND11_MODULE(MQT_QMAP_MODULE_NAME, m, py::mod_gil_not_used()) {
  // Neutral Atom State Preparation
  py::class_<na::NASolver>(m, "NAStatePreparationSolver")
      .def(py::init<uint16_t, uint16_t, uint16_t, uint16_t, uint16_t, uint16_t,
                    uint16_t, uint16_t, uint16_t, uint16_t>(),
           "max_x"_a, "max_y"_a, "max_c"_a, "max_r"_a, "max_h_offset"_a,
           "max_v_offset"_a, "max_h_dist"_a, "max_v_dist"_a,
           "min_entangling_y"_a, "max_entangling_y"_a)
      .def("solve", &na::NASolver::solve, "ops"_a, "num_qubits"_a,
           "num_stages"_a, "num_transfers"_a, "mind_ops_order"_a,
           "shield_idle_qubits"_a);

  py::class_<na::NASolver::Result>(m, "NAStatePreparationSolver.Result")
      .def(py::init<>())
      .def("json",
           [](const na::NASolver::Result& result) { return result.json(); });

  m.def(
      "generate_code",
      [](const qc::QuantumComputation& qc, const na::NASolver::Result& result,
         const uint16_t minAtomDist, const uint16_t noInteractionRadius,
         const uint16_t zoneDist) {
        return na::CodeGenerator::generate(qc, result, minAtomDist,
                                           noInteractionRadius, zoneDist)
            .toString();
      },
      "qc"_a, "result"_a, "min_atom_dist"_a = 1, "no_interaction_radius"_a = 10,
      "zone_dist"_a = 24);

  m.def(
      "get_ops_for_solver",
      [](const qc::QuantumComputation& qc, const std::string& operationType,
         const uint64_t numControls, const bool quiet) {
        auto opTypeLowerStr = operationType;
        std::ranges::transform(opTypeLowerStr, opTypeLowerStr.begin(),
                               [](unsigned char c) { return std::tolower(c); });
        return na::NASolver::getOpsForSolver(
            qc, qc::opTypeFromString(operationType), numControls, quiet);
      },
      "qc"_a, "operation_type"_a = "Z", "num_operands"_a = 1, "quiet"_a = true);
}
