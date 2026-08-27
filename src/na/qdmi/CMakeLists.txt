# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

set(TARGET_NAME ${MQT_QMAP_TARGET_NAME}-na-qdmi-device-config)

if(NOT TARGET ${TARGET_NAME})
  file(GLOB HEADERS ${MQT_QMAP_INCLUDE_BUILD_DIR}/na/qdmi/Configuration.hpp)

  add_library(${TARGET_NAME} ${HEADERS} Configuration.cpp)

  target_include_directories(${TARGET_NAME} PUBLIC $<BUILD_INTERFACE:${MQT_QMAP_INCLUDE_BUILD_DIR}>)

  target_link_libraries(
    ${TARGET_NAME}
    PUBLIC nlohmann_json::nlohmann_json
    PRIVATE spdlog::spdlog MQT::ProjectOptions MQT::ProjectWarnings)

  # The configuration schema uses the three-way comparison operator. Unlike the other targets, this
  # one links no MQT::Core library to inherit the standard from.
  target_compile_features(${TARGET_NAME} PUBLIC cxx_std_20)

  add_library(MQT::QMapNAQDMIDeviceConfig ALIAS ${TARGET_NAME})

  # Place the library in the common output directories. The device is a shared module that links
  # this static library, so it must contain position-independent code.
  set_target_properties(
    ${TARGET_NAME}
    PROPERTIES POSITION_INDEPENDENT_CODE ON
               LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}"
               ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}"
               RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}")
endif()

set(TARGET_NAME ${MQT_QMAP_TARGET_NAME}-na-qdmi-device)

# MQT Core ships its own neutral-atom device under `mqt.na.default`, so this provider claims a
# QMAP-owned identifier and prefix to keep both registrable side by side.
set(QDMI_PREFIX "MQT_QMAP_NA")

if(NOT TARGET ${TARGET_NAME})
  add_library(${TARGET_NAME} SHARED Device.cpp)
  set_target_properties(
    ${TARGET_NAME}
    PROPERTIES C_VISIBILITY_PRESET hidden
               CXX_VISIBILITY_PRESET hidden
               VISIBILITY_INLINES_HIDDEN 1
               POSITION_INDEPENDENT_CODE ON
               WINDOWS_EXPORT_ALL_SYMBOLS OFF)

  generate_prefixed_qdmi_headers(${QDMI_PREFIX})

  target_include_directories(
    ${TARGET_NAME} PUBLIC $<BUILD_INTERFACE:${MQT_QMAP_INCLUDE_BUILD_DIR}>
                          $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>)

  target_link_libraries(
    ${TARGET_NAME} PRIVATE MQT::CoreQDMICommon MQT::QMapNAQDMIDeviceConfig spdlog::spdlog
                           MQT::ProjectOptions MQT::ProjectWarnings)

  # The device reports its own version. Wheel builds get it from scikit-build-core; plain CMake
  # builds have no project version to report.
  if(DEFINED SKBUILD_PROJECT_VERSION)
    set(MQT_QMAP_DEVICE_VERSION "${SKBUILD_PROJECT_VERSION}")
  else()
    set(MQT_QMAP_DEVICE_VERSION "0.0.0")
  endif()
  target_compile_definitions(${TARGET_NAME} PRIVATE MQT_QMAP_VERSION="${MQT_QMAP_DEVICE_VERSION}")

  # The device reads its bundled description from beside the shared module. MQT Core v3.9.0 copies
  # runtime files into the build tree and exposes them to `mqt_copy_qdmi_runtime` consumers.
  mqt_configure_qdmi_device(
    ${TARGET_NAME}
    ID
    mqt.qmap.na.default
    PREFIX
    ${QDMI_PREFIX}
    RUNTIME_FILES
    "${PROJECT_SOURCE_DIR}/json/na/mqt-qmap-qdmi-na-device.json")

  add_library(MQT::QMapNAQDMIDevice ALIAS ${TARGET_NAME})

  # Ship the provider so that the Python package can register it. The device reads its bundled
  # description from beside the module, so both land in the same directory. Windows treats a shared
  # module as a runtime artifact and installs it under bin.
  if(WIN32)
    set(MQT_QMAP_NA_DEVICE_INSTALL_DIR ${CMAKE_INSTALL_BINDIR})
  else()
    set(MQT_QMAP_NA_DEVICE_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR})
  endif()
  install(
    TARGETS ${TARGET_NAME}
    LIBRARY DESTINATION ${MQT_QMAP_NA_DEVICE_INSTALL_DIR} COMPONENT ${MQT_QMAP_TARGET_NAME}_Python
    RUNTIME DESTINATION ${MQT_QMAP_NA_DEVICE_INSTALL_DIR} COMPONENT ${MQT_QMAP_TARGET_NAME}_Python)
  install(
    FILES "${PROJECT_SOURCE_DIR}/json/na/mqt-qmap-qdmi-na-device.json"
    DESTINATION ${MQT_QMAP_NA_DEVICE_INSTALL_DIR}
    COMPONENT ${MQT_QMAP_TARGET_NAME}_Python
    EXCLUDE_FROM_ALL)
endif()
