<!-- Entries in each category are sorted by merge time, with the latest PRs appearing first. -->

# Changelog

All notable changes to this project will be documented in this file.

The format is based on a mixture of [Keep a Changelog] and [Common Changelog].
This project adheres to [Semantic Versioning], with the exception that minor releases may include breaking changes.

## [Unreleased]

### Removed

- 🔥 Drop support for Python 3.9 ([#767]) ([**@denialhaag**])
- 🔥 Stop testing on and shipping wheels for x86 macOS systems ([#760]) ([**@denialhaag**])

## [3.3.1] - 2025-08-07

### Fixed

- 🐛 Fix lookup of discrete columns in routing-aware compiler ([#728]) ([**@ystade**])

## [3.3.0] - 2025-08-04

_If you are upgrading: please see [`UPGRADING.md`](UPGRADING.md#330)._

### Added

- 🐍 Build Python 3.14 wheels ([#717]) ([**@denialhaag**])

### Changed

- **Breaking**: ⬆️ Bump minimum required `mqt-core` version to `3.2.1` ([#720]) ([**@denialhaag**])
- **Breaking**: ⬆️ Require C++20 ([#720]) ([**@denialhaag**])
- **Breaking**: ♻️ Neutral Atom Compiler: Merge Placement and Routing stage into a Layout Synthesis stage ([#713]) ([**@ystade**])
- **Breaking**: ✨ Expose enums to Python via `pybind11`'s new (`enum.Enum`-compatible) `py::native_enum` ([#715]) ([**@denialhaag**])

### Fixed

- 🚸 Make function to export architecture in `.namachine` format available from Python ([#719]) ([**@ystade**])
- 🚸 Increase binary compatibility between `mqt-qmap`, `mqt-core`, and `mqt-qcec` ([#714]) ([**@denialhaag**])

## [3.2.0] - 2025-07-16

_If you are upgrading: please see [`UPGRADING.md`](UPGRADING.md#320)._

### Changed

- **Breaking**: ♻️ Restructure the Python code to introduce modules ([#665]) ([**@denialhaag**])
- ♻️ Restructure the C++ code for the Python bindings to mirror the introduced Python modules ([#665]) ([**@denialhaag**])
- **Breaking**: ⬆️ Bump minimum required `mqt-core` version to `3.1.0` ([#694]) ([**@denialhaag**])
- **Breaking**: ⬆️ Bump minimum required `pybind11` version to `3.0.0` ([#694]) ([**@denialhaag**])

### Fixed

- 🐛 Fix out-of-bounds access to the vector of reuse qubits ([#712]) ([**@ystade**])

### Removed

- 🔥 Remove CMake function `add_mqt_qmap_binding` ([#665]) ([**@denialhaag**])

## [3.1.3] - 2025-05-28

### Fixed

- 🐛 Fix the CD pipeline by changing the statistics struct in the zoned neutral atom compiler ([#661]) ([**@ystade**])

## [3.1.2] - 2025-05-27

### Fixed

- 🐛 Entirely deactivate PDF export in documentation build ([#660]) ([**@ystade**])
- 📝 Append the docstring for the `__init__` method to the class docstring in the documentation ([#660]) ([**@ystade**])

## [3.1.1] - 2025-05-27

### Fixed

- 🐛 Deactivate PDF export in the documentation build ([#659]) ([**@ystade**])

## [3.1.0] - 2025-05-26

### Added

- ✨ Add new compilers for zoned neutral atom architectures (a routing-agnostic and routing-aware compiler) ([#624]) ([**@ystade**])
- ✨ Add a new CMake function `add_mqt_qmap_binding` to add a Python binding library ([#624]) ([**@ystade**])

### Changed

- ♻️ Move the C++ code for the Python bindings to the top-level `bindings` directory ([#624]) ([**@ystade**])
- ♻️ Move all Python code (no tests) to the top-level `python` directory ([#624]) ([**@ystade**])
- ♻️ Restructure the Python bindings for neutral atom tools into separate modules ([#624]) ([**@ystade**])

## [3.0.0] - 2025-05-08

_If you are upgrading: please see [`UPGRADING.md`](UPGRADING.md#300)._

### Added

- ✨ Support Qiskit 2.0+ ([#610]) ([**@burgholzer**])

### Changed

- **Breaking**: 🚚 Move MQT QMAP to the [munich-quantum-toolkit] GitHub organization
- **Breaking**: ♻️ Use the `mqt-core` Python package for handling circuits ([#418]) ([**@burgholzer**])
- **Breaking**: ⬆️ Bump minimum required CMake version to `3.24.0` ([#621]) ([**@burgholzer**])
- **Breaking**: ♻️ Adopt new `NAComputation` in NASP tool ([#608]) ([**@ystade**])
- **Breaking**: ♻️ Isolate NALAC from the main library ([#608], [#609]) ([**@ystade**])
- 📝 Rework existing project documentation ([#614]) ([**@burgholzer**])

### Removed

- **Breaking**: 🔥 Remove teleportation support for the heuristic mapping ([#621]) ([**@burgholzer**])
- **Breaking**: 🔥 Remove support for `BackendV1` Qiskit backends ([#610]) ([**@burgholzer**])
- **Breaking**: 🔥 Remove support for `.real`, `.qc`, `.tfc`, and `GRCS` files ([#621]) ([**@burgholzer**])
- **Breaking**: 🔥 Remove `yaml-cpp` dependency ([#608]) ([**@ystade**])

## [2.8.0] - 2024-11-18

_📚 Refer to the [GitHub Release Notes] for previous changelogs._

<!-- Version links -->

[unreleased]: https://github.com/munich-quantum-toolkit/qmap/compare/v3.3.1...HEAD
[3.3.1]: https://github.com/munich-quantum-toolkit/qmap/releases/tag/v3.3.1
[3.3.0]: https://github.com/munich-quantum-toolkit/qmap/releases/tag/v3.3.0
[3.2.0]: https://github.com/munich-quantum-toolkit/qmap/releases/tag/v3.2.0
[3.1.3]: https://github.com/munich-quantum-toolkit/qmap/releases/tag/v3.1.3
[3.1.2]: https://github.com/munich-quantum-toolkit/qmap/releases/tag/v3.1.2
[3.1.1]: https://github.com/munich-quantum-toolkit/qmap/releases/tag/v3.1.1
[3.1.0]: https://github.com/munich-quantum-toolkit/qmap/releases/tag/v3.1.0
[3.0.0]: https://github.com/munich-quantum-toolkit/qmap/releases/tag/v3.0.0
[2.8.0]: https://github.com/munich-quantum-toolkit/qmap/releases/tag/v2.8.0

<!-- PR links -->

[#767]: https://github.com/munich-quantum-toolkit/qmap/pull/767
[#760]: https://github.com/munich-quantum-toolkit/qmap/pull/760
[#728]: https://github.com/munich-quantum-toolkit/qmap/pull/728
[#720]: https://github.com/munich-quantum-toolkit/qmap/pull/720
[#719]: https://github.com/munich-quantum-toolkit/qmap/pull/719
[#717]: https://github.com/munich-quantum-toolkit/qmap/pull/717
[#715]: https://github.com/munich-quantum-toolkit/qmap/pull/715
[#714]: https://github.com/munich-quantum-toolkit/qmap/pull/714
[#713]: https://github.com/munich-quantum-toolkit/qmap/pull/713
[#712]: https://github.com/munich-quantum-toolkit/qmap/pull/712
[#694]: https://github.com/munich-quantum-toolkit/qmap/pull/694
[#665]: https://github.com/munich-quantum-toolkit/qmap/pull/665
[#661]: https://github.com/munich-quantum-toolkit/qmap/pull/661
[#660]: https://github.com/munich-quantum-toolkit/qmap/pull/660
[#659]: https://github.com/munich-quantum-toolkit/qmap/pull/659
[#624]: https://github.com/munich-quantum-toolkit/qmap/pull/624
[#621]: https://github.com/munich-quantum-toolkit/qmap/pull/621
[#614]: https://github.com/munich-quantum-toolkit/qmap/pull/614
[#610]: https://github.com/munich-quantum-toolkit/qmap/pull/610
[#609]: https://github.com/munich-quantum-toolkit/qmap/pull/609
[#608]: https://github.com/munich-quantum-toolkit/qmap/pull/608
[#418]: https://github.com/munich-quantum-toolkit/qmap/pull/418

<!-- Contributor -->

[**@burgholzer**]: https://github.com/burgholzer
[**@ystade**]: https://github.com/ystade
[**@denialhaag**]: https://github.com/denialhaag

<!-- General links -->

[Keep a Changelog]: https://keepachangelog.com/en/1.1.0/
[Common Changelog]: https://common-changelog.org
[Semantic Versioning]: https://semver.org/spec/v2.0.0.html
[GitHub Release Notes]: https://github.com/munich-quantum-toolkit/qmap/releases
[munich-quantum-toolkit]: https://github.com/munich-quantum-toolkit
