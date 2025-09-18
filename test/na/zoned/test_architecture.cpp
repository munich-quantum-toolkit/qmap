/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "na/zoned/Architecture.hpp"

#include <cstddef>
#include <gtest/gtest.h>
#include <sstream>

namespace na::zoned {
constexpr std::string_view architectureJson = R"({
  "name": "full_compute_store_architecture",
  "operation_duration": {"rydberg_gate": 0.36, "single_qubit_gate": 52, "atom_transfer": 15},
  "operation_fidelity": {
    "rydberg_gate": 0.995,
    "single_qubit_gate": 0.9997,
    "atom_transfer": 0.999
  },
  "qubit_spec": {"T": 1.5e6},
  "storage_zones": [{
    "slms": [{
      "id": 0,
      "site_separation": [3, 3],
      "r": 20,
      "c": 20,
      "location": [0, 0]}],
    "offset": [0, 0],
    "dimension": [60, 60]
  }],
  "entanglement_zones": [{
    "zone_id": 0,
    "slms": [
      {
        "id": 1,
        "site_separation": [12, 10],
        "r": 4,
        "c": 4,
        "location": [5, 70]
      },
      {
        "id": 2,
        "site_separation": [12, 10],
        "r": 4,
        "c": 4,
        "location": [7, 70]
      }],
    "offset": [5, 70],
    "dimension": [50, 40]
  }],
  "aods":[{"id": 0, "site_separation": 2, "r": 20, "c": 20}],
  "rydberg_range": [[[0, 57], [65, 105]]]
})";
class TwoZoneArchitectureTest : public ::testing::Test {
protected:
  Architecture arch;
  TwoZoneArchitectureTest()
      : arch(Architecture::fromJSONString(architectureJson)) {}
};
TEST_F(TwoZoneArchitectureTest, Load) {}
TEST_F(TwoZoneArchitectureTest, Storage) {
  EXPECT_EQ(arch.storageZones.size(), 1);
  EXPECT_EQ(arch.storageZones.front()->nRows, 20);
  EXPECT_EQ(arch.storageZones.front()->nCols, 20);
}
TEST_F(TwoZoneArchitectureTest, Distance) {
  const auto& slm1 = *arch.storageZones.front();
  EXPECT_EQ(arch.distance(slm1, 0, 0, slm1, 0, 1), slm1.siteSeparation.first);
  EXPECT_EQ(arch.distance(slm1, 0, 0, slm1, 1, 0), slm1.siteSeparation.second);

  const auto& slm2 = arch.entanglementZones.front()->front();
  EXPECT_EQ(arch.distance(slm1, 0, 0, slm2, 0, 0),
            std::hypot(static_cast<double>(slm1.location.first) -
                           static_cast<double>(slm2.location.first),
                       static_cast<double>(slm1.location.second) -
                           static_cast<double>(slm2.location.second)));
}
TEST_F(TwoZoneArchitectureTest, NearestStorageSite) {
  const auto& entanglementSLM = arch.entanglementZones.front()->front();
  const auto& [nearestSLM, nearestRow, nearestCol] =
      arch.nearestStorageSite(entanglementSLM, 0, 0);
  const auto minDistance =
      arch.distance(entanglementSLM, 0, 0, nearestSLM, nearestRow, nearestCol);
  for (const auto& slm : arch.storageZones) {
    for (std::size_t r = 0; r < slm->nRows; ++r) {
      for (std::size_t c = 0; c < slm->nCols; ++c) {
        const auto distance = arch.distance(entanglementSLM, 0, 0, *slm, r, c);
        EXPECT_GE(distance, minDistance);
      }
    }
  }
}
TEST_F(TwoZoneArchitectureTest, NearestEntanglementSite) {
  const auto& storageSLM = *arch.storageZones.front();
  const auto& [nearestSLM, nearestRow, nearestCol] =
      arch.nearestEntanglementSite(storageSLM, 0, 0, storageSLM, 0, 1);
  const auto minDistance =
      arch.distance(storageSLM, 0, 0, nearestSLM, nearestRow, nearestCol) +
      arch.distance(storageSLM, 0, 1, nearestSLM, nearestRow, nearestCol);
  for (const auto& slms : arch.entanglementZones) {
    for (const auto& slm : *slms) {
      for (std::size_t r = 0; r < slm.nRows; ++r) {
        for (std::size_t c = 0; c < slm.nCols; ++c) {
          const auto distance = arch.distance(storageSLM, 0, 0, slm, r, c) +
                                arch.distance(storageSLM, 0, 1, slm, r, c);
          EXPECT_GE(distance, minDistance);
        }
      }
    }
  }
}
TEST_F(TwoZoneArchitectureTest, ExportNoThrow) {
  ASSERT_NO_THROW(arch.exportNAVizMachine(arch.name + ".namachine"));
}
TEST(ArchitectureTest, InvalidName) {
  nlohmann::json spec = R"({
  "name": 42
})"_json;
  EXPECT_THROW(std::ignore = Architecture::fromJSON(spec),
               std::invalid_argument);
}
TEST(ArchitectureTest, MissingName) {
  nlohmann::json spec = R"({
})"_json;
  EXPECT_THROW(std::ignore = Architecture::fromJSON(spec),
               std::invalid_argument);
}
TEST(ArchitectureTest, InvalidDurations) {
  nlohmann::json spec = R"({
  "name": "invalid_architecture",
  "operation_duration": 0
})"_json;
  EXPECT_THROW(std::ignore = Architecture::fromJSON(spec),
               std::invalid_argument);
}
TEST(ArchitectureTest, InvalidRydbergDuration) {
  nlohmann::json spec = R"({
  "name": "invalid_architecture",
  "operation_duration": {"rydberg_gate": "0.36µs", "single_qubit_gate": 52, "atom_transfer": 15}
})"_json;
  EXPECT_THROW(std::ignore = Architecture::fromJSON(spec),
               std::invalid_argument);
}
TEST(ArchitectureTest, MissingRydbergDuration) {
  nlohmann::json spec = R"({
  "name": "invalid_architecture",
  "operation_duration": {"single_qubit_gate": 52, "atom_transfer": 15}
})"_json;
  EXPECT_THROW(std::ignore = Architecture::fromJSON(spec),
               std::invalid_argument);
}
TEST(ArchitectureTest, InvalidTransferDuration) {
  nlohmann::json spec = R"({
  "name": "invalid_architecture",
  "operation_duration": {"rydberg_gate": 0.36, "single_qubit_gate": 52, "atom_transfer": "15 us"}
})"_json;
  EXPECT_THROW(std::ignore = Architecture::fromJSON(spec),
               std::invalid_argument);
}
TEST(ArchitectureTest, MissingTransferDuration) {
  nlohmann::json spec = R"({
  "name": "invalid_architecture",
  "operation_duration": {"rydberg_gate": 0.36, "single_qubit_gate": 52}
})"_json;
  EXPECT_THROW(std::ignore = Architecture::fromJSON(spec),
               std::invalid_argument);
}
TEST(ArchitectureTest, InvalidSingleQubitOperationDuration) {
  nlohmann::json spec = R"({
  "name": "invalid_architecture",
  "operation_duration": {"rydberg_gate": 0.36, "single_qubit_gate": "52us", "atom_transfer": 15}
})"_json;
  EXPECT_THROW(std::ignore = Architecture::fromJSON(spec),
               std::invalid_argument);
}
TEST(ArchitectureTest, MissingSingleQubitOperationDuration) {
  nlohmann::json spec = R"({
  "name": "invalid_architecture",
  "operation_duration": {"rydberg_gate": 0.36, "atom_transfer": 15}
})"_json;
  EXPECT_THROW(std::ignore = Architecture::fromJSON(spec),
               std::invalid_argument);
}
TEST(ArchitectureTest, InvalidFidelities) {
  nlohmann::json spec = R"({
  "name": "invalid_architecture",
  "operation_duration": 0
})"_json;
  EXPECT_THROW(std::ignore = Architecture::fromJSON(spec),
               std::invalid_argument);
}
TEST(ArchitectureTest, InvalidRydbergFidelity) {
  nlohmann::json spec = R"({
  "name": "invalid_architecture",
  "operation_fidelity": {
    "rydberg_gate": "0.995",
    "single_qubit_gate": 0.9997,
    "atom_transfer": 0.999
  }
})"_json;
  EXPECT_THROW(std::ignore = Architecture::fromJSON(spec),
               std::invalid_argument);
}
TEST(ArchitectureTest, MissingRydbergFidelity) {
  nlohmann::json spec = R"({
  "name": "invalid_architecture",
  "operation_fidelity": {
    "single_qubit_gate": 0.9997,
    "atom_transfer": 0.999
  }
})"_json;
  EXPECT_THROW(std::ignore = Architecture::fromJSON(spec),
               std::invalid_argument);
}
TEST(ArchitectureTest, InvalidTransferFidelity) {
  nlohmann::json spec = R"({
  "name": "invalid_architecture",
  "operation_fidelity": {
    "rydberg_gate": 0.995,
    "single_qubit_gate": 0.9997,
    "atom_transfer": "0.999"
  }
})"_json;
  EXPECT_THROW(std::ignore = Architecture::fromJSON(spec),
               std::invalid_argument);
}
TEST(ArchitectureTest, MissingTransferFidelity) {
  nlohmann::json spec = R"({
  "name": "invalid_architecture",
  "operation_fidelity": {
    "rydberg_gate": 0.995,
    "single_qubit_gate": 0.9997
  }
})"_json;
  EXPECT_THROW(std::ignore = Architecture::fromJSON(spec),
               std::invalid_argument);
}
TEST(ArchitectureTest, InvalidSingleQubitOperationFidelity) {
  nlohmann::json spec = R"({
  "name": "invalid_architecture",
  "operation_fidelity": {
    "rydberg_gate": 0.995,
    "single_qubit_gate": "0.9997",
    "atom_transfer": 0.999
  }
})"_json;
  EXPECT_THROW(std::ignore = Architecture::fromJSON(spec),
               std::invalid_argument);
}
TEST(ArchitectureTest, MissingSingleQubitOperationFidelity) {
  nlohmann::json spec = R"({
  "name": "invalid_architecture",
  "operation_fidelity": {
    "rydberg_gate": 0.995,
    "atom_transfer": 0.999
  }
})"_json;
  EXPECT_THROW(std::ignore = Architecture::fromJSON(spec),
               std::invalid_argument);
}
TEST(ArchitectureTest, InvalidQubitSpec) {
  nlohmann::json spec = R"({
  "name": "invalid_architecture",
  "qubit_spec": 1.5e6
})"_json;
  EXPECT_THROW(std::ignore = Architecture::fromJSON(spec),
               std::invalid_argument);
}
TEST(ArchitectureTest, InvalidT1) {
  nlohmann::json spec = R"({
  "name": "invalid_architecture",
  "qubit_spec": {"T": "1.5e6"}
})"_json;
  EXPECT_THROW(std::ignore = Architecture::fromJSON(spec),
               std::invalid_argument);
}
TEST(ArchitectureTest, MissingT1) {
  nlohmann::json spec = R"({
  "name": "invalid_architecture",
  "qubit_spec": {}
})"_json;
  EXPECT_THROW(std::ignore = Architecture::fromJSON(spec),
               std::invalid_argument);
}
TEST(ArchitectureTest, InvalidRydbergRange1) {
  nlohmann::json spec = R"({
  "name": "invalid_architecture",
  "rydberg_range": []
})"_json;
  EXPECT_THROW(std::ignore = Architecture::fromJSON(spec),
               std::invalid_argument);
}
TEST(ArchitectureTest, InvalidRydbergRange2) {
  nlohmann::json spec = R"({
  "name": "invalid_architecture",
  "rydberg_range": [[[2, 1]]]
})"_json;
  EXPECT_THROW(std::ignore = Architecture::fromJSON(spec),
               std::invalid_argument);
}
TEST(ArchitectureTest, MissingRydbergRange) {
  nlohmann::json spec = R"({
  "name": "invalid_architecture"
})"_json;
  EXPECT_THROW(std::ignore = Architecture::fromJSON(spec),
               std::invalid_argument);
}
TEST(ArchitectureTest, MissingStorage) {
  nlohmann::json spec = R"({
  "name": "invalid_architecture",
  "rydberg_range": [[[0, 0], [2, 1]]]
})"_json;
  EXPECT_THROW(std::ignore = Architecture::fromJSON(spec),
               std::invalid_argument);
}
TEST(ArchitectureTest, InvalidStorage1) {
  nlohmann::json spec = R"({
  "name": "invalid_architecture",
  "storage_zones": {
    "slms": [{
      "id": "one",
      "site_separation": [3, 3],
      "r": 20,
      "c": 20,
      "location": [0, 0]}],
    "offset": [0, 0],
    "dimension": [60, 60]
  },
  "rydberg_range": [[[0, 0], [2, 1]]]
})"_json;
  EXPECT_THROW(std::ignore = Architecture::fromJSON(spec),
               std::invalid_argument);
}
TEST(ArchitectureTest, InvalidStorage2) {
  nlohmann::json spec = R"({
  "name": "invalid_architecture",
  "storage_zones": [],
  "rydberg_range": [[[0, 0], [2, 1]]]
})"_json;
  EXPECT_THROW(std::ignore = Architecture::fromJSON(spec),
               std::invalid_argument);
}
TEST(ArchitectureTest, InvalidStorage3) {
  nlohmann::json spec = R"({
  "name": "invalid_architecture",
  "storage_zones": [{
    "offset": [0, 0],
    "dimension": [60, 60]
  }],
  "rydberg_range": [[[0, 0], [2, 1]]]
})"_json;
  EXPECT_THROW(std::ignore = Architecture::fromJSON(spec),
               std::invalid_argument);
}
TEST(ArchitectureTest, InvalidSLMId) {
  nlohmann::json spec = R"({
  "name": "invalid_architecture",
  "storage_zones": [{
    "slms": [{
      "id": "one",
      "site_separation": [3, 3],
      "r": 20,
      "c": 20,
      "location": [0, 0]}],
    "offset": [0, 0],
    "dimension": [60, 60]
  }],
  "rydberg_range": [[[0, 0], [2, 1]]]
})"_json;
  EXPECT_THROW(std::ignore = Architecture::fromJSON(spec),
               std::invalid_argument);
}
TEST(ArchitectureTest, MissingSLMId) {
  nlohmann::json spec = R"({
  "name": "invalid_architecture",
  "storage_zones": [{
    "slms": [{
      "site_separation": [3, 3],
      "r": 20,
      "c": 20,
      "location": [0, 0]}],
    "offset": [0, 0],
    "dimension": [60, 60]
  }],
  "rydberg_range": [[[0, 0], [2, 1]]]
})"_json;
  EXPECT_THROW(std::ignore = Architecture::fromJSON(spec),
               std::invalid_argument);
}
TEST(ArchitectureTest, InvalidSLMSeparation) {
  nlohmann::json spec = R"({
  "name": "invalid_architecture",
  "storage_zones": [{
    "slms": [{
      "id": 0,
      "site_separation": 3,
      "r": 20,
      "c": 20,
      "location": [0, 0]}],
    "offset": [0, 0],
    "dimension": [60, 60]
  }],
  "rydberg_range": [[[0, 0], [2, 1]]]
})"_json;
  EXPECT_THROW(std::ignore = Architecture::fromJSON(spec),
               std::invalid_argument);
}
TEST(ArchitectureTest, MissingSLMSeparation) {
  nlohmann::json spec = R"({
  "name": "invalid_architecture",
  "storage_zones": [{
    "slms": [{
      "id": 0,
      "r": 20,
      "c": 20,
      "location": [0, 0]}],
    "offset": [0, 0],
    "dimension": [60, 60]
  }],
  "rydberg_range": [[[0, 0], [2, 1]]]
})"_json;
  EXPECT_THROW(std::ignore = Architecture::fromJSON(spec),
               std::invalid_argument);
}
TEST(ArchitectureTest, InvalidSLMLocation) {
  nlohmann::json spec = R"({
  "name": "invalid_architecture",
  "storage_zones": [{
    "slms": [{
      "id": 0,
      "site_separation": [3, 3],
      "r": 20,
      "c": 20,
      "location": 0}],
    "offset": [0, 0],
    "dimension": [60, 60]
  }],
  "rydberg_range": [[[0, 0], [2, 1]]]
})"_json;
  EXPECT_THROW(std::ignore = Architecture::fromJSON(spec),
               std::invalid_argument);
}
TEST(ArchitectureTest, MissingSLMLocation) {
  nlohmann::json spec = R"({
  "name": "invalid_architecture",
  "storage_zones": [{
    "slms": [{
      "id": 0,
      "site_separation": [3, 3],
      "r": 20,
      "c": 20}],
    "offset": [0, 0],
    "dimension": [60, 60]
  }],
  "rydberg_range": [[[0, 0], [2, 1]]]
})"_json;
  EXPECT_THROW(std::ignore = Architecture::fromJSON(spec),
               std::invalid_argument);
}
TEST(ArchitectureTest, InvalidSLMRows) {
  nlohmann::json spec = R"({
  "name": "invalid_architecture",
  "storage_zones": [{
    "slms": [{
      "id": 0,
      "site_separation": [3, 3],
      "r": "twenty",
      "c": 20,
      "location": [0, 0]}],
    "offset": [0, 0],
    "dimension": [60, 60]
  }],
  "rydberg_range": [[[0, 0], [2, 1]]]
})"_json;
  EXPECT_THROW(std::ignore = Architecture::fromJSON(spec),
               std::invalid_argument);
}
TEST(ArchitectureTest, MissingSLMRows) {
  nlohmann::json spec = R"({
  "name": "invalid_architecture",
  "storage_zones": [{
    "slms": [{
      "id": 0,
      "site_separation": [3, 3],
      "c": 20,
      "location": [0, 0]}],
    "offset": [0, 0],
    "dimension": [60, 60]
  }],
  "rydberg_range": [[[0, 0], [2, 1]]]
})"_json;
  EXPECT_THROW(std::ignore = Architecture::fromJSON(spec),
               std::invalid_argument);
}
TEST(ArchitectureTest, InvalidSLMColumns) {
  nlohmann::json spec = R"({
  "name": "invalid_architecture",
  "storage_zones": [{
    "slms": [{
      "id": 0,
      "site_separation": [3, 3],
      "r": 20,
      "c": "twenty",
      "location": [0, 0]}],
    "offset": [0, 0],
    "dimension": [60, 60]
  }],
  "rydberg_range": [[[0, 0], [2, 1]]]
})"_json;
  EXPECT_THROW(std::ignore = Architecture::fromJSON(spec),
               std::invalid_argument);
}
TEST(ArchitectureTest, MissingSLMColumns) {
  nlohmann::json spec = R"({
  "name": "invalid_architecture",
  "storage_zones": [{
    "slms": [{
      "id": 0,
      "site_separation": [3, 3],
      "r": 20,
      "location": [0, 0]}],
    "offset": [0, 0],
    "dimension": [60, 60]
  }],
  "rydberg_range": [[[0, 0], [2, 1]]]
})"_json;
  EXPECT_THROW(std::ignore = Architecture::fromJSON(spec),
               std::invalid_argument);
}
TEST(ArchitectureTest, SLMEqualityOperator) {
  const auto slm = SLM::fromJSON(R"({
  "id": 0,
  "site_separation": [3, 3],
  "r": 20,
  "c": 20,
  "location": [0, 0]
})"_json);
  // &other == this
  EXPECT_TRUE(slm == slm);
  const auto slmOther = SLM::fromJSON(R"({
  "id": 0,
  "site_separation": [3, 3],
  "r": 20,
  "c": 20,
  "location": [0, 0]
})"_json);
  // equal slm
  EXPECT_TRUE(slm == slmOther);
  const auto slmOtherLocation = SLM::fromJSON(R"({
  "id": 0,
  "site_separation": [3, 3],
  "r": 20,
  "c": 20,
  "location": [1, 0]
})"_json);
  // other.location != location
  EXPECT_FALSE(slm == slmOtherLocation);
  const auto slmOtherRows = SLM::fromJSON(R"({
  "id": 0,
  "site_separation": [3, 3],
  "r": 21,
  "c": 20,
  "location": [0, 0]
})"_json);
  // other.nRows != nRows || other.nCols != nCols
  EXPECT_FALSE(slm == slmOtherRows);
  const auto slmOtherSeparation = SLM::fromJSON(R"({
  "id": 0,
  "site_separation": [4, 3],
  "r": 20,
  "c": 20,
  "location": [0, 0]
})"_json);
  // other.siteSeparation != siteSeparation
  EXPECT_FALSE(slm == slmOtherSeparation);
  auto slmEntanglement = SLM::fromJSON(R"({
  "id": 0,
  "site_separation": [4, 3],
  "r": 20,
  "c": 20,
  "location": [0, 0]
})"_json);
  slmEntanglement.entanglementId_ = 0;
  // other.entanglementZone_ != entanglementZone_
  EXPECT_FALSE(slm == slmEntanglement);
  auto slmOtherEntanglement = SLM::fromJSON(R"({
  "id": 0,
  "site_separation": [4, 3],
  "r": 20,
  "c": 20,
  "location": [0, 0]
})"_json);
  slmEntanglement.entanglementId_ = 1;
  // other.entanglementZone_ != entanglementZone_
  EXPECT_FALSE(slm == slmOtherEntanglement);
}
TEST(ArchitectureTest, InvalidAODId) {
  nlohmann::json spec = R"({
  "name": "invalid_architecture",
  "storage_zones": [{
    "slms": [{
      "id": 0,
      "site_separation": [3, 3],
      "r": 20,
      "c": 20,
      "location": [0, 0]}],
    "offset": [0, 0],
    "dimension": [60, 60]
  }],
  "entanglement_zones": [{
    "zone_id": 0,
    "slms": [
      {
        "id": 1,
        "site_separation": [12, 10],
        "r": 4,
        "c": 4,
        "location": [5, 70]
      },
      {
        "id": 2,
        "site_separation": [12, 10],
        "r": 4,
        "c": 4,
        "location": [7, 70]
      }],
    "offset": [5, 70],
    "dimension": [50, 40]
  }],
  "aods":[{"id": "one", "site_separation": 2, "r": 20, "c": 20}],
  "rydberg_range": [[[0, 0], [2, 1]]]
})"_json;
  EXPECT_THROW(std::ignore = Architecture::fromJSON(spec),
               std::invalid_argument);
}
TEST(ArchitectureTest, MissingAODId) {
  nlohmann::json spec = R"({
  "name": "invalid_architecture",
  "storage_zones": [{
    "slms": [{
      "id": 0,
      "site_separation": [3, 3],
      "r": 20,
      "c": 20,
      "location": [0, 0]}],
    "offset": [0, 0],
    "dimension": [60, 60]
  }],
  "entanglement_zones": [{
    "zone_id": 0,
    "slms": [
      {
        "id": 1,
        "site_separation": [12, 10],
        "r": 4,
        "c": 4,
        "location": [5, 70]
      },
      {
        "id": 2,
        "site_separation": [12, 10],
        "r": 4,
        "c": 4,
        "location": [7, 70]
      }],
    "offset": [5, 70],
    "dimension": [50, 40]
  }],
  "aods":[{"site_separation": 2, "r": 20, "c": 20}],
  "rydberg_range": [[[0, 0], [2, 1]]]
})"_json;
  EXPECT_THROW(std::ignore = Architecture::fromJSON(spec),
               std::invalid_argument);
}
TEST(ArchitectureTest, InvalidAODSeparation) {
  nlohmann::json spec = R"({
  "name": "invalid_architecture",
  "storage_zones": [{
    "slms": [{
      "id": 0,
      "site_separation": [3, 3],
      "r": 20,
      "c": 20,
      "location": [0, 0]}],
    "offset": [0, 0],
    "dimension": [60, 60]
  }],
  "entanglement_zones": [{
    "zone_id": 0,
    "slms": [
      {
        "id": 1,
        "site_separation": [12, 10],
        "r": 4,
        "c": 4,
        "location": [5, 70]
      },
      {
        "id": 2,
        "site_separation": [12, 10],
        "r": 4,
        "c": 4,
        "location": [7, 70]
      }],
    "offset": [5, 70],
    "dimension": [50, 40]
  }],
  "aods":[{"id": 0, "site_separation": "2 um", "r": 20, "c": 20}],
  "rydberg_range": [[[0, 0], [2, 1]]]
})"_json;
  EXPECT_THROW(std::ignore = Architecture::fromJSON(spec),
               std::invalid_argument);
}
TEST(ArchitectureTest, MissingAODSeparation) {
  nlohmann::json spec = R"({
  "name": "invalid_architecture",
  "storage_zones": [{
    "slms": [{
      "id": 0,
      "site_separation": [3, 3],
      "r": 20,
      "c": 20,
      "location": [0, 0]}],
    "offset": [0, 0],
    "dimension": [60, 60]
  }],
  "entanglement_zones": [{
    "zone_id": 0,
    "slms": [
      {
        "id": 1,
        "site_separation": [12, 10],
        "r": 4,
        "c": 4,
        "location": [5, 70]
      },
      {
        "id": 2,
        "site_separation": [12, 10],
        "r": 4,
        "c": 4,
        "location": [7, 70]
      }],
    "offset": [5, 70],
    "dimension": [50, 40]
  }],
  "aods":[{"id": 0, "r": 20, "c": 20}],
  "rydberg_range": [[[0, 0], [2, 1]]]
})"_json;
  EXPECT_THROW(std::ignore = Architecture::fromJSON(spec),
               std::invalid_argument);
}
TEST(ArchitectureTest, InvalidAODRows) {
  nlohmann::json spec = R"({
  "name": "invalid_architecture",
  "storage_zones": [{
    "slms": [{
      "id": 0,
      "site_separation": [3, 3],
      "r": 20,
      "c": 20,
      "location": [0, 0]}],
    "offset": [0, 0],
    "dimension": [60, 60]
  }],
  "entanglement_zones": [{
    "zone_id": 0,
    "slms": [
      {
        "id": 1,
        "site_separation": [12, 10],
        "r": 4,
        "c": 4,
        "location": [5, 70]
      },
      {
        "id": 2,
        "site_separation": [12, 10],
        "r": 4,
        "c": 4,
        "location": [7, 70]
      }],
    "offset": [5, 70],
    "dimension": [50, 40]
  }],
  "aods":[{"id": 0, "site_separation": 2, "r": "twenty", "c": 20}],
  "rydberg_range": [[[0, 0], [2, 1]]]
})"_json;
  EXPECT_THROW(std::ignore = Architecture::fromJSON(spec),
               std::invalid_argument);
}
TEST(ArchitectureTest, MissingAODRows) {
  nlohmann::json spec = R"({
  "name": "invalid_architecture",
  "storage_zones": [{
    "slms": [{
      "id": 0,
      "site_separation": [3, 3],
      "r": 20,
      "c": 20,
      "location": [0, 0]}],
    "offset": [0, 0],
    "dimension": [60, 60]
  }],
  "entanglement_zones": [{
    "zone_id": 0,
    "slms": [
      {
        "id": 1,
        "site_separation": [12, 10],
        "r": 4,
        "c": 4,
        "location": [5, 70]
      },
      {
        "id": 2,
        "site_separation": [12, 10],
        "r": 4,
        "c": 4,
        "location": [7, 70]
      }],
    "offset": [5, 70],
    "dimension": [50, 40]
  }],
  "aods":[{"id": 0, "site_separation": 2, "c": 20}],
  "rydberg_range": [[[0, 0], [2, 1]]]
})"_json;
  EXPECT_THROW(std::ignore = Architecture::fromJSON(spec),
               std::invalid_argument);
}
TEST(ArchitectureTest, InvalidAODColumns) {
  nlohmann::json spec = R"({
  "name": "invalid_architecture",
  "storage_zones": [{
    "slms": [{
      "id": 0,
      "site_separation": [3, 3],
      "r": 20,
      "c": 20,
      "location": [0, 0]}],
    "offset": [0, 0],
    "dimension": [60, 60]
  }],
  "entanglement_zones": [{
    "zone_id": 0,
    "slms": [
      {
        "id": 1,
        "site_separation": [12, 10],
        "r": 4,
        "c": 4,
        "location": [5, 70]
      },
      {
        "id": 2,
        "site_separation": [12, 10],
        "r": 4,
        "c": 4,
        "location": [7, 70]
      }],
    "offset": [5, 70],
    "dimension": [50, 40]
  }],
  "aods":[{"id": 0, "site_separation": 2, "r": 20, "c": "twenty"}],
  "rydberg_range": [[[0, 0], [2, 1]]]
})"_json;
  EXPECT_THROW(std::ignore = Architecture::fromJSON(spec),
               std::invalid_argument);
}
TEST(ArchitectureTest, MissingAODColumns) {
  nlohmann::json spec = R"({
  "name": "invalid_architecture",
  "storage_zones": [{
    "slms": [{
      "id": 0,
      "site_separation": [3, 3],
      "r": 20,
      "c": 20,
      "location": [0, 0]}],
    "offset": [0, 0],
    "dimension": [60, 60]
  }],
  "entanglement_zones": [{
    "zone_id": 0,
    "slms": [
      {
        "id": 1,
        "site_separation": [12, 10],
        "r": 4,
        "c": 4,
        "location": [5, 70]
      },
      {
        "id": 2,
        "site_separation": [12, 10],
        "r": 4,
        "c": 4,
        "location": [7, 70]
      }],
    "offset": [5, 70],
    "dimension": [50, 40]
  }],
  "aods":[{"id": 0, "site_separation": 2, "r": 20}],
  "rydberg_range": [[[0, 0], [2, 1]]]
})"_json;
  EXPECT_THROW(std::ignore = Architecture::fromJSON(spec),
               std::invalid_argument);
}
} // namespace na::zoned
