# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
# Copyright (c) 2025 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

from __future__ import annotations

from importlib.resources import as_file, files

__all__ = ["as_file", "files"]


def __dir__() -> list[str]:
    return __all__
