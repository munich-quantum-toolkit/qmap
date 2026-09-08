# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""Resolved install paths for the packaged QDMI device libraries."""

from __future__ import annotations

import sys
from importlib.metadata import distribution
from pathlib import Path

__all__ = ["NA_QDMI_DEVICE_ID", "NA_QDMI_DEVICE_LIBRARY_PATH", "NA_QDMI_PREFIX"]

NA_QDMI_DEVICE_ID = "mqt.qmap.na.default"
"""The identifier under which MQT QMAP registers its neutral-atom QDMI device."""

NA_QDMI_PREFIX = "MQT_QMAP_NA"
"""The symbol prefix of the neutral-atom QDMI device library."""


def __dir__() -> list[str]:
    return __all__


def _library_name() -> str:
    """Return the file name of the packaged neutral-atom device library.

    Returns:
        The platform-specific file name.
    """
    if sys.platform == "win32":
        return "mqt-qmap-na-qdmi-device.dll"
    if sys.platform == "darwin":
        return "libmqt-qmap-na-qdmi-device.dylib"
    return "libmqt-qmap-na-qdmi-device.so"


def _resolve_library_path() -> str:
    """Return the path of the packaged neutral-atom device library.

    The path is resolved through the distribution rather than through this file. An editable
    install serves the Python modules from the source tree, while the built artifacts live
    beside the installed package.

    Returns:
        The path of the library, or an empty string if it is not installed.
    """
    located = distribution("mqt-qmap").locate_file("mqt/qmap")
    package_root = Path(str(located))
    directories = [package_root / "bin"] if sys.platform == "win32" else [package_root / "lib", package_root / "lib64"]
    name = _library_name()
    for directory in directories:
        candidate = directory / name
        if candidate.is_file():
            return str(candidate)
    return ""


NA_QDMI_DEVICE_LIBRARY_PATH = _resolve_library_path()
"""The path of the packaged neutral-atom device library, empty if it is not installed."""
