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

import pytest  # noqa: E402


@pytest.fixture
def ideal_bs_chip4():
    from mqt.qmap.ph.graph import generate_beam_splitter_matrix

    return generate_beam_splitter_matrix(chip_size=4, ideal_bs=True)


@pytest.fixture
def ones_transmissions_chip4():
    import numpy as np

    return np.ones(4)


@pytest.fixture
def haar_unitary_dim2():
    import torch

    from mqt.qmap.ph.unitary_to_phase_compilation import get_haar_random_unitary

    rng = torch.Generator().manual_seed(7)
    return get_haar_random_unitary(2, rng, dtype=torch.complex128)
