# MQT QMAP - A Tool for Mapping Quantum Circuits onto various Hardware Technologies

```{raw} latex
\begin{abstract}
```

MQT QMAP is an open-source C++20 and Python library for mapping quantum circuits onto various hardware technologies developed as part of the _{doc}`Munich Quantum Toolkit (MQT) <mqt:index>`_.

This documentation provides a comprehensive guide to the MQT QMAP library, including {doc}`installation instructions <installation>`, demo notebooks, and detailed {doc}`API documentation <api/mqt/qmap/index>`.
The source code of MQT QMAP is publicly available on GitHub at [munich-quantum-toolkit/qmap](https://github.com/munich-quantum-toolkit/qmap), while pre-built binaries are available via [PyPI](https://pypi.org/project/mqt.qmap/) for all major operating systems and all modern Python versions.
MQT QMAP is fully compatible with Qiskit 1.0 and above.

We recommend you to start with the {doc}`installation instructions <installation>` or by reading our overview paper {cite:p}`wille2023qmap`.
Then proceed to the {doc}`mapping page <mapping>`, the {doc}`synthesis/optimization page <synthesis>`, the {doc}`neutral atom state preparation page <na_state_prep>`, or the {doc}`zoned neutral atom compiler <na_zoned_compiler>`, and read the {doc}`reference documentation <api/mqt/qmap/index>`.
If you are interested in the theory behind MQT QMAP, have a look at the publications in the {doc}`publication list <references>`.

We appreciate any feedback and contributions to the project. If you want to contribute, you can find more information in the {doc}`Contribution <contributing>` guide.
If you are having trouble with the installation or the usage of MQT QMAP, please let us know at our {doc}`Support <support>` page or by reaching out to us at [quantum.cda@xcit.tum.de](mailto:quantum.cda@xcit.tum.de).

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
```
````

```{toctree}
:hidden:
:maxdepth: 6
:caption: API Reference

api/mqt/qmap/index
```

```{only} html
## Contributors and Supporters

The _[Munich Quantum Toolkit (MQT)](https://mqt.readthedocs.io)_ is developed by the [Chair for Design Automation](https://www.cda.cit.tum.de/) at the [Technical University of Munich](https://www.tum.de/) and supported by the [Munich Quantum Software Company (MQSC)](https://munichquantum.software).
Among others, it is part of the [Munich Quantum Software Stack (MQSS)](https://www.munich-quantum-valley.de/research/research-areas/mqss) ecosystem, which is being developed as part of the [Munich Quantum Valley (MQV)](https://www.munich-quantum-valley.de) initiative.

<div style="margin-top: 0.5em">
<div class="only-light" align="center">
  <img src="https://raw.githubusercontent.com/munich-quantum-toolkit/.github/refs/heads/main/docs/_static/mqt-logo-banner-light.svg" width="90%" alt="MQT Banner">
</div>
<div class="only-dark" align="center">
  <img src="https://raw.githubusercontent.com/munich-quantum-toolkit/.github/refs/heads/main/docs/_static/mqt-logo-banner-dark.svg" width="90%" alt="MQT Banner">
</div>
</div>

Thank you to all the contributors who have helped make MQT QMAP a reality!

<p align="center">
<a href="https://github.com/munich-quantum-toolkit/qmap/graphs/contributors">
  <img src="https://contrib.rocks/image?repo=munich-quantum-toolkit/qmap" alt="Contributors to munich-quantum-toolkit/qmap" />
</a>
</p>

The MQT will remain free, open-source, and permissively licensed—now and in the future.
We are firmly committed to keeping it open and actively maintained for the quantum computing community.

To support this endeavor, please consider:

- Starring and sharing our repositories: [https://github.com/munich-quantum-toolkit](https://github.com/munich-quantum-toolkit)
- Contributing code, documentation, tests, or examples via issues and pull requests
- Citing the MQT in your publications (see {doc}`References <references>`)
- Using the MQT in research and teaching, and sharing feedback and use cases
- Sponsoring us on GitHub: [https://github.com/sponsors/munich-quantum-toolkit](https://github.com/sponsors/munich-quantum-toolkit)

<p align="center">
<iframe src="https://github.com/sponsors/munich-quantum-toolkit/button" title="Sponsor munich-quantum-toolkit" height="32" width="114" style="border: 0; border-radius: 6px;"></iframe>
</p>
```
