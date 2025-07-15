/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "cliffordsynthesis/CliffordSynthesizer.hpp"
#include "cliffordsynthesis/Configuration.hpp"
#include "cliffordsynthesis/Results.hpp"
#include "cliffordsynthesis/Tableau.hpp"
#include "cliffordsynthesis/TargetMetric.hpp"
#include "ir/QuantumComputation.hpp"
#include "qasm3/Importer.hpp"

#include <cstddef>
#include <nlohmann/json.hpp>
#include <plog/Severity.h>
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
  // Target metric for the Clifford synthesizer
  py::enum_<cs::TargetMetric>(m, "TargetMetric")
      .value("gates", cs::TargetMetric::Gates, "Optimize gate count.")
      .value("two_qubit_gates", cs::TargetMetric::TwoQubitGates,
             "Optimize two-qubit gate count.")
      .value("depth", cs::TargetMetric::Depth, "Optimize circuit depth.")
      .export_values()
      .def(py::init([](const std::string& name) {
        return cs::targetMetricFromString(name);
      }));
  py::implicitly_convertible<py::str, cs::TargetMetric>();

  py::enum_<plog::Severity>(m, "Verbosity")
      .value("none", plog::Severity::none, "No output.")
      .value("fatal", plog::Severity::fatal, "Only show fatal errors.")
      .value("error", plog::Severity::error, "Show errors.")
      .value("warning", plog::Severity::warning, "Show warnings.")
      .value("info", plog::Severity::info, "Show general information.")
      .value("debug", plog::Severity::debug,
             "Show additional debug information.")
      .value("verbose", plog::Severity::verbose, "Show all information.")
      .export_values()
      .def(py::init([](const std::string& name) {
        return plog::severityFromString(name.c_str());
      }));
  py::implicitly_convertible<py::str, plog::Severity>();

  // Configuration for the synthesis
  py::class_<cs::Configuration>(
      m, "SynthesisConfiguration",
      "Configuration options for the MQT QMAP Clifford synthesis tool.")
      .def(py::init<>())
      .def_readwrite("initial_timestep_limit",
                     &cs::Configuration::initialTimestepLimit,
                     "Initial timestep limit for the Clifford synthesis. "
                     "Defaults to `0`, which implies that the initial timestep "
                     "limit is determined automatically.")
      .def_readwrite(
          "minimal_timesteps", &cs::Configuration::minimalTimesteps,
          "Minimal timestep considered for the Clifford synthesis. "
          "This option limits the lower bound of the interval in "
          "which the binary search method looks for solutions. "
          "Set this if you know a lower bound for the circuit depth. "
          "Defaults to `0`, which implies that no lower bound for depth "
          "is known.")
      .def_readwrite(
          "use_maxsat", &cs::Configuration::useMaxSAT,
          "Use MaxSAT to solve the synthesis problem or to really on the "
          "binary search scheme for finding the optimum. Defaults to `false`.")
      .def_readwrite("linear_search", &cs::Configuration::linearSearch,
                     "Use liner search instead of binary search "
                     "scheme for finding the optimum. Defaults to `false`.")
      .def_readwrite(
          "target_metric", &cs::Configuration::target,
          "Target metric for the Clifford synthesis. Defaults to `gates`.")
      .def_readwrite("use_symmetry_breaking",
                     &cs::Configuration::useSymmetryBreaking,
                     "Use symmetry breaking clauses to speed up the synthesis "
                     "process. Defaults to `true`.")
      .def_readwrite("dump_intermediate_results",
                     &cs::Configuration::dumpIntermediateResults,
                     "Dump intermediate results of the synthesis process. "
                     "Defaults to `false`.")
      .def_readwrite("intermediate_results_path",
                     &cs::Configuration::intermediateResultsPath,
                     "Path to the directory where intermediate results should "
                     "be dumped. Defaults to `./`. The path needs to include a "
                     "path separator at the end.")
      .def_readwrite(
          "verbosity", &cs::Configuration::verbosity,
          "Verbosity level for the synthesis process. Defaults to 'warning'.")
      .def_readwrite("solver_parameters", &cs::Configuration::solverParameters,
                     "Parameters to be passed to Z3 as dict[str, bool | int | "
                     "float | str]")
      .def_readwrite(
          "minimize_gates_after_depth_optimization",
          &cs::Configuration::minimizeGatesAfterDepthOptimization,
          "Depth optimization might produce a circuit with more gates than "
          "necessary. This option enables an additional run of the synthesizer "
          "to minimize the overall number of gates. Defaults to `false`.")
      .def_readwrite(
          "try_higher_gate_limit_for_two_qubit_gate_optimization",
          &cs::Configuration::tryHigherGateLimitForTwoQubitGateOptimization,
          "When optimizing two-qubit gates, the synthesizer might fail "
          "to find an optimal solution for a certain timestep limit, but there "
          "might be a better solution for some higher timestep limit. This "
          "option enables an additional run of the synthesizer with a higher "
          "gate limit. Defaults to `false`.")
      .def_readwrite("gate_limit_factor", &cs::Configuration::gateLimitFactor,
                     "Factor by which the gate limit is increased when "
                     "trying to find a better solution for the two-qubit "
                     "gate optimization. Defaults to `1.1`.")
      .def_readwrite(
          "minimize_gates_after_two_qubit_gate_optimization",
          &cs::Configuration::minimizeGatesAfterTwoQubitGateOptimization,
          "Two-qubit gate optimization might produce a circuit "
          "with more gates than necessary. This option enables "
          "an additional run of the synthesizer to minimize the "
          "overall number of gates. Defaults to `false`.")
      .def_readwrite("heuristic", &cs::Configuration::heuristic,
                     "Use heuristic to synthesize the circuit. "
                     "This method synthesizes shallow intermediate circuits "
                     "and combines them. Defaults to `false`.")
      .def_readwrite("split_size", &cs::Configuration::splitSize,
                     "Size of subcircuits used in heuristic. "
                     "Defaults to `5`.")
      .def_readwrite(
          "n_threads_heuristic", &cs::Configuration::nThreadsHeuristic,
          "Maximum number of threads used for the heuristic optimizer. "
          "Defaults to the number of available threads on the system.")
      .def("json", &cs::Configuration::json,
           "Returns a JSON-style dictionary of all the information present in "
           "the :class:`.Configuration`")
      .def(
          "__repr__",
          [](const cs::Configuration& config) { return config.json().dump(2); },
          "Prints a JSON-formatted representation of all the information "
          "present in the :class:`.Configuration`");

  // Results of the synthesis
  py::class_<cs::Results>(m, "SynthesisResults",
                          "Results of the MQT QMAP Clifford synthesis tool.")
      .def(py::init<>())
      .def_property_readonly("gates", &cs::Results::getGates,
                             "Returns the number of gates in the circuit.")
      .def_property_readonly("single_qubit_gates",
                             &cs::Results::getSingleQubitGates,
                             "Returns the number of single-qubit gates in the "
                             "synthesized circuit.")
      .def_property_readonly("two_qubit_gates", &cs::Results::getTwoQubitGates,
                             "Returns the number of two-qubit gates in the "
                             "synthesized circuit.")
      .def_property_readonly("depth", &cs::Results::getDepth,
                             "Returns the depth of the synthesized circuit.")
      .def_property_readonly("runtime", &cs::Results::getRuntime,
                             "Returns the runtime of the synthesis in seconds.")
      .def_property_readonly("solver_calls", &cs::Results::getSolverCalls,
                             "Returns the number of calls to the SAT solver.")
      .def_property_readonly(
          "circuit", &cs::Results::getResultCircuit,
          "Returns the synthesized circuit as a qasm string.")
      .def_property_readonly("tableau", &cs::Results::getResultTableau,
                             "Returns a string representation of the "
                             "synthesized circuit's tableau.")
      .def("sat", &cs::Results::sat,
           "Returns `true` if the synthesis was successful.")
      .def("unsat", &cs::Results::unsat,
           "Returns `true` if the synthesis was unsuccessful.");

  auto tableau = py::class_<cs::Tableau>(
      m, "Tableau", "A class for representing stabilizer tableaus.");
  tableau.def(py::init<std::size_t, bool>(), "n"_a,
              "include_destabilizers"_a = false,
              "Creates a tableau for an n-qubit Clifford.");
  tableau.def(
      py::init<const std::string&>(), "tableau"_a,
      "Constructs a tableau from a string description. This can either be a "
      "semicolon separated binary matrix or a list of Pauli strings.");
  tableau.def(
      py::init<const std::string&, const std::string&>(), "stabilizers"_a,
      "destabilizers"_a,
      "Constructs a tableau from two lists of Pauli strings, the Stabilizers"
      "and Destabilizers.");

  auto synthesizer = py::class_<cs::CliffordSynthesizer>(
      m, "CliffordSynthesizer", "A class for synthesizing Clifford circuits.");

  synthesizer.def(py::init<cs::Tableau, cs::Tableau>(), "initial_tableau"_a,
                  "target_tableau"_a,
                  "Constructs a synthesizer for two tableaus representing the "
                  "initial and target state.");
  synthesizer.def(py::init<cs::Tableau>(), "target_tableau"_a,
                  "Constructs a synthesizer for a tableau representing the "
                  "target state.");
  synthesizer.def(py::init<qc::QuantumComputation&, bool>(), "qc"_a,
                  "use_destabilizers"_a
                  "Constructs a synthesizer for a quantum computation "
                  "representing the target state.");
  synthesizer.def(
      py::init<cs::Tableau, qc::QuantumComputation&>(), "initial_tableau"_a,
      "qc"_a,
      "Constructs a synthesizer for a quantum computation representing the "
      "target state that starts in an initial state represented by a tableau.");
  synthesizer.def("synthesize", &cs::CliffordSynthesizer::synthesize,
                  "config"_a = cs::Configuration(),
                  "Runs the synthesis with the given configuration.");
  synthesizer.def_property_readonly("results",
                                    &cs::CliffordSynthesizer::getResults,
                                    "Returns the results of the synthesis.");
  synthesizer.def_property_readonly(
      "result_circuit", [](cs::CliffordSynthesizer& self) {
        return qasm3::Importer::imports(self.getResults().getResultCircuit());
      });
}
