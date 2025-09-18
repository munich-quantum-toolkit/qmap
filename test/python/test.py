# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
# Copyright (c) 2025 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""A simple example of using QMAP."""

from __future__ import annotations

from qiskit import QuantumCircuit
from qiskit.providers.fake_provider import Fake5QV1

from mqt.qmap.plugins.qiskit.sc import compile  # noqa: A004

if __name__ == "__main__":
    qc = QuantumCircuit(3)
    qc.h(0)
    qc.cx(0, 1)
    qc.cx(1, 2)
    qc.measure_all()
    print(qc.draw(fold=-1))

    # compile the circuit
    qc_mapped, results = compile(qc, arch=Fake5QV1())
    print(qc_mapped.draw(fold=-1))
