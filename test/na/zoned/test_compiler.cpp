/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "circuit_optimizer/CircuitOptimizer.hpp"
#include "ir/QuantumComputation.hpp"
#include "na/zoned/Compiler.hpp"
#include "qasm3/Importer.hpp"

#include <cstddef>
#include <gtest/gtest.h>
#include <utility>

namespace na::zoned {

constexpr std::string_view architectureSpecification = R"({
  "name": "compiler_architecture",
  "storage_zones": [{
    "zone_id": 0,
    "slms": [{"id": 0, "site_separation": [3, 3], "r": 20, "c": 20, "location": [0, 0]}],
    "offset": [0, 0],
    "dimension": [60, 60]
  }],
  "entanglement_zones": [{
    "zone_id": 0,
    "slms": [
      {"id": 1, "site_separation": [12, 10], "r": 4, "c": 4, "location": [5, 70]},
      {"id": 2, "site_separation": [12, 10], "r": 4, "c": 4, "location": [7, 70]}
    ],
    "offset": [5, 70],
    "dimension": [50, 40]
  }],
  "aods":[{"id": 0, "site_separation": 2, "r": 20, "c": 20}],
  "rydberg_range": [[[5, 70], [55, 110]]]
})";
constexpr std::string_view strictRoutingAgnosticConfiguration = R"({
  "logLevel" : 1,
  "layoutSynthesizerConfig" : {
    "placerConfig" : {
      "useWindow" : true,
      "windowSize" : 10,
      "dynamicPlacement" : true
    },
    "routerConfig" : {
      "method" : "strict"
    }
  },
  "codeGeneratorConfig" : {
    "warnUnsupportedGates" : false
  }
})";
constexpr std::string_view strictRoutingAwareConfiguration = R"({
  "logLevel" : 1,
  "codeGeneratorConfig" : {
    "warnUnsupportedGates" : false
  },
  "layoutSynthesizerConfig" : {
    "placerConfig" : {
      "useWindow" : true,
      "windowMinWidth" : 4,
      "windowRatio" : 1.5,
      "windowShare" : 0.6,
      "method" : "astar",
      "deepeningFactor" : 0.6,
      "deepeningValue" : 0.2,
      "lookaheadFactor": 0.2,
      "reuseLevel": 5.0
    },
    "routerConfig" : {
      "method" : "strict"
    }
  }
})";
constexpr std::string_view relaxedRoutingAwareConfiguration = R"({
  "logLevel" : 1,
  "codeGeneratorConfig" : {
    "warnUnsupportedGates" : false
  },
  "layoutSynthesizerConfig" : {
    "placerConfig" : {
      "useWindow" : true,
      "windowMinWidth" : 4,
      "windowRatio" : 1.5,
      "windowShare" : 0.6,
      "method" : "astar",
      "deepeningFactor" : 0.6,
      "deepeningValue" : 0.2,
      "lookaheadFactor": 0.2,
      "reuseLevel": 5.0
    },
    "routerConfig" : {
      "method" : "relaxed",
      "preferSplit" : 0.0
    }
  }
})";
constexpr std::string_view fastRelaxedRoutingAwareConfiguration = R"({
  "logLevel" : 1,
  "codeGeneratorConfig" : {
    "warnUnsupportedGates" : false
  },
  "layoutSynthesizerConfig" : {
    "placerConfig" : {
      "useWindow" : true,
      "windowMinWidth" : 4,
      "windowRatio" : 1.5,
      "windowShare" : 0.6,
      "method" : "ids",
      "deepeningFactor" : 0.01,
      "deepeningValue" : 0.0,
      "lookaheadFactor": 0.4,
      "reuseLevel": 5.0
    },
    "routerConfig" : {
      "method" : "relaxed",
      "preferSplit" : 0.0
    }
  }
})";
#define COMPILER_TEST(test_name, compiler_type, config)                        \
  TEST(test_name##Test, ConstructorWithoutConfig) {                            \
    Architecture architecture(                                                 \
        Architecture::fromJSONString(architectureSpecification));              \
    /* expected not to lead to a segfault */                                   \
    [[maybe_unused]] compiler_type compiler(architecture);                     \
  }                                                                            \
  class test_name##Test : public ::testing::TestWithParam<std::string> {       \
    compiler_type::Config config_;                                             \
    Architecture architecture_;                                                \
                                                                               \
  protected:                                                                   \
    qc::QuantumComputation circ_;                                              \
    compiler_type compiler_;                                                   \
    test_name##Test()                                                          \
        : config_(nlohmann::json::parse(config)),                              \
          architecture_(                                                       \
              Architecture::fromJSONString(architectureSpecification)),        \
          compiler_(architecture_, config_) {}                                 \
    void SetUp() override { circ_ = qasm3::Importer::importf(GetParam()); }    \
  };                                                                           \
  /*=========================== END TO END TESTS ===========================*/ \
  TEST_P(test_name##Test, EndToEnd) {                                          \
    const auto& code = this->compiler_.compile(this->circ_);                   \
    EXPECT_TRUE(code.validate().first);                                        \
    /*===----------------------------------------------------------------===*/ \
    /* write code to a file with extension .naviz in a directory converted */  \
    std::filesystem::path inputFile(GetParam());                               \
    std::filesystem::path outputFile = inputFile.parent_path() / "converted" / \
                                       #test_name /                            \
                                       (inputFile.stem().string() + ".naviz"); \
    std::filesystem::create_directories(outputFile.parent_path());             \
    std::ofstream output(outputFile);                                          \
    output << code;                                                            \
    /*===----------------------------------------------------------------===*/ \
    double timeSum = 0;                                                        \
    const nlohmann::json stats = this->compiler_.getStatistics();              \
    for (const auto& [key, value] : stats.items()) {                           \
      if (key != "totalTime" && value.is_number()) {                           \
        timeSum += value.get<double>();                                        \
      }                                                                        \
    }                                                                          \
    EXPECT_GE(stats["totalTime"], timeSum);                                    \
  }                                                                            \
  /*========================================================================*/ \
  INSTANTIATE_TEST_SUITE_P(                                                    \
      test_name##TestWithCircuits,      /* Custom instantiation name */        \
      test_name##Test,                  /* Test suite name */                  \
      ::testing::Values(TEST_CIRCUITS), /* Parameters to test with */          \
      [](const ::testing::TestParamInfo<std::string>& pinfo) {                 \
        const std::filesystem::path path(pinfo.param);                         \
        return path.stem().string();                                           \
      })
/*============================== INSTANTIATIONS ==============================*/
COMPILER_TEST(StrictRoutingAgnosticCompiler, RoutingAgnosticCompiler,
              strictRoutingAgnosticConfiguration);
COMPILER_TEST(StrictRoutingAwareCompiler, RoutingAwareCompiler,
              strictRoutingAwareConfiguration);
COMPILER_TEST(RelaxedRoutingAwareCompiler, RoutingAwareCompiler,
              relaxedRoutingAwareConfiguration);
COMPILER_TEST(FastRelaxedRoutingAwareCompiler, RoutingAwareCompiler,
              fastRelaxedRoutingAwareConfiguration);

// Tests that the bug described in issue
// https://github.com/munich-quantum-toolkit/qmap/issues/727 is fixed.
constexpr std::string_view architectureSpecification727 = R"({
  "name": "Architecture with one entanglement and one storage zone",
  "operation_duration": {
    "rydberg_gate": 0.36,
    "single_qubit_gate": 52,
    "atom_transfer": 15
  },
  "operation_fidelity": {
    "rydberg_gate": 0.995,
    "single_qubit_gate": 0.9997,
    "atom_transfer": 0.999
  },
  "qubit_spec": { "T": 1.5e6 },
  "storage_zones": [
    {
      "zone_id": 0,
      "slms": [
        {
          "id": 0,
          "site_separation": [3, 3],
          "r": 5,
          "c": 10,
          "location": [42, 30]
        }
      ],
      "offset": [0, 0],
      "dimension": [297, 57]
    }
  ],
  "entanglement_zones": [
    {
      "zone_id": 0,
      "slms": [
        {
          "id": 1,
          "site_separation": [12, 10],
          "r": 2,
          "c": 4,
          "location": [35, 67]
        },
        {
          "id": 2,
          "site_separation": [12, 10],
          "r": 2,
          "c": 4,
          "location": [37, 67]
        }
      ],
      "offset": [35, 67],
      "dimension": [230, 60]
    }
  ],
  "aods": [{ "id": 0, "site_separation": 2, "r": 100, "c": 100 }],
  "rydberg_range": [
    [
      [30, 62],
      [80, 82]
    ]
  ]
})";

TEST(RoutingAwareCompilerTest, Issue727) {
  qc::QuantumComputation circ(50);
  circ.cz(0, 3);
  const auto arch = Architecture::fromJSONString(architectureSpecification727);
  RoutingAwareCompiler compiler(arch);
  const auto& code = compiler.compile(circ);
  EXPECT_TRUE(code.validate().first);
}

// Tests that the bug described in issue
// https://github.com/munich-quantum-toolkit/qmap/issues/792 is fixed.
constexpr std::string_view architectureSpecification792 = R"({
    "name": "Architecture with one entanglement and one storage zone",
    "operation_duration": {"rydberg_gate": 5, "single_qubit_gate": 10, "atom_transfer": 15},
    "operation_fidelity": {"rydberg_gate": 0.995, "single_qubit_gate": 0.9997, "atom_transfer": 0.999},
    "qubit_spec": {"T": 1.5e6},
    "storage_zones": [{
      "zone_id": 0,
      "slms": [{"id": 0, "site_separation": [3, 3], "r": 2, "c": 50, "location": [15, 0]}],
      "offset": [15, 0],
      "dimension": [260, 3]
      }],
    "entanglement_zones": [{
          "zone_id": 0,
          "slms": [
              {"id": 1, "site_separation": [12, 10], "r": 1, "c": 27, "location": [5, 20]},
              {"id": 2, "site_separation": [12, 10], "r": 1, "c": 27, "location": [7, 20]}
          ],
          "offset": [5, 20],
          "dimension": [290, 10]
      }],
      "aods": [{"id": 0, "site_separation": 2, "r": 10, "c": 10}],
      "rydberg_range": [
          [
              [0, 17],
              [290, 33]
          ]
      ]
  })";

// this configuration was not used in the original issue. It purposefully
// deviates from the default by increasing the window size to fix issue 792.
constexpr std::string_view routingAwareConfiguration792 = R"({
  "layoutSynthesizerConfig" : {
    "placerConfig" : {
      "windowShare" : 1.0
    }
  }
})";

// the QASM file is reduced
constexpr std::string_view circuit792 = R"(OPENQASM 2.0;
include "qelib1.inc";
qreg q[64];
cz q[2],q[3];
cz q[6],q[7];
cz q[13],q[14];
cz q[15],q[16];
cz q[17],q[18];
cz q[19],q[20];
cz q[22],q[23];
cz q[24],q[25];
cz q[26],q[27];
cz q[28],q[29];
cz q[30],q[31];
cz q[35],q[36];
cz q[37],q[38];
cz q[39],q[40];
cz q[41],q[42];
cz q[44],q[45];
cz q[46],q[47];
cz q[48],q[49];
cz q[50],q[51];
cz q[52],q[53];
//============//
cz q[1],q[2];
cz q[3],q[13];
cz q[5],q[6];
cz q[7],q[17];
cz q[12],q[22];
cz q[14],q[15];
cz q[16],q[26];
cz q[18],q[19];
cz q[20],q[30];
cz q[23],q[24];
cz q[25],q[35];
cz q[27],q[28];
cz q[29],q[39];
cz q[34],q[44];
cz q[36],q[37];
cz q[38],q[48];
cz q[40],q[41];
cz q[42],q[52];
cz q[45],q[46];
cz q[49],q[50];
)";

TEST(RoutingAwareCompilerTest, Issue792) {
  const auto qc = qasm3::Importer::imports(circuit792.data());
  const auto arch = Architecture::fromJSONString(architectureSpecification792);
  const auto config = nlohmann::json::parse(routingAwareConfiguration792);
  RoutingAwareCompiler compiler(arch, config);
  EXPECT_NO_THROW(std::ignore = compiler.compile(qc));
}

constexpr std::string_view architectureSpecificationGraphstate = R"({
  "name": "full_compute_store_architecture",
  "operation_duration": {
    "rydberg_gate": 0.36,
    "single_qubit_gate": 52,
    "atom_transfer": 15
  },
  "operation_fidelity": {
    "rydberg_gate": 0.995,
    "single_qubit_gate": 0.9997,
    "atom_transfer": 0.999
  },
  "qubit_spec": {
    "T": 1.5e6
  },
  "storage_zones": [
    {
      "zone_id": 0,
      "slms": [
        {
          "id": 0,
          "site_separation": [3, 3],
          "r": 20,
          "c": 100,
          "location": [0, 0]
        }
      ],
      "offset": [0, 0],
      "dimension": [297, 57]
    }
  ],
  "entanglement_zones": [
    {
      "zone_id": 0,
      "slms": [
        {
          "id": 1,
          "site_separation": [12, 10],
          "r": 7,
          "c": 20,
          "location": [35, 67]
        },
        {
          "id": 2,
          "site_separation": [12, 10],
          "r": 7,
          "c": 20,
          "location": [37, 67]
        }
      ],
      "offset": [35, 67],
      "dimension": [230, 60]
    }
  ],
  "aods": [
    {
      "id": 0,
      "site_separation": 2,
      "r": 100,
      "c": 100
    }
  ],
  "arch_range": [
    [0, 0],
    [297, 162]
  ],
  "rydberg_range": [
    [
      [30, 62],
      [270, 132]
    ]
  ]
})";

constexpr std::string_view circuitGraphstate = R"(OPENQASM 3.0;
include "stdgates.inc";
qubit[20] q;
bit[20] c;
h q[0];
h q[1];
h q[2];
h q[3];
h q[4];
h q[5];
h q[6];
cz q[1], q[6];
cz q[4], q[6];
h q[7];
h q[8];
cz q[2], q[8];
h q[9];
cz q[2], q[9];
h q[10];
cz q[3], q[10];
h q[11];
cz q[5], q[11];
h q[12];
cz q[8], q[12];
h q[13];
cz q[0], q[13];
cz q[4], q[13];
h q[14];
cz q[0], q[14];
cz q[5], q[14];
h q[15];
cz q[1], q[15];
h q[16];
cz q[10], q[16];
cz q[15], q[16];
h q[17];
cz q[3], q[17];
cz q[9], q[17];
h q[18];
cz q[7], q[18];
cz q[11], q[18];
h q[19];
cz q[7], q[19];
cz q[12], q[19];)";

TEST(FastRoutingAwareCompilerTest, ImprovedLoadingStoring) {
  const auto qc = qasm3::Importer::imports(circuitGraphstate.data());
  const auto arch =
      Architecture::fromJSONString(architectureSpecificationGraphstate);
  const RoutingAwareCompiler::Config config = {
      .layoutSynthesizerConfig =
          {.placerConfig = HeuristicPlacer::Config::createForMethod(
               HeuristicPlacer::Config::Method::IDS),
           .routerConfig = {.method =
                                IndependentSetRouter::Config::Method::RELAXED}},
      .codeGeneratorConfig = {.warnUnsupportedGates = false}};
  RoutingAwareCompiler compiler(arch, config);
  EXPECT_NO_THROW(std::ignore = compiler.compile(qc));
}
} // namespace na::zoned
