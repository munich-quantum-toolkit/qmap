# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
# Copyright (c) 2025 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

add_subdirectory(nasp)
add_subdirectory(zoned)

# Install the Python stub files in editable mode for better IDE support
if(SKBUILD_STATE STREQUAL "editable")
  file(GLOB PYI_FILES ${PROJECT_SOURCE_DIR}/python/mqt/qmap/na/*.pyi)
  install(
    FILES ${PYI_FILES}
    DESTINATION ./na
    COMPONENT ${MQT_QMAP_TARGET_NAME}_Python)
endif()
