# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""Pytest configuration for the eval/ph evaluation tests.

The evaluation code in ``eval/ph`` is paper-reproduction code, not part of the
installable ``mqt.qmap`` package. Add ``eval/ph`` to ``sys.path`` so these tests
can import the evaluation modules (``hardware_model``, ...) by bare name.
"""

from __future__ import annotations

import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).parents[1]))
