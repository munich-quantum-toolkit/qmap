# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
# Copyright (c) 2025 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

#
# This file is part of the MQT QMAP library released under the MIT license. See README.md or go to
# https://github.com/cda-tum/mqt-qmap for more information.
#

set(MQT_QMAP_SC_HEURISTIC_TARGET_NAME ${MQT_QMAP_TARGET_NAME}-sc-heuristic)

if(NOT TARGET ${MQT_QMAP_SC_HEURISTIC_TARGET_NAME})
  # collect headers and source files
  file(GLOB SC_HEURISTIC_HEADERS ${MQT_QMAP_INCLUDE_BUILD_DIR}/sc/heuristic/*.hpp)
  file(GLOB SC_HEURISTIC_SOURCES **.cpp)

  # add SC Heuristic Package library
  add_library(${MQT_QMAP_SC_HEURISTIC_TARGET_NAME} ${SC_HEURISTIC_HEADERS} ${SC_HEURISTIC_SOURCES})

  # set include directories
  target_include_directories(${MQT_QMAP_SC_HEURISTIC_TARGET_NAME}
                             PUBLIC $<BUILD_INTERFACE:${MQT_QMAP_INCLUDE_BUILD_DIR}>)

  # link to the MQT::Core libraries
  target_link_libraries(
    ${MQT_QMAP_SC_HEURISTIC_TARGET_NAME}
    PUBLIC MQT::QMapSC
    PRIVATE MQT::ProjectWarnings MQT::ProjectOptions)

  # add MQT alias
  add_library(MQT::QMapSCHeuristic ALIAS ${MQT_QMAP_SC_HEURISTIC_TARGET_NAME})
endif()
