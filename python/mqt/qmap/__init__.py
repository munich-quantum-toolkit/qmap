# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""MQT QMAP library."""

from __future__ import annotations

import sys
from importlib.metadata import version

# On Windows, add MQT Core's DLL directory to the search path.
if sys.platform == "win32":  # ruff:ignore[non-empty-init-module] This is actually required on Windows
    import os
    import sysconfig
    from pathlib import Path

    def _dll_patch() -> None:
        site_packages = Path(sysconfig.get_paths()["purelib"])
        bin_dir = site_packages / "mqt" / "core" / "bin"
        os.add_dll_directory(str(bin_dir))

        if "Z3_ROOT" in os.environ:  # pragma: no cover
            lib_path = Path(os.environ["Z3_ROOT"]) / "lib"
            if lib_path.exists():
                os.add_dll_directory(str(lib_path))
            bin_path = Path(os.environ["Z3_ROOT"]) / "bin"
            if bin_path.exists():
                os.add_dll_directory(str(bin_path))

        z3_dir = site_packages / "z3"
        if z3_dir.exists():  # pragma: no cover
            lib_path = z3_dir / "lib"
            if lib_path.exists():
                os.add_dll_directory(str(lib_path))
            bin_path = z3_dir / "bin"
            if bin_path.exists():
                os.add_dll_directory(str(bin_path))

    _dll_patch()
    del _dll_patch


__version__ = version("mqt-qmap")

__all__ = [
    "__version__",
]
