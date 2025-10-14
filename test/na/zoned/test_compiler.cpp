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
constexpr std::string_view routingAgnosticConfiguration = R"({
  "logLevel" : 1,
  "layoutSynthesizerConfig" : {
    "placerConfig" : {
      "useWindow" : true,
      "windowSize" : 10,
      "dynamicPlacement" : true
    }
  },
  "codeGeneratorConfig" : {
    "parkingOffset" : 1,
    "warnUnsupportedGates" : false
  }
})";
constexpr std::string_view routingAwareConfiguration = R"({
  "logLevel" : 1,
  "codeGeneratorConfig" : {
    "parkingOffset" : 1,
    "warnUnsupportedGates" : false
  },
  "layoutSynthesizerConfig" : {
    "placerConfig" : {
      "useWindow" : true,
      "windowMinWidth" : 4,
      "windowRatio" : 1.5,
      "windowShare" : 0.6,
      "deepeningFactor" : 0.6,
      "deepeningValue" : 0.2,
      "lookaheadFactor": 0.2,
      "reuseLevel": 5.0
    }
  }
})";
#define COMPILER_TEST(compiler_type, config)                                   \
  TEST(compiler_type##Test, ConstructorWithoutConfig) {                        \
    Architecture architecture(                                                 \
        Architecture::fromJSONString(architectureSpecification));              \
    /* expected not to lead to a segfault */                                   \
    [[maybe_unused]] compiler_type compiler(architecture);                     \
  }                                                                            \
  class compiler_type##Test : public ::testing::TestWithParam<std::string> {   \
    compiler_type::Config config_;                                             \
    Architecture architecture_;                                                \
                                                                               \
  protected:                                                                   \
    qc::QuantumComputation circ_;                                              \
    compiler_type compiler_;                                                   \
    compiler_type##Test()                                                      \
        : config_(nlohmann::json::parse(config)),                              \
          architecture_(                                                       \
              Architecture::fromJSONString(architectureSpecification)),        \
          compiler_(architecture_, config_) {}                                 \
    void SetUp() override { circ_ = qasm3::Importer::importf(GetParam()); }    \
  };                                                                           \
  /*=========================== END TO END TESTS ===========================*/ \
  TEST_P(compiler_type##Test, EndToEnd) {                                      \
    const auto& code = this->compiler_.compile(this->circ_);                   \
    EXPECT_TRUE(code.validate().first);                                        \
    /*===----------------------------------------------------------------===*/ \
    /* write code to a file with extension .naviz in a directory converted */  \
    std::filesystem::path inputFile(GetParam());                               \
    std::filesystem::path outputFile = inputFile.parent_path() / "converted" / \
                                       #compiler_type /                        \
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
      compiler_type##TestWithCircuits,  /* Custom instantiation name */        \
      compiler_type##Test,              /* Test suite name */                  \
      ::testing::Values(TEST_CIRCUITS), /* Parameters to test with */          \
      [](const ::testing::TestParamInfo<std::string>& pinfo) {                 \
        const auto& path = pinfo.param;                                        \
        const auto& filename = path.substr(path.find_last_of("/") + 1);        \
        return filename.substr(0, filename.find_last_of("."));                 \
      })
/*============================== INSTANTIATIONS ==============================*/
COMPILER_TEST(RoutingAgnosticCompiler, routingAgnosticConfiguration);
COMPILER_TEST(RoutingAwareCompiler, routingAwareConfiguration);

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

constexpr std::string_view circuit792 = R"(OPENQASM 2.0;
include "qelib1.inc";
gate circuit_168 q0,q1,q2,q3,q4,q5,q6,q7,q8,q9,q10,q11,q12,q13,q14,q15,q16,q17,q18,q19,q20,q21,q22,q23,q24,q25,q26,q27,q28,q29,q30,q31,q32,q33,q34,q35,q36,q37,q38,q39,q40,q41,q42,q43,q44,q45,q46,q47,q48,q49,q50,q51,q52,q53,q54,q55,q56,q57,q58,q59,q60,q61,q62,q63 { ry(-pi/4) q0; ry(-pi/4) q1; ry(-pi/4) q2; ry(-pi/4) q3; ry(-pi/4) q4; ry(-pi/4) q5; ry(-pi/4) q6; ry(-pi/4) q7; ry(-pi/4) q8; ry(-pi/4) q9; ry(-pi/4) q10; ry(-pi/4) q11; ry(-pi/4) q12; ry(-pi/4) q13; ry(-pi/4) q14; ry(-pi/4) q15; ry(-pi/4) q16; ry(-pi/4) q17; ry(-pi/4) q18; ry(-pi/4) q19; ry(-pi/4) q20; ry(-pi/4) q21; ry(-pi/4) q22; ry(-pi/4) q23; ry(-pi/4) q24; ry(-pi/4) q25; ry(-pi/4) q26; ry(-pi/4) q27; ry(-pi/4) q28; ry(-pi/4) q29; ry(-pi/4) q30; ry(-pi/4) q31; ry(-pi/4) q32; ry(-pi/4) q33; ry(-pi/4) q34; ry(-pi/4) q35; ry(-pi/4) q36; ry(-pi/4) q37; ry(-pi/4) q38; ry(-pi/4) q39; ry(-pi/4) q40; ry(-pi/4) q41; ry(-pi/4) q42; ry(-pi/4) q43; ry(-pi/4) q44; ry(-pi/4) q45; ry(-pi/4) q46; ry(-pi/4) q47; ry(-pi/4) q48; ry(-pi/4) q49; ry(-pi/4) q50; ry(-pi/4) q51; ry(-pi/4) q52; ry(-pi/4) q53; ry(-pi/4) q54; ry(-pi/4) q55; ry(-pi/4) q56; ry(-pi/4) q57; ry(-pi/4) q58; ry(-pi/4) q59; ry(-pi/4) q60; ry(-pi/4) q61; ry(-pi/4) q62; ry(-pi/4) q63; }
gate circuit_171 q0,q1,q2,q3,q4,q5,q6,q7,q8,q9,q10,q11,q12,q13,q14,q15,q16,q17,q18,q19,q20,q21,q22,q23,q24,q25,q26,q27,q28,q29,q30,q31,q32,q33,q34,q35,q36,q37,q38,q39,q40,q41,q42,q43,q44,q45,q46,q47,q48,q49,q50,q51,q52,q53,q54,q55,q56,q57,q58,q59,q60,q61,q62,q63 { ry(pi/4) q0; ry(pi/4) q1; ry(pi/4) q2; ry(pi/4) q3; ry(pi/4) q4; ry(pi/4) q5; ry(pi/4) q6; ry(pi/4) q7; ry(pi/4) q8; ry(pi/4) q9; ry(pi/4) q10; ry(pi/4) q11; ry(pi/4) q12; ry(pi/4) q13; ry(pi/4) q14; ry(pi/4) q15; ry(pi/4) q16; ry(pi/4) q17; ry(pi/4) q18; ry(pi/4) q19; ry(pi/4) q20; ry(pi/4) q21; ry(pi/4) q22; ry(pi/4) q23; ry(pi/4) q24; ry(pi/4) q25; ry(pi/4) q26; ry(pi/4) q27; ry(pi/4) q28; ry(pi/4) q29; ry(pi/4) q30; ry(pi/4) q31; ry(pi/4) q32; ry(pi/4) q33; ry(pi/4) q34; ry(pi/4) q35; ry(pi/4) q36; ry(pi/4) q37; ry(pi/4) q38; ry(pi/4) q39; ry(pi/4) q40; ry(pi/4) q41; ry(pi/4) q42; ry(pi/4) q43; ry(pi/4) q44; ry(pi/4) q45; ry(pi/4) q46; ry(pi/4) q47; ry(pi/4) q48; ry(pi/4) q49; ry(pi/4) q50; ry(pi/4) q51; ry(pi/4) q52; ry(pi/4) q53; ry(pi/4) q54; ry(pi/4) q55; ry(pi/4) q56; ry(pi/4) q57; ry(pi/4) q58; ry(pi/4) q59; ry(pi/4) q60; ry(pi/4) q61; ry(pi/4) q62; ry(pi/4) q63; }
gate circuit_174 q0,q1,q2,q3,q4,q5,q6,q7,q8,q9,q10,q11,q12,q13,q14,q15,q16,q17,q18,q19,q20,q21,q22,q23,q24,q25,q26,q27,q28,q29,q30,q31,q32,q33,q34,q35,q36,q37,q38,q39,q40,q41,q42,q43,q44,q45,q46,q47,q48,q49,q50,q51,q52,q53,q54,q55,q56,q57,q58,q59,q60,q61,q62,q63 { ry(-pi/4) q0; ry(-pi/4) q1; ry(-pi/4) q2; ry(-pi/4) q3; ry(-pi/4) q4; ry(-pi/4) q5; ry(-pi/4) q6; ry(-pi/4) q7; ry(-pi/4) q8; ry(-pi/4) q9; ry(-pi/4) q10; ry(-pi/4) q11; ry(-pi/4) q12; ry(-pi/4) q13; ry(-pi/4) q14; ry(-pi/4) q15; ry(-pi/4) q16; ry(-pi/4) q17; ry(-pi/4) q18; ry(-pi/4) q19; ry(-pi/4) q20; ry(-pi/4) q21; ry(-pi/4) q22; ry(-pi/4) q23; ry(-pi/4) q24; ry(-pi/4) q25; ry(-pi/4) q26; ry(-pi/4) q27; ry(-pi/4) q28; ry(-pi/4) q29; ry(-pi/4) q30; ry(-pi/4) q31; ry(-pi/4) q32; ry(-pi/4) q33; ry(-pi/4) q34; ry(-pi/4) q35; ry(-pi/4) q36; ry(-pi/4) q37; ry(-pi/4) q38; ry(-pi/4) q39; ry(-pi/4) q40; ry(-pi/4) q41; ry(-pi/4) q42; ry(-pi/4) q43; ry(-pi/4) q44; ry(-pi/4) q45; ry(-pi/4) q46; ry(-pi/4) q47; ry(-pi/4) q48; ry(-pi/4) q49; ry(-pi/4) q50; ry(-pi/4) q51; ry(-pi/4) q52; ry(-pi/4) q53; ry(-pi/4) q54; ry(-pi/4) q55; ry(-pi/4) q56; ry(-pi/4) q57; ry(-pi/4) q58; ry(-pi/4) q59; ry(-pi/4) q60; ry(-pi/4) q61; ry(-pi/4) q62; ry(-pi/4) q63; }
gate circuit_177 q0,q1,q2,q3,q4,q5,q6,q7,q8,q9,q10,q11,q12,q13,q14,q15,q16,q17,q18,q19,q20,q21,q22,q23,q24,q25,q26,q27,q28,q29,q30,q31,q32,q33,q34,q35,q36,q37,q38,q39,q40,q41,q42,q43,q44,q45,q46,q47,q48,q49,q50,q51,q52,q53,q54,q55,q56,q57,q58,q59,q60,q61,q62,q63 { ry(pi/4) q0; ry(pi/4) q1; ry(pi/4) q2; ry(pi/4) q3; ry(pi/4) q4; ry(pi/4) q5; ry(pi/4) q6; ry(pi/4) q7; ry(pi/4) q8; ry(pi/4) q9; ry(pi/4) q10; ry(pi/4) q11; ry(pi/4) q12; ry(pi/4) q13; ry(pi/4) q14; ry(pi/4) q15; ry(pi/4) q16; ry(pi/4) q17; ry(pi/4) q18; ry(pi/4) q19; ry(pi/4) q20; ry(pi/4) q21; ry(pi/4) q22; ry(pi/4) q23; ry(pi/4) q24; ry(pi/4) q25; ry(pi/4) q26; ry(pi/4) q27; ry(pi/4) q28; ry(pi/4) q29; ry(pi/4) q30; ry(pi/4) q31; ry(pi/4) q32; ry(pi/4) q33; ry(pi/4) q34; ry(pi/4) q35; ry(pi/4) q36; ry(pi/4) q37; ry(pi/4) q38; ry(pi/4) q39; ry(pi/4) q40; ry(pi/4) q41; ry(pi/4) q42; ry(pi/4) q43; ry(pi/4) q44; ry(pi/4) q45; ry(pi/4) q46; ry(pi/4) q47; ry(pi/4) q48; ry(pi/4) q49; ry(pi/4) q50; ry(pi/4) q51; ry(pi/4) q52; ry(pi/4) q53; ry(pi/4) q54; ry(pi/4) q55; ry(pi/4) q56; ry(pi/4) q57; ry(pi/4) q58; ry(pi/4) q59; ry(pi/4) q60; ry(pi/4) q61; ry(pi/4) q62; ry(pi/4) q63; }
gate circuit_180 q0,q1,q2,q3,q4,q5,q6,q7,q8,q9,q10,q11,q12,q13,q14,q15,q16,q17,q18,q19,q20,q21,q22,q23,q24,q25,q26,q27,q28,q29,q30,q31,q32,q33,q34,q35,q36,q37,q38,q39,q40,q41,q42,q43,q44,q45,q46,q47,q48,q49,q50,q51,q52,q53,q54,q55,q56,q57,q58,q59,q60,q61,q62,q63 { ry(-pi/4) q0; ry(-pi/4) q1; ry(-pi/4) q2; ry(-pi/4) q3; ry(-pi/4) q4; ry(-pi/4) q5; ry(-pi/4) q6; ry(-pi/4) q7; ry(-pi/4) q8; ry(-pi/4) q9; ry(-pi/4) q10; ry(-pi/4) q11; ry(-pi/4) q12; ry(-pi/4) q13; ry(-pi/4) q14; ry(-pi/4) q15; ry(-pi/4) q16; ry(-pi/4) q17; ry(-pi/4) q18; ry(-pi/4) q19; ry(-pi/4) q20; ry(-pi/4) q21; ry(-pi/4) q22; ry(-pi/4) q23; ry(-pi/4) q24; ry(-pi/4) q25; ry(-pi/4) q26; ry(-pi/4) q27; ry(-pi/4) q28; ry(-pi/4) q29; ry(-pi/4) q30; ry(-pi/4) q31; ry(-pi/4) q32; ry(-pi/4) q33; ry(-pi/4) q34; ry(-pi/4) q35; ry(-pi/4) q36; ry(-pi/4) q37; ry(-pi/4) q38; ry(-pi/4) q39; ry(-pi/4) q40; ry(-pi/4) q41; ry(-pi/4) q42; ry(-pi/4) q43; ry(-pi/4) q44; ry(-pi/4) q45; ry(-pi/4) q46; ry(-pi/4) q47; ry(-pi/4) q48; ry(-pi/4) q49; ry(-pi/4) q50; ry(-pi/4) q51; ry(-pi/4) q52; ry(-pi/4) q53; ry(-pi/4) q54; ry(-pi/4) q55; ry(-pi/4) q56; ry(-pi/4) q57; ry(-pi/4) q58; ry(-pi/4) q59; ry(-pi/4) q60; ry(-pi/4) q61; ry(-pi/4) q62; ry(-pi/4) q63; }
gate circuit_183 q0,q1,q2,q3,q4,q5,q6,q7,q8,q9,q10,q11,q12,q13,q14,q15,q16,q17,q18,q19,q20,q21,q22,q23,q24,q25,q26,q27,q28,q29,q30,q31,q32,q33,q34,q35,q36,q37,q38,q39,q40,q41,q42,q43,q44,q45,q46,q47,q48,q49,q50,q51,q52,q53,q54,q55,q56,q57,q58,q59,q60,q61,q62,q63 { ry(pi/4) q0; ry(pi/4) q1; ry(pi/4) q2; ry(pi/4) q3; ry(pi/4) q4; ry(pi/4) q5; ry(pi/4) q6; ry(pi/4) q7; ry(pi/4) q8; ry(pi/4) q9; ry(pi/4) q10; ry(pi/4) q11; ry(pi/4) q12; ry(pi/4) q13; ry(pi/4) q14; ry(pi/4) q15; ry(pi/4) q16; ry(pi/4) q17; ry(pi/4) q18; ry(pi/4) q19; ry(pi/4) q20; ry(pi/4) q21; ry(pi/4) q22; ry(pi/4) q23; ry(pi/4) q24; ry(pi/4) q25; ry(pi/4) q26; ry(pi/4) q27; ry(pi/4) q28; ry(pi/4) q29; ry(pi/4) q30; ry(pi/4) q31; ry(pi/4) q32; ry(pi/4) q33; ry(pi/4) q34; ry(pi/4) q35; ry(pi/4) q36; ry(pi/4) q37; ry(pi/4) q38; ry(pi/4) q39; ry(pi/4) q40; ry(pi/4) q41; ry(pi/4) q42; ry(pi/4) q43; ry(pi/4) q44; ry(pi/4) q45; ry(pi/4) q46; ry(pi/4) q47; ry(pi/4) q48; ry(pi/4) q49; ry(pi/4) q50; ry(pi/4) q51; ry(pi/4) q52; ry(pi/4) q53; ry(pi/4) q54; ry(pi/4) q55; ry(pi/4) q56; ry(pi/4) q57; ry(pi/4) q58; ry(pi/4) q59; ry(pi/4) q60; ry(pi/4) q61; ry(pi/4) q62; ry(pi/4) q63; }
gate circuit_186 q0,q1,q2,q3,q4,q5,q6,q7,q8,q9,q10,q11,q12,q13,q14,q15,q16,q17,q18,q19,q20,q21,q22,q23,q24,q25,q26,q27,q28,q29,q30,q31,q32,q33,q34,q35,q36,q37,q38,q39,q40,q41,q42,q43,q44,q45,q46,q47,q48,q49,q50,q51,q52,q53,q54,q55,q56,q57,q58,q59,q60,q61,q62,q63 { ry(-pi/4) q0; ry(-pi/4) q1; ry(-pi/4) q2; ry(-pi/4) q3; ry(-pi/4) q4; ry(-pi/4) q5; ry(-pi/4) q6; ry(-pi/4) q7; ry(-pi/4) q8; ry(-pi/4) q9; ry(-pi/4) q10; ry(-pi/4) q11; ry(-pi/4) q12; ry(-pi/4) q13; ry(-pi/4) q14; ry(-pi/4) q15; ry(-pi/4) q16; ry(-pi/4) q17; ry(-pi/4) q18; ry(-pi/4) q19; ry(-pi/4) q20; ry(-pi/4) q21; ry(-pi/4) q22; ry(-pi/4) q23; ry(-pi/4) q24; ry(-pi/4) q25; ry(-pi/4) q26; ry(-pi/4) q27; ry(-pi/4) q28; ry(-pi/4) q29; ry(-pi/4) q30; ry(-pi/4) q31; ry(-pi/4) q32; ry(-pi/4) q33; ry(-pi/4) q34; ry(-pi/4) q35; ry(-pi/4) q36; ry(-pi/4) q37; ry(-pi/4) q38; ry(-pi/4) q39; ry(-pi/4) q40; ry(-pi/4) q41; ry(-pi/4) q42; ry(-pi/4) q43; ry(-pi/4) q44; ry(-pi/4) q45; ry(-pi/4) q46; ry(-pi/4) q47; ry(-pi/4) q48; ry(-pi/4) q49; ry(-pi/4) q50; ry(-pi/4) q51; ry(-pi/4) q52; ry(-pi/4) q53; ry(-pi/4) q54; ry(-pi/4) q55; ry(-pi/4) q56; ry(-pi/4) q57; ry(-pi/4) q58; ry(-pi/4) q59; ry(-pi/4) q60; ry(-pi/4) q61; ry(-pi/4) q62; ry(-pi/4) q63; }
gate circuit_189 q0,q1,q2,q3,q4,q5,q6,q7,q8,q9,q10,q11,q12,q13,q14,q15,q16,q17,q18,q19,q20,q21,q22,q23,q24,q25,q26,q27,q28,q29,q30,q31,q32,q33,q34,q35,q36,q37,q38,q39,q40,q41,q42,q43,q44,q45,q46,q47,q48,q49,q50,q51,q52,q53,q54,q55,q56,q57,q58,q59,q60,q61,q62,q63 { ry(pi/4) q0; ry(pi/4) q1; ry(pi/4) q2; ry(pi/4) q3; ry(pi/4) q4; ry(pi/4) q5; ry(pi/4) q6; ry(pi/4) q7; ry(pi/4) q8; ry(pi/4) q9; ry(pi/4) q10; ry(pi/4) q11; ry(pi/4) q12; ry(pi/4) q13; ry(pi/4) q14; ry(pi/4) q15; ry(pi/4) q16; ry(pi/4) q17; ry(pi/4) q18; ry(pi/4) q19; ry(pi/4) q20; ry(pi/4) q21; ry(pi/4) q22; ry(pi/4) q23; ry(pi/4) q24; ry(pi/4) q25; ry(pi/4) q26; ry(pi/4) q27; ry(pi/4) q28; ry(pi/4) q29; ry(pi/4) q30; ry(pi/4) q31; ry(pi/4) q32; ry(pi/4) q33; ry(pi/4) q34; ry(pi/4) q35; ry(pi/4) q36; ry(pi/4) q37; ry(pi/4) q38; ry(pi/4) q39; ry(pi/4) q40; ry(pi/4) q41; ry(pi/4) q42; ry(pi/4) q43; ry(pi/4) q44; ry(pi/4) q45; ry(pi/4) q46; ry(pi/4) q47; ry(pi/4) q48; ry(pi/4) q49; ry(pi/4) q50; ry(pi/4) q51; ry(pi/4) q52; ry(pi/4) q53; ry(pi/4) q54; ry(pi/4) q55; ry(pi/4) q56; ry(pi/4) q57; ry(pi/4) q58; ry(pi/4) q59; ry(pi/4) q60; ry(pi/4) q61; ry(pi/4) q62; ry(pi/4) q63; }
qreg q[64];
barrier q[0],q[1],q[2],q[3],q[4],q[5],q[6],q[7],q[8],q[9],q[10],q[11],q[12],q[13],q[14],q[15],q[16],q[17],q[18],q[19],q[20],q[21],q[22],q[23],q[24],q[25],q[26],q[27],q[28],q[29],q[30],q[31],q[32],q[33],q[34],q[35],q[36],q[37],q[38],q[39],q[40],q[41],q[42],q[43],q[44],q[45],q[46],q[47],q[48],q[49],q[50],q[51],q[52],q[53],q[54],q[55],q[56],q[57],q[58],q[59],q[60],q[61],q[62],q[63];
circuit_168 q[0],q[1],q[2],q[3],q[4],q[5],q[6],q[7],q[8],q[9],q[10],q[11],q[12],q[13],q[14],q[15],q[16],q[17],q[18],q[19],q[20],q[21],q[22],q[23],q[24],q[25],q[26],q[27],q[28],q[29],q[30],q[31],q[32],q[33],q[34],q[35],q[36],q[37],q[38],q[39],q[40],q[41],q[42],q[43],q[44],q[45],q[46],q[47],q[48],q[49],q[50],q[51],q[52],q[53],q[54],q[55],q[56],q[57],q[58],q[59],q[60],q[61],q[62],q[63];
barrier q[0],q[1],q[2],q[3],q[4],q[5],q[6],q[7],q[8],q[9],q[10],q[11],q[12],q[13],q[14],q[15],q[16],q[17],q[18],q[19],q[20],q[21],q[22],q[23],q[24],q[25],q[26],q[27],q[28],q[29],q[30],q[31],q[32],q[33],q[34],q[35],q[36],q[37],q[38],q[39],q[40],q[41],q[42],q[43],q[44],q[45],q[46],q[47],q[48],q[49],q[50],q[51],q[52],q[53],q[54],q[55],q[56],q[57],q[58],q[59],q[60],q[61],q[62],q[63];
z q[1];
z q[2];
z q[3];
z q[5];
z q[6];
z q[7];
z q[9];
z q[13];
z q[15];
z q[16];
z q[17];
z q[19];
z q[20];
z q[21];
z q[22];
z q[24];
z q[25];
z q[26];
z q[28];
z q[29];
z q[30];
z q[35];
z q[37];
z q[38];
z q[39];
z q[41];
z q[42];
z q[43];
z q[44];
z q[46];
z q[47];
z q[48];
z q[50];
z q[51];
z q[52];
z q[59];
z q[63];
barrier q[0],q[1],q[2],q[3],q[4],q[5],q[6],q[7],q[8],q[9],q[10],q[11],q[12],q[13],q[14],q[15],q[16],q[17],q[18],q[19],q[20],q[21],q[22],q[23],q[24],q[25],q[26],q[27],q[28],q[29],q[30],q[31],q[32],q[33],q[34],q[35],q[36],q[37],q[38],q[39],q[40],q[41],q[42],q[43],q[44],q[45],q[46],q[47],q[48],q[49],q[50],q[51],q[52],q[53],q[54],q[55],q[56],q[57],q[58],q[59],q[60],q[61],q[62],q[63];
circuit_171 q[0],q[1],q[2],q[3],q[4],q[5],q[6],q[7],q[8],q[9],q[10],q[11],q[12],q[13],q[14],q[15],q[16],q[17],q[18],q[19],q[20],q[21],q[22],q[23],q[24],q[25],q[26],q[27],q[28],q[29],q[30],q[31],q[32],q[33],q[34],q[35],q[36],q[37],q[38],q[39],q[40],q[41],q[42],q[43],q[44],q[45],q[46],q[47],q[48],q[49],q[50],q[51],q[52],q[53],q[54],q[55],q[56],q[57],q[58],q[59],q[60],q[61],q[62],q[63];
barrier q[0],q[1],q[2],q[3],q[4],q[5],q[6],q[7],q[8],q[9],q[10],q[11],q[12],q[13],q[14],q[15],q[16],q[17],q[18],q[19],q[20],q[21],q[22],q[23],q[24],q[25],q[26],q[27],q[28],q[29],q[30],q[31],q[32],q[33],q[34],q[35],q[36],q[37],q[38],q[39],q[40],q[41],q[42],q[43],q[44],q[45],q[46],q[47],q[48],q[49],q[50],q[51],q[52],q[53],q[54],q[55],q[56],q[57],q[58],q[59],q[60],q[61],q[62],q[63];
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
barrier q[0],q[1],q[2],q[3],q[4],q[5],q[6],q[7],q[8],q[9],q[10],q[11],q[12],q[13],q[14],q[15],q[16],q[17],q[18],q[19],q[20],q[21],q[22],q[23],q[24],q[25],q[26],q[27],q[28],q[29],q[30],q[31],q[32],q[33],q[34],q[35],q[36],q[37],q[38],q[39],q[40],q[41],q[42],q[43],q[44],q[45],q[46],q[47],q[48],q[49],q[50],q[51],q[52],q[53],q[54],q[55],q[56],q[57],q[58],q[59],q[60],q[61],q[62],q[63];
circuit_174 q[0],q[1],q[2],q[3],q[4],q[5],q[6],q[7],q[8],q[9],q[10],q[11],q[12],q[13],q[14],q[15],q[16],q[17],q[18],q[19],q[20],q[21],q[22],q[23],q[24],q[25],q[26],q[27],q[28],q[29],q[30],q[31],q[32],q[33],q[34],q[35],q[36],q[37],q[38],q[39],q[40],q[41],q[42],q[43],q[44],q[45],q[46],q[47],q[48],q[49],q[50],q[51],q[52],q[53],q[54],q[55],q[56],q[57],q[58],q[59],q[60],q[61],q[62],q[63];
barrier q[0],q[1],q[2],q[3],q[4],q[5],q[6],q[7],q[8],q[9],q[10],q[11],q[12],q[13],q[14],q[15],q[16],q[17],q[18],q[19],q[20],q[21],q[22],q[23],q[24],q[25],q[26],q[27],q[28],q[29],q[30],q[31],q[32],q[33],q[34],q[35],q[36],q[37],q[38],q[39],q[40],q[41],q[42],q[43],q[44],q[45],q[46],q[47],q[48],q[49],q[50],q[51],q[52],q[53],q[54],q[55],q[56],q[57],q[58],q[59],q[60],q[61],q[62],q[63];
z q[3];
z q[7];
z q[14];
z q[16];
z q[18];
z q[20];
z q[23];
z q[25];
z q[27];
z q[29];
z q[31];
z q[36];
z q[38];
z q[40];
z q[42];
z q[45];
z q[47];
z q[49];
z q[51];
z q[53];
barrier q[0],q[1],q[2],q[3],q[4],q[5],q[6],q[7],q[8],q[9],q[10],q[11],q[12],q[13],q[14],q[15],q[16],q[17],q[18],q[19],q[20],q[21],q[22],q[23],q[24],q[25],q[26],q[27],q[28],q[29],q[30],q[31],q[32],q[33],q[34],q[35],q[36],q[37],q[38],q[39],q[40],q[41],q[42],q[43],q[44],q[45],q[46],q[47],q[48],q[49],q[50],q[51],q[52],q[53],q[54],q[55],q[56],q[57],q[58],q[59],q[60],q[61],q[62],q[63];
circuit_177 q[0],q[1],q[2],q[3],q[4],q[5],q[6],q[7],q[8],q[9],q[10],q[11],q[12],q[13],q[14],q[15],q[16],q[17],q[18],q[19],q[20],q[21],q[22],q[23],q[24],q[25],q[26],q[27],q[28],q[29],q[30],q[31],q[32],q[33],q[34],q[35],q[36],q[37],q[38],q[39],q[40],q[41],q[42],q[43],q[44],q[45],q[46],q[47],q[48],q[49],q[50],q[51],q[52],q[53],q[54],q[55],q[56],q[57],q[58],q[59],q[60],q[61],q[62],q[63];
barrier q[0],q[1],q[2],q[3],q[4],q[5],q[6],q[7],q[8],q[9],q[10],q[11],q[12],q[13],q[14],q[15],q[16],q[17],q[18],q[19],q[20],q[21],q[22],q[23],q[24],q[25],q[26],q[27],q[28],q[29],q[30],q[31],q[32],q[33],q[34],q[35],q[36],q[37],q[38],q[39],q[40],q[41],q[42],q[43],q[44],q[45],q[46],q[47],q[48],q[49],q[50],q[51],q[52],q[53],q[54],q[55],q[56],q[57],q[58],q[59],q[60],q[61],q[62],q[63];
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
barrier q[0],q[1],q[2],q[3],q[4],q[5],q[6],q[7],q[8],q[9],q[10],q[11],q[12],q[13],q[14],q[15],q[16],q[17],q[18],q[19],q[20],q[21],q[22],q[23],q[24],q[25],q[26],q[27],q[28],q[29],q[30],q[31],q[32],q[33],q[34],q[35],q[36],q[37],q[38],q[39],q[40],q[41],q[42],q[43],q[44],q[45],q[46],q[47],q[48],q[49],q[50],q[51],q[52],q[53],q[54],q[55],q[56],q[57],q[58],q[59],q[60],q[61],q[62],q[63];
cz q[5],q[15];
cz q[9],q[19];
cz q[12],q[13];
cz q[14],q[24];
cz q[16],q[17];
cz q[18],q[28];
cz q[20],q[21];
cz q[25],q[26];
cz q[27],q[37];
cz q[29],q[30];
cz q[31],q[41];
cz q[34],q[35];
cz q[36],q[46];
cz q[38],q[39];
cz q[40],q[50];
cz q[42],q[43];
cz q[47],q[48];
cz q[49],q[59];
cz q[51],q[52];
cz q[53],q[63];
barrier q[0],q[1],q[2],q[3],q[4],q[5],q[6],q[7],q[8],q[9],q[10],q[11],q[12],q[13],q[14],q[15],q[16],q[17],q[18],q[19],q[20],q[21],q[22],q[23],q[24],q[25],q[26],q[27],q[28],q[29],q[30],q[31],q[32],q[33],q[34],q[35],q[36],q[37],q[38],q[39],q[40],q[41],q[42],q[43],q[44],q[45],q[46],q[47],q[48],q[49],q[50],q[51],q[52],q[53],q[54],q[55],q[56],q[57],q[58],q[59],q[60],q[61],q[62],q[63];
circuit_180 q[0],q[1],q[2],q[3],q[4],q[5],q[6],q[7],q[8],q[9],q[10],q[11],q[12],q[13],q[14],q[15],q[16],q[17],q[18],q[19],q[20],q[21],q[22],q[23],q[24],q[25],q[26],q[27],q[28],q[29],q[30],q[31],q[32],q[33],q[34],q[35],q[36],q[37],q[38],q[39],q[40],q[41],q[42],q[43],q[44],q[45],q[46],q[47],q[48],q[49],q[50],q[51],q[52],q[53],q[54],q[55],q[56],q[57],q[58],q[59],q[60],q[61],q[62],q[63];
barrier q[0],q[1],q[2],q[3],q[4],q[5],q[6],q[7],q[8],q[9],q[10],q[11],q[12],q[13],q[14],q[15],q[16],q[17],q[18],q[19],q[20],q[21],q[22],q[23],q[24],q[25],q[26],q[27],q[28],q[29],q[30],q[31],q[32],q[33],q[34],q[35],q[36],q[37],q[38],q[39],q[40],q[41],q[42],q[43],q[44],q[45],q[46],q[47],q[48],q[49],q[50],q[51],q[52],q[53],q[54],q[55],q[56],q[57],q[58],q[59],q[60],q[61],q[62],q[63];
z q[1];
z q[3];
z q[5];
z q[7];
z q[9];
z q[12];
z q[14];
z q[16];
z q[18];
z q[22];
z q[23];
z q[25];
z q[27];
z q[29];
z q[31];
z q[34];
z q[36];
z q[38];
z q[40];
z q[44];
z q[45];
z q[47];
z q[49];
z q[51];
z q[53];
barrier q[0],q[1],q[2],q[3],q[4],q[5],q[6],q[7],q[8],q[9],q[10],q[11],q[12],q[13],q[14],q[15],q[16],q[17],q[18],q[19],q[20],q[21],q[22],q[23],q[24],q[25],q[26],q[27],q[28],q[29],q[30],q[31],q[32],q[33],q[34],q[35],q[36],q[37],q[38],q[39],q[40],q[41],q[42],q[43],q[44],q[45],q[46],q[47],q[48],q[49],q[50],q[51],q[52],q[53],q[54],q[55],q[56],q[57],q[58],q[59],q[60],q[61],q[62],q[63];
circuit_183 q[0],q[1],q[2],q[3],q[4],q[5],q[6],q[7],q[8],q[9],q[10],q[11],q[12],q[13],q[14],q[15],q[16],q[17],q[18],q[19],q[20],q[21],q[22],q[23],q[24],q[25],q[26],q[27],q[28],q[29],q[30],q[31],q[32],q[33],q[34],q[35],q[36],q[37],q[38],q[39],q[40],q[41],q[42],q[43],q[44],q[45],q[46],q[47],q[48],q[49],q[50],q[51],q[52],q[53],q[54],q[55],q[56],q[57],q[58],q[59],q[60],q[61],q[62],q[63];
barrier q[0],q[1],q[2],q[3],q[4],q[5],q[6],q[7],q[8],q[9],q[10],q[11],q[12],q[13],q[14],q[15],q[16],q[17],q[18],q[19],q[20],q[21],q[22],q[23],q[24],q[25],q[26],q[27],q[28],q[29],q[30],q[31],q[32],q[33],q[34],q[35],q[36],q[37],q[38],q[39],q[40],q[41],q[42],q[43],q[44],q[45],q[46],q[47],q[48],q[49],q[50],q[51],q[52],q[53],q[54],q[55],q[56],q[57],q[58],q[59],q[60],q[61],q[62],q[63];
cz q[1],q[13];
cz q[3],q[15];
cz q[5],q[17];
cz q[7],q[19];
cz q[9],q[21];
cz q[12],q[24];
cz q[14],q[26];
cz q[16],q[28];
cz q[18],q[30];
cz q[23],q[35];
cz q[25],q[37];
cz q[27],q[39];
cz q[29],q[41];
cz q[31],q[43];
cz q[34],q[46];
cz q[36],q[48];
cz q[38],q[50];
cz q[40],q[52];
cz q[47],q[59];
cz q[51],q[63];
barrier q[0],q[1],q[2],q[3],q[4],q[5],q[6],q[7],q[8],q[9],q[10],q[11],q[12],q[13],q[14],q[15],q[16],q[17],q[18],q[19],q[20],q[21],q[22],q[23],q[24],q[25],q[26],q[27],q[28],q[29],q[30],q[31],q[32],q[33],q[34],q[35],q[36],q[37],q[38],q[39],q[40],q[41],q[42],q[43],q[44],q[45],q[46],q[47],q[48],q[49],q[50],q[51],q[52],q[53],q[54],q[55],q[56],q[57],q[58],q[59],q[60],q[61],q[62],q[63];
circuit_186 q[0],q[1],q[2],q[3],q[4],q[5],q[6],q[7],q[8],q[9],q[10],q[11],q[12],q[13],q[14],q[15],q[16],q[17],q[18],q[19],q[20],q[21],q[22],q[23],q[24],q[25],q[26],q[27],q[28],q[29],q[30],q[31],q[32],q[33],q[34],q[35],q[36],q[37],q[38],q[39],q[40],q[41],q[42],q[43],q[44],q[45],q[46],q[47],q[48],q[49],q[50],q[51],q[52],q[53],q[54],q[55],q[56],q[57],q[58],q[59],q[60],q[61],q[62],q[63];
barrier q[0],q[1],q[2],q[3],q[4],q[5],q[6],q[7],q[8],q[9],q[10],q[11],q[12],q[13],q[14],q[15],q[16],q[17],q[18],q[19],q[20],q[21],q[22],q[23],q[24],q[25],q[26],q[27],q[28],q[29],q[30],q[31],q[32],q[33],q[34],q[35],q[36],q[37],q[38],q[39],q[40],q[41],q[42],q[43],q[44],q[45],q[46],q[47],q[48],q[49],q[50],q[51],q[52],q[53],q[54],q[55],q[56],q[57],q[58],q[59],q[60],q[61],q[62],q[63];
z q[2];
z q[3];
z q[6];
z q[7];
z q[12];
z q[13];
z q[15];
z q[16];
z q[17];
z q[19];
z q[21];
z q[24];
z q[25];
z q[26];
z q[28];
z q[29];
z q[30];
z q[34];
z q[35];
z q[37];
z q[38];
z q[39];
z q[41];
z q[43];
z q[46];
z q[47];
z q[48];
z q[50];
z q[51];
z q[52];
z q[59];
z q[63];
barrier q[0],q[1],q[2],q[3],q[4],q[5],q[6],q[7],q[8],q[9],q[10],q[11],q[12],q[13],q[14],q[15],q[16],q[17],q[18],q[19],q[20],q[21],q[22],q[23],q[24],q[25],q[26],q[27],q[28],q[29],q[30],q[31],q[32],q[33],q[34],q[35],q[36],q[37],q[38],q[39],q[40],q[41],q[42],q[43],q[44],q[45],q[46],q[47],q[48],q[49],q[50],q[51],q[52],q[53],q[54],q[55],q[56],q[57],q[58],q[59],q[60],q[61],q[62],q[63];
circuit_189 q[0],q[1],q[2],q[3],q[4],q[5],q[6],q[7],q[8],q[9],q[10],q[11],q[12],q[13],q[14],q[15],q[16],q[17],q[18],q[19],q[20],q[21],q[22],q[23],q[24],q[25],q[26],q[27],q[28],q[29],q[30],q[31],q[32],q[33],q[34],q[35],q[36],q[37],q[38],q[39],q[40],q[41],q[42],q[43],q[44],q[45],q[46],q[47],q[48],q[49],q[50],q[51],q[52],q[53],q[54],q[55],q[56],q[57],q[58],q[59],q[60],q[61],q[62],q[63];
barrier q[0],q[1],q[2],q[3],q[4],q[5],q[6],q[7],q[8],q[9],q[10],q[11],q[12],q[13],q[14],q[15],q[16],q[17],q[18],q[19],q[20],q[21],q[22],q[23],q[24],q[25],q[26],q[27],q[28],q[29],q[30],q[31],q[32],q[33],q[34],q[35],q[36],q[37],q[38],q[39],q[40],q[41],q[42],q[43],q[44],q[45],q[46],q[47],q[48],q[49],q[50],q[51],q[52],q[53],q[54],q[55],q[56],q[57],q[58],q[59],q[60],q[61],q[62],q[63];)";

TEST(RoutingAwareCompielrTest, Issue792) {
  const auto& qc = qasm3::Importer::imports(circuit792.data());
  const auto arch = Architecture::fromJSONString(architectureSpecification792);
  const auto config = nlohmann::json::parse(routingAwareConfiguration792);
  RoutingAwareCompiler compiler(arch, config);
  EXPECT_NO_THROW(std::ignore = compiler.compile(qc));
}
} // namespace na::zoned
