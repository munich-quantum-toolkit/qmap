# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
# Copyright (c) 2025 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

if(TARGET MQT::QMapSCHeuristic)
  file(GLOB_RECURSE SC_HEURISTIC_TEST_SOURCES *.cpp)
  package_add_test(mqt-qmap-sc-heuristic-test MQT::QMapSCHeuristic ${SC_HEURISTIC_TEST_SOURCES})
  target_link_libraries(mqt-qmap-sc-heuristic-test PRIVATE MQT::CoreQASM)
endif()
