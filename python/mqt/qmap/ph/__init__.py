# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""MQT QMAP photonics subcircuit compiler.

Compiles a target unitary onto a physical MZI-mesh chip by finding the
optimal photon routing and optimising phase-shifter parameters via gradient
descent.

Typical usage::

    from mqt.qmap.ph.subcircuit_compilation import OptimizationConfig, compile_subcircuit

Optional dependencies: ``perceval-quandela``, ``pandas`` (install via
``pip install mqt.qmap[photonics]``) and ``torch`` (install separately
following https://pytorch.org/get-started/locally/ to select the right
CPU/CUDA variant).
"""

from __future__ import annotations

__all__: list[str] = []
