[![PyPI](https://img.shields.io/pypi/v/mqt.qmap?logo=pypi&style=flat-square)](https://pypi.org/project/mqt.qmap/)
![OS](https://img.shields.io/badge/os-linux%20%7C%20macos%20%7C%20windows-blue?style=flat-square)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg?style=flat-square)](https://opensource.org/licenses/MIT)
[![CI](https://img.shields.io/github/actions/workflow/status/cda-tum/mqt-qmap/ci.yml?branch=main&style=flat-square&logo=github&label=ci)](https://github.com/cda-tum/mqt-qmap/actions/workflows/ci.yml)
[![CD](https://img.shields.io/github/actions/workflow/status/cda-tum/mqt-qmap/cd.yml?style=flat-square&logo=github&label=cd)](https://github.com/cda-tum/mqt-qmap/actions/workflows/cd.yml)
[![Documentation](https://img.shields.io/readthedocs/mqtqmap?logo=readthedocs&style=flat-square)](https://mqt.readthedocs.io/projects/qmap)
[![codecov](https://img.shields.io/codecov/c/github/cda-tum/mqt-qmap?style=flat-square&logo=codecov)](https://codecov.io/gh/cda-tum/mqt-qmap)

<p align="center">
  <a href="https://mqt.readthedocs.io">
   <picture>
     <source media="(prefers-color-scheme: dark)" srcset="https://raw.githubusercontent.com/cda-tum/mqt/main/docs/_static/mqt_light.png" width="60%">
     <img src="https://raw.githubusercontent.com/cda-tum/mqt/main/docs/_static/mqt_dark.png" width="60%">
   </picture>
  </a>
</p>

# MQT QMAP - A tool for Quantum Circuit Compilation

A tool for quantum circuit compilation developed as part of the [_Munich Quantum Toolkit (MQT)_](https://mqt.readthedocs.io) by the [Chair for Design Automation](https://www.cda.cit.tum.de/) at the [Technical University of Munich](https://www.tum.de/).
It builds upon [MQT Core](https://github.com/cda-tum/mqt-core), which forms the backbone of the MQT.

<p align="center">
  <a href="https://mqt.readthedocs.io/projects/qmap">
  <img width=30% src="https://img.shields.io/badge/documentation-blue?style=for-the-badge&logo=read%20the%20docs" alt="Documentation" />
  </a>
</p>

If you have any questions, feel free to contact us via [quantum.cda@xcit.tum.de](mailto:quantum.cda@xcit.tum.de) or by creating an issue on [GitHub](https://github.com/cda-tum/mqt-qmap/issues).

## Getting Started

<p align="center">
  <a href="https://arxiv.org/abs/2301.11935">
  <img width=30% src="https://img.shields.io/badge/overview paper-blue?style=for-the-badge&logo=arxiv" alt="Overview Paper" />
  </a>
</p>

QMAP is available via [PyPI](https://pypi.org/project/mqt.qmap/) for Linux, macOS, and Windows and supports Python 3.9 to 3.13.

```console
(venv) $ pip install mqt.qmap
```

Compiling a given quantum circuit to a certain device is as easy as

```python3
from mqt import qmap
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
circ_mapped, results = qmap.compile(circ, arch=arch)
```

Optimizing a Clifford circuit is as easy as

```python3
from mqt import qmap
from qiskit import QuantumCircuit

circ = QuantumCircuit(2)
circ.h(1)
circ.cx(0, 1)
circ.h(0)
circ.h(1)

circ_opt, results = qmap.optimize_clifford(circ)
```

**Detailed documentation on all available methods, options, and input formats is available at [ReadTheDocs](https://mqt.readthedocs.io/projects/qmap).**

## System Requirements and Building

The implementation is compatible with any C++17 compiler, a minimum CMake version of 3.19, and Python 3.9+.
Please refer to the [documentation](https://mqt.readthedocs.io/projects/qmap) on how to build the project.

Building (and running) is continuously tested under Linux, macOS, and Windows using the [latest available system versions for GitHub Actions](https://github.com/actions/virtual-environments).

## References

QMAP has been developed based on methods proposed in the following papers:

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

[[11]](https://arxiv.org/pdf/2405.08068.pdf)
Y. Stade, L. Schmid, L. Burgholzer, and R. Wille. An Abstract Model and Efficient Routing for Logical Entangling Gates on Zoned Neutral Atom Architectures.
_arXiv:2405.08068_, 2024.

---

## Acknowledgements

The Munich Quantum Toolkit has been supported by the European
Research Council (ERC) under the European Union's Horizon 2020 research and innovation program (grant agreement
No. 101001318), the Bavarian State Ministry for Science and Arts through the Distinguished Professorship Program, as well as the
Munich Quantum Valley, which is supported by the Bavarian state government with funds from the Hightech Agenda Bayern Plus.

<p align="center">
<picture>
<source media="(prefers-color-scheme: dark)" srcset="https://raw.githubusercontent.com/cda-tum/mqt/main/docs/_static/tum_dark.svg" width="28%">
<img src="https://raw.githubusercontent.com/cda-tum/mqt/main/docs/_static/tum_light.svg" width="28%" alt="TUM Logo">
</picture>
<picture>
<img src="https://raw.githubusercontent.com/cda-tum/mqt/main/docs/_static/logo-bavaria.svg" width="16%" alt="Coat of Arms of Bavaria">
</picture>
<picture>
<source media="(prefers-color-scheme: dark)" srcset="https://raw.githubusercontent.com/cda-tum/mqt/main/docs/_static/erc_dark.svg" width="24%">
<img src="https://raw.githubusercontent.com/cda-tum/mqt/main/docs/_static/erc_light.svg" width="24%" alt="ERC Logo">
</picture>
<picture>
<img src="https://raw.githubusercontent.com/cda-tum/mqt/main/docs/_static/logo-mqv.svg" width="28%" alt="MQV Logo">
</picture>
</p>
