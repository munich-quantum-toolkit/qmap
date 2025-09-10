[![PyPI](https://img.shields.io/pypi/v/mqt.qmap?logo=pypi&style=flat-square)](https://pypi.org/project/mqt.qmap/)
![OS](https://img.shields.io/badge/os-linux%20%7C%20macos%20%7C%20windows-blue?style=flat-square)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg?style=flat-square)](https://opensource.org/licenses/MIT)
[![CI](https://img.shields.io/github/actions/workflow/status/munich-quantum-toolkit/qmap/ci.yml?branch=main&style=flat-square&logo=github&label=ci)](https://github.com/munich-quantum-toolkit/qmap/actions/workflows/ci.yml)
[![CD](https://img.shields.io/github/actions/workflow/status/munich-quantum-toolkit/qmap/cd.yml?style=flat-square&logo=github&label=cd)](https://github.com/munich-quantum-toolkit/qmap/actions/workflows/cd.yml)
[![Documentation](https://img.shields.io/readthedocs/mqtqmap?logo=readthedocs&style=flat-square)](https://mqt.readthedocs.io/projects/qmap)
[![codecov](https://img.shields.io/codecov/c/github/munich-quantum-toolkit/qmap?style=flat-square&logo=codecov)](https://codecov.io/gh/munich-quantum-toolkit/qmap)

<p align="center">
  <a href="https://mqt.readthedocs.io">
    <picture>
      <source media="(prefers-color-scheme: dark)" srcset="https://raw.githubusercontent.com/munich-quantum-toolkit/.github/refs/heads/main/docs/_static/mqt-banner-dark.svg" width="90%">
      <img src="https://raw.githubusercontent.com/munich-quantum-toolkit/.github/refs/heads/main/docs/_static/mqt-banner-light.svg" width="90%" alt="MQT Banner">
    </picture>
  </a>
</p>

# MQT QMAP - A tool for Quantum Circuit Compilation

A tool for quantum circuit compilation developed as part of the [_Munich Quantum Toolkit (MQT)_](https://mqt.readthedocs.io).
It builds upon [MQT Core](https://github.com/munich-quantum-toolkit/core), which forms the backbone of the MQT.

<p align="center">
  <a href="https://mqt.readthedocs.io/projects/qmap">
  <img width=30% src="https://img.shields.io/badge/documentation-blue?style=for-the-badge&logo=read%20the%20docs" alt="Documentation" />
  </a>
</p>

## Key Features

- Exact and heuristic circuit mapping to superconducting devices: gate-optimal MaxSAT/SMT-based mapping with Z3 for small circuits and scalable A\*-search–based mapping for larger ones. [Guide](https://mqt.readthedocs.io/projects/qmap/en/latest/mapping.html) • [Exact](https://mqt.readthedocs.io/projects/qmap/en/latest/mapping.html#exact-mapping) • [Heuristic](https://mqt.readthedocs.io/projects/qmap/en/latest/mapping.html#heuristic-mapping)
- Clifford circuit synthesis and optimization: SAT-based depth/gate-optimal Clifford synthesis with optional destabilizer preservation, plus a fast heuristic splitter for larger circuits. [Guide](https://mqt.readthedocs.io/projects/qmap/en/latest/synthesis.html)
- Zoned neutral-atom compilers: routing-agnostic and routing-aware flows that place, route, and schedule atom transfers between storage/entanglement zones. [Guide](https://mqt.readthedocs.io/projects/qmap/en/latest/na_zoned_compiler.html)
- Neutral-atom logical state preparation (NASP): SMT-based generator for optimal preparation schedules of logical graph states on zoned architectures. [Guide](https://mqt.readthedocs.io/projects/qmap/en/latest/na_state_prep.html)
- Hybrid circuit mapper for neutral atom quantum computers: a hybrid approach combining superconducting mapping techniques with atom shuttling.
- Python-first API with Qiskit integration: pass `QuantumCircuit` or OpenQASM; one-call `compile()` or `optimize_clifford()` via plugin wrappers. [API](https://mqt.readthedocs.io/projects/qmap/en/latest/api/mqt/qmap/index.html)
- Efficient and portable: C++20 core with Z3-backed solvers, prebuilt wheels for Linux/macOS/Windows via [PyPI](https://pypi.org/project/mqt.qmap/).

If you have any questions, feel free to create a [discussion](https://github.com/munich-quantum-toolkit/qmap/discussions) or an [issue](https://github.com/munich-quantum-toolkit/qmap/issues) on [GitHub](https://github.com/munich-quantum-toolkit/qmap).

## Contributors and Supporters

The _[Munich Quantum Toolkit (MQT)](https://mqt.readthedocs.io)_ is developed by the [Chair for Design Automation](https://www.cda.cit.tum.de/) at the [Technical University of Munich](https://www.tum.de/) and supported by the [Munich Quantum Software Company (MQSC)](https://munichquantum.software).
Among others, it is part of the [Munich Quantum Software Stack (MQSS)](https://www.munich-quantum-valley.de/research/research-areas/mqss) ecosystem, which is being developed as part of the [Munich Quantum Valley (MQV)](https://www.munich-quantum-valley.de) initiative.

<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="https://raw.githubusercontent.com/munich-quantum-toolkit/.github/refs/heads/main/docs/_static/mqt-logo-banner-dark.svg" width="90%">
    <img src="https://raw.githubusercontent.com/munich-quantum-toolkit/.github/refs/heads/main/docs/_static/mqt-logo-banner-light.svg" width="90%" alt="MQT Partner Logos">
  </picture>
</p>

Thank you to all the contributors who have helped make MQT QMAP a reality!

<p align="center">
  <a href="https://github.com/munich-quantum-toolkit/qmap/graphs/contributors">
  <img src="https://contrib.rocks/image?repo=munich-quantum-toolkit/qmap" alt="Contributors to munich-quantum-toolkit/qmap" />
  </a>
</p>

The MQT will remain free, open-source, and permissively licensed—now and in the future.
We are firmly committed to keeping it open and actively maintained for the quantum computing community.

To support this endeavor, please consider:

- Starring and sharing our repositories: https://github.com/munich-quantum-toolkit
- Contributing code, documentation, tests, or examples via issues and pull requests
- Citing the MQT in your publications (see [Cite This](#cite-this))
- Citing our research in your publications (see [References](https://mqt.readthedocs.io/projects/qmap/en/latest/references.html))
- Using the MQT in research and teaching, and sharing feedback and use cases
- Sponsoring us on GitHub: https://github.com/sponsors/munich-quantum-toolkit

<p align="center">
  <a href="https://github.com/sponsors/munich-quantum-toolkit">
  <img width=20% src="https://img.shields.io/badge/Sponsor-white?style=for-the-badge&logo=githubsponsors&labelColor=black&color=blue" alt="Sponsor the MQT" />
  </a>
</p>

## Getting Started

MQT QMAP is available via [PyPI](https://pypi.org/project/mqt.qmap/).

```console
(venv) $ pip install mqt.qmap
```

Compiling a given quantum circuit to a certain device is as easy as

```python3
from mqt.qmap.plugins.qiskit.sc import compile
from qiskit import QuantumCircuit
from qiskit.providers.fake_provider import GenericBackendV2

circ = QuantumCircuit(3)
circ.h(0)
circ.cx(0, 1)
circ.cx(0, 2)

arch = GenericBackendV2(
    num_qubits=5,
    coupling_map=[[0, 1], [1, 0], [1, 2], [2, 1], [1, 3], [3, 1], [3, 4], [4, 3]],
)
circ_mapped, results = compile(circ, arch=arch)
```

Optimizing a Clifford circuit is as easy as

```python3
from mqt.qmap.plugins.qiskit.clifford_synthesis import optimize_clifford
from qiskit import QuantumCircuit

circ = QuantumCircuit(2)
circ.h(1)
circ.cx(0, 1)
circ.h(0)
circ.h(1)

circ_opt, results = optimize_clifford(circ)
```

**Detailed documentation and examples are available at [ReadTheDocs](https://mqt.readthedocs.io/projects/qmap).**

## System Requirements and Building

Building the project requires a C++ compiler with support for C++20 and CMake 3.24 or newer.
For details on how to build the project, please refer to the [documentation](https://mqt.readthedocs.io/projects/qmap).
Building (and running) is continuously tested under Linux, macOS, and Windows using the [latest available system versions for GitHub Actions](https://github.com/actions/runner-images).
MQT QMAP is compatible with all [officially supported Python versions](https://devguide.python.org/versions/).

## Cite This

Please cite the work that best fits your use case.

### MQT QMAP (the tool)

When citing the software itself or results produced with it, cite the MQT QMAP paper:

```bibtex
@inproceedings{wille2023qmap,
  title        = {{{MQT QMAP}}: {{Efficient}} quantum circuit mapping},
  author       = {Wille, Robert and Burgholzer, Lukas},
  year         = 2023,
  booktitle    = {International Symp. on Physical Design},
  doi          = {10.1145/3569052.3578928}
}
```

### The Munich Quantum Toolkit (the project)

When discussing the overall MQT project or its ecosystem, cite the MQT Handbook:

```bibtex
@inproceedings{mqt,
  title        = {The {{MQT}} Handbook: {{A}} Summary of Design Automation Tools and Software for Quantum Computing},
  shorttitle   = {{The MQT Handbook}},
  author       = {Wille, Robert and Berent, Lucas and Forster, Tobias and Kunasaikaran, Jagatheesan and Mato, Kevin and Peham, Tom and Quetschlich, Nils and Rovara, Damian and Sander, Aaron and Schmid, Ludwig and Schoenberger, Daniel and Stade, Yannick and Burgholzer, Lukas},
  year         = 2024,
  booktitle    = {IEEE International Conference on Quantum Software (QSW)},
  doi          = {10.1109/QSW62656.2024.00013},
  eprint       = {2405.17543},
  eprinttype   = {arxiv},
  addendum     = {A live version of this document is available at \url{https://mqt.readthedocs.io}}
}
```

### Peer-Reviewed Research

When citing the underlying methods and research, please reference the most relevant peer-reviewed publications from the list below:

[[1]](https://www.cda.cit.tum.de/files/eda/2023_ispd_mqt_qmap_efficient_quantum_circuit_mapping.pdf)
R. Wille and L. Burgholzer. MQT QMAP: Efficient Quantum Circuit Mapping.
In _International Symposium on Physical Design (ISPD)_, 2023.

[[2]](https://www.cda.cit.tum.de/files/eda/2018_tcad_efficient_mapping_of_quantum_circuits_to_ibm_qx_architectures.pdf)
A. Zulehner, A. Paler, and R. Wille. An Efficient Methodology for Mapping Quantum Circuits to the IBM QX Architectures.
_IEEE Transactions on Computer Aided Design of Integrated Circuits and Systems (TCAD)_, 2018.

[[3]](https://www.cda.cit.tum.de/files/eda/2019_dac_mapping_quantum_circuits_ibm_architectures_using_minimal_number_swap_h_gates.pdf)
R. Wille, L. Burgholzer, and A. Zulehner. Mapping Quantum Circuits to IBM QX Architectures Using the Minimal Number of SWAP and H Operations.
In _Design Automation Conference (DAC)_, 2019.

[[4]](https://www.cda.cit.tum.de/files/eda/2021_aspdac_exploiting_teleportation_in_quantum_circuit_mappping.pdf)
S. Hillmich, A. Zulehner, and R. Wille. Exploiting Quantum Teleportation in Quantum Circuit Mapping.
In _Asia and South Pacific Design Automation Conference (ASP-DAC)_, 2021.

[[5]](https://www.cda.cit.tum.de/files/eda/2022_aspdac_limiting_search_space_optimal_quantum_circuit_mapping.pdf)
L. Burgholzer, S. Schneider, and R. Wille. Limiting the Search Space in Optimal Quantum Circuit Mapping.
In _Asia and South Pacific Design Automation Conference (ASP-DAC)_, 2022.

[[6]](https://arxiv.org/pdf/2210.09321.pdf)
T. Peham, L. Burgholzer, and R. Wille. On Optimal Subarchitectures for Quantum Circuit Mapping.
_ACM Transactions on Quantum Computing (TQC)_, 2023.

[[7]](https://arxiv.org/pdf/2208.11713.pdf)
S. Schneider, L. Burgholzer, and R. Wille. A SAT Encoding for Optimal Clifford Circuit Synthesis.
In _Asia and South Pacific Design Automation Conference (ASP-DAC)_, 2023.

[[8]](https://arxiv.org/pdf/2305.01674.pdf)
T. Peham, N. Brandl, R. Kueng, R. Wille, and L. Burgholzer. Depth-Optimal Synthesis of Clifford Circuits with SAT Solvers.
In _IEEE International Conference on Quantum Computing and Engineering (QCE)_, 2023.

[[9]](https://arxiv.org/pdf/2309.08656.pdf)
L. Schmid, D. F. Locher, M. Rispler, S. Blatt, J. Zeiher, M. Müller, and R. Wille. Computational Capabilities and Compiler Development for Neutral Atom Quantum Processors: Connecting Tool Developers and Hardware Experts.
_Quantum Science and Technology_, 2024.

[[10]](https://arxiv.org/pdf/2311.14164.pdf)
L. Schmid, S. Park, S. Kang, and R. Wille. Hybrid Circuit Mapping: Leveraging the Full Spectrum of Computational Capabilities of Neutral Atom Quantum Computers.
In _Design Automation Conference (DAC)_, 2024.

[[11]](https://www.cda.cit.tum.de/files/eda/2024_qce_an_abstract_model_and_efficient_routing_for_logical_entangling_gates_on_Zoned_neutral_atom_architectures.pdf)
Y. Stade, L. Schmid, L. Burgholzer, and R. Wille. An Abstract Model and Efficient Routing for Logical Entangling Gates on Zoned Neutral Atom Architectures.
In _Int'l Conf. on Quantum Computing and Engineering_, 2024.

[[12]](https://www.cda.cit.tum.de/files/eda/2025_date_optimal_state_preparation_for_logical_arrays_on_zoned_neutral_atom_quantum_computers.pdf)
Y. Stade, L. Schmid, L. Burgholzer, and R. Wille. Optimal State Preparation for Logical Arrays on Zoned Neutral Atom Quantum Computers.
In _Design, Automation and Test in Europe_, 2024.

[[13]](https://www.cda.cit.tum.de/files/eda/2025_iccad_routing-aware_placement_zoned_neutral_atom.pdf)
Y. Stade, W.-H. Lin, J. Cong, and R. Wille. Routing-Aware Placement for Zoned Neutral Atom-based Quantum Computing.
In _Int'l Conference on CAD_, 2025.

---

## Acknowledgements

The Munich Quantum Toolkit has been supported by the European Research Council (ERC) under the European Union's Horizon 2020 research and innovation program (grant agreement No. 101001318), the Bavarian State Ministry for Science and Arts through the Distinguished Professorship Program, as well as the Munich Quantum Valley, which is supported by the Bavarian state government with funds from the Hightech Agenda Bayern Plus.

<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="https://raw.githubusercontent.com/munich-quantum-toolkit/.github/refs/heads/main/docs/_static/mqt-funding-footer-dark.svg" width="90%">
    <img src="https://raw.githubusercontent.com/munich-quantum-toolkit/.github/refs/heads/main/docs/_static/mqt-funding-footer-light.svg" width="90%" alt="MQT Funding Footer">
  </picture>
</p>
