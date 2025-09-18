# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
# Copyright (c) 2025 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

if(TARGET MQT::QMapHybrid)
  file(COPY ${PROJECT_SOURCE_DIR}/test/hybridmap/architectures
       DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
  file(COPY ${PROJECT_SOURCE_DIR}/test/hybridmap/circuits DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

  file(GLOB_RECURSE HYBRIDMAP_TEST_SOURCES *.cpp)
  package_add_test_with_working_dir(${PROJECT_NAME}-hybridmap-test MQT::QMapHybrid
                                    ${CMAKE_CURRENT_BINARY_DIR} ${HYBRIDMAP_TEST_SOURCES})
  target_link_libraries(${PROJECT_NAME}-hybridmap-test PRIVATE MQT::CoreQASM)
endif()
