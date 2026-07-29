# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""Pytest configuration for the mqt.qmap.ph test suite.

Stubs the mqt and mqt.qmap parent packages so the photonics subpackage is
importable without building the C extensions.
"""

from __future__ import annotations

import sys
from pathlib import Path
from types import ModuleType

_PYTHON_SRC = Path(__file__).parents[3] / "python"

for _pkg, _path in [
    ("mqt", _PYTHON_SRC / "mqt"),
    ("mqt.qmap", _PYTHON_SRC / "mqt" / "qmap"),
]:
    if _pkg not in sys.modules:
        _mod = ModuleType(_pkg)
        _mod.__path__ = [str(_path)]
        _mod.__package__ = _pkg
        sys.modules[_pkg] = _mod

sys.path.insert(0, str(_PYTHON_SRC))

import pytest

from mqt.qmap.ph.graph import generate_beam_splitter_matrix


@pytest.fixture
def ideal_bs_chip4():
    """Return ideal 50/50 beam-splitter reflectivities for a 4-mode chip."""
    return generate_beam_splitter_matrix(chip_size=4, ideal_bs=True).tolist()


@pytest.fixture
def nonideal_bs_chip4():
    """Return a hand-crafted non-ideal BS list for a 4-mode chip.

    Layout: ``[in0, out0, in1, out1, ...]`` - 6 MZIs x 2 values = 12 entries.
    Values are chosen so that each MZI pair is unique, making wrong pairings
    detectable in tests.
    """
    return [0.40, 0.60, 0.30, 0.70, 0.45, 0.55, 0.35, 0.65, 0.48, 0.52, 0.42, 0.58]


@pytest.fixture
def ones_transmissions_chip4():
    """Return all-ones transmission list for a 4-mode chip."""
    return [1.0, 1.0, 1.0, 1.0]


@pytest.fixture
def haar_unitary_dim2():
    """Return a Haar-random 2x2 unitary with a fixed seed."""
    torch = pytest.importorskip("torch")
    from mqt.qmap.ph.unitary_to_phase_compilation import (  # ruff:ignore[import-outside-top-level]
        get_haar_random_unitary,
    )

    rng = torch.Generator().manual_seed(7)
    return get_haar_random_unitary(2, rng, dtype=torch.complex128)
