```{raw} latex
\begingroup
\renewcommand\section[1]{\endgroup}
\phantomsection
```

````{only} html
# References

*MQT QMAP* has a strong foundation in peer‑reviewed research.
Many of its built‑in algorithms are based on methods published in scientific journals and conferences.
For an overview of *MQT QMAP* and its features, see {cite:p}`wille2023qmap`.
If you want to cite this article, please use the following BibTeX entry:

```bibtex
@inproceedings{wille2023qmap,
    title               = {{{MQT QMAP}}: {{Efficient}} quantum circuit mapping},
    author              = {Wille, Robert and Burgholzer, Lukas},
    year                = {2023},
    booktitle           = {International Symp. on Physical Design},
    doi                 = {10.1145/3569052.3578928},
}
```

*MQT QMAP* is part of the Munich Quantum Toolkit, which is described in {cite:p}`mqt`.
If you want to cite the Munich Quantum Toolkit, please use the following BibTeX entry:

```bibtex
@inproceedings{mqt,
    title        = {The {{MQT}} Handbook: {{A}} Summary of Design Automation Tools and Software for Quantum Computing},
    shorttitle   = {{The MQT Handbook}},
    author       = {Wille, Robert and Berent, Lucas and Forster, Tobias and Kunasaikaran, Jagatheesan and Mato, Kevin and Peham, Tom and Quetschlich, Nils and Rovara, Damian and Sander, Aaron and Schmid, Ludwig and Schoenberger, Daniel and Stade, Yannick and Burgholzer, Lukas},
    booktitle    = {IEEE International Conference on Quantum Software (QSW)},
    doi          = {10.1109/QSW62656.2024.00013},
    year         = 2024,
    eprint       = {2405.17543},
    eprinttype   = {arxiv},
    addendum     = {A live version of this document is available at \url{https://mqt.readthedocs.io}},
}
```

If you use *MQT QMAP* in your work, we would appreciate if you cited

- {cite:p}`zulehnerEfficientMethodologyMapping2019` when using the heuristic mapper,
- {cite:p}`willeMappingQuantumCircuits2019` when using the exact mapper,
- {cite:p}`peham2023DepthOptimalSynthesis` when using the Clifford circuit synthesis approach,
- {cite:p}`schmid2024HybridCircuitMapping` when using the hybrid mapper for neutral atom quantum computers,
- {cite:p}`stadeAbstractModelEfficient2024` when using the neutral atom logical array compiler (NALAC),
- {cite:p}`stadeOptimalStatePreparation2024` when using the optimal state preparation for neutral atoms (NASP), and
- {cite:p}`stadeRoutingAwarePlacement2025` when using the routing-aware placement for zoned neutral atom devices.

Furthermore, if you use any of the particular algorithms such as

- the heuristic mapping scheme using teleportation {cite:p}`hillmichExlpoitingQuantumTeleportation2021`
- the search space limitation techniques of the exact mapper (some of which are enabled per default) {cite:p}`burgholzer2022limitingSearchSpace`
- the method for finding (near-)optimal subarchitectures {cite:p}`peham2023OptimalSubarchitectures`

please consider citing their respective papers as well.

A full list of references is given below.
````

```{bibliography}

```
