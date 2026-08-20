/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "TestUtils.hpp"
#include "mqt_qmap_na_qdmi/device.h"
#include "na/qdmi/Configuration.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <functional>
#include <future>
#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp> // NOLINT(misc-include-cleaner)
#include <nlohmann/json_fwd.hpp>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace testing {
namespace {
std::string stringConcat5(const std::string& a, const std::string& b,
                          const std::string& c, const std::string& d,
                          const std::string& e) {
  std::stringstream ss;
  ss << a << b << c << d << e;
  return ss.str();
}
// NOLINTBEGIN(readability-identifier-naming,cppcoreguidelines-avoid-const-or-ref-data-members)
MATCHER_P2(IsBetween, a, b,
           stringConcat5(negation ? "isn't" : "is", " between ",
                         PrintToString(a), " and ", PrintToString(b))) {
  return a <= arg && arg <= b;
}
// NOLINTEND(readability-identifier-naming,cppcoreguidelines-avoid-const-or-ref-data-members)
} // namespace
} // namespace testing

namespace {

/// Hash function for a pair
struct PairHash {
  template <class T, class U>
  std::size_t operator()(const std::pair<T, U>& p) const noexcept {
    // Use the hash of the first and second element of the pair
    return std::hash<T>{}(p.first) ^ std::hash<U>{}(p.second);
  }
};

using mqt::test::ScopedEnvironmentVariable;

[[nodiscard]] std::string queryName(MQT_QMAP_NA_QDMI_Device_Session session) {
  size_t size = 0;
  if (MQT_QMAP_NA_QDMI_device_session_query_device_property(
          session, QDMI_DEVICE_PROPERTY_NAME, 0, nullptr, &size) !=
      QDMI_SUCCESS) {
    throw std::runtime_error("Failed to query device name size");
  }
  std::string name(size, '\0');
  if (MQT_QMAP_NA_QDMI_device_session_query_device_property(
          session, QDMI_DEVICE_PROPERTY_NAME, size, name.data(), nullptr) !=
      QDMI_SUCCESS) {
    throw std::runtime_error("Failed to query device name");
  }
  name.pop_back();
  return name;
}

[[nodiscard]] std::vector<MQT_QMAP_NA_QDMI_Site>
querySites(MQT_QMAP_NA_QDMI_Device_Session session) {
  size_t size = 0;
  if (MQT_QMAP_NA_QDMI_device_session_query_device_property(
          session, QDMI_DEVICE_PROPERTY_SITES, 0, nullptr, &size) !=
      QDMI_SUCCESS) {
    throw std::runtime_error("Failed to query sites");
  }
  if (size == 0) {
    throw std::runtime_error("No sites available");
  }
  std::vector<MQT_QMAP_NA_QDMI_Site> sites(size /
                                           sizeof(MQT_QMAP_NA_QDMI_Site));
  if (MQT_QMAP_NA_QDMI_device_session_query_device_property(
          session, QDMI_DEVICE_PROPERTY_SITES, size,
          static_cast<void*>(sites.data()), nullptr) != QDMI_SUCCESS) {
    throw std::runtime_error("Failed to query sites");
  }
  return sites;
}
[[nodiscard]] std::vector<MQT_QMAP_NA_QDMI_Operation>
queryOperations(MQT_QMAP_NA_QDMI_Device_Session session) {
  size_t size = 0;
  if (MQT_QMAP_NA_QDMI_device_session_query_device_property(
          session, QDMI_DEVICE_PROPERTY_OPERATIONS, 0, nullptr, &size) !=
      QDMI_SUCCESS) {
    throw std::runtime_error("Failed to query operations");
  }
  if (size == 0) {
    throw std::runtime_error("No operations available");
  }
  std::vector<MQT_QMAP_NA_QDMI_Operation> operations(
      size / sizeof(MQT_QMAP_NA_QDMI_Operation));
  if (MQT_QMAP_NA_QDMI_device_session_query_device_property(
          session, QDMI_DEVICE_PROPERTY_OPERATIONS, size,
          static_cast<void*>(operations.data()), nullptr) != QDMI_SUCCESS) {
    throw std::runtime_error("Failed to query operations");
  }
  return operations;
}

class NaQDMISpecificationTest : public ::testing::Test {
protected:
  MQT_QMAP_NA_QDMI_Device_Session session = nullptr;

  void SetUp() override {
    ASSERT_EQ(MQT_QMAP_NA_QDMI_device_initialize(), QDMI_SUCCESS)
        << "Failed to initialize the device";

    ASSERT_EQ(MQT_QMAP_NA_QDMI_device_session_alloc(&session), QDMI_SUCCESS)
        << "Failed to allocate a session";

    ASSERT_EQ(MQT_QMAP_NA_QDMI_device_session_init(session), QDMI_SUCCESS)
        << "Failed to initialize a session. Potential errors: Wrong or missing "
           "authentication information, device status is offline, or in "
           "maintenance. To provide credentials, take a look in " __FILE__
        << (__LINE__ - 4);
  }

  void TearDown() override {
    if (session != nullptr) {
      MQT_QMAP_NA_QDMI_device_session_free(session);
      session = nullptr;
    }
    MQT_QMAP_NA_QDMI_device_finalize();
  }
};

class NaQDMIJobSpecificationTest : public NaQDMISpecificationTest {
protected:
  MQT_QMAP_NA_QDMI_Device_Job job = nullptr;

  void SetUp() override {
    NaQDMISpecificationTest::SetUp();
    ASSERT_EQ(MQT_QMAP_NA_QDMI_device_session_create_device_job(session, &job),
              QDMI_SUCCESS)
        << "Failed to create a device job.";
  }

  void TearDown() override {
    if (job != nullptr) {
      MQT_QMAP_NA_QDMI_device_job_free(job);
      job = nullptr;
    }
    NaQDMISpecificationTest::TearDown();
  }
};

} // namespace

TEST(NaRuntimeConfiguration,
     SessionsOwnIndependentModelsAndRejectForeignHandles) {
  std::ifstream input(NA_DEVICE_JSON);
  ASSERT_TRUE(input);
  nlohmann::json customJson;
  input >> customJson;
  customJson["name"] = "Custom NA";
  customJson["numQubits"] = 5;
  const auto customConfiguration = customJson.dump();

  MQT_QMAP_NA_QDMI_Device_Session custom = nullptr;
  MQT_QMAP_NA_QDMI_Device_Session bundled = nullptr;
  ASSERT_EQ(MQT_QMAP_NA_QDMI_device_session_alloc(&custom), QDMI_SUCCESS);
  ASSERT_EQ(MQT_QMAP_NA_QDMI_device_session_alloc(&bundled), QDMI_SUCCESS);
  ASSERT_EQ(MQT_QMAP_NA_QDMI_device_session_set_parameter(
                custom, QDMI_DEVICE_SESSION_PARAMETER_CUSTOM1,
                customConfiguration.size() + 1, customConfiguration.c_str()),
            QDMI_SUCCESS);
  ASSERT_EQ(MQT_QMAP_NA_QDMI_device_session_init(custom), QDMI_SUCCESS);
  ASSERT_EQ(MQT_QMAP_NA_QDMI_device_session_init(bundled), QDMI_SUCCESS);

  size_t customQubits = 0;
  size_t bundledQubits = 0;
  ASSERT_EQ(MQT_QMAP_NA_QDMI_device_session_query_device_property(
                custom, QDMI_DEVICE_PROPERTY_QUBITSNUM, sizeof(customQubits),
                &customQubits, nullptr),
            QDMI_SUCCESS);
  ASSERT_EQ(MQT_QMAP_NA_QDMI_device_session_query_device_property(
                bundled, QDMI_DEVICE_PROPERTY_QUBITSNUM, sizeof(bundledQubits),
                &bundledQubits, nullptr),
            QDMI_SUCCESS);
  EXPECT_EQ(customQubits, 5);
  EXPECT_EQ(bundledQubits, 100);

  auto* const customSite = querySites(custom).front();
  auto* const customOperation = queryOperations(custom).front();
  EXPECT_EQ(
      MQT_QMAP_NA_QDMI_device_session_query_site_property(
          bundled, customSite, QDMI_SITE_PROPERTY_INDEX, 0, nullptr, nullptr),
      QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_query_operation_property(
                bundled, customOperation, 0, nullptr, 0, nullptr,
                QDMI_OPERATION_PROPERTY_NAME, 0, nullptr, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);

  MQT_QMAP_NA_QDMI_device_session_free(bundled);
  MQT_QMAP_NA_QDMI_device_session_free(custom);
}

TEST(NaRuntimeConfiguration, ValidatesRawParameterStringsAndRetry) {
  MQT_QMAP_NA_QDMI_Device_Session session = nullptr;
  ASSERT_EQ(MQT_QMAP_NA_QDMI_device_session_alloc(&session), QDMI_SUCCESS);
  EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_set_parameter(
                session, QDMI_DEVICE_SESSION_PARAMETER_CUSTOM2, 0, nullptr),
            QDMI_SUCCESS);
  EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_set_parameter(
                session, QDMI_DEVICE_SESSION_PARAMETER_CUSTOM1, 0, nullptr),
            QDMI_SUCCESS);
  constexpr std::array embeddedNul{'x', '\0', 'y', '\0'};
  EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_set_parameter(
                session, QDMI_DEVICE_SESSION_PARAMETER_CUSTOM2,
                embeddedNul.size(), embeddedNul.data()),
            QDMI_ERROR_INVALIDARGUMENT);
  constexpr std::array missingTerminator{'x'};
  EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_set_parameter(
                session, QDMI_DEVICE_SESSION_PARAMETER_CUSTOM1,
                missingTerminator.size(), missingTerminator.data()),
            QDMI_ERROR_INVALIDARGUMENT);
  constexpr auto missing = std::to_array("does-not-exist.json");
  ASSERT_EQ(MQT_QMAP_NA_QDMI_device_session_set_parameter(
                session, QDMI_DEVICE_SESSION_PARAMETER_CUSTOM2, missing.size(),
                missing.data()),
            QDMI_SUCCESS);
  EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_init(session), QDMI_ERROR_NOTFOUND);
  constexpr auto malformed = std::to_array("{");
  ASSERT_EQ(MQT_QMAP_NA_QDMI_device_session_set_parameter(
                session, QDMI_DEVICE_SESSION_PARAMETER_CUSTOM1,
                malformed.size(), malformed.data()),
            QDMI_SUCCESS);
  EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_init(session),
            QDMI_ERROR_INVALIDARGUMENT);
  ASSERT_EQ(MQT_QMAP_NA_QDMI_device_session_set_parameter(
                session, QDMI_DEVICE_SESSION_PARAMETER_CUSTOM1, 1, ""),
            QDMI_SUCCESS);
  ASSERT_EQ(MQT_QMAP_NA_QDMI_device_session_set_parameter(
                session, QDMI_DEVICE_SESSION_PARAMETER_CUSTOM2,
                std::strlen(NA_DEVICE_JSON) + 1, NA_DEVICE_JSON),
            QDMI_SUCCESS);
  EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_init(session), QDMI_SUCCESS);
  EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_set_parameter(
                session, QDMI_DEVICE_SESSION_PARAMETER_CUSTOM2, 1, ""),
            QDMI_ERROR_BADSTATE);
  MQT_QMAP_NA_QDMI_device_session_free(session);
}

TEST(NaRuntimeConfiguration, SelectsEnvironmentAndExplicitSources) {
  std::ifstream input(NA_DEVICE_JSON);
  ASSERT_TRUE(input);
  nlohmann::json environmentJson;
  input >> environmentJson;
  environmentJson["name"] = "Environment NA";
  const auto inlineEnvironment = environmentJson.dump();

  const ScopedEnvironmentVariable environmentInline(
      "MQT_QMAP_QDMI_NA_CONFIG_JSON", inlineEnvironment);
  const ScopedEnvironmentVariable environmentFile(
      "MQT_QMAP_QDMI_NA_CONFIG_FILE", "");
  MQT_QMAP_NA_QDMI_Device_Session environmentSession = nullptr;
  ASSERT_EQ(MQT_QMAP_NA_QDMI_device_session_alloc(&environmentSession),
            QDMI_SUCCESS);
  ASSERT_EQ(MQT_QMAP_NA_QDMI_device_session_init(environmentSession),
            QDMI_SUCCESS);
  EXPECT_EQ(queryName(environmentSession), "Environment NA");
  MQT_QMAP_NA_QDMI_device_session_free(environmentSession);

  MQT_QMAP_NA_QDMI_Device_Session explicitSession = nullptr;
  ASSERT_EQ(MQT_QMAP_NA_QDMI_device_session_alloc(&explicitSession),
            QDMI_SUCCESS);
  ASSERT_EQ(MQT_QMAP_NA_QDMI_device_session_set_parameter(
                explicitSession, QDMI_DEVICE_SESSION_PARAMETER_CUSTOM2,
                std::strlen(NA_DEVICE_JSON) + 1, NA_DEVICE_JSON),
            QDMI_SUCCESS);
  ASSERT_EQ(MQT_QMAP_NA_QDMI_device_session_init(explicitSession),
            QDMI_SUCCESS);
  EXPECT_EQ(queryName(explicitSession), "MQT QMAP NA Default QDMI Device");
  MQT_QMAP_NA_QDMI_device_session_free(explicitSession);
}

TEST(NaRuntimeConfiguration, InlineWinsOverExplicitFile) {
  std::ifstream input(NA_DEVICE_JSON);
  ASSERT_TRUE(input);
  nlohmann::json inlineJson;
  input >> inlineJson;
  inlineJson["name"] = "Inline NA";
  const auto configuration = inlineJson.dump();

  MQT_QMAP_NA_QDMI_Device_Session session = nullptr;
  ASSERT_EQ(MQT_QMAP_NA_QDMI_device_session_alloc(&session), QDMI_SUCCESS);
  ASSERT_EQ(MQT_QMAP_NA_QDMI_device_session_set_parameter(
                session, QDMI_DEVICE_SESSION_PARAMETER_CUSTOM2,
                std::strlen(NA_DEVICE_JSON) + 1, NA_DEVICE_JSON),
            QDMI_SUCCESS);
  ASSERT_EQ(MQT_QMAP_NA_QDMI_device_session_set_parameter(
                session, QDMI_DEVICE_SESSION_PARAMETER_CUSTOM1,
                configuration.size() + 1, configuration.c_str()),
            QDMI_SUCCESS);
  ASSERT_EQ(MQT_QMAP_NA_QDMI_device_session_init(session), QDMI_SUCCESS);
  EXPECT_EQ(queryName(session), "Inline NA");
  MQT_QMAP_NA_QDMI_device_session_free(session);
}

TEST(NaRuntimeConfiguration, RejectsConflictingEnvironmentSources) {
  const ScopedEnvironmentVariable environmentInline(
      "MQT_QMAP_QDMI_NA_CONFIG_JSON", "{}");
  const ScopedEnvironmentVariable environmentFile(
      "MQT_QMAP_QDMI_NA_CONFIG_FILE", NA_DEVICE_JSON);
  MQT_QMAP_NA_QDMI_Device_Session session = nullptr;
  ASSERT_EQ(MQT_QMAP_NA_QDMI_device_session_alloc(&session), QDMI_SUCCESS);
  EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_init(session),
            QDMI_ERROR_INVALIDARGUMENT);
  MQT_QMAP_NA_QDMI_device_session_free(session);
}

TEST(NaRuntimeConfiguration, InitializesIndependentSessionsConcurrently) {
  std::ifstream input(NA_DEVICE_JSON);
  ASSERT_TRUE(input);
  nlohmann::json base;
  input >> base;

  std::vector<std::future<std::string>> sessions;
  for (size_t i = 0; i < 4; ++i) {
    auto configuration = base;
    const auto name = "Concurrent NA " + std::to_string(i);
    configuration["name"] = name;
    // Exceptions are intentionally transported to the caller by the future.
    sessions.emplace_back(std::async(
        std::launch::async,
        // NOLINTNEXTLINE(bugprone-exception-escape)
        [configuration = configuration.dump(), name] {
          MQT_QMAP_NA_QDMI_Device_Session session = nullptr;
          if (MQT_QMAP_NA_QDMI_device_session_alloc(&session) != QDMI_SUCCESS) {
            return std::string{"allocation failed"};
          }
          const auto configured =
              MQT_QMAP_NA_QDMI_device_session_set_parameter(
                  session, QDMI_DEVICE_SESSION_PARAMETER_CUSTOM1,
                  configuration.size() + 1,
                  configuration.c_str()) == QDMI_SUCCESS;
          const auto initialized =
              configured &&
              MQT_QMAP_NA_QDMI_device_session_init(session) == QDMI_SUCCESS;
          const auto result = initialized
                                  ? queryName(session)
                                  : std::string{"initialization failed"};
          MQT_QMAP_NA_QDMI_device_session_free(session);
          return result;
        }));
  }
  for (size_t i = 0; i < sessions.size(); ++i) {
    EXPECT_EQ(sessions[i].get(), "Concurrent NA " + std::to_string(i));
  }
}

TEST_F(NaQDMISpecificationTest, SessionAlloc) {
  EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_alloc(nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(NaQDMISpecificationTest, SessionInit) {
  EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_init(session), QDMI_ERROR_BADSTATE);
  EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_init(nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(NaQDMISpecificationTest, SessionSetParameter) {
  MQT_QMAP_NA_QDMI_Device_Session uninitializedSession = nullptr;
  ASSERT_EQ(MQT_QMAP_NA_QDMI_device_session_alloc(&uninitializedSession),
            QDMI_SUCCESS);
  EXPECT_THAT(MQT_QMAP_NA_QDMI_device_session_set_parameter(
                  uninitializedSession, QDMI_DEVICE_SESSION_PARAMETER_BASEURL,
                  20, "https://example.com"),
              testing::AnyOf(QDMI_SUCCESS, QDMI_ERROR_NOTSUPPORTED,
                             QDMI_ERROR_INVALIDARGUMENT));
  EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_set_parameter(
                session, QDMI_DEVICE_SESSION_PARAMETER_BASEURL, 20,
                "https://example.com"),
            QDMI_ERROR_BADSTATE);
  EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_set_parameter(
                session, QDMI_DEVICE_SESSION_PARAMETER_MAX, 0, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(NaQDMISpecificationTest, JobCreate) {
  MQT_QMAP_NA_QDMI_Device_Session uninitializedSession = nullptr;
  MQT_QMAP_NA_QDMI_Device_Job job = nullptr;
  ASSERT_EQ(MQT_QMAP_NA_QDMI_device_session_alloc(&uninitializedSession),
            QDMI_SUCCESS);
  EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_create_device_job(
                uninitializedSession, &job),
            QDMI_ERROR_BADSTATE);
  EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_create_device_job(session, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_create_device_job(nullptr, &job),
            QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_THAT(MQT_QMAP_NA_QDMI_device_session_create_device_job(session, &job),
              testing::AnyOf(QDMI_SUCCESS, QDMI_ERROR_NOTSUPPORTED));
  MQT_QMAP_NA_QDMI_device_job_free(job);
}

TEST_F(NaQDMISpecificationTest, JobSetParameter) {
  EXPECT_EQ(MQT_QMAP_NA_QDMI_device_job_set_parameter(
                nullptr, QDMI_DEVICE_JOB_PARAMETER_MAX, 0, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(NaQDMIJobSpecificationTest, JobSetParameter) {
  QDMI_Program_Format value = QDMI_PROGRAM_FORMAT_QASM2;
  EXPECT_THAT(MQT_QMAP_NA_QDMI_device_job_set_parameter(
                  job, QDMI_DEVICE_JOB_PARAMETER_PROGRAMFORMAT,
                  sizeof(QDMI_Program_Format), &value),
              testing::AnyOf(QDMI_SUCCESS, QDMI_ERROR_NOTSUPPORTED));
  EXPECT_EQ(MQT_QMAP_NA_QDMI_device_job_set_parameter(
                job, QDMI_DEVICE_JOB_PARAMETER_MAX, 0, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(NaQDMISpecificationTest, JobQueryProperty) {
  EXPECT_EQ(MQT_QMAP_NA_QDMI_device_job_query_property(
                nullptr, QDMI_DEVICE_JOB_PROPERTY_MAX, 0, nullptr, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(NaQDMIJobSpecificationTest, JobQueryProperty) {
  EXPECT_THAT(MQT_QMAP_NA_QDMI_device_job_query_property(
                  job, QDMI_DEVICE_JOB_PROPERTY_ID, 0, nullptr, nullptr),
              testing::AnyOf(QDMI_SUCCESS, QDMI_ERROR_NOTSUPPORTED));
  EXPECT_EQ(MQT_QMAP_NA_QDMI_device_job_query_property(
                job, QDMI_DEVICE_JOB_PROPERTY_MAX, 0, nullptr, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(NaQDMIJobSpecificationTest, QueryJobId) {
  size_t size = 0;
  const auto status = MQT_QMAP_NA_QDMI_device_job_query_property(
      job, QDMI_DEVICE_JOB_PROPERTY_ID, 0, nullptr, &size);
  ASSERT_THAT(status, testing::AnyOf(QDMI_SUCCESS, QDMI_ERROR_NOTSUPPORTED));
  if (status == QDMI_ERROR_NOTSUPPORTED) {
    GTEST_SKIP() << "Job ID property is not supported by the device";
  }
  ASSERT_GT(size, 0);
  std::string id(size - 1, '\0');
  EXPECT_THAT(MQT_QMAP_NA_QDMI_device_job_query_property(
                  job, QDMI_DEVICE_JOB_PROPERTY_ID, size, id.data(), nullptr),
              testing::AnyOf(QDMI_SUCCESS, QDMI_ERROR_NOTSUPPORTED));
}

TEST_F(NaQDMISpecificationTest, JobSubmit) {
  EXPECT_EQ(MQT_QMAP_NA_QDMI_device_job_submit(nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(NaQDMIJobSpecificationTest, JobSubmit) {
  const auto status = MQT_QMAP_NA_QDMI_device_job_submit(job);
  ASSERT_THAT(status, testing::AnyOf(QDMI_SUCCESS, QDMI_ERROR_NOTSUPPORTED));
}

TEST_F(NaQDMISpecificationTest, JobCancel) {
  EXPECT_EQ(MQT_QMAP_NA_QDMI_device_job_cancel(nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(NaQDMIJobSpecificationTest, JobCancel) {
  const auto status = MQT_QMAP_NA_QDMI_device_job_cancel(job);
  ASSERT_THAT(status, testing::AnyOf(QDMI_SUCCESS, QDMI_ERROR_INVALIDARGUMENT,
                                     QDMI_ERROR_NOTSUPPORTED));
}

TEST_F(NaQDMISpecificationTest, JobCheck) {
  EXPECT_EQ(MQT_QMAP_NA_QDMI_device_job_check(nullptr, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(NaQDMIJobSpecificationTest, JobCheck) {
  QDMI_Job_Status jobStatus = QDMI_JOB_STATUS_RUNNING;
  const auto status = MQT_QMAP_NA_QDMI_device_job_check(job, &jobStatus);
  ASSERT_THAT(status, testing::AnyOf(QDMI_SUCCESS, QDMI_ERROR_NOTSUPPORTED));
}

TEST_F(NaQDMISpecificationTest, JobWait) {
  EXPECT_EQ(MQT_QMAP_NA_QDMI_device_job_wait(nullptr, 0),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(NaQDMIJobSpecificationTest, JobWait) {
  const auto status = MQT_QMAP_NA_QDMI_device_job_wait(job, 1);
  ASSERT_THAT(status, testing::AnyOf(QDMI_SUCCESS, QDMI_ERROR_NOTSUPPORTED,
                                     QDMI_ERROR_TIMEOUT));
}

TEST_F(NaQDMISpecificationTest, JobGetResults) {
  EXPECT_EQ(MQT_QMAP_NA_QDMI_device_job_get_results(
                nullptr, QDMI_JOB_RESULT_MAX, 0, nullptr, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(NaQDMIJobSpecificationTest, JobGetResults) {
  EXPECT_THAT(MQT_QMAP_NA_QDMI_device_job_get_results(
                  job, QDMI_JOB_RESULT_SHOTS, 0, nullptr, nullptr),
              testing::AnyOf(QDMI_SUCCESS, QDMI_ERROR_NOTSUPPORTED));
  EXPECT_EQ(MQT_QMAP_NA_QDMI_device_job_get_results(job, QDMI_JOB_RESULT_MAX, 0,
                                                    nullptr, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(NaQDMISpecificationTest, QueryDeviceProperty) {
  MQT_QMAP_NA_QDMI_Device_Session uninitializedSession = nullptr;
  ASSERT_EQ(MQT_QMAP_NA_QDMI_device_session_alloc(&uninitializedSession),
            QDMI_SUCCESS);
  EXPECT_EQ(
      MQT_QMAP_NA_QDMI_device_session_query_device_property(
          uninitializedSession, QDMI_DEVICE_PROPERTY_NAME, 0, nullptr, nullptr),
      QDMI_ERROR_BADSTATE);
  EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_query_device_property(
                nullptr, QDMI_DEVICE_PROPERTY_NAME, 0, nullptr, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_query_device_property(
                session, QDMI_DEVICE_PROPERTY_MAX, 0, nullptr, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_THAT(
      MQT_QMAP_NA_QDMI_device_session_query_device_property(
          session, QDMI_DEVICE_PROPERTY_COUPLINGMAP, 0, nullptr, nullptr),
      testing::AnyOf(QDMI_SUCCESS, QDMI_ERROR_NOTSUPPORTED));
}

TEST_F(NaQDMISpecificationTest, QuerySiteProperty) {
  MQT_QMAP_NA_QDMI_Site site = querySites(session).front();
  EXPECT_EQ(
      MQT_QMAP_NA_QDMI_device_session_query_site_property(
          session, nullptr, QDMI_SITE_PROPERTY_INDEX, 0, nullptr, nullptr),
      QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_query_site_property(
                nullptr, site, QDMI_SITE_PROPERTY_INDEX, 0, nullptr, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_query_site_property(
                session, site, QDMI_SITE_PROPERTY_MAX, 0, nullptr, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_THAT(MQT_QMAP_NA_QDMI_device_session_query_site_property(
                  session, site, QDMI_SITE_PROPERTY_NAME, 0, nullptr, nullptr),
              testing::AnyOf(QDMI_SUCCESS, QDMI_ERROR_NOTSUPPORTED));
}

TEST_F(NaQDMISpecificationTest, QueryOperationProperty) {
  MQT_QMAP_NA_QDMI_Operation operation = queryOperations(session).front();
  EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_query_operation_property(
                nullptr, operation, 0, nullptr, 0, nullptr,
                QDMI_OPERATION_PROPERTY_NAME, 0, nullptr, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_query_operation_property(
                session, operation, 0, nullptr, 0, nullptr,
                QDMI_OPERATION_PROPERTY_MAX, 0, nullptr, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_THAT(MQT_QMAP_NA_QDMI_device_session_query_operation_property(
                  session, operation, 0, nullptr, 0, nullptr,
                  QDMI_OPERATION_PROPERTY_QUBITSNUM, 0, nullptr, nullptr),
              testing::AnyOf(QDMI_SUCCESS, QDMI_ERROR_NOTSUPPORTED));
}

TEST_F(NaQDMISpecificationTest, QueryDeviceName) {
  size_t size = 0;
  ASSERT_EQ(MQT_QMAP_NA_QDMI_device_session_query_device_property(
                session, QDMI_DEVICE_PROPERTY_NAME, 0, nullptr, &size),
            QDMI_SUCCESS)
      << "Devices must provide a name";
  std::string value(size - 1, '\0');
  ASSERT_EQ(
      MQT_QMAP_NA_QDMI_device_session_query_device_property(
          session, QDMI_DEVICE_PROPERTY_NAME, size, value.data(), nullptr),
      QDMI_SUCCESS)
      << "Devices must provide a name";
  EXPECT_FALSE(value.empty()) << "Devices must provide a name";
}

TEST_F(NaQDMISpecificationTest, QueryDeviceVersion) {
  size_t size = 0;
  ASSERT_EQ(MQT_QMAP_NA_QDMI_device_session_query_device_property(
                session, QDMI_DEVICE_PROPERTY_VERSION, 0, nullptr, &size),
            QDMI_SUCCESS)
      << "Devices must provide a version";
  std::string value(size - 1, '\0');
  ASSERT_EQ(
      MQT_QMAP_NA_QDMI_device_session_query_device_property(
          session, QDMI_DEVICE_PROPERTY_VERSION, size, value.data(), nullptr),
      QDMI_SUCCESS)
      << "Devices must provide a version";
  EXPECT_FALSE(value.empty()) << "Devices must provide a version";
}

TEST_F(NaQDMISpecificationTest, QueryDeviceLibraryVersion) {
  size_t size = 0;
  ASSERT_EQ(
      MQT_QMAP_NA_QDMI_device_session_query_device_property(
          session, QDMI_DEVICE_PROPERTY_LIBRARYVERSION, 0, nullptr, &size),
      QDMI_SUCCESS)
      << "Devices must provide a library version";
  std::string value(size - 1, '\0');
  ASSERT_EQ(MQT_QMAP_NA_QDMI_device_session_query_device_property(
                session, QDMI_DEVICE_PROPERTY_LIBRARYVERSION, size,
                value.data(), nullptr),
            QDMI_SUCCESS)
      << "Devices must provide a library version";
  EXPECT_FALSE(value.empty()) << "Devices must provide a library version";
}

TEST_F(NaQDMISpecificationTest, QueryDeviceLengthUnit) {
  size_t size = 0;
  ASSERT_EQ(MQT_QMAP_NA_QDMI_device_session_query_device_property(
                session, QDMI_DEVICE_PROPERTY_LENGTHUNIT, 0, nullptr, &size),
            QDMI_SUCCESS);
  std::string value(size - 1, '\0');
  ASSERT_EQ(MQT_QMAP_NA_QDMI_device_session_query_device_property(
                session, QDMI_DEVICE_PROPERTY_LENGTHUNIT, size, value.data(),
                nullptr),
            QDMI_SUCCESS);
  EXPECT_THAT(value, testing::AnyOf("nm", "um", "mm"));
  double scaleFactor = 0.;
  const auto result = MQT_QMAP_NA_QDMI_device_session_query_device_property(
      session, QDMI_DEVICE_PROPERTY_LENGTHSCALEFACTOR, sizeof(double),
      &scaleFactor, nullptr);
  EXPECT_THAT(result, testing::AnyOf(QDMI_SUCCESS, QDMI_ERROR_NOTSUPPORTED));
  if (result == QDMI_SUCCESS) {
    EXPECT_GT(scaleFactor, 0.);
  }
}

TEST_F(NaQDMISpecificationTest, QueryDeviceDurationUnit) {
  size_t size = 0;
  ASSERT_EQ(MQT_QMAP_NA_QDMI_device_session_query_device_property(
                session, QDMI_DEVICE_PROPERTY_DURATIONUNIT, 0, nullptr, &size),
            QDMI_SUCCESS);
  std::string value(size - 1, '\0');
  ASSERT_EQ(MQT_QMAP_NA_QDMI_device_session_query_device_property(
                session, QDMI_DEVICE_PROPERTY_DURATIONUNIT, size, value.data(),
                nullptr),
            QDMI_SUCCESS);
  EXPECT_THAT(value, testing::AnyOf("ns", "us", "ms"));
  double scaleFactor = 0.;
  const auto result = MQT_QMAP_NA_QDMI_device_session_query_device_property(
      session, QDMI_DEVICE_PROPERTY_DURATIONSCALEFACTOR, sizeof(double),
      &scaleFactor, nullptr);
  EXPECT_THAT(result, testing::AnyOf(QDMI_SUCCESS, QDMI_ERROR_NOTSUPPORTED));
  if (result == QDMI_SUCCESS) {
    EXPECT_GT(scaleFactor, 0.);
  }
}

TEST_F(NaQDMISpecificationTest, QueryDeviceMinAtomDistance) {
  uint64_t minAtomDistance = 0;
  EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_query_device_property(
                session, QDMI_DEVICE_PROPERTY_MINATOMDISTANCE, sizeof(uint64_t),
                &minAtomDistance, nullptr),
            QDMI_SUCCESS);
}

TEST_F(NaQDMISpecificationTest, QuerySiteIndex) {
  size_t id = 0;
  EXPECT_NO_THROW(for (auto* site : querySites(session)) {
    EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_query_site_property(
                  session, site, QDMI_SITE_PROPERTY_INDEX, sizeof(size_t), &id,
                  nullptr),
              QDMI_SUCCESS)
        << "Devices must provide a site id";
  }) << "Devices must provide a list of sites";
}

TEST_F(NaQDMISpecificationTest, QueryOperationName) {
  size_t nameSize = 0;
  EXPECT_NO_THROW(for (auto* operation : queryOperations(session)) {
    EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_query_operation_property(
                  session, operation, 0, nullptr, 0, nullptr,
                  QDMI_OPERATION_PROPERTY_NAME, 0, nullptr, &nameSize),
              QDMI_SUCCESS)
        << "Devices must provide a operation name";
    std::string name(nameSize - 1, '\0');
    EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_query_operation_property(
                  session, operation, 0, nullptr, 0, nullptr,
                  QDMI_OPERATION_PROPERTY_NAME, nameSize, name.data(), nullptr),
              QDMI_SUCCESS)
        << "Devices must provide a operation name";
  }) << "Devices must provide a list of operations";
}

TEST_F(NaQDMISpecificationTest, QueryDeviceQubitNum) {
  size_t numQubits = 0;
  EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_query_device_property(
                session, QDMI_DEVICE_PROPERTY_QUBITSNUM, sizeof(size_t),
                &numQubits, nullptr),
            QDMI_SUCCESS);
}

namespace {

class NADeviceTest : public NaQDMISpecificationTest {
protected:
  // NOLINTNEXTLINE(misc-include-cleaner)
  na::Device device;

  void SetUp() override {
    NaQDMISpecificationTest::SetUp();
    // Open the file
    // NOLINTNEXTLINE(misc-include-cleaner)
    std::ifstream file(NA_DEVICE_JSON);
    ASSERT_TRUE(file.is_open()) << "Failed to open json file: " NA_DEVICE_JSON;

    // Parse the JSON file
    try {
      // NOLINTNEXTLINE(misc-include-cleaner)
      device = nlohmann::json::parse(file);
    } catch (const nlohmann::json::parse_error& e) {
      GTEST_FAIL() << "JSON parsing error: " << e.what();
    }
  }

  void TearDown() override { NaQDMISpecificationTest::TearDown(); }
};

} // namespace

TEST_F(NADeviceTest, QuerySiteData) {
  std::vector<MQT_QMAP_NA_QDMI_Site> sites;
  EXPECT_NO_THROW(sites = querySites(session))
      << "Devices must provide a sites";
  EXPECT_GT(sites.size(), 0);
  for (auto* site : sites) {
    int64_t x = 0;
    EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_query_site_property(
                  session, site, QDMI_SITE_PROPERTY_XCOORDINATE,
                  sizeof(int64_t), &x, nullptr),
              QDMI_SUCCESS);
    int64_t y = 0;
    EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_query_site_property(
                  session, site, QDMI_SITE_PROPERTY_YCOORDINATE,
                  sizeof(int64_t), &y, nullptr),
              QDMI_SUCCESS);
    bool isZone = false;
    EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_query_site_property(
                  session, site, QDMI_SITE_PROPERTY_ISZONE, sizeof(bool),
                  &isZone, nullptr),
              QDMI_SUCCESS);
    if (isZone) {
      uint64_t width = 0;
      EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_query_site_property(
                    session, site, QDMI_SITE_PROPERTY_XEXTENT, sizeof(uint64_t),
                    &width, nullptr),
                QDMI_SUCCESS);
      uint64_t height = 0;
      EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_query_site_property(
                    session, site, QDMI_SITE_PROPERTY_YEXTENT, sizeof(uint64_t),
                    &height, nullptr),
                QDMI_SUCCESS);
    } else {
      uint64_t module = 0;
      EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_query_site_property(
                    session, site, QDMI_SITE_PROPERTY_MODULEINDEX,
                    sizeof(uint64_t), &module, nullptr),
                QDMI_SUCCESS);
      uint64_t subModule = 0;
      EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_query_site_property(
                    session, site, QDMI_SITE_PROPERTY_SUBMODULEINDEX,
                    sizeof(uint64_t), &subModule, nullptr),
                QDMI_SUCCESS);
      int64_t originX = 0;
      uint64_t width = 0;
      originX = device.traps[module].extent.origin.x;
      width = device.traps[module].extent.size.width;
      EXPECT_THAT(x, ::testing::IsBetween(
                         originX, originX + static_cast<int64_t>(width)));
      int64_t originY = 0;
      uint64_t height = 0;
      originY = device.traps[module].extent.origin.y;
      height = device.traps[module].extent.size.height;
      EXPECT_THAT(y, ::testing::IsBetween(
                         originY, originY + static_cast<int64_t>(height)));
      EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_query_site_property(
                    session, site, QDMI_SITE_PROPERTY_XEXTENT, sizeof(uint64_t),
                    nullptr, nullptr),
                QDMI_ERROR_NOTSUPPORTED);
      EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_query_site_property(
                    session, site, QDMI_SITE_PROPERTY_YEXTENT, sizeof(uint64_t),
                    nullptr, nullptr),
                QDMI_ERROR_NOTSUPPORTED);
    }
    uint64_t t1 = 0;
    EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_query_site_property(
                  session, site, QDMI_SITE_PROPERTY_T1, sizeof(uint64_t), &t1,
                  nullptr),
              QDMI_SUCCESS);
    EXPECT_EQ(t1, device.decoherenceTimes.t1);
    uint64_t t2 = 0;
    EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_query_site_property(
                  session, site, QDMI_SITE_PROPERTY_T2, sizeof(uint64_t), &t2,
                  nullptr),
              QDMI_SUCCESS);
    EXPECT_EQ(t2, device.decoherenceTimes.t2);
  }
}

TEST_F(NADeviceTest, QueryOperationData) {
  std::vector<MQT_QMAP_NA_QDMI_Site> sites;
  EXPECT_NO_THROW(sites = querySites(session));
  std::vector<MQT_QMAP_NA_QDMI_Operation> operations;
  EXPECT_NO_THROW(operations = queryOperations(session));
  for (auto* operation : operations) {
    uint64_t duration = 0;
    bool isDurationSupported = false;
    uint64_t meanShuttlingSpeed = 0;
    bool isMeanShuttlingSpeedSupported = false;
    double fidelity = 0;
    bool isFidelitySupported = false;
    size_t numQubits = 0;
    bool isNumQubitsSupported = false;
    size_t numParameters = 0;
    bool isZoned = false;
    size_t nameSize = 0;
    ASSERT_EQ(MQT_QMAP_NA_QDMI_device_session_query_operation_property(
                  session, operation, 0, nullptr, 0, nullptr,
                  QDMI_OPERATION_PROPERTY_NAME, 0, nullptr, &nameSize),
              QDMI_SUCCESS);
    std::string name(nameSize - 1, '\0');
    EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_query_operation_property(
                  session, operation, 0, nullptr, 0, nullptr,
                  QDMI_OPERATION_PROPERTY_NAME, nameSize, name.data(), nullptr),
              QDMI_SUCCESS);
    auto result = MQT_QMAP_NA_QDMI_device_session_query_operation_property(
        session, operation, 0, nullptr, 0, nullptr,
        QDMI_OPERATION_PROPERTY_DURATION, sizeof(uint64_t), &duration, nullptr);
    EXPECT_THAT(result, testing::AnyOf(QDMI_SUCCESS, QDMI_ERROR_NOTSUPPORTED));
    if (result == QDMI_SUCCESS) {
      isDurationSupported = true;
      EXPECT_GT(duration, 0);
    }
    result = MQT_QMAP_NA_QDMI_device_session_query_operation_property(
        session, operation, 0, nullptr, 0, nullptr,
        QDMI_OPERATION_PROPERTY_FIDELITY, sizeof(double), &fidelity, nullptr);
    EXPECT_THAT(result, testing::AnyOf(QDMI_SUCCESS, QDMI_ERROR_NOTSUPPORTED));
    if (result == QDMI_SUCCESS) {
      isFidelitySupported = true;
      EXPECT_GT(fidelity, 0.);
      EXPECT_THAT(fidelity, testing::IsBetween(0., 1.));
    }
    result = MQT_QMAP_NA_QDMI_device_session_query_operation_property(
        session, operation, 0, nullptr, 0, nullptr,
        QDMI_OPERATION_PROPERTY_MEANSHUTTLINGSPEED, sizeof(uint64_t),
        &meanShuttlingSpeed, nullptr);
    EXPECT_THAT(result, testing::AnyOf(QDMI_SUCCESS, QDMI_ERROR_NOTSUPPORTED));
    if (result == QDMI_SUCCESS) {
      isMeanShuttlingSpeedSupported = true;
      EXPECT_GT(meanShuttlingSpeed, 0);
    }
    result = MQT_QMAP_NA_QDMI_device_session_query_operation_property(
        session, operation, 0, nullptr, 0, nullptr,
        QDMI_OPERATION_PROPERTY_QUBITSNUM, sizeof(size_t), &numQubits, nullptr);
    EXPECT_THAT(result, testing::AnyOf(QDMI_SUCCESS, QDMI_ERROR_NOTSUPPORTED));
    if (result == QDMI_SUCCESS) {
      isNumQubitsSupported = true;
      EXPECT_GT(numQubits, 0);
    }
    EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_query_operation_property(
                  session, operation, 0, nullptr, 0, nullptr,
                  QDMI_OPERATION_PROPERTY_ISZONED, sizeof(bool), &isZoned,
                  nullptr),
              QDMI_SUCCESS);
    EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_query_operation_property(
                  session, operation, 0, nullptr, 0, nullptr,
                  QDMI_OPERATION_PROPERTY_PARAMETERSNUM, sizeof(size_t),
                  &numParameters, nullptr),
              QDMI_SUCCESS);
    // isDurationSupported <==> isFidelitySupported
    EXPECT_EQ(isDurationSupported, isFidelitySupported);
    // isMeanShuttlingSpeedSupported <==> not isDurationSupported
    EXPECT_EQ(isMeanShuttlingSpeedSupported, !isDurationSupported);
    // isMeanShuttlingSpeedSupported ==> not isNumQubitsSupported
    EXPECT_TRUE(!isMeanShuttlingSpeedSupported || !isNumQubitsSupported);
    // isMeanShuttlingSpeedSupported ==> isZoned
    EXPECT_TRUE(!isMeanShuttlingSpeedSupported || isZoned);
    // not isZoned ==> isNumQubitsSupported
    EXPECT_TRUE(isZoned || isNumQubitsSupported);
    EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_query_operation_property(
                  session, operation, 0, nullptr, 0, nullptr,
                  QDMI_OPERATION_PROPERTY_PARAMETERSNUM, sizeof(size_t),
                  &numParameters, nullptr),
              QDMI_SUCCESS);
    size_t sitesSize = 0;
    EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_query_operation_property(
                  session, operation, 0, nullptr, 0, nullptr,
                  QDMI_OPERATION_PROPERTY_SITES, 0, nullptr, &sitesSize),
              QDMI_SUCCESS);
    if (isNumQubitsSupported && numQubits == 1) {
      std::unordered_set<MQT_QMAP_NA_QDMI_Site> supportedSites;
      for (const auto& site : sites) {
        result = MQT_QMAP_NA_QDMI_device_session_query_operation_property(
            session, operation, 1, &site, 0, nullptr,
            QDMI_OPERATION_PROPERTY_NAME, 0, nullptr, nullptr);
        ASSERT_THAT(result,
                    testing::AnyOf(QDMI_SUCCESS, QDMI_ERROR_NOTSUPPORTED));
        if (result == QDMI_SUCCESS) {
          supportedSites.emplace(site);
          bool isZone = false;
          EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_query_site_property(
                        session, site, QDMI_SITE_PROPERTY_ISZONE, sizeof(bool),
                        &isZone, nullptr),
                    QDMI_SUCCESS);
          EXPECT_EQ(isZone, isZoned);
          EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_query_operation_property(
                        session, operation, 0, nullptr, 0, nullptr,
                        QDMI_OPERATION_PROPERTY_INTERACTIONRADIUS, 0, nullptr,
                        nullptr),
                    QDMI_ERROR_NOTSUPPORTED);
          EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_query_operation_property(
                        session, operation, 0, nullptr, 0, nullptr,
                        QDMI_OPERATION_PROPERTY_BLOCKINGRADIUS, 0, nullptr,
                        nullptr),
                    QDMI_ERROR_NOTSUPPORTED);
          EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_query_operation_property(
                        session, operation, 0, nullptr, 0, nullptr,
                        QDMI_OPERATION_PROPERTY_IDLINGFIDELITY, 0, nullptr,
                        nullptr),
                    QDMI_ERROR_NOTSUPPORTED);
          if (!isZoned) {
            // operation is a local one
            int64_t x = 0;
            EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_query_site_property(
                          session, site, QDMI_SITE_PROPERTY_XCOORDINATE,
                          sizeof(int64_t), &x, nullptr),
                      QDMI_SUCCESS);
            int64_t y = 0;
            EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_query_site_property(
                          session, site, QDMI_SITE_PROPERTY_YCOORDINATE,
                          sizeof(int64_t), &y, nullptr),
                      QDMI_SUCCESS);
            int64_t minX = 0;
            int64_t maxX = 0;
            int64_t minY = 0;
            int64_t maxY = 0;
            if (const auto it = std::ranges::find_if(
                    device.localSingleQubitOperations,
                    [&name](const auto& op) { return op.name == name; });
                it != device.localSingleQubitOperations.end()) {
              minX = it->region.origin.x;
              maxX = minX + static_cast<int64_t>(it->region.size.width);
              minY = it->region.origin.y;
              maxY = minY + static_cast<int64_t>(it->region.size.height);
            } else {
              const auto it2 = std::ranges::find_if(
                  device.localMultiQubitOperations,
                  [&name](const auto& op) { return op.name == name; });
              ASSERT_NE(it2, device.localMultiQubitOperations.end());
              minX = it2->region.origin.x;
              maxX = minX + static_cast<int64_t>(it2->region.size.width);
              minY = it2->region.origin.y;
              maxY = minY + static_cast<int64_t>(it2->region.size.height);
            }
            EXPECT_THAT(x, ::testing::IsBetween(minX, maxX));
            EXPECT_THAT(y, ::testing::IsBetween(minY, maxY));
          }
        }
      }
      std::vector<MQT_QMAP_NA_QDMI_Site> queriedSupportedSitesVec(
          sitesSize / sizeof(MQT_QMAP_NA_QDMI_Site), nullptr);
      EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_query_operation_property(
                    session, operation, 0, nullptr, 0, nullptr,
                    QDMI_OPERATION_PROPERTY_SITES, sitesSize,
                    static_cast<void*>(queriedSupportedSitesVec.data()),
                    nullptr),
                QDMI_SUCCESS);
      const std::unordered_set queriedSupportedSitesSet(
          queriedSupportedSitesVec.cbegin(), queriedSupportedSitesVec.cend());
      EXPECT_EQ(queriedSupportedSitesSet, supportedSites);
    } else if (isNumQubitsSupported && numQubits == 2) {
      if (!isZoned) {
        EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_query_operation_property(
                      session, operation, 0, nullptr, 0, nullptr,
                      QDMI_OPERATION_PROPERTY_INTERACTIONRADIUS, 0, nullptr,
                      nullptr),
                  QDMI_SUCCESS);
        EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_query_operation_property(
                      session, operation, 0, nullptr, 0, nullptr,
                      QDMI_OPERATION_PROPERTY_BLOCKINGRADIUS, 0, nullptr,
                      nullptr),
                  QDMI_SUCCESS);
        EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_query_operation_property(
                      session, operation, 0, nullptr, 0, nullptr,
                      QDMI_OPERATION_PROPERTY_IDLINGFIDELITY, 0, nullptr,
                      nullptr),
                  QDMI_ERROR_NOTSUPPORTED);
        std::unordered_set<
            std::pair<MQT_QMAP_NA_QDMI_Site, MQT_QMAP_NA_QDMI_Site>, PairHash>
            supportedSites;
        for (const auto& site1 : sites) {
          for (const auto& site2 : sites) {
            if (site1 != site2) {
              const std::pair sitePair{site1, site2};
              result = MQT_QMAP_NA_QDMI_device_session_query_operation_property(
                  session, operation, 2,
                  reinterpret_cast<const MQT_QMAP_NA_QDMI_Site*>(&sitePair), 0,
                  nullptr, QDMI_OPERATION_PROPERTY_NAME, 0, nullptr, nullptr);
              ASSERT_THAT(result, testing::AnyOf(QDMI_SUCCESS,
                                                 QDMI_ERROR_NOTSUPPORTED));
              if (result == QDMI_SUCCESS) {
                // Ensure the pair is always in the same order when inserted
                // into the set, so that we can compare the sets later.
                if (sitePair.first < sitePair.second) {
                  supportedSites.emplace(sitePair);
                } else {
                  supportedSites.emplace(sitePair.second, sitePair.first);
                }
                bool isZone = false;
                EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_query_site_property(
                              session, sitePair.first,
                              QDMI_SITE_PROPERTY_ISZONE, sizeof(bool), &isZone,
                              nullptr),
                          QDMI_SUCCESS);
                EXPECT_FALSE(isZone);
                isZone = true;
                EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_query_site_property(
                              session, sitePair.second,
                              QDMI_SITE_PROPERTY_ISZONE, sizeof(bool), &isZone,
                              nullptr),
                          QDMI_SUCCESS);
                EXPECT_FALSE(isZone);
                int64_t minX = 0;
                int64_t maxX = 0;
                int64_t minY = 0;
                int64_t maxY = 0;
                if (const auto it = std::ranges::find_if(
                        device.localSingleQubitOperations,
                        [&name](const auto& op) { return op.name == name; });
                    it != device.localSingleQubitOperations.end()) {
                  minX = it->region.origin.x;
                  maxX = minX + static_cast<int64_t>(it->region.size.width);
                  minY = it->region.origin.y;
                  maxY = minY + static_cast<int64_t>(it->region.size.height);
                } else {
                  const auto it2 = std::ranges::find_if(
                      device.localMultiQubitOperations,
                      [&name](const auto& op) { return op.name == name; });
                  ASSERT_NE(it2, device.localMultiQubitOperations.end());
                  minX = it2->region.origin.x;
                  maxX = minX + static_cast<int64_t>(it2->region.size.width);
                  minY = it2->region.origin.y;
                  maxY = minY + static_cast<int64_t>(it2->region.size.height);
                }
                int64_t x = 0;
                EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_query_site_property(
                              session, sitePair.first,
                              QDMI_SITE_PROPERTY_XCOORDINATE, sizeof(int64_t),
                              &x, nullptr),
                          QDMI_SUCCESS);
                int64_t y = 0;
                EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_query_site_property(
                              session, sitePair.first,
                              QDMI_SITE_PROPERTY_YCOORDINATE, sizeof(int64_t),
                              &y, nullptr),
                          QDMI_SUCCESS);
                EXPECT_THAT(x, ::testing::IsBetween(minX, maxX));
                EXPECT_THAT(y, ::testing::IsBetween(minY, maxY));
                x = 0;
                EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_query_site_property(
                              session, sitePair.second,
                              QDMI_SITE_PROPERTY_XCOORDINATE, sizeof(int64_t),
                              &x, nullptr),
                          QDMI_SUCCESS);
                y = 0;
                EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_query_site_property(
                              session, sitePair.second,
                              QDMI_SITE_PROPERTY_YCOORDINATE, sizeof(int64_t),
                              &y, nullptr),
                          QDMI_SUCCESS);
                EXPECT_THAT(x, ::testing::IsBetween(minX, maxX));
                EXPECT_THAT(y, ::testing::IsBetween(minY, maxY));
              }
            }
          }
        }
        std::vector<std::pair<MQT_QMAP_NA_QDMI_Site, MQT_QMAP_NA_QDMI_Site>>
            queriedSupportedSitesVec(
                sitesSize / sizeof(std::pair<MQT_QMAP_NA_QDMI_Site,
                                             MQT_QMAP_NA_QDMI_Site>),
                {nullptr, nullptr});
        EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_query_operation_property(
                      session, operation, 0, nullptr, 0, nullptr,
                      QDMI_OPERATION_PROPERTY_SITES, sitesSize,
                      queriedSupportedSitesVec.data(), nullptr),
                  QDMI_SUCCESS);
        const std::unordered_set<
            std::pair<MQT_QMAP_NA_QDMI_Site, MQT_QMAP_NA_QDMI_Site>, PairHash>
            queriedSupportedSitesSet(queriedSupportedSitesVec.cbegin(),
                                     queriedSupportedSitesVec.cend());
        EXPECT_EQ(queriedSupportedSitesSet, supportedSites);
      } else {
        uint64_t interactionRadius = 0;
        EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_query_operation_property(
                      session, operation, 0, nullptr, 0, nullptr,
                      QDMI_OPERATION_PROPERTY_INTERACTIONRADIUS,
                      sizeof(uint64_t), &interactionRadius, nullptr),
                  QDMI_SUCCESS);
        EXPECT_GT(interactionRadius, 0);
        uint64_t blockingRadius = 0;
        EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_query_operation_property(
                      session, operation, 0, nullptr, 0, nullptr,
                      QDMI_OPERATION_PROPERTY_BLOCKINGRADIUS, sizeof(uint64_t),
                      &blockingRadius, nullptr),
                  QDMI_SUCCESS);
        EXPECT_GE(blockingRadius, interactionRadius);
        double idlingFidelity = 0.;
        EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_query_operation_property(
                      session, operation, 0, nullptr, 0, nullptr,
                      QDMI_OPERATION_PROPERTY_IDLINGFIDELITY, sizeof(double),
                      &idlingFidelity, nullptr),
                  QDMI_SUCCESS);
        EXPECT_GT(idlingFidelity, 0.);
        EXPECT_THAT(idlingFidelity, testing::IsBetween(0., 1.));
        std::unordered_set<MQT_QMAP_NA_QDMI_Site> supportedSites;
        for (const auto& site : sites) {
          result = MQT_QMAP_NA_QDMI_device_session_query_operation_property(
              session, operation, 1, &site, 0, nullptr,
              QDMI_OPERATION_PROPERTY_NAME, 0, nullptr, nullptr);
          ASSERT_THAT(result,
                      testing::AnyOf(QDMI_SUCCESS, QDMI_ERROR_NOTSUPPORTED));
          if (result == QDMI_SUCCESS) {
            supportedSites.emplace(site);
            bool isZone = false;
            EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_query_site_property(
                          session, site, QDMI_SITE_PROPERTY_ISZONE,
                          sizeof(bool), &isZone, nullptr),
                      QDMI_SUCCESS);
            EXPECT_TRUE(isZone);
          }
        }
        std::vector<MQT_QMAP_NA_QDMI_Site> queriedSupportedSitesVec(
            sitesSize / sizeof(MQT_QMAP_NA_QDMI_Site), nullptr);
        EXPECT_EQ(MQT_QMAP_NA_QDMI_device_session_query_operation_property(
                      session, operation, 0, nullptr, 0, nullptr,
                      QDMI_OPERATION_PROPERTY_SITES, sitesSize,
                      static_cast<void*>(queriedSupportedSitesVec.data()),
                      nullptr),
                  QDMI_SUCCESS);
        const std::unordered_set queriedSupportedSitesSet(
            queriedSupportedSitesVec.cbegin(), queriedSupportedSitesVec.cend());
        EXPECT_EQ(queriedSupportedSitesSet, supportedSites);
      }
    } // we do not test operations with more than two qubits
  }
}
