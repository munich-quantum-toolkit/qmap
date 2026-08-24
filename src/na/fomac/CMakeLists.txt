# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

set(TARGET_NAME ${MQT_QMAP_TARGET_NAME}-na-qdmi-client)

if(NOT TARGET ${TARGET_NAME})
  file(GLOB HEADERS ${MQT_QMAP_INCLUDE_BUILD_DIR}/na/fomac/*.hpp)
  file(GLOB SOURCES *.cpp)

  add_library(${TARGET_NAME} ${HEADERS} ${SOURCES})

  target_include_directories(${TARGET_NAME} PUBLIC $<BUILD_INTERFACE:${MQT_QMAP_INCLUDE_BUILD_DIR}>)

  if(TARGET MQT::CoreQDMI)
    set(MQT_QMAP_QDMI_CLIENT_TARGET MQT::CoreQDMI)
  elseif(TARGET MQT::CoreFoMaC)
    set(MQT_QMAP_QDMI_CLIENT_TARGET MQT::CoreFoMaC)
  else()
    message(FATAL_ERROR "MQT Core does not provide a compatible QDMI C++ client target")
  endif()

  target_link_libraries(
    ${TARGET_NAME}
    PUBLIC ${MQT_QMAP_QDMI_CLIENT_TARGET} MQT::QMapNAQDMIDeviceConfig nlohmann_json::nlohmann_json
    PRIVATE spdlog::spdlog MQT::CoreIR MQT::ProjectOptions MQT::ProjectWarnings)

  add_library(MQT::QMapNAQDMIClient ALIAS ${TARGET_NAME})
endif()
