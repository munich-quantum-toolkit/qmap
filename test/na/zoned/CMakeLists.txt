# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
# Copyright (c) 2025 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

if(TARGET MQT::QMapNAZoned)
  file(GLOB SOURCES *.cpp)
  file(GLOB CIRCUITS circuits/*.qasm)
  # make list of files a comma separated list of strings
  string(REPLACE ";" "\",\"" CIRCUITS "\"${CIRCUITS}\"")
  package_add_test(mqt-qmap-na-zoned-test MQT::QMapNAZoned ${SOURCES})
  target_link_libraries(mqt-qmap-na-zoned-test PRIVATE MQT::CoreCircuitOptimizer MQT::CoreQASM)
  target_compile_definitions(mqt-qmap-na-zoned-test PRIVATE TEST_CIRCUITS=${CIRCUITS})
endif()
