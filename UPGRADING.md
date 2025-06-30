# Upgrade Guide

This document describes breaking changes and how to upgrade. For a complete list of changes including minor and patch releases, please refer to the [changelog](CHANGELOG.md).

## [Unreleased]

With this release, the Python package has been restructured.
In particular, the `mqt.qmap.pyqmap` module has been discontinued.
Classes and functions can now be imported from the more descriptive `mqt.qmap.clifford_synthesis`, `mqt.qmap.hybrid_mapper`, `mqt.qmap.na`, and `mqt.qmap.sc` modules.

## [3.1.0]

This minor release initiates the efforts to re-structure the Python bindings and make them more modular.
Even tough this is not a breaking change, it is worth mentioning to developers of MQT QMAP that all Python code (except tests) has been moved to the top-level `python` directory.
Furthermore, the C++ code for the Python bindings has been moved to the top-level `bindings` directory.

## [3.0.0]

This major release introduces several breaking changes, including the removal of deprecated features.
The following paragraphs describe the most important changes and how to adapt your code accordingly.
We intend to provide a more comprehensive migration guide for future releases.

The major change in this major release is the move to the MQT Core Python package.
This move allows us to make `qiskit` a fully optional dependency and entirely rely on the MQT Core IR for representing circuits.
Additionally, the `mqt-core` Python package now ships all its C++ libraries as shared libraries so that these need not be fetched or built as part of the build process.
This was tricky to achieve cross-platform, and you can find some more backstory in the corresponding [PR](https://github.com/munich-quantum-toolkit/qmap/pulls/418).
We expect this integration to mature over the next few releases.
If you encounter any issues, please let us know.

Support for `BackendV1` Qiskit backends has been removed in accordance with Qiskit's 2.0 release dropping support for these backends.
If you still require support for these backends, please use the last version of MQT QMAP that supports them, which is `2.8.0`.
However, we strongly recommend that you upgrade to Qiskit 2.0 or higher and use the new `BackendV2` interface.

Teleportation support for the heuristic mapping has been removed.
If you still require this feature, please use the last version of MQT QMAP that supports it, which is `2.8.0`.

MQT Core itself dropped support for several parsers in `v3.0.0`, including the `.real`, `.qc`, `.tfc`, and `GRCS` parsers.
The `.real` parser lives on as part of the [MQT SyReC] project. All others have been removed without replacement.
Consequently, these input formats are no longer supported in MQT QMAP.

MQT QMAP has moved to the [munich-quantum-toolkit](https://github.com/munich-quantum-toolkit) GitHub organization under https://github.com/munich-quantum-toolkit/qmap.
While most links should be automatically redirected, please update any links in your code to point to the new location.
All links in the documentation have been updated accordingly.

MQT QMAP now requires CMake 3.24 or higher.
Most modern operating systems should have this version available in their package manager.
Alternatively, CMake can be conveniently installed from PyPI using the [`cmake`](https://pypi.org/project/cmake/) package.

[MQT SyReC]: https://github.com/cda-tum/mqt-syrec
[unreleased]: https://github.com/munich-quantum-toolkit/qmap/compare/v3.1.0...HEAD
[3.1.0]: https://github.com/munich-quantum-toolkit/qmap/compare/v3.0.0...v3.1.0
[3.0.0]: https://github.com/munich-quantum-toolkit/qmap/compare/v2.8.0...v3.0.0
