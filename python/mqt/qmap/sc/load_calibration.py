# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
# Copyright (c) 2025 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""Load a calibration for a super-conducting architecture."""

from __future__ import annotations

from typing import TYPE_CHECKING

from qiskit.transpiler.target import Target

if TYPE_CHECKING:
    from mqt.qmap.sc.sc import Architecture


def load_calibration(architecture: Architecture, calibration: str | Target | None = None) -> None:
    """Load a super-conducting calibration from a string, BackendProperties, or Target.

    Args:
        architecture: The architecture to load the calibration into.
        calibration: The calibration to load.
    """
    if calibration is None:
        return

    if isinstance(calibration, str):
        architecture.load_properties(calibration)
    elif isinstance(calibration, Target):
        from mqt.qmap.plugins.qiskit import import_target

        architecture.load_properties(import_target(calibration))
    else:  # pragma: no cover
        msg = f"Calibration type {type(calibration)} not supported."
        raise TypeError(msg)
