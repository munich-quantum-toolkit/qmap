# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

# Declare all external dependencies and make sure that they are available.

include(CMakeDependentOption)
include(FetchContent)
set(FETCH_PACKAGES "")

# search for Z3
find_package(Z3 4.8.15)
if(NOT Z3_FOUND)
  message(
    WARNING "Did not find Z3. Exact mapper and Clifford synthesis libraries will not be available")
endif()

if(BUILD_MQT_QMAP_BINDINGS)
  # Manually detect the installed mqt-core package.
  execute_process(
    COMMAND "${Python_EXECUTABLE}" -m mqt.core --cmake_dir
    OUTPUT_STRIP_TRAILING_WHITESPACE
    OUTPUT_VARIABLE mqt-core_DIR
    ERROR_QUIET)

  # Add the detected directory to the CMake prefix path.
  if(mqt-core_DIR)
    list(APPEND CMAKE_PREFIX_PATH "${mqt-core_DIR}")
    message(STATUS "Found mqt-core package: ${mqt-core_DIR}")
  endif()

  execute_process(
    COMMAND "${Python_EXECUTABLE}" -m nanobind --cmake_dir
    OUTPUT_STRIP_TRAILING_WHITESPACE
    OUTPUT_VARIABLE nanobind_ROOT)
  find_package(nanobind CONFIG REQUIRED)
endif()

if(TARGET qdmi::qdmi)
  message(STATUS "QDMI is already available.")
else()
  message(STATUS "QDMI will be included via FetchContent")
  # cmake-format: off
  set(QDMI_MINIMUM_VERSION 1.4.0
      CACHE STRING "Minimum QDMI version")
  set(QDMI_VERSION 1.4.0
      CACHE STRING "QDMI version")
  # renovate: datasource=github-tags depName=Munich-Quantum-Software-Stack/QDMI
  set(QDMI_REV "6db03d83ad602aaa9fe928b5c3fc5cce84b10061" # tag=v1.4.0
      CACHE STRING "QDMI identifier (tag, branch or commit hash)")
  set(QDMI_REPO_OWNER "Munich-Quantum-Software-Stack"
      CACHE STRING "QDMI repository owner (change when using a fork)")
  set(INSTALL_QDMI
      OFF
      CACHE BOOL "Generate installation instructions for QDMI")
  # cmake-format: on
  FetchContent_Declare(
    qdmi
    GIT_REPOSITORY https://github.com/${QDMI_REPO_OWNER}/qdmi.git
    GIT_TAG ${QDMI_REV}
    FIND_PACKAGE_ARGS ${QDMI_MINIMUM_VERSION})
  list(APPEND FETCH_PACKAGES qdmi)
endif()

# cmake-format: off
set(MQT_CORE_MINIMUM_VERSION 3.12.0
    CACHE STRING "MQT Core minimum version")
set(MQT_CORE_VERSION 3.12.0
    CACHE STRING "MQT Core version")
# renovate: datasource=github-tags depName=munich-quantum-toolkit/core
set(MQT_CORE_REV "4012aa9c4e497561d56bea678e532fd38c24c421" # tag=v3.12.0
    CACHE STRING "MQT Core identifier (tag, branch or commit hash)")
set(MQT_CORE_REPO_OWNER "munich-quantum-toolkit"
    CACHE STRING "MQT Core repository owner (change when using a fork)")
# cmake-format: on
FetchContent_Declare(
  mqt-core
  GIT_REPOSITORY https://github.com/${MQT_CORE_REPO_OWNER}/core.git
  GIT_TAG ${MQT_CORE_REV}
  SYSTEM EXCLUDE_FROM_ALL FIND_PACKAGE_ARGS ${MQT_CORE_MINIMUM_VERSION})
list(APPEND FETCH_PACKAGES mqt-core)

cmake_dependent_option(JSON_Install "Install nlohmann_json library" ON "MQT_QMAP_INSTALL" OFF)
# cmake-format: off
# renovate: datasource=github-releases depName=nlohmann/json
set(JSON_TAG v3.12.0)
string(REGEX REPLACE "^v" "" JSON_VERSION "${JSON_TAG}")
FetchContent_Declare(
  nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/${JSON_TAG}/json.tar.xz
      FIND_PACKAGE_ARGS ${JSON_VERSION})
set(JSON_SystemInclude
    ON
    CACHE INTERNAL "Treat JSON headers as system headers")
list(APPEND FETCH_PACKAGES nlohmann_json)
# cmake-format: on

set(PLOG_REV
    "94899e0b926ac1b0f4750bfbd495167b4a6ae9ef"
    CACHE STRING "Plog revision")
FetchContent_Declare(
  plog
  GIT_REPOSITORY https://github.com/SergiusTheBest/plog.git
  GIT_TAG ${PLOG_REV}
  SYSTEM FIND_PACKAGE_ARGS)
list(APPEND FETCH_PACKAGES plog)

# renovate: datasource=github-releases depName=gabime/spdlog
set(SPDLOG_TAG v1.17.0)
string(REGEX REPLACE "^v" "" SPDLOG_VERSION "${SPDLOG_TAG}")
# Add position independent code for spdlog, this is required for python bindings on linux
set(SPDLOG_BUILD_PIC ON)
FetchContent_Declare(
  spdlog URL https://github.com/gabime/spdlog/archive/refs/tags/${SPDLOG_TAG}.tar.gz SYSTEM
             EXCLUDE_FROM_ALL FIND_PACKAGE_ARGS ${SPDLOG_VERSION})
list(APPEND FETCH_PACKAGES spdlog)

if(BUILD_MQT_QMAP_TESTS)
  set(gtest_force_shared_crt
      ON
      CACHE BOOL "" FORCE)
  set(GTEST_VERSION
      1.18.0
      CACHE STRING "Minimum installed GoogleTest version")
  # renovate: datasource=github-tags depName=google/googletest
  set(GTEST_REV
      "063de7e9578f82b369302001269680b4b1553359" # tag=v1.18.0
      CACHE STRING "GoogleTest source revision")
  FetchContent_Declare(
    googletest URL https://github.com/google/googletest/archive/${GTEST_REV}.tar.gz
                   EXCLUDE_FROM_ALL FIND_PACKAGE_ARGS ${GTEST_VERSION} NAMES GTest)
  list(APPEND FETCH_PACKAGES googletest)
endif()

# Make all declared dependencies available.
FetchContent_MakeAvailable(${FETCH_PACKAGES})
