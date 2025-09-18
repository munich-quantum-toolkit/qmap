"""MQT QMAP library.

This file is part of the MQT QMAP library released under the MIT license.
See README.md or go to https://github.com/cda-tum/qmap for more information.
"""

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
from .compile import compile  # noqa: A004
from .subarchitectures import SubarchitectureOrder

__all__ = [
    "SubarchitectureOrder",
    "__version__",
    "compile",
    "optimize_clifford",
    "synthesize_clifford",
]
