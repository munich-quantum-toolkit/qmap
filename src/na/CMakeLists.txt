# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

add_subdirectory(computation)
# The QDMI device configuration must be available before the client layer that builds on it.
add_subdirectory(qdmi)
add_subdirectory(fomac)
add_subdirectory(nalac)
if(Z3_FOUND)
  add_subdirectory(nasp)
endif()
add_subdirectory(zoned)
