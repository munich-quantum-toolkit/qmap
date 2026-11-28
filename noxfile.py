#!/usr/bin/env -S uv run --script --quiet
# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

# /// script
# dependencies = [
#   "nox>=2026.10.20",
#   "mqt-nox @ git+https://github.com/munich-quantum-toolkit/nox.git@23dc034f43cb0f2e75c09b4796f33a0cf7158d14",
# ]
# ///

"""Nox sessions."""

from __future__ import annotations

import nox
from mqt.nox import python, utils

nox.needs_version = ">=2026.10.20"

PYTHON_ALL_VERSIONS = ["3.11", "3.12", "3.13", "3.14", "3.15"]


@nox.session(reuse_venv=True, default=True)
def lint(session: nox.Session) -> None:
    """Run the linter."""
    python.lint(session)


@nox.session(python=PYTHON_ALL_VERSIONS, reuse_venv=True, default=True)
def tests(session: nox.Session) -> None:
    """Run the test suite."""
    python.tests(session, build_isolation=False)


@nox.session(python="3.14", reuse_venv=True, default=False)
def evaluation(session: nox.Session) -> None:
    """Run the photonic paper-evaluation tests in eval/ph/tests.

    Installs the ``evaluation`` dependency group (Perceval, pandas, torch, numpy) and runs the evaluation tests.
    Deliberately not a default session: the Perceval-based evaluation is paper-reproduction work, not the shipped
    compiler's contract, so it runs on demand (``nox -s evaluation``) rather than in the default CI matrix.
    """
    utils.install(session, build_isolation=False, groups=["test", "evaluation"])
    utils.run_pytest(session, pytest_args=["eval/ph/tests"])


@nox.session(python=PYTHON_ALL_VERSIONS, reuse_venv=True, default=True)
def minimums(session: nox.Session) -> None:
    """Test the minimum versions of dependencies."""
    python.minimums(session, build_isolation=False)


@nox.session(reuse_venv=True, python=PYTHON_ALL_VERSIONS)
def qiskit(session: nox.Session) -> None:
    """Test against Qiskit's main branch."""
    python.qiskit(session, build_isolation=False)


@nox.session(python="3.14", reuse_venv=True)
def docs(session: nox.Session) -> None:
    """Build the docs.

    Use "--non-interactive" to avoid serving. Pass "-b linkcheck" to check links.
    """
    python.docs(session, build_isolation=False)


@nox.session(reuse_venv=True)
def stubs(session: nox.Session) -> None:
    """Generate type stubs for Python bindings using nanobind."""
    python.stubs(
        session,
        modules=["mqt.qmap.na", "mqt.qmap.clifford_synthesis", "mqt.qmap.hybrid_mapper", "mqt.qmap.sc"],
    )


if __name__ == "__main__":
    nox.main()
