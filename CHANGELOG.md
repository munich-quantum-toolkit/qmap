<!-- Entries in each category are sorted by merge time, with the latest PRs appearing first. -->

# Changelog

All notable changes to this project will be documented in this file.

The format is based on a mixture of [Keep a Changelog] and [Common Changelog].
This project adheres to [Semantic Versioning], with the exception that minor releases may include breaking changes.

## [Unreleased]

_If you are upgrading: please see [`UPGRADING.md`](UPGRADING.md#unreleased)._

## [3.6.0] - 2026-02-24

_If you are upgrading: please see [`UPGRADING.md`](UPGRADING.md#360)._

### Added

- ✨ Add a (no-op) decomposer component to the zoned neutral atom compiler ([#902]) ([**@ystade**])

### Changed

- ⬆️ Update `spdlog` to version 1.17.0 ([#921]) ([**@denialhaag**])
- ⬆️ Update `mqt-core` to version 3.4.1 ([#924]) ([**@denialhaag**])
- ⬆️ Update `nanobind` to version 2.11.0 ([#924]) ([**@denialhaag**])
- 🔧 Replace `mypy` with `ty` ([#912]) ([**@denialhaag**])
- ♻️ Migrate Python bindings from `pybind11` to `nanobind` ([#911], [#916]) ([**@denialhaag**])
- 📦️ Provide Stable ABI wheels for Python 3.12+ ([#911]) ([**@denialhaag**])

## [3.5.0] - 2025-12-16

_If you are upgrading: please see [`UPGRADING.md`](UPGRADING.md#350)._

### Added

- ✨ Add iterative diving search as a more efficient placement heuristic method ([#862]) ([**@ystade**])
- ✨ Add relaxed routing method to the zoned neutral atom compiler ([#859]) ([**@ystade**])

### Changed

- 👷 Stop testing on `ubuntu-22.04` and `ubuntu-22.04-arm` runners ([#874]) ([**@denialhaag**])
- 👷 Stop testing with `clang-19` and start testing with `clang-21` ([#874]) ([**@denialhaag**])
- 👷 Fix macOS tests with Homebrew Clang via new `munich-quantum-toolkit/workflows` version ([#874]) ([**@denialhaag**])
- 👷 Re-enable macOS tests with GCC by disabling module scanning ([#874]) ([**@denialhaag**])
- ✨ Enable code generation for relaxed routing constraints ([#848]) ([**@ystade**])
- ✨ Add `max_filling_factor` to scheduler in Zoned Neutral Atom Compiler ([#847]) ([**@ystade**])
- ✨ Added extension to the hybrid routing mapper to also support Bridge gates, Passby moves and Flying ancillas ([#832]) ([**@lsschmid**])
- ✨ Added hybrid synthesis routing for iterative circuit constructions ([#832]) ([**@lsschmid**])

### Removed

- 🔥 Remove wheel builds for Python 3.13t ([#874]) ([**@denialhaag**])

## [3.4.0] - 2025-10-15

_If you are upgrading: please see [`UPGRADING.md`](UPGRADING.md#340)._

### Added

- 👷 Enable testing on Python 3.14 ([#796]) ([**@denialhaag**])

### Changed

- 📝 Improve error message in the NA compiler ([#804]) ([**@ystade**])
- ⬆️ Bump minimum required `mqt-core` version to `3.3.1` ([#803]) ([**@denialhaag**], [**@ystade**])

### Fixed

- 🐛 Fix logging level parameter values and error/warning messages ([#793]) ([**@ystade**])

### Removed

- 🔥 Drop support for Python 3.9 ([#767]) ([**@denialhaag**])

## [3.3.1] - 2025-08-07

### Fixed

- 🐛 Fix lookup of discrete columns in routing-aware compiler ([#728]) ([**@ystade**])

## [3.3.0] - 2025-08-04

_If you are upgrading: please see [`UPGRADING.md`](UPGRADING.md#330)._

### Added

- 🐍 Build Python 3.14 wheels ([#717]) ([**@denialhaag**])

### Changed

- ⬆️ Bump minimum required `mqt-core` version to `3.2.1` ([#720]) ([**@denialhaag**])
- ⬆️ Require C++20 ([#720]) ([**@denialhaag**])
- ♻️ Neutral Atom Compiler: Merge Placement and Routing stage into a Layout Synthesis stage ([#713]) ([**@ystade**])
- ✨ Expose enums to Python via `pybind11`'s new (`enum.Enum`-compatible) `py::native_enum` ([#715]) ([**@denialhaag**])

### Fixed

- 🚸 Make function to export architecture in `.namachine` format available from Python ([#719]) ([**@ystade**])
- 🚸 Increase binary compatibility between `mqt-qmap`, `mqt-core`, and `mqt-qcec` ([#714]) ([**@denialhaag**])

## [3.2.0] - 2025-07-16

_If you are upgrading: please see [`UPGRADING.md`](UPGRADING.md#320)._

### Changed

- ♻️ Restructure the Python code to introduce modules ([#665]) ([**@denialhaag**])
- ♻️ Restructure the C++ code for the Python bindings to mirror the introduced Python modules ([#665]) ([**@denialhaag**])
- ⬆️ Bump minimum required `mqt-core` version to `3.1.0` ([#694]) ([**@denialhaag**])
- ⬆️ Bump minimum required `pybind11` version to `3.0.0` ([#694]) ([**@denialhaag**])

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

- 🚚 Move MQT QMAP to the [munich-quantum-toolkit] GitHub organization
- ♻️ Use the `mqt-core` Python package for handling circuits ([#418]) ([**@burgholzer**])
- ⬆️ Bump minimum required CMake version to `3.24.0` ([#621]) ([**@burgholzer**])
- ♻️ Adopt new `NAComputation` in NASP tool ([#608]) ([**@ystade**])
- ♻️ Isolate NALAC from the main library ([#608], [#609]) ([**@ystade**])
- 📝 Rework existing project documentation ([#614]) ([**@burgholzer**])

### Removed

- 🔥 Remove teleportation support for the heuristic mapping ([#621]) ([**@burgholzer**])
- 🔥 Remove support for `BackendV1` Qiskit backends ([#610]) ([**@burgholzer**])
- 🔥 Remove support for `.real`, `.qc`, `.tfc`, and `GRCS` files ([#621]) ([**@burgholzer**])
- 🔥 Remove `yaml-cpp` dependency ([#608]) ([**@ystade**])

## [2.8.0] - 2024-11-18

_📚 Refer to the [GitHub Release Notes] for previous changelogs._

<!-- Version links -->

[unreleased]: https://github.com/munich-quantum-toolkit/qmap/compare/v3.6.0...HEAD
[3.6.0]: https://github.com/munich-quantum-toolkit/qmap/releases/tag/v3.6.0
[3.5.0]: https://github.com/munich-quantum-toolkit/qmap/releases/tag/v3.5.0
[3.4.0]: https://github.com/munich-quantum-toolkit/qmap/releases/tag/v3.4.0
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

[#924]: https://github.com/munich-quantum-toolkit/qmap/pull/924
[#921]: https://github.com/munich-quantum-toolkit/qmap/pull/921
[#916]: https://github.com/munich-quantum-toolkit/qmap/pull/916
[#912]: https://github.com/munich-quantum-toolkit/qmap/pull/912
[#911]: https://github.com/munich-quantum-toolkit/qmap/pull/911
[#902]: https://github.com/munich-quantum-toolkit/qmap/pull/902
[#874]: https://github.com/munich-quantum-toolkit/qmap/pull/874
[#862]: https://github.com/munich-quantum-toolkit/qmap/pull/862
[#859]: https://github.com/munich-quantum-toolkit/qmap/pull/859
[#848]: https://github.com/munich-quantum-toolkit/qmap/pull/848
[#847]: https://github.com/munich-quantum-toolkit/qmap/pull/847
[#832]: https://github.com/munich-quantum-toolkit/qmap/pull/832
[#804]: https://github.com/munich-quantum-toolkit/qmap/pull/804
[#803]: https://github.com/munich-quantum-toolkit/qmap/pull/803
[#796]: https://github.com/munich-quantum-toolkit/qmap/pull/796
[#793]: https://github.com/munich-quantum-toolkit/qmap/pull/793
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
[**@lsschmid**]: https://github.com/lsschmid

<!-- General links -->

[Keep a Changelog]: https://keepachangelog.com/en/1.1.0/
[Common Changelog]: https://common-changelog.org
[Semantic Versioning]: https://semver.org/spec/v2.0.0.html
[GitHub Release Notes]: https://github.com/munich-quantum-toolkit/qmap/releases
[munich-quantum-toolkit]: https://github.com/munich-quantum-toolkit
