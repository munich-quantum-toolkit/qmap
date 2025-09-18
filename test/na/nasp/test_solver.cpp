#include "ir/QuantumComputation.hpp"
#include "ir/operations/OpType.hpp"
#include "na/nasp/Solver.hpp"
#include "qasm3/Importer.hpp"

#include <algorithm>
#include <cstdint>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <optional>
#include <stdexcept>
#include <tuple>

TEST(Solver, SteaneDoubleSidedStorage) {
  const auto& circ =
      qasm3::Importer::importf(TEST_CIRCUITS_PATH "/steane.qasm");
  // create solver
  na::NASolver solver(3, 7, 2, 3, 2, 2, 2, 2, 2, 4);
  // get operations for solver
  const auto& pairs = na::NASolver::getOpsForSolver(circ, qc::Z, 1, true);
  // solve
  const auto result =
      solver.solve(pairs, static_cast<uint16_t>(circ.getNqubits()), 4,
                   std::nullopt, false, true);
  EXPECT_TRUE(result.sat);
  EXPECT_EQ(result.stages.size(), 4);
}

TEST(Solver, ShorDoubleSidedStorage) {
  const auto& circ = qasm3::Importer::importf(TEST_CIRCUITS_PATH "/shor.qasm");
  // create solver
  na::NASolver solver(3, 7, 2, 3, 2, 2, 2, 2, 2, 4);
  // get operations for solver
  const auto& pairs = na::NASolver::getOpsForSolver(circ, qc::Z, 1, true);
  // solve
  const auto result =
      solver.solve(pairs, static_cast<uint16_t>(circ.getNqubits()), 4,
                   std::nullopt, false, true);
  EXPECT_FALSE(result.sat);
}

TEST(Solver, Surface3DoubleSidedStorage) {
  const auto& circ =
      qasm3::Importer::importf(TEST_CIRCUITS_PATH "/surface_3.qasm");
  // create solver
  na::NASolver solver(3, 7, 2, 3, 2, 2, 2, 2, 2, 4);
  // get operations for solver
  const auto& pairs = na::NASolver::getOpsForSolver(circ, qc::Z, 1, true);
  // solve
  const auto result =
      solver.solve(pairs, static_cast<uint16_t>(circ.getNqubits()), 4,
                   std::nullopt, false, true);
  EXPECT_TRUE(result.sat);
  EXPECT_EQ(result.stages.size(), 4);
}

TEST(Solver, SteaneBottomStorage) {
  const auto& circ =
      qasm3::Importer::importf(TEST_CIRCUITS_PATH "/steane.qasm");
  // create solver
  na::NASolver solver(3, 7, 2, 3, 2, 2, 2, 2, 0, 4);
  // get operations for solver
  const auto& pairs = na::NASolver::getOpsForSolver(circ, qc::Z, 1, true);
  // solve
  const auto resultUnsat =
      solver.solve(pairs, static_cast<uint16_t>(circ.getNqubits()), 4,
                   std::nullopt, false, true);
  EXPECT_FALSE(resultUnsat.sat);
  const auto resultSat =
      solver.solve(pairs, static_cast<uint16_t>(circ.getNqubits()), 5,
                   std::nullopt, false, true);
  EXPECT_TRUE(resultSat.sat);
  EXPECT_TRUE(resultSat.stages.front().rydberg);
  for (const auto& q : resultSat.stages.front().qubits) {
    EXPECT_GE(q.x, 0);
    EXPECT_LE(q.x, 3);
    EXPECT_GE(q.y, 0);
    EXPECT_LE(q.y, 7);
    EXPECT_GE(q.c, 0);
    EXPECT_LE(q.c, 2);
    EXPECT_GE(q.r, 0);
    EXPECT_LE(q.r, 3);
    EXPECT_GE(q.h, -2);
    EXPECT_LE(q.h, 2);
    EXPECT_GE(q.v, -2);
    EXPECT_LE(q.v, 2);
    EXPECT_NO_THROW(std::ignore = q.a);
  }
  for (const auto& g : resultSat.stages.front().gates) {
    EXPECT_TRUE(std::find(pairs.cbegin(), pairs.cend(), g.qubits) !=
                pairs.cend());
  }
}

TEST(Solver, NoShieldingFixedOrder) {
  const auto& circ =
      qasm3::Importer::importf(TEST_CIRCUITS_PATH "/steane.qasm");
  // create solver
  na::NASolver solver(3, 7, 2, 3, 2, 2, 2, 2, 0, 7);
  // get operations for solver
  const auto& pairs = na::NASolver::getOpsForSolver(circ, qc::Z, 1, true);
  // solve
  const auto result =
      solver.solve(pairs, static_cast<uint16_t>(circ.getNqubits()), 3,
                   std::nullopt, false, false);
  EXPECT_TRUE(result.sat);
}

TEST(Solver, FixedTransfer) {
  const auto& circ =
      qasm3::Importer::importf(TEST_CIRCUITS_PATH "/steane.qasm");
  // create solver
  na::NASolver solver(3, 7, 2, 3, 2, 2, 2, 2, 2, 4);
  // get operations for solver
  const auto& pairs = na::NASolver::getOpsForSolver(circ, qc::Z, 1, true);
  // solve
  const auto result = solver.solve(
      pairs, static_cast<uint16_t>(circ.getNqubits()), 5, 2, false, true);
  EXPECT_TRUE(result.sat);
}

TEST(Solver, Unsat) {
  const auto& circ =
      qasm3::Importer::importf(TEST_CIRCUITS_PATH "/steane.qasm");
  // create solver
  na::NASolver solver(3, 7, 2, 3, 2, 2, 2, 2, 2, 4);
  // get operations for solver
  const auto& pairs = na::NASolver::getOpsForSolver(circ, qc::Z, 1, true);
  // solve
  const auto result =
      solver.solve(pairs, static_cast<uint16_t>(circ.getNqubits()), 3,
                   std::nullopt, false, true);
  EXPECT_FALSE(result.sat);
}

TEST(Solver, Exceptions) {
  // One-sided storage zone is only supported below the entangling zone (higher
  // Y), i.e., minEntanglingY must be 0 or maxEntanglingY must less than maxY.
  EXPECT_THROW(std::ignore = na::NASolver(3, 7, 2, 3, 2, 2, 2, 2, 2, 7),
               std::invalid_argument);
  na::NASolver solver(3, 7, 2, 3, 2, 2, 2, 2, 0, 7);
  // In order to shield qubits, there must be a storage zone, i.e.,
  // maxEntanglingY must be less than maxY.
  EXPECT_THROW(std::ignore =
                   solver.solve({{0, 1}}, 3, 1, std::nullopt, false, true),
               std::invalid_argument);
  na::NASolver solver2(3, 7, 2, 3, 2, 2, 2, 2, 2, 4);
  // In order to shield qubits, there must be a storage zone, i.e.,
  // maxEntanglingY must be less than maxY.
  EXPECT_THROW(std::ignore =
                   solver2.solve({{0, 1}}, 1, 1, std::nullopt, false, true),
               std::invalid_argument);
}

TEST(Solver, JSONRoundTrip) {
  const auto& circ =
      qasm3::Importer::importf(TEST_CIRCUITS_PATH "/steane.qasm");
  // create solver
  na::NASolver solver(3, 7, 2, 3, 2, 2, 2, 2, 2, 4);
  // get operations for solver
  const auto& pairs = na::NASolver::getOpsForSolver(circ, qc::Z, 1, true);
  // solve
  const auto result =
      solver.solve(pairs, static_cast<uint16_t>(circ.getNqubits()), 4,
                   std::nullopt, false, true);
  const auto resultRT = na::NASolver::Result::fromJSON(result.json());
  EXPECT_EQ(resultRT, result);
}

TEST(Solver, GetOpsForSolver) {
  const auto& circ = qasm3::Importer::imports(R"(
OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];
h q;
cz q[0], q[1];
)");
  // get operations for solver
  EXPECT_NO_THROW({
    const auto& pairs = na::NASolver::getOpsForSolver(circ, qc::Z, 1, true);
    EXPECT_EQ(pairs.size(), 1);
    EXPECT_EQ(pairs.front(), (std::pair{0U, 1U}));
  });
  EXPECT_THROW(std::ignore =
                   na::NASolver::getOpsForSolver(circ, qc::Z, 1, false),
               std::invalid_argument);
  EXPECT_THROW(std::ignore =
                   na::NASolver::getOpsForSolver(circ, qc::H, 0, true),
               std::invalid_argument);
}
