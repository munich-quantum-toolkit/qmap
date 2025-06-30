# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
# Copyright (c) 2025 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""MQT QMAP library."""

from __future__ import annotations

import sys

# under Windows, make sure to add the appropriate DLL directory to the PATH
if sys.platform == "win32":

    def _dll_patch() -> None:
        """Add the DLL directory to the PATH."""
        import os
        import sysconfig
        from pathlib import Path

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


from ._version import version as __version__
from .clifford_synthesis import optimize_clifford, synthesize_clifford
from .sc import (
    Architecture,
    SubarchitectureOrder,
    compile,  # noqa: A004
)

__all__ = [
    "Architecture",
    "SubarchitectureOrder",
    "__version__",
    "compile",
    "optimize_clifford",
    "synthesize_clifford",
]
