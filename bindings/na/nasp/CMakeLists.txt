# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
# Copyright (c) 2025 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

add_mqt_python_binding(
  QMAP
  ${MQT_QMAP_TARGET_NAME}-na-nasp-bindings
  nasp.cpp
  MODULE_NAME
  state_preparation
  INSTALL_DIR
  ./na
  LINK_LIBS
  MQT::NASP
  MQT::CoreQASM
  pybind11_json)
