# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
# Copyright (c) 2025 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

add_subdirectory(nalac)
if(Z3_FOUND)
  add_subdirectory(nasp)
endif()
add_subdirectory(zoned)
