# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
# Copyright (c) 2025 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

if(TARGET MQT::QMapSC)
  file(GLOB SC_TEST_SOURCES *.cpp)
  package_add_test(mqt-qmap-sc-test MQT::QMapSC ${SC_TEST_SOURCES})
endif()

add_subdirectory(heuristic)
add_subdirectory(exact)
