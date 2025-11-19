# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
# Copyright (c) 2025 Munich Quantum Software Company GmbH
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

  if(NOT SKBUILD)
    # Manually detect the installed pybind11 package.
    execute_process(
      COMMAND "${Python_EXECUTABLE}" -m pybind11 --cmakedir
      OUTPUT_STRIP_TRAILING_WHITESPACE
      OUTPUT_VARIABLE pybind11_DIR)

    # Add the detected directory to the CMake prefix path.
    list(APPEND CMAKE_PREFIX_PATH "${pybind11_DIR}")
  endif()

  # add pybind11 library
  find_package(pybind11 3.0.0 CONFIG REQUIRED)
endif()

# cmake-format: off
set(MQT_CORE_MINIMUM_VERSION 3.3.3
    CACHE STRING "MQT Core minimum version")
set(MQT_CORE_VERSION 3.3.3
    CACHE STRING "MQT Core version")
set(MQT_CORE_REV "8c9f6ab24968401e450812fc0ff7d05b5ae07a63"
    CACHE STRING "MQT Core identifier (tag, branch or commit hash)")
set(MQT_CORE_REPO_OWNER "munich-quantum-toolkit"
    CACHE STRING "MQT Core repository owner (change when using a fork)")
# cmake-format: on
FetchContent_Declare(
  mqt-core
  GIT_REPOSITORY https://github.com/${MQT_CORE_REPO_OWNER}/core.git
  GIT_TAG ${MQT_CORE_REV}
  FIND_PACKAGE_ARGS ${MQT_CORE_MINIMUM_VERSION})
list(APPEND FETCH_PACKAGES mqt-core)

set(JSON_VERSION
    3.12.0
    CACHE STRING "nlohmann_json version")
set(JSON_URL https://github.com/nlohmann/json/releases/download/v${JSON_VERSION}/json.tar.xz)
set(JSON_SystemInclude
    ON
    CACHE INTERNAL "Treat the library headers like system headers")
cmake_dependent_option(JSON_Install "Install nlohmann_json library" ON "MQT_QMAP_INSTALL" OFF)
FetchContent_Declare(nlohmann_json URL ${JSON_URL} FIND_PACKAGE_ARGS ${JSON_VERSION})
list(APPEND FETCH_PACKAGES nlohmann_json)

set(PLOG_REV
    "94899e0b926ac1b0f4750bfbd495167b4a6ae9ef"
    CACHE STRING "Plog revision")
set(PLOG_URL https://github.com/SergiusTheBest/plog/archive/refs/tags/${PLOG_VERSION}.tar.gz)
FetchContent_Declare(
  plog
  GIT_REPOSITORY https://github.com/SergiusTheBest/plog.git
  GIT_TAG ${PLOG_REV}
  FIND_PACKAGE_ARGS)
list(APPEND FETCH_PACKAGES plog)

set(SPDLOG_VERSION
    1.15.3
    CACHE STRING "spdlog version")
set(SPDLOG_URL https://github.com/gabime/spdlog/archive/refs/tags/v${SPDLOG_VERSION}.tar.gz)
# Add position independent code for spdlog, this is required for python bindings on linux
set(SPDLOG_BUILD_PIC ON)
FetchContent_Declare(spdlog URL ${SPDLOG_URL} FIND_PACKAGE_ARGS ${SPDLOG_VERSION})
list(APPEND FETCH_PACKAGES spdlog)

if(BUILD_MQT_QMAP_TESTS)
  set(gtest_force_shared_crt
      ON
      CACHE BOOL "" FORCE)
  set(GTEST_VERSION
      1.17.0
      CACHE STRING "Google Test version")
  set(GTEST_URL https://github.com/google/googletest/archive/refs/tags/v${GTEST_VERSION}.tar.gz)
  FetchContent_Declare(googletest URL ${GTEST_URL} FIND_PACKAGE_ARGS ${GTEST_VERSION} NAMES GTest)
  list(APPEND FETCH_PACKAGES googletest)
endif()

if(BUILD_MQT_QMAP_BINDINGS)
  # add pybind11_json library
  FetchContent_Declare(
    pybind11_json
    GIT_REPOSITORY https://github.com/pybind/pybind11_json
    FIND_PACKAGE_ARGS)
  list(APPEND FETCH_PACKAGES pybind11_json)
endif()

# Make all declared dependencies available.
FetchContent_MakeAvailable(${FETCH_PACKAGES})

# Mark the plog includes as SYSTEM includes to suppress warnings.
get_target_property(PLOG_IID plog INTERFACE_INCLUDE_DIRECTORIES)
set_target_properties(plog PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${PLOG_IID}")
