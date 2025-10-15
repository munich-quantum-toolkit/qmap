# Upgrade Guide

This document describes breaking changes and how to upgrade. For a complete list of changes including minor and patch releases, please refer to the [changelog](CHANGELOG.md).

## [Unreleased]

## [3.4.0]

### End of support for Python 3.9

Starting with this release, MQT QMAP no longer supports Python 3.9.
This is in line with the scheduled end of life of the version.
As a result, MQT QMAP is no longer tested under Python 3.9 and no longer ships Python 3.9 wheels.

## [3.3.0]

Testing previous versions of the `mqt-qmap` package built via `uv sync` or simple `(uv) pip install .` generally failed due to binary incompatibility of the `mqt-core` compiled extension packages and the `mqt-qmap` one.
This required building `mqt-core` from source and without build isolation to get a working local setup.
By using the latest `pybind11` release (`v3`), the binary compatibility between extension modules compiled under different circumstances (such as different compilers) has been greatly increased.
As such, it is no longer necessary to build `mqt-core` (and `mqt-qcec` for testing) from source and without build isolation when locally working on `mqt-qmap`.
A simple `uv sync` is enough to successfully run `pytest`.

All Python enums (e.g., `sc.Method`) are now exposed via `pybind11`'s new `py::native_enum`, which makes them compatible with Python's `enum.Enum` class (PEP 435).
As a result, the enums can no longer be initialized using a string.
Instead of `Method("exact")` or `"exact"`, use `Method.exact`.

This release restructures the neutral atom compiler which has consequences for its configuration and the reporting of statistics.
The placement and routing stages have been merged into a single layout synthesis stage.
There is a new `PlaceAndRouteSynthesizer` that combines the previously separate placement and routing stages.
Consequently, the configuration for the placement and routing stages must now be wrapped in a configuration for the layout synthesis stage when using the C++ API.
The Python API did not change in this regard.
Furthermore, when reporting the statistics of the neutral atom compiler, the statistics for placement and routing are now reported as part of the layout synthesis statistics.
The latter affects both the C++ and Python APIs.

Finally, the minimum required C++ version has been raised from C++17 to C++20.
The default compilers of our test systems support all relevant features of the standard.

## [3.2.0]

With this release, the Python package has been restructured.
In particular, the `mqt.qmap.pyqmap` module has been discontinued.
Classes and functions can now be imported from the more descriptive `mqt.qmap.clifford_synthesis`, `mqt.qmap.hybrid_mapper`, `mqt.qmap.na`, and `mqt.qmap.sc` modules.
The superconducting module's `compile()` function has been moved to `mqt.qmap.plugins.qiskit.sc`.
The entrypoints `synthesize_clifford()` and `optimize_clifford()` of the Clifford synthesis module have been moved to `mqt.qmap.plugins.qiskit.clifford_synthesis`.

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

<!-- Version links -->

[unreleased]: https://github.com/munich-quantum-toolkit/qmap/compare/v3.4.0...HEAD
[3.4.0]: https://github.com/munich-quantum-toolkit/qmap/compare/v3.3.0...v3.4.0
[3.3.0]: https://github.com/munich-quantum-toolkit/qmap/compare/v3.2.0...v3.3.0
[3.2.0]: https://github.com/munich-quantum-toolkit/qmap/compare/v3.1.0...v3.2.0
[3.1.0]: https://github.com/munich-quantum-toolkit/qmap/compare/v3.0.0...v3.1.0
[3.0.0]: https://github.com/munich-quantum-toolkit/qmap/compare/v2.8.0...v3.0.0

<!-- Other links -->

[MQT SyReC]: https://github.com/munich-quantum-toolkit/syrec
