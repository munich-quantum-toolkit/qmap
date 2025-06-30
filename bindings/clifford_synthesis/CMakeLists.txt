# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
# Copyright (c) 2025 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

add_mqt_python_binding(
  QMAP
  ${MQT_QMAP_TARGET_NAME}-clifford_synthesis-bindings
  clifford_synthesis.cpp
  MODULE_NAME
  clifford_synthesis
  INSTALL_DIR
  ./clifford_synthesis
  LINK_LIBS
  MQT::QMapCliffordSynthesis
  MQT::CoreQASM
  pybind11_json)

# install the Python stub files in editable mode for better IDE support
if(SKBUILD_STATE STREQUAL "editable")
  install(
    FILES ${PROJECT_SOURCE_DIR}/python/mqt/qmap/clifford_synthesis/clifford_synthesis.pyi
    DESTINATION ./clifford_synthesis
    COMPONENT ${MQT_QMAP_TARGET_NAME}_Python)
endif()
