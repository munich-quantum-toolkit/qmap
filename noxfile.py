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
#   "mqt-nox @ git+https://github.com/munich-quantum-toolkit/nox.git@fb97d4b296be234feb9a373df60874012cf90ece",
# ]
# ///

"""Nox sessions."""

from __future__ import annotations

import os
from pathlib import Path

import nox
from mqt.nox import common, cpp

nox.needs_version = ">=2026.10.20"
nox.options.default_venv_backend = "uv"

PYTHON_ALL_VERSIONS = ["3.11", "3.12", "3.13", "3.14"]

if os.environ.get("CI", None):
    nox.options.error_on_missing_interpreters = True


@nox.session(reuse_venv=True, default=True)
def lint(session: nox.Session) -> None:
    """Run the linter."""
    common.lint(session)


@nox.session(python=PYTHON_ALL_VERSIONS, reuse_venv=True, default=True)
def tests(session: nox.Session) -> None:
    """Run the test suite."""
    cpp.tests(session, package="mqt-qmap")


@nox.session(reuse_venv=True, venv_backend="uv", default=False)
def evaluation(session: nox.Session) -> None:
    """Run the photonic paper-evaluation tests in eval/ph/tests.

    Installs the ``evaluation`` dependency group (Perceval, pandas, torch, numpy)
    and runs the evaluation tests. Deliberately not a default session: the
    Perceval-based evaluation is paper-reproduction work, not the shipped
    compiler's contract, so it runs on demand (``nox -s evaluation``) rather than
    in the default CI matrix.
    """
    cpp.install(session, package="mqt-qmap", groups=("test", "evaluation"))
    common.run_pytest(session, pytest_args=("eval/ph/tests",))


@nox.session(python=PYTHON_ALL_VERSIONS, reuse_venv=True, venv_backend="uv", default=True)
def minimums(session: nox.Session) -> None:
    """Test the minimum versions of dependencies."""
    cpp.minimums(session, package="mqt-qmap")


@nox.session(reuse_venv=True, venv_backend="uv", python=PYTHON_ALL_VERSIONS)
def qiskit(session: nox.Session) -> None:
    """Tests against the latest version of Qiskit."""
    cpp.tests(
        session,
        package="mqt-qmap",
        extra_deps=("qiskit[qasm3-import] @ git+https://github.com/Qiskit/qiskit.git",),
    )


@nox.session(python="3.14", reuse_venv=True)
def docs(session: nox.Session) -> None:
    """Build the docs. Use "--non-interactive" to avoid serving. Pass "-b linkcheck" to check links."""
    cpp.docs(session, package="mqt-qmap")


@nox.session(reuse_venv=True, venv_backend="uv")
def stubs(session: nox.Session) -> None:
    """Generate type stubs for Python bindings using nanobind."""
    cpp.stubs(
        session,
        package="mqt-qmap",
        modules=("mqt.qmap.na", "mqt.qmap.clifford_synthesis", "mqt.qmap.hybrid_mapper", "mqt.qmap.sc"),
        output_dir=Path(__file__).parent / "python" / "mqt" / "qmap",
    )


if __name__ == "__main__":
    nox.main()
