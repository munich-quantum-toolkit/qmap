# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
# Copyright (c) 2025 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

set(TARGET_NAME ${MQT_QMAP_TARGET_NAME}-na-zoned)

if(NOT TARGET ${TARGET_NAME})
  file(GLOB_RECURSE HEADERS ${MQT_QMAP_INCLUDE_BUILD_DIR}/na/zoned/*.hpp)
  file(GLOB_RECURSE SOURCES *.cpp)

  add_library(${TARGET_NAME} ${HEADERS} ${SOURCES})

  target_include_directories(${TARGET_NAME} PUBLIC $<BUILD_INTERFACE:${MQT_QMAP_INCLUDE_BUILD_DIR}>)

  target_link_libraries(
    ${TARGET_NAME}
    PUBLIC MQT::CoreIR MQT::CoreNA nlohmann_json::nlohmann_json spdlog::spdlog
    PRIVATE MQT::ProjectOptions MQT::ProjectWarnings)

  add_library(MQT::QMapNAZoned ALIAS ${TARGET_NAME})
endif()
