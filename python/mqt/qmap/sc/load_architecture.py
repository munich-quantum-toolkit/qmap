# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
# Copyright (c) 2025 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""Load a super-conducting architecture."""

from __future__ import annotations

from qiskit.providers import Backend

from mqt.qmap.sc.sc import Arch, Architecture


def load_architecture(arch: str | Arch | Architecture | Backend | None = None) -> Architecture:
    """Load a super-conducting architecture from a string, Arch, Architecture, or Backend.

    If None is passed, no architecture is loaded.

    Args:
        arch: The architecture to load.

    Returns:
        The loaded architecture.
    """
    architecture = Architecture()

    if arch is not None:
        if isinstance(arch, str):
            try:
                architecture.load_coupling_map(Arch(arch))
            except ValueError:
                architecture.load_coupling_map(arch)
        elif isinstance(arch, Arch):
            architecture.load_coupling_map(arch)
        elif isinstance(arch, Architecture):
            architecture = arch
        elif isinstance(arch, Backend):
            from mqt.qmap.plugins.qiskit import import_backend

            architecture = import_backend(arch)
        else:  # pragma: no cover
            msg = f"Architecture type {type(arch)} not supported."
            raise TypeError(msg)

    return architecture
