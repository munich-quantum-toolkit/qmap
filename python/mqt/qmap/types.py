# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
# Copyright (c) 2025 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""Module for types."""

from __future__ import annotations

from os import PathLike
from typing import Union

from mqt.core.ir import QuantumComputation
from qiskit.circuit import QuantumCircuit

CircuitInputType = Union[QuantumComputation, str, PathLike[str], QuantumCircuit]
