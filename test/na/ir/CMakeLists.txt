# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

if(TARGET MQT::QMapNAIR)
  file(GLOB SOURCES *.cpp)
  package_add_test(mqt-qmap-na-ir-test MQT::QMapNAIR ${SOURCES})
endif()
