# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
# Copyright (c) 2025 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

# exact mapper project library
if(Z3_FOUND)
  # add LogicBlocks library
  add_subdirectory(logicblocks)

  add_subdirectory(cliffordsynthesis)
endif()

add_subdirectory(sc)

add_subdirectory(hybridmap)

add_subdirectory(na)
