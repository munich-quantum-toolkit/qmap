# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

if(TARGET MQT::QMapNAQDMIDeviceConfig AND TARGET MQT::QMapNAQDMIDevice)
  set(QDMI_PREFIX "MQT_QMAP_NA")
  # Enforce that the device implements every interface function.
  generate_device_defs_executable(${QDMI_PREFIX} TARGET MQT::QMapNAQDMIDevice)

  set(TARGET_NAME mqt-qmap-na-qdmi-device-test)
  if(WIN32)
    # Discover tests after the provider and its runtime assets have been copied.
    set(CMAKE_GTEST_DISCOVER_TESTS_DISCOVERY_MODE PRE_TEST)
  endif()

  package_add_test(${TARGET_NAME} MQT::QMapNAQDMIDevice test_configuration.cpp test_device.cpp)
  add_dependencies(${TARGET_NAME} qdmi_test_mqt_qmap_na_device_defs)
  target_link_libraries(${TARGET_NAME} PRIVATE MQT::QMapNAQDMIDeviceConfig
                                               nlohmann_json::nlohmann_json)

  if(DEFINED SKBUILD_PROJECT_VERSION)
    set(MQT_QMAP_DEVICE_VERSION "${SKBUILD_PROJECT_VERSION}")
  else()
    set(MQT_QMAP_DEVICE_VERSION "0.0.0")
  endif()
  target_compile_definitions(
    ${TARGET_NAME}
    PRIVATE MQT_QMAP_VERSION="${MQT_QMAP_DEVICE_VERSION}"
            NA_DEVICE_JSON="${PROJECT_SOURCE_DIR}/json/na/mqt-qmap-qdmi-na-device.json")

  # On Windows, copy the provider and its runtime assets beside the test executable.
  if(WIN32)
    mqt_copy_qdmi_runtime(${TARGET_NAME} MQT::QMapNAQDMIDevice)
  endif()
endif()
