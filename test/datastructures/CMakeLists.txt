# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

if(TARGET MQT::QMapDS)
  file(GLOB_RECURSE DS_TEST_SOURCES *.cpp)
  package_add_test(mqt-qmap-ds-test MQT::QMapDS ${DS_TEST_SOURCES})
endif()
