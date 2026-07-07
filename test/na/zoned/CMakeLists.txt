# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

if(TARGET MQT::QMapNAZoned)
  file(GLOB SOURCES *.cpp)
  file(GLOB HEADERS *.hpp)
  file(GLOB CIRCUITS circuits/*.qasm)
  # make list of files a comma separated list of strings
  string(REPLACE ";" "\",\"" CIRCUITS "\"${CIRCUITS}\"")
  package_add_test(mqt-qmap-na-zoned-test MQT::QMapNAZoned ${SOURCES})
  target_sources(mqt-qmap-na-zoned-test PRIVATE FILE_SET HEADERS BASE_DIRS ${CMAKE_SOURCE_DIR}/test
                                                FILES ${HEADERS})
  target_link_libraries(mqt-qmap-na-zoned-test PRIVATE MQT::CoreCircuitOptimizer MQT::CoreQASM)
  target_compile_definitions(mqt-qmap-na-zoned-test PRIVATE TEST_CIRCUITS=${CIRCUITS})
endif()
