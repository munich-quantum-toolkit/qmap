# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

file(GLOB_RECURSE NA_SOURCES **.cpp)

add_mqt_python_binding_nanobind(
  QMAP
  ${MQT_QMAP_TARGET_NAME}-na-bindings
  ${NA_SOURCES}
  MODULE_NAME
  na
  INSTALL_DIR
  .
  LINK_LIBS
  MQT::NASP
  MQT::QMapNAZoned
  MQT::CoreQASM)

# Install the Python stub files in editable mode for better IDE support
if(SKBUILD_STATE STREQUAL "editable")
  file(GLOB PYI_FILES ${PROJECT_SOURCE_DIR}/python/mqt/qmap/na/*.pyi)
  install(
    FILES ${PYI_FILES}
    DESTINATION ./na
    COMPONENT ${MQT_QMAP_TARGET_NAME}_Python)
endif()
