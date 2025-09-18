# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
# Copyright (c) 2025 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

if(TARGET MQT::LogicBlocks)
  file(GLOB_RECURSE LOGICBLOCKS_TEST_SOURCES *.cpp)
  package_add_test(mqt-logic-blocks-test MQT::LogicBlocks ${LOGICBLOCKS_TEST_SOURCES})
endif()
