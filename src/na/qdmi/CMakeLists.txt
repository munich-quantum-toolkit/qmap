# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

set(TARGET_NAME ${MQT_QMAP_TARGET_NAME}-na-qdmi-device-config)

if(NOT TARGET ${TARGET_NAME})
  file(GLOB HEADERS ${MQT_QMAP_INCLUDE_BUILD_DIR}/na/qdmi/Configuration.hpp)

  add_library(${TARGET_NAME} ${HEADERS} Configuration.cpp)

  target_include_directories(${TARGET_NAME} PUBLIC $<BUILD_INTERFACE:${MQT_QMAP_INCLUDE_BUILD_DIR}>)

  target_link_libraries(
    ${TARGET_NAME}
    PUBLIC nlohmann_json::nlohmann_json
    PRIVATE MQT::ProjectOptions MQT::ProjectWarnings)

  add_library(MQT::QMapNAQDMIDeviceConfig ALIAS ${TARGET_NAME})

  # Place the library in the common output directories.
  set_target_properties(
    ${TARGET_NAME}
    PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}"
               ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}"
               RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}")
endif()
