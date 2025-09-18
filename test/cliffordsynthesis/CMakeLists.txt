# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
# Copyright (c) 2025 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

if(TARGET MQT::QMapCliffordSynthesis)
  configure_file(${PROJECT_SOURCE_DIR}/test/cliffordsynthesis/tableaus.json
                 ${CMAKE_CURRENT_BINARY_DIR}/tableaus.json COPYONLY)
  configure_file(${PROJECT_SOURCE_DIR}/test/cliffordsynthesis/circuits.json
                 ${CMAKE_CURRENT_BINARY_DIR}/circuits.json COPYONLY)

  file(GLOB_RECURSE CLIFFORDSYNTHESIS_TEST_SOURCES *.cpp)
  package_add_test(mqt-qmap-cliffordsynthesis-test MQT::QMapCliffordSynthesis
                   ${CLIFFORDSYNTHESIS_TEST_SOURCES})
  target_link_libraries(mqt-qmap-cliffordsynthesis-test PRIVATE MQT::CoreQASM)
endif()
