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
#include "ir/operations/OpType.hpp"
#include "na/nasp/CodeGenerator.hpp"
#include "na/nasp/Solver.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <nanobind/nanobind.h>
#include <nanobind/stl/optional.h> // NOLINT(misc-include-cleaner)
#include <nanobind/stl/pair.h>     // NOLINT(misc-include-cleaner)
#include <nanobind/stl/string.h>   // NOLINT(misc-include-cleaner)
#include <nanobind/stl/vector.h>   // NOLINT(misc-include-cleaner)
#include <nlohmann/json.hpp>       // NOLINT(misc-include-cleaner)
#include <string>

namespace nb = nanobind;
using namespace nb::literals;

NB_MODULE(MQT_QMAP_MODULE_NAME, m) {
  nb::module_::import_("mqt.core.ir");

  // Neutral Atom State Preparation
  nb::class_<na::NASolver>(m, "NAStatePreparationSolver")
      .def(nb::init<uint16_t, uint16_t, uint16_t, uint16_t, uint16_t, uint16_t,
                    uint16_t, uint16_t, uint16_t, uint16_t>(),
           "max_x"_a, "max_y"_a, "max_c"_a, "max_r"_a, "max_h_offset"_a,
           "max_v_offset"_a, "max_h_dist"_a, "max_v_dist"_a,
           "min_entangling_y"_a, "max_entangling_y"_a)
      .def("solve", &na::NASolver::solve, "ops"_a, "num_qubits"_a,
           "num_stages"_a, "num_transfers"_a = nb::none(),
           "mind_ops_order"_a = false, "shield_idle_qubits"_a = true);

  nb::class_<na::NASolver::Result>(m, "NAStatePreparationSolver.Result")
      .def(nb::init<>())
      .def("json", [](const na::NASolver::Result& result) {
        const nb::module_ json = nb::module_::import_("json");
        const nb::object loads = json.attr("loads");
        return loads(result.json().dump());
      });

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
