# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

if(TARGET MQT::QMapNAQDMIDeviceConfig)
  set(TARGET_NAME mqt-qmap-na-qdmi-device-config-test)
  package_add_test(${TARGET_NAME} MQT::QMapNAQDMIDeviceConfig test_configuration.cpp)
  target_link_libraries(${TARGET_NAME} PRIVATE nlohmann_json::nlohmann_json)
  target_compile_definitions(
    ${TARGET_NAME}
    PRIVATE NA_DEVICE_JSON="${PROJECT_SOURCE_DIR}/json/na/mqt-qmap-qdmi-na-device.json")
endif()
