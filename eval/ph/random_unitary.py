# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""Haar-random unitary generation for the photonic evaluation.

The compiler takes a target unitary *as input*; generating random unitaries is
only needed to drive the paper's benchmark, so this helper lives under
``eval/ph`` rather than in the installed ``mqt.qmap.ph`` compiler.
"""

from __future__ import annotations

import torch


def get_haar_random_unitary(
    num_modes: int,
    generator: torch.Generator | None = None,
    dtype: torch.dtype = torch.complex128,
) -> torch.Tensor:
    """Generate a Haar-random unitary matrix.

    Uses the QR decomposition method: draw a complex Gaussian matrix, QR-
    decompose it, and correct the phases of the diagonal of R to ensure
    uniformity over the Haar measure.

    Args:
        num_modes: Dimension of the unitary matrix.
        generator: Optional :class:`torch.Generator` for reproducible results.
        dtype: Complex dtype of the output tensor.

    Returns:
        Complex tensor of shape ``(num_modes, num_modes)`` representing a
        Haar-random unitary.
    """
    z = torch.randn(num_modes, num_modes, generator=generator, dtype=dtype)
    q, r = torch.linalg.qr(z)
    r_diag = torch.diagonal(r)
    lambda_diag = r_diag / torch.abs(r_diag)
    return q * lambda_diag.unsqueeze(0)
