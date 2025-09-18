#include "ir/QuantumComputation.hpp"
#include "ir/operations/OpType.hpp"
#include "na/NAComputation.hpp"
#include "na/nasp/CodeGenerator.hpp"
#include "na/nasp/Solver.hpp"
#include "na/nasp/SolverFactory.hpp"
#include "qasm3/Importer.hpp"

#include <cstdint>
#include <gtest/gtest.h>
#include <optional>

TEST(CodeGenerator, Generate) {
  const auto& circ =
      qasm3::Importer::importf((TEST_CIRCUITS_PATH "/steane.qasm"));
  // initialize a solver with the following parameters
  // - 3 interaction sites in the horizontal direction
  // - 7 interaction sites in the vertical direction
  // - 2 AOD columns
  // - 3 AOD rows
  // - 5 rows and columns in every interaction site which corresponds to a
  //   maximum offset of 2 in both directions
  // - qubits can interact with directly or diagonally adjacent qubits, which
  //   corresponds to a maximum distance of 2 in both directions
  // - the entangling zone starts at y = 2 and ends at y = 4 which implies a
  //   storage zone at the top and at the bottom
  na::NASolver solver(3, 7, 2, 3, 2, 2, 2, 2, 2, 4);
  // get operations for solver
  const auto& pairs =
      na::SolverFactory::getOpsForSolver(circ, {qc::Z, 1}, true);
  // solve
  const auto result =
      solver.solve(pairs, static_cast<uint16_t>(circ.getNqubits()), 4,
                   std::nullopt, false, true);
  const auto& comp = na::CodeGenerator::generate(circ, result);
  const auto valid = comp.validateAODConstraints();
  EXPECT_TRUE(valid);
}
