# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
# Copyright (c) 2025 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

add_mqt_python_binding(
  QMAP
  ${MQT_QMAP_TARGET_NAME}-na-zoned-bindings
  zoned.cpp
  MODULE_NAME
  zoned
  INSTALL_DIR
  ./na
  LINK_LIBS
  MQT::QMapNAZoned
  MQT::CoreQASM
  pybind11_json)
