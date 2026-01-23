/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include <nanobind/nanobind.h>

namespace nb = nanobind;

// forward declarations
void registerStatePreparation(nb::module_& m);
void registerZoned(nb::module_& m);

NB_MODULE(MQT_QMAP_MODULE_NAME, m) {
  auto statePreparation = m.def_submodule("state_preparation");
  registerStatePreparation(statePreparation);

  auto zoned = m.def_submodule("zoned");
  registerZoned(zoned);
}
