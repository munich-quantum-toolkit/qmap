# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
# Copyright (c) 2025 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

add_mqt_python_binding(
  QMAP
  ${MQT_QMAP_TARGET_NAME}-sc-bindings
  sc.cpp
  MODULE_NAME
  sc
  INSTALL_DIR
  ./sc
  LINK_LIBS
  MQT::QMapSCExact
  MQT::QMapSCHeuristic
  MQT::CoreQASM
  pybind11_json)
target_compile_definitions(${MQT_QMAP_TARGET_NAME}-sc-bindings PRIVATE Z3_FOUND)

# install the Python stub files in editable mode for better IDE support
if(SKBUILD_STATE STREQUAL "editable")
  install(
    FILES ${PROJECT_SOURCE_DIR}/python/mqt/qmap/sc/sc.pyi
    DESTINATION ./sc
    COMPONENT ${MQT_QMAP_TARGET_NAME}_Python)
endif()
