/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include <iostream>
#include <z3.h> // NOLINT(misc-include-cleaner)

int main() {
  std::cout << Z3_get_full_version(); // NOLINT(misc-include-cleaner)
  return 0;
}
