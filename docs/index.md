# MQT QMAP - A Tool for Mapping Quantum Circuits onto various Hardware Technologies

```{raw} latex
\begin{abstract}
```

MQT QMAP is an open-source C++20 and Python library for mapping quantum circuits onto various hardware technologies developed as part of the _{doc}`Munich Quantum Toolkit (MQT) <mqt:index>`_ [^1].

This documentation provides a comprehensive guide to the MQT QMAP library, including {doc}`installation instructions <installation>`, demo notebooks, and detailed {doc}`API documentation <api/mqt/qmap/index>`.
The source code of MQT QMAP is publicly available on GitHub at [munich-quantum-toolkit/qmap](https://github.com/munich-quantum-toolkit/qmap), while pre-built binaries are available via [PyPI](https://pypi.org/project/mqt.qmap/) for all major operating systems and all modern Python versions.
MQT QMAP is fully compatible with Qiskit 1.0 and above.

We recommend you to start with the {doc}`installation instructions <installation>` or by reading our overview paper {cite:p}`wille2023qmap`.
Then proceed to the {doc}`mapping page <mapping>`, the {doc}`synthesis/optimization page <synthesis>`, the {doc}`neutral atom state preparation page <na_state_prep>`, or the {doc}`zoned neutral atom compiler <na_zoned_compiler>`, and read the {doc}`reference documentation <api/mqt/qmap/index>`.
If you are interested in the theory behind QMAP, have a look at the publications in the {doc}`publication list <references>`.

We appreciate any feedback and contributions to the project. If you want to contribute, you can find more information in the {doc}`Contribution <contributing>` guide.
If you are having trouble with the installation or the usage of QMAP, please let us know at our {doc}`Support <support>` page or by reaching out to us at [quantum.cda@xcit.tum.de](mailto:quantum.cda@xcit.tum.de).

[^1]:
    The _[Munich Quantum Toolkit (MQT)](https://mqt.readthedocs.io)_ is a collection of software tools for quantum computing developed by the [Chair for Design Automation](https://www.cda.cit.tum.de/) at the [Technical University of Munich](https://www.tum.de/) as well as the [Munich Quantum Software Company (MQSC)](https://munichquantum.software).
    Among others, it is part of the [Munich Quantum Software Stack (MQSS)](https://www.munich-quantum-valley.de/research/research-areas/mqss) ecosystem, which is being developed as part of the [Munich Quantum Valley (MQV)](https://www.munich-quantum-valley.de) initiative.

````{only} latex
```{note}
A live version of this document is available at [mqt.readthedocs.io/projects/qmap](https://mqt.readthedocs.io/projects/qmap).
```
````

```{raw} latex
\end{abstract}

\sphinxtableofcontents
```

```{toctree}
:hidden:

self
```

```{toctree}
:maxdepth: 2
:caption: User Guide

installation
mapping
synthesis
na_state_prep
na_zoned_compiler
references
CHANGELOG
UPGRADING
```

````{only} not latex
```{toctree}
:maxdepth: 2
:titlesonly:
:caption: Developers
:glob:

contributing
support
development_guide
```
````

```{toctree}
:hidden:
:maxdepth: 6
:caption: API Reference

api/mqt/qmap/index
```
