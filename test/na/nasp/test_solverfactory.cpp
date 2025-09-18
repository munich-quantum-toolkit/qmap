#include "ir/QuantumComputation.hpp"
#include "ir/operations/OpType.hpp"
#include "na/Architecture.hpp"
#include "na/nasp/SolverFactory.hpp"
#include "qasm3/Importer.hpp"

#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <tuple>

TEST(SolverFactory, Create) {
  na::Architecture arch;
  // write content to a file
  std::istringstream archIS(R"({
    "name": "Nature",
    "initialZones": [
        "storage"
    ],
    "zones": [
        {
            "name": "entangling",
            "xmin": -300,
            "xmax": 656,
            "ymin": -20,
            "ymax": 46,
            "fidelity": 0.9959
        },
        {
            "name": "storage",
            "xmin": -300,
            "xmax": 656,
            "ymin": 47,
            "ymax": 121,
            "fidelity": 1
        },
        {
            "name": "readout",
            "xmin": -300,
            "xmax": 656,
            "ymin": 122,
            "ymax": 156,
            "fidelity": 0.99
        }
    ],
    "operations": [
        {
            "name": "rz",
            "type": "local",
            "zones": [
                "entangling",
                "storage",
                "readout"
            ],
            "time": 0.5,
            "fidelity": 0.999
        },
        {
            "name": "ry",
            "type": "global",
            "zones": [
                "entangling",
                "storage",
                "readout"
            ],
            "time": 0.5,
            "fidelity": 0.999
        },
        {
            "name": "cz",
            "type": "global",
            "zones": [
                "entangling"
            ],
            "time": 0.2,
            "fidelity": 0.9959
        },
        {
            "name": "measure",
            "type": "global",
            "zones": [
                "readout"
            ],
            "time": 0.2,
            "fidelity": 0.95
        }
    ],
    "decoherence": {
        "t1": 100000000,
        "t2": 1500000
    },
    "interactionRadius": 2,
    "noInteractionRadius": 5,
    "minAtomDistance": 1,
    "shuttling": [
        {
            "rows": 5,
            "columns": 5,
            "xmin": -2.5,
            "xmax": 2.5,
            "ymin": -2.5,
            "ymax": 2.5,
            "move": {
                "speed": 0.55,
                "fidelity": 1
            },
            "load": {
                "time": 20,
                "fidelity": 1
            },
            "store": {
                "time": 20,
                "fidelity": 1
            }
        }
    ]
})");
  std::stringstream gridSS;
  gridSS << "x,y\n";
  // entangling zone (4 x 36 = 144 sites)
  for (size_t y = 0; y <= 36; y += 12) {
    for (size_t x = 3; x <= 353; x += 10) {
      gridSS << x << "," << y << "\n";
    }
  }
  // storage zone (12 x 72 = 864 sites)
  for (size_t y = 56; y <= 111; y += 5) {
    for (size_t x = 0; x <= 355; x += 5) {
      gridSS << x << "," << y << "\n";
    }
  }
  // readout zone (4 x 72 = 288 sites)
  for (size_t y = 131; y <= 146; y += 5) {
    for (size_t x = 0; x <= 355; x += 5) {
      gridSS << x << "," << y << "\n";
    }
  }
  // total: 1296 sites
  arch.fromFileStream(archIS, gridSS);
  // create solver
  auto solver = na::SolverFactory::create(arch);
  const auto& circ =
      qasm3::Importer::importf(TEST_CIRCUITS_PATH "/steane.qasm");
  // get operations for solver
  const auto& pairs =
      na::SolverFactory::getOpsForSolver(circ, {qc::Z, 1}, true);
  // solve
  const auto result =
      solver.solve(pairs, static_cast<uint16_t>(circ.getNqubits()), 5,
                   std::nullopt, false, true);
  EXPECT_TRUE(result.sat);
}

TEST(SolverFactory, CreateExceptions) {
  na::Architecture arch;
  // write content to a file
  std::istringstream archIS(R"({
    "name": "Nature",
    "initialZones": [
        "storage"
    ],
    "zones": [
        {
            "name": "entangling",
            "xmin": -300,
            "xmax": 656,
            "ymin": -20,
            "ymax": 46,
            "fidelity": 0.9959
        },
        {
            "name": "storage",
            "xmin": -300,
            "xmax": 656,
            "ymin": 47,
            "ymax": 121,
            "fidelity": 1
        },
        {
            "name": "readout",
            "xmin": -300,
            "xmax": 656,
            "ymin": 122,
            "ymax": 156,
            "fidelity": 0.99
        }
    ],
    "operations": [
        {
            "name": "rz",
            "type": "local",
            "zones": [
                "entangling",
                "storage",
                "readout"
            ],
            "time": 0.5,
            "fidelity": 0.999
        },
        {
            "name": "ry",
            "type": "global",
            "zones": [
                "entangling",
                "storage",
                "readout"
            ],
            "time": 0.5,
            "fidelity": 0.999
        },
        {
            "name": "cz",
            "type": "global",
            "zones": [
                "entangling"
            ],
            "time": 0.2,
            "fidelity": 0.9959
        },
        {
            "name": "measure",
            "type": "global",
            "zones": [
                "readout"
            ],
            "time": 0.2,
            "fidelity": 0.95
        }
    ],
    "decoherence": {
        "t1": 100000000,
        "t2": 1500000
    },
    "interactionRadius": 2,
    "noInteractionRadius": 5,
    "minAtomDistance": 1,
    "shuttling": [
        {
            "rows": 5,
            "columns": 5,
            "xmin": -2.5,
            "xmax": 2.5,
            "ymin": -2.5,
            "ymax": 2.5,
            "move": {
                "speed": 0.55,
                "fidelity": 1
            },
            "load": {
                "time": 20,
                "fidelity": 1
            },
            "store": {
                "time": 20,
                "fidelity": 1
            }
        }
    ]
})");
  std::stringstream gridSS;
  gridSS << "x,y\n";
  // entangling zone (1 x 36 = 36 sites)
  for (size_t y = 0; y <= 0; y += 12) {
    for (size_t x = 3; x <= 353; x += 10) {
      gridSS << x << "," << y << "\n";
    }
  }
  // storage zone (12 x 72 = 864 sites)
  for (size_t y = 56; y <= 111; y += 5) {
    for (size_t x = 0; x <= 355; x += 5) {
      gridSS << x << "," << y << "\n";
    }
  }
  // readout zone (4 x 72 = 288 sites)
  for (size_t y = 131; y <= 146; y += 5) {
    for (size_t x = 0; x <= 355; x += 5) {
      gridSS << x << "," << y << "\n";
    }
  }
  arch.fromFileStream(archIS, gridSS);
  EXPECT_THROW(std::ignore = na::SolverFactory::create(arch),
               std::invalid_argument);

  auto circ = qc::QuantumComputation(3);
  circ.h(0);
  circ.cz(0, 1);
  circ.cecr(0, 1, 2);
  // get operations for solver
  // when the parameter quiet is false and the circuit contains an operation
  // that is not of type Z and does not have 1 control, an exception is thrown
  EXPECT_THROW(std::ignore =
                   na::SolverFactory::getOpsForSolver(circ, {qc::Z, 1}, false),
               std::invalid_argument);
  // At the moment the function can only handle operation types that lead to two
  // operands, in this example the operation has three operands.
  EXPECT_THROW(std::ignore =
                   na::SolverFactory::getOpsForSolver(circ, {qc::ECR, 1}, true),
               std::invalid_argument);
}
