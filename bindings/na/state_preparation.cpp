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

// NOLINTNEXTLINE(misc-use-internal-linkage)
void registerStatePreparation(nb::module_& m) {
  nb::module_::import_("mqt.core.ir");

  // Neutral Atom State Preparation
  auto solver =
      nb::class_<na::NASolver>(m, "NAStatePreparationSolver",
                               R"pb(Neutral atom state preparation solver.

The neutral atom state preparation solver generates an optimal sequence of neutral atom operations for a given state preparation circuit.)pb")
          .def(
              nb::init<uint16_t, uint16_t, uint16_t, uint16_t, uint16_t,
                       uint16_t, uint16_t, uint16_t, uint16_t, uint16_t>(),
              "max_x"_a, "max_y"_a, "max_c"_a, "max_r"_a, "max_h_offset"_a,
              "max_v_offset"_a, "max_h_dist"_a, "max_v_dist"_a,
              "min_entangling_y"_a, "max_entangling_y"_a,
              R"pb(Create a solver instance for the neutral atom state preparation problem.

The solver is based on a 2D grid abstraction of the neutral atom quantum computer.
The 2D plane is divided into interaction sites.
Each interaction site is denoted by abstract x- and y-coordinates.
The parameter `max_x` specifies the maximum x-coordinate, and `max_y` specifies the maximum y-coordinate.
In the center of an interaction site, sits an SLM trap.
Around that trap there are several possible discrete AOD positions arranged as a grid.
The specific position of an atom within an interaction site is determined by the x- and y-offset from the SLM trap, i.e., those can be positive and negative.
The maximum absolute value of the x- and y-offset is specified by `max_h_offset` and `max_v_offset`, respectively.
Then, the parameter `max_c` specifies the maximum number of AOD columns, and `max_r` specifies the maximum number of AOD rows.
Finally, in order to interact during a Rydberg stage, atoms must be located within a certain distance.
The maximum horizontal and vertical distance between two atoms is specified by `max_h_dist` and `max_v_dist`, respectively.
The parameter `min_entangling_y` specifies the minimum y-coordinate for entangling operations, and `max_entangling_y` specifies the maximum y-coordinate for entangling operations.
Hence, y-coordinates outside of this range are located in the storage zone.

Note:
    The solver can only handle a single storage zone below the entangling zone, i.e., in this case `min_entangling_y` must be zero and `max_entangling_y` must be less than `max_y`.

Args:
    max_x: The maximum discrete x-coordinate of the interaction sites
    max_y: The maximum discrete y-coordinate of the interaction sites
    max_c: The maximum number of AOD columns
    max_r: The maximum number of AOD rows
    max_h_offset: The maximum horizontal offset of the atoms
    max_v_offset: The maximum vertical offset of the atoms
    max_h_dist: The maximum horizontal distance between two atoms
    max_v_dist: The maximum vertical distance between two atoms
    min_entangling_y: The minimum y-coordinate for entangling operations
    max_entangling_y: The maximum y-coordinate for entangling operations

Raises:
    ValueError: If one of the parameters is invalid, e.g., is a negative value)pb")

          .def("solve", &na::NASolver::solve, "ops"_a, "num_qubits"_a,
               "num_stages"_a, "num_transfers"_a = nb::none(),
               "mind_ops_order"_a = false, "shield_idle_qubits"_a = true,
               R"pb(Solve the neutral atom state preparation problem.

The solver generates an optimal sequence of neutral atom operations for a given state preparation circuit.
The circuit is given as a list of operations, where each operation is a pair of qubits.
The sequence is divided into stages.
Each stage is either a Rydberg stage or a transfer stage.
In a Rydberg stage, adjacent qubits in the entangling zone undergo an entangling gate.
In a transfer stage, atoms can be stored from AOD into SLM traps and loaded from SLM traps into AOD.
At the end of each stage, the atoms are shuttled to their next position.
The number of stages is specified by `num_stages`.
The number of transfers is fixed by `num_transfers` if given.
If this parameter is not specified, then the solver will determine the optimal number of transfers.
The parameter `mind_ops_order` specifies whether the order of the operations in the circuit should be preserved.
The parameter `shield_idle_qubits` specifies whether idle qubits should be shielded from the entangling operations.

Note:
    To retrieve the list of qubit pairs from a quantum circuit, use the function :func:`get_ops_for_solver`.

    The returned solver's result can either be directly exported to the JSON format by calling the method :func:`json` on the result object or the result object can be passed to the function :func:`generate_code` to generate code consisting of neutral atom operations.

Args:
    ops: The list of operations in the circuit
    num_qubits: The number of qubits in the circuit
    num_stages: The number of stages in the sequence
    num_transfers: The number of transfers in the sequence. Can be `None`
    mind_ops_order: Whether the order of the operations should be preserved
    shield_idle_qubits: Whether idle qubits should be shielded

Returns:
    The result of the solver

Raises:
    ValueError: if one of the numeral parameters is invalid, e.g., is a negative value)pb");

  nb::class_<na::NASolver::Result>(
      solver, "Result", "The result of a :class:`~.NAStatePreparationSolver`.")
      .def(nb::init<>(), "Create a result object.")
      .def(
          "json",
          [](const na::NASolver::Result& result) {
            const auto json = nb::module_::import_("json");
            const auto loads = json.attr("loads");
            const auto dict = loads(result.json().dump());
            return nb::cast<nb::typed<nb::dict, nb::str, nb::any>>(dict);
          },
          R"pb(Returns the result as JSON-style dictionary.

Returns:
    The result as a JSON-style dictionary)pb");

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
      "zone_dist"_a = 24,
      R"pb(Generate code for the given circuit using the solver's result.

Some parameters of the abstraction from the 2D grid used for the solver must be provided again.

Args:
    qc: The quantum circuit
    result: The result of the solver
    min_atom_dist: The minimum distance between atoms
    no_interaction_radius: The radius around an atom where no other atom can be placed during an entangling operation that should not interact with the atom
    zone_dist: The distance between zones, i.e., the minimal distance between two atoms in different zones

Returns:
    The generated code as a string

Raises:
    ValueError: If one of the numeral parameters is invalid, e.g., is a negative value)pb");

  m.def(
      "get_ops_for_solver",
      [](const qc::QuantumComputation& qc, const std::string& operationType,
         const uint64_t numControls, const bool quiet) {
        auto operationTypeLower = operationType;
        std::ranges::transform(operationTypeLower, operationTypeLower.begin(),
                               [](unsigned char c) { return std::tolower(c); });
        return na::NASolver::getOpsForSolver(
            qc, qc::opTypeFromString(operationTypeLower), numControls, quiet);
      },
      "qc"_a, "operation_type"_a = "Z", "num_controls"_a = 1, "quiet"_a = true,
      R"pb(Extract entangling operations as list of qubit pairs from the circuit.

Note:
    This function can only extract qubit pairs of two-qubit operations.
    I.e., the operands of the operation plus the controls must be equal to two.

Args:
    qc: The quantum circuit
    operation_type: The type of operation to extract, e.g., "z" for CZ gates
    num_controls: The number of controls the operation acts on, e.g., 1 for CZ gates
    quiet: Whether to suppress warnings when the circuit contains operations other than the specified operation type

Returns:
    List of qubit pairs

Raises:
    ValueError: If the circuit contains operations other than the specified operation type and quiet is False
    ValueError: If the operation has more than two operands including controls)pb");
}
