/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "ir/operations/CompoundOperation.hpp"
#include "ir/operations/NonUnitaryOperation.hpp"
#include "ir/operations/StandardOperation.hpp"
#include "ir/operations/SymbolicOperation.hpp"
#include "na/zoned/code_generator/CodeGenerator.hpp"

#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>
#include <memory>
#include <utility>
#include <vector>

namespace na::zoned {
template <typename OpType, typename... Args>
auto makeSingleOpLayer(Args&&... args) {
  SingleQubitGateLayer layer;
  layer.emplace_back(std::make_unique<OpType>(std::forward<Args>(args)...));
  std::vector<SingleQubitGateLayer> layers;
  layers.emplace_back(std::move(layer));
  return layers;
}
constexpr std::string_view architectureJson = R"({
  "name": "code_generator_architecture",
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
constexpr std::string_view configJson = R"({
  "warnUnsupportedGates" : true
})";
class CodeGeneratorGenerateTest : public ::testing::Test {
protected:
  Architecture architecture;
  nlohmann::json config;
  CodeGenerator codeGenerator;
  CodeGeneratorGenerateTest()
      : architecture(Architecture::fromJSONString(architectureJson)),
        config(nlohmann::json::parse(configJson)),
        codeGenerator(architecture, config) {}
};
TEST_F(CodeGeneratorGenerateTest, Empty) {
  const auto& slm = *architecture.storageZones.front();
  EXPECT_EQ(
      codeGenerator
          .generate(std::vector<SingleQubitGateLayer>{},
                    std::vector<std::vector<std::tuple<
                        std::reference_wrapper<const SLM>, size_t, size_t>>>{
                        {{slm, 0, 0}}},
                    std::vector<std::vector<std::vector<qc::Qubit>>>{})
          .toString(),
      "atom (0.000, 0.000) atom0\n");
}
TEST_F(CodeGeneratorGenerateTest, GlobalRYGate) {
  const auto& slm = *architecture.storageZones.front();
  EXPECT_EQ(
      codeGenerator
          .generate(makeSingleOpLayer<qc::StandardOperation>(0U, qc::RY,
                                                             std::vector{0.1}),
                    std::vector<std::vector<std::tuple<
                        std::reference_wrapper<const SLM>, size_t, size_t>>>{
                        {{slm, 0, 0}}},
                    std::vector<std::vector<std::vector<qc::Qubit>>>{})
          .toString(),
      "atom (0.000, 0.000) atom0\n"
      "@+ ry 0.10000 global\n");
}
TEST_F(CodeGeneratorGenerateTest, GlobalYGate) {
  const auto& slm = *architecture.storageZones.front();
  EXPECT_EQ(
      codeGenerator
          .generate(makeSingleOpLayer<qc::StandardOperation>(0U, qc::Y),
                    std::vector<std::vector<std::tuple<
                        std::reference_wrapper<const SLM>, size_t, size_t>>>{
                        {{slm, 0, 0}}},
                    std::vector<std::vector<std::vector<qc::Qubit>>>{})
          .toString(),
      "atom (0.000, 0.000) atom0\n"
      "@+ ry 3.14159 global\n");
}
TEST_F(CodeGeneratorGenerateTest, GlobalCompoundRYGate) {
  const auto& slm = *architecture.storageZones.front();
  // Create a compound operation with a single RY gate
  qc::CompoundOperation cry;
  cry.emplace_back<qc::StandardOperation>(0U, qc::RY, std::vector{0.1});
  SingleQubitGateLayer layer;
  layer.emplace_back(std::make_unique<qc::CompoundOperation>(std::move(cry)));
  std::vector<SingleQubitGateLayer> layers;
  layers.emplace_back(std::move(layer));
  EXPECT_EQ(
      codeGenerator
          .generate(layers,
                    std::vector<std::vector<std::tuple<
                        std::reference_wrapper<const SLM>, size_t, size_t>>>{
                        {{slm, 0, 0}}},
                    std::vector<std::vector<std::vector<qc::Qubit>>>{})
          .toString(),
      "atom (0.000, 0.000) atom0\n"
      "@+ ry 0.10000 global\n");
}
TEST_F(CodeGeneratorGenerateTest, GlobalCompoundYGate) {
  const auto& slm = *architecture.storageZones.front();
  // Create a compound operation with a single Y gate
  qc::CompoundOperation cy;
  cy.emplace_back<qc::StandardOperation>(0U, qc::Y);
  SingleQubitGateLayer layer;
  layer.emplace_back(std::make_unique<qc::CompoundOperation>(std::move(cy)));
  std::vector<SingleQubitGateLayer> layers;
  layers.emplace_back(std::move(layer));
  EXPECT_EQ(
      codeGenerator
          .generate(layers,
                    std::vector<std::vector<std::tuple<
                        std::reference_wrapper<const SLM>, size_t, size_t>>>{
                        {{slm, 0, 0}}},
                    std::vector<std::vector<std::vector<qc::Qubit>>>{})
          .toString(),
      "atom (0.000, 0.000) atom0\n"
      "@+ ry 3.14159 global\n");
}
TEST_F(CodeGeneratorGenerateTest, RZGate) {
  const auto& slm = *architecture.storageZones.front();
  EXPECT_EQ(
      codeGenerator
          .generate(makeSingleOpLayer<qc::StandardOperation>(0U, qc::RZ,
                                                             std::vector{0.1}),
                    std::vector<std::vector<std::tuple<
                        std::reference_wrapper<const SLM>, size_t, size_t>>>{
                        {{slm, 0, 0}}},
                    std::vector<std::vector<std::vector<qc::Qubit>>>{})
          .toString(),
      "atom (0.000, 0.000) atom0\n"
      "@+ rz 0.10000 atom0\n");
}
TEST_F(CodeGeneratorGenerateTest, PGate) {
  const auto& slm = *architecture.storageZones.front();
  EXPECT_EQ(
      codeGenerator
          .generate(makeSingleOpLayer<qc::StandardOperation>(0U, qc::P,
                                                             std::vector{0.1}),
                    std::vector<std::vector<std::tuple<
                        std::reference_wrapper<const SLM>, size_t, size_t>>>{
                        {{slm, 0, 0}}},
                    std::vector<std::vector<std::vector<qc::Qubit>>>{})
          .toString(),
      "atom (0.000, 0.000) atom0\n"
      "@+ rz 0.10000 atom0\n");
}
TEST_F(CodeGeneratorGenerateTest, ZGate) {
  const auto& slm = *architecture.storageZones.front();
  EXPECT_EQ(
      codeGenerator
          .generate(makeSingleOpLayer<qc::StandardOperation>(0U, qc::Z),
                    std::vector<std::vector<std::tuple<
                        std::reference_wrapper<const SLM>, size_t, size_t>>>{
                        {{slm, 0, 0}}},
                    std::vector<std::vector<std::vector<qc::Qubit>>>{})
          .toString(),
      "atom (0.000, 0.000) atom0\n"
      "@+ rz 3.14159 atom0\n");
}
TEST_F(CodeGeneratorGenerateTest, SGate) {
  const auto& slm = *architecture.storageZones.front();
  EXPECT_EQ(
      codeGenerator
          .generate(makeSingleOpLayer<qc::StandardOperation>(0U, qc::S),
                    std::vector<std::vector<std::tuple<
                        std::reference_wrapper<const SLM>, size_t, size_t>>>{
                        {{slm, 0, 0}}},
                    std::vector<std::vector<std::vector<qc::Qubit>>>{})
          .toString(),
      "atom (0.000, 0.000) atom0\n"
      "@+ rz 1.57080 atom0\n");
}
TEST_F(CodeGeneratorGenerateTest, SdgGate) {
  const auto& slm = *architecture.storageZones.front();
  EXPECT_EQ(
      codeGenerator
          .generate(makeSingleOpLayer<qc::StandardOperation>(0U, qc::Sdg),
                    std::vector<std::vector<std::tuple<
                        std::reference_wrapper<const SLM>, size_t, size_t>>>{
                        {{slm, 0, 0}}},
                    std::vector<std::vector<std::vector<qc::Qubit>>>{})
          .toString(),
      "atom (0.000, 0.000) atom0\n"
      "@+ rz -1.57080 atom0\n");
}
TEST_F(CodeGeneratorGenerateTest, TGate) {
  const auto& slm = *architecture.storageZones.front();
  EXPECT_EQ(
      codeGenerator
          .generate(makeSingleOpLayer<qc::StandardOperation>(0U, qc::T),
                    std::vector<std::vector<std::tuple<
                        std::reference_wrapper<const SLM>, size_t, size_t>>>{
                        {{slm, 0, 0}}},
                    std::vector<std::vector<std::vector<qc::Qubit>>>{})
          .toString(),
      "atom (0.000, 0.000) atom0\n"
      "@+ rz 0.78540 atom0\n");
}
TEST_F(CodeGeneratorGenerateTest, TdgGate) {
  const auto& slm = *architecture.storageZones.front();
  EXPECT_EQ(
      codeGenerator
          .generate(makeSingleOpLayer<qc::StandardOperation>(0U, qc::Tdg),
                    std::vector<std::vector<std::tuple<
                        std::reference_wrapper<const SLM>, size_t, size_t>>>{
                        {{slm, 0, 0}}},
                    std::vector<std::vector<std::vector<qc::Qubit>>>{})
          .toString(),
      "atom (0.000, 0.000) atom0\n"
      "@+ rz -0.78540 atom0\n");
}
TEST_F(CodeGeneratorGenerateTest, U3Gate) {
  const auto& slm = *architecture.storageZones.front();
  EXPECT_EQ(
      codeGenerator
          .generate(makeSingleOpLayer<qc::StandardOperation>(
                        0U, qc::U, std::vector{0.1, 0.2, 0.3}),
                    std::vector<std::vector<std::tuple<
                        std::reference_wrapper<const SLM>, size_t, size_t>>>{
                        {{slm, 0, 0}}},
                    std::vector<std::vector<std::vector<qc::Qubit>>>{})
          .toString(),
      "atom (0.000, 0.000) atom0\n"
      "@+ u 0.10000 0.20000 0.30000 atom0\n");
}
TEST_F(CodeGeneratorGenerateTest, U2Gate) {
  const auto& slm = *architecture.storageZones.front();
  EXPECT_EQ(
      codeGenerator
          .generate(makeSingleOpLayer<qc::StandardOperation>(
                        0U, qc::U2, std::vector{0.1, 0.2}),
                    std::vector<std::vector<std::tuple<
                        std::reference_wrapper<const SLM>, size_t, size_t>>>{
                        {{slm, 0, 0}}},
                    std::vector<std::vector<std::vector<qc::Qubit>>>{})
          .toString(),
      "atom (0.000, 0.000) atom0\n"
      "@+ u 1.57080 0.10000 0.20000 atom0\n");
}
TEST_F(CodeGeneratorGenerateTest, RXGate) {
  const auto& slm = *architecture.storageZones.front();
  EXPECT_EQ(
      codeGenerator
          .generate(makeSingleOpLayer<qc::StandardOperation>(0U, qc::RX,
                                                             std::vector{0.1}),
                    std::vector<std::vector<std::tuple<
                        std::reference_wrapper<const SLM>, size_t, size_t>>>{
                        {{slm, 0, 0}}},
                    std::vector<std::vector<std::vector<qc::Qubit>>>{})
          .toString(),
      "atom (0.000, 0.000) atom0\n"
      "@+ u 0.10000 -1.57080 1.57080 atom0\n");
}
TEST_F(CodeGeneratorGenerateTest, RYGate) {
  const auto& slm = *architecture.storageZones.front();
  EXPECT_EQ(
      codeGenerator
          .generate(makeSingleOpLayer<qc::StandardOperation>(0U, qc::RY,
                                                             std::vector{0.1}),
                    std::vector<std::vector<std::tuple<
                        std::reference_wrapper<const SLM>, size_t, size_t>>>{
                        {{slm, 0, 0}, {slm, 0, 1}}},
                    std::vector<std::vector<std::vector<qc::Qubit>>>{})
          .toString(),
      "atom (0.000, 0.000) atom0\n"
      "atom (3.000, 0.000) atom1\n"
      "@+ u 0.10000 0.00000 0.00000 atom0\n");
}
TEST_F(CodeGeneratorGenerateTest, YGate) {
  const auto& slm = *architecture.storageZones.front();
  EXPECT_EQ(
      codeGenerator
          .generate(makeSingleOpLayer<qc::StandardOperation>(0U, qc::Y),
                    std::vector<std::vector<std::tuple<
                        std::reference_wrapper<const SLM>, size_t, size_t>>>{
                        {{slm, 0, 0}, {slm, 0, 1}}},
                    std::vector<std::vector<std::vector<qc::Qubit>>>{})
          .toString(),
      "atom (0.000, 0.000) atom0\n"
      "atom (3.000, 0.000) atom1\n"
      "@+ u 3.14159 1.57080 1.57080 atom0\n");
}
TEST_F(CodeGeneratorGenerateTest, HGate) {
  const auto& slm = *architecture.storageZones.front();
  EXPECT_EQ(
      codeGenerator
          .generate(makeSingleOpLayer<qc::StandardOperation>(0U, qc::H),
                    std::vector<std::vector<std::tuple<
                        std::reference_wrapper<const SLM>, size_t, size_t>>>{
                        {{slm, 0, 0}}},
                    std::vector<std::vector<std::vector<qc::Qubit>>>{})
          .toString(),
      "atom (0.000, 0.000) atom0\n"
      "@+ u 1.57080 0.00000 3.14159 atom0\n");
}
TEST_F(CodeGeneratorGenerateTest, XGate) {
  const auto& slm = *architecture.storageZones.front();
  EXPECT_EQ(
      codeGenerator
          .generate(makeSingleOpLayer<qc::StandardOperation>(0U, qc::X),
                    std::vector<std::vector<std::tuple<
                        std::reference_wrapper<const SLM>, size_t, size_t>>>{
                        {{slm, 0, 0}}},
                    std::vector<std::vector<std::vector<qc::Qubit>>>{})
          .toString(),
      "atom (0.000, 0.000) atom0\n"
      "@+ u 3.14159 0.00000 3.14159 atom0\n");
}
TEST_F(CodeGeneratorGenerateTest, VGate) {
  const auto& slm = *architecture.storageZones.front();
  EXPECT_EQ(
      codeGenerator
          .generate(makeSingleOpLayer<qc::StandardOperation>(0U, qc::V),
                    std::vector<std::vector<std::tuple<
                        std::reference_wrapper<const SLM>, size_t, size_t>>>{
                        {{slm, 0, 0}}},
                    std::vector<std::vector<std::vector<qc::Qubit>>>{})
          .toString(),
      "atom (0.000, 0.000) atom0\n"
      "@+ u -1.57080 -1.57080 1.57080 atom0\n");
}
TEST_F(CodeGeneratorGenerateTest, VdgGate) {
  const auto& slm = *architecture.storageZones.front();
  EXPECT_EQ(
      codeGenerator
          .generate(makeSingleOpLayer<qc::StandardOperation>(0U, qc::Vdg),
                    std::vector<std::vector<std::tuple<
                        std::reference_wrapper<const SLM>, size_t, size_t>>>{
                        {{slm, 0, 0}}},
                    std::vector<std::vector<std::vector<qc::Qubit>>>{})
          .toString(),
      "atom (0.000, 0.000) atom0\n"
      "@+ u -1.57080 1.57080 -1.57080 atom0\n");
}
TEST_F(CodeGeneratorGenerateTest, SXGate) {
  const auto& slm = *architecture.storageZones.front();
  EXPECT_EQ(
      codeGenerator
          .generate(makeSingleOpLayer<qc::StandardOperation>(0U, qc::SX),
                    std::vector<std::vector<std::tuple<
                        std::reference_wrapper<const SLM>, size_t, size_t>>>{
                        {{slm, 0, 0}}},
                    std::vector<std::vector<std::vector<qc::Qubit>>>{})
          .toString(),
      "atom (0.000, 0.000) atom0\n"
      "@+ u 1.57080 -1.57080 1.57080 atom0\n");
}
TEST_F(CodeGeneratorGenerateTest, SXdgGate) {
  const auto& slm = *architecture.storageZones.front();
  EXPECT_EQ(
      codeGenerator
          .generate(makeSingleOpLayer<qc::StandardOperation>(0U, qc::SXdg),
                    std::vector<std::vector<std::tuple<
                        std::reference_wrapper<const SLM>, size_t, size_t>>>{
                        {{slm, 0, 0}}},
                    std::vector<std::vector<std::vector<qc::Qubit>>>{})
          .toString(),
      "atom (0.000, 0.000) atom0\n"
      "@+ u -1.57080 -1.57080 1.57080 atom0\n");
}
TEST_F(CodeGeneratorGenerateTest, UnsupportedGate) {
  const auto& slm = *architecture.storageZones.front();
  EXPECT_THROW(
      std::ignore = codeGenerator.generate(
          makeSingleOpLayer<qc::NonUnitaryOperation>(0U, 0U),
          std::vector<std::vector<
              std::tuple<std::reference_wrapper<const SLM>, size_t, size_t>>>{
              {{slm, 0, 0}}},
          std::vector<std::vector<std::vector<qc::Qubit>>>{}),
      std::invalid_argument);
}
TEST_F(CodeGeneratorGenerateTest, TwoQubitGate) {
  const auto& storage = *architecture.storageZones.front();
  const auto& entanglementLeft =
      architecture.entanglementZones.front()->front();
  const auto& entanglementRight =
      architecture.entanglementZones.front()->back();
  const std::vector<SingleQubitGateLayer> layers(2);
  EXPECT_EQ(
      codeGenerator
          .generate(layers,
                    std::vector<std::vector<std::tuple<
                        std::reference_wrapper<const SLM>, size_t, size_t>>>{
                        {{storage, 19, 0}, {storage, 19, 1}},
                        {{entanglementLeft, 0, 0}, {entanglementRight, 0, 0}},
                        {{storage, 19, 0}, {storage, 19, 1}}},
                    std::vector<std::vector<std::vector<qc::Qubit>>>{
                        {{0U, 1U}}, {{0U, 1U}}})
          .toString(),
      R"(atom (0.000, 57.000) atom0
atom (3.000, 57.000) atom1
@+ load [
    atom0
    atom1
]
@+ move [
    (1.000, 58.000) atom0
    (4.000, 58.000) atom1
]
@+ move [
    (2.000, 65.000) atom0
    (10.000, 65.000) atom1
]
@+ move [
    (5.000, 70.000) atom0
    (7.000, 70.000) atom1
]
@+ store [
    atom0
    atom1
]
@+ cz zone_cz0
@+ load [
    atom0
    atom1
]
@+ move [
    (2.000, 65.000) atom0
    (10.000, 65.000) atom1
]
@+ move [
    (1.000, 58.000) atom0
    (4.000, 58.000) atom1
]
@+ move [
    (0.000, 57.000) atom0
    (3.000, 57.000) atom1
]
@+ store [
    atom0
    atom1
]
)");
}
TEST_F(CodeGeneratorGenerateTest, Offset) {
  // STORAGE     ...         │ ...         │ ...
  //         18  0 1 o o ... │ o o o o ... │ 0 1 o o ...
  //         19  2 3 o o ... │ o o o o ... │ 2 3 o o ...
  //                         │  ╲╲         │ ↑ ↑
  // ENTANGLEMENT            │   ↓↓        │  ╲╲
  //          0    oo    ... │   01    ... │   oo    ...
  //          1    oo    ... │   23    ... │   oo    ...
  //               ...       │   ...       │   ...
  const auto& storage = *architecture.storageZones.front();
  const auto& entanglementLeft =
      architecture.entanglementZones.front()->front();
  const auto& entanglementRight =
      architecture.entanglementZones.front()->back();
  const std::vector<SingleQubitGateLayer> layers(2);
  EXPECT_EQ(
      codeGenerator
          .generate(layers,
                    std::vector<std::vector<std::tuple<
                        std::reference_wrapper<const SLM>, size_t, size_t>>>{
                        {{storage, 18, 0},
                         {storage, 18, 1},
                         {storage, 19, 0},
                         {storage, 19, 1}},
                        {{entanglementLeft, 0, 0},
                         {entanglementRight, 0, 0},
                         {entanglementLeft, 1, 0},
                         {entanglementRight, 1, 0}},
                        {{storage, 18, 0},
                         {storage, 18, 1},
                         {storage, 19, 0},
                         {storage, 19, 1}}},
                    std::vector<std::vector<std::vector<qc::Qubit>>>{
                        {{0U, 1U, 2U, 3U}}, {{0U, 1U, 2U, 3U}}})
          .toString(),
      R"(atom (0.000, 54.000) atom0
atom (0.000, 57.000) atom2
atom (3.000, 54.000) atom1
atom (3.000, 57.000) atom3
@+ load [
    atom0
    atom1
    atom2
    atom3
]
@+ move [
    (1.000, 55.000) atom0
    (4.000, 55.000) atom1
    (1.000, 58.000) atom2
    (4.000, 58.000) atom3
]
@+ move [
    (2.000, 65.000) atom0
    (10.000, 65.000) atom1
    (2.000, 75.000) atom2
    (10.000, 75.000) atom3
]
@+ move [
    (5.000, 70.000) atom0
    (7.000, 70.000) atom1
    (5.000, 80.000) atom2
    (7.000, 80.000) atom3
]
@+ store [
    atom0
    atom1
    atom2
    atom3
]
@+ cz zone_cz0
@+ load [
    atom0
    atom1
    atom2
    atom3
]
@+ move [
    (2.000, 65.000) atom0
    (10.000, 65.000) atom1
    (2.000, 75.000) atom2
    (10.000, 75.000) atom3
]
@+ move [
    (1.000, 55.000) atom0
    (4.000, 55.000) atom1
    (1.000, 58.000) atom2
    (4.000, 58.000) atom3
]
@+ move [
    (0.000, 54.000) atom0
    (3.000, 54.000) atom1
    (0.000, 57.000) atom2
    (3.000, 57.000) atom3
]
@+ store [
    atom0
    atom1
    atom2
    atom3
]
)");
}
TEST_F(CodeGeneratorGenerateTest, ColumnByColumn) {
  // STORAGE     ...         │ ...         │ ...
  //         17  0 1 o o ... │ o o o o ... │ 0 1 o o ...
  //         18  2 3 o o ... │ o o o o ... │ 2 3 o o ...
  //         19  4 5 o o ... │ o o o o ... │ 4 5 o o ...
  //                         │  ╲╲         │ ↑ ↑
  // ENTANGLEMENT            │   ↓↓        │  ╲╲
  //          0    oo    ... │   01    ... │   oo    ...
  //          1    oo    ... │   23    ... │   oo    ...
  //          2    oo    ... │   45    ... │   oo    ...
  //               ...       │   ...       │   ...
  const auto& storage = *architecture.storageZones.front();
  const auto& entanglementLeft =
      architecture.entanglementZones.front()->front();
  const auto& entanglementRight =
      architecture.entanglementZones.front()->back();
  const std::vector<SingleQubitGateLayer> layers(2);
  EXPECT_TRUE(
      codeGenerator
          .generate(layers,
                    std::vector<std::vector<std::tuple<
                        std::reference_wrapper<const SLM>, size_t, size_t>>>{
                        {{storage, 17, 0},
                         {storage, 17, 1},
                         {storage, 18, 0},
                         {storage, 18, 1},
                         {storage, 19, 0},
                         {storage, 19, 1}},
                        {{entanglementLeft, 0, 0},
                         {entanglementRight, 0, 0},
                         {entanglementLeft, 1, 0},
                         {entanglementRight, 1, 0},
                         {entanglementLeft, 2, 0},
                         {entanglementRight, 2, 0}},
                        {{storage, 17, 0},
                         {storage, 17, 1},
                         {storage, 18, 0},
                         {storage, 18, 1},
                         {storage, 19, 0},
                         {storage, 19, 1}}},
                    std::vector<std::vector<std::vector<qc::Qubit>>>{
                        {{0U, 1U, 2U, 3U, 4U, 5U}}, {{0U, 1U, 2U, 3U, 4U, 5U}}})
          .validate()
          .first);
}
TEST_F(CodeGeneratorGenerateTest, RelaxedRouting) {
  // STORAGE     ...         │ ...         │ ...
  //         18  0 1 o o ... │ o o o o ... │ 0 1 o o ...
  //         19  2 3 o o ... │ o o o o ... │ 2 3 o o ...
  //                         │  ╲╲         │ ↑ ↑
  // ENTANGLEMENT            │   ↓↓        │  ╲╲
  //          0    oo    ... │   32    ... │   oo    ...
  //          1    oo    ... │   10    ... │   oo    ...
  //               ...       │   ...       │   ...
  const auto& storage = *architecture.storageZones.front();
  const auto& entanglementLeft =
      architecture.entanglementZones.front()->front();
  const auto& entanglementRight =
      architecture.entanglementZones.front()->back();
  const std::vector<SingleQubitGateLayer> layers(2);
  EXPECT_TRUE(
      codeGenerator
          .generate(layers,
                    std::vector<std::vector<std::tuple<
                        std::reference_wrapper<const SLM>, size_t, size_t>>>{
                        {{storage, 18, 0},
                         {storage, 18, 1},
                         {storage, 19, 0},
                         {storage, 19, 1}},
                        {{entanglementRight, 1, 0},
                         {entanglementLeft, 1, 0},
                         {entanglementRight, 0, 0},
                         {entanglementLeft, 0, 0}},
                        {{storage, 18, 0},
                         {storage, 18, 1},
                         {storage, 19, 0},
                         {storage, 19, 1}}},
                    std::vector<std::vector<std::vector<qc::Qubit>>>{
                        {{0U, 1U, 2U, 3U}}, {{0U, 1U, 2U, 3U}}})
          .validate()
          .first);
}
} // namespace na::zoned
