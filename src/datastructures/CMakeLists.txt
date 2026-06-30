# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

set(TARGET_NAME ${MQT_QMAP_TARGET_NAME}-ds)

if(NOT TARGET ${TARGET_NAME})
  # collect headers and source files
  file(GLOB_RECURSE DS_HEADERS ${MQT_QMAP_INCLUDE_BUILD_DIR}/datastructures/**.hpp)
  file(GLOB_RECURSE DS_SOURCES **.cpp)

  # create the library target (initially empty)
  add_library(${TARGET_NAME} ${DS_HEADERS} ${DS_SOURCES})

  target_include_directories(${TARGET_NAME} PUBLIC $<BUILD_INTERFACE:${MQT_QMAP_INCLUDE_BUILD_DIR}>)

  target_link_libraries(
    ${TARGET_NAME}
    PUBLIC MQT::CoreIR
    PRIVATE MQT::ProjectOptions MQT::ProjectWarnings)

  add_library(MQT::QMapDS ALIAS ${TARGET_NAME})
endif()
