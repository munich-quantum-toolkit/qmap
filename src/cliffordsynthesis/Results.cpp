//
// This file is part of the MQT QMAP library released under the MIT license.
// See README.md or go to https://github.com/cda-tum/qmap for more information.
//

#include "cliffordsynthesis/Results.hpp"

#include "Logic.hpp"
#include "circuit_optimizer/CircuitOptimizer.hpp"
#include "cliffordsynthesis/Tableau.hpp"
#include "ir/QuantumComputation.hpp"

#include <nlohmann/json.hpp>
#include <ostream>
#include <sstream>

namespace cs {

Results::Results(qc::QuantumComputation& qc, const Tableau& tableau) {
  // SWAP gates are not natively supported in the encoding, so we need to
  // decompose them into sequences of three CNOTs.
  qc::CircuitOptimizer::decomposeSWAP(qc, false);

  setResultCircuit(qc);
  setResultTableau(tableau);
  setDepth(qc.getDepth());
  setSingleQubitGates(qc.getNsingleQubitOps());
  setTwoQubitGates(qc.getNindividualOps() - singleQubitGates);
  setSolverResult(logicbase::Result::SAT);
}

void Results::setResultCircuit(const qc::QuantumComputation& qc) {
  std::stringstream ss;
  qc.dumpOpenQASM(ss);
  resultCircuit = ss.str();
}

void Results::setResultTableau(const Tableau& tableau) {
  std::stringstream ss;
  ss << tableau;
  resultTableau = ss.str();
}

nlohmann::basic_json<> Results::json() const {
  nlohmann::basic_json resultJSON{};
  resultJSON["solver_result"] = toString(solverResult);
  resultJSON["single_qubit_gates"] = singleQubitGates;
  resultJSON["two_qubit_gates"] = twoQubitGates;
  resultJSON["depth"] = depth;
  resultJSON["runtime"] = runtime;
  resultJSON["solver_calls"] = solverCalls;

  return resultJSON;
}

std::ostream& operator<<(std::ostream& os, const Results& config) {
  os << config.json().dump(2);
  return os;
}
} // namespace cs
