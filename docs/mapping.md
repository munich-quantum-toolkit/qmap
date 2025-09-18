---
file_format: mystnb
kernelspec:
  name: python3
mystnb:
  number_source_lines: true
---

```{code-cell} ipython3
:tags: [remove-cell]
%config InlineBackend.figure_formats = ['svg']
```

<style>.widget-subarea{display:none;} /*hide widgets as they do not work with sphinx*/</style>

# Quantum Circuit Mapping

Many quantum computing architectures limit the pairs of qubits that two-qubit operations can be applied to.
This is commonly described by a device's _coupling map_.
To execute a generic quantum circuit (with arbitrary interactions between its qubits) on such an architecture, the circuit needs to be _mapped_.
This involves _qubit allocation_, where logical qubits are assigned to physical qubits in an _initial layout_, and _routing_, where the original circuit is augmented with SWAP gates such that it adheres to the target device's _coupling map_.

Consider the following circuit.

```{code-cell} ipython3
from qiskit import QuantumCircuit

qc = QuantumCircuit(4)
qc.h(0)
qc.cx(0, 1)
qc.cx(0, 2)
qc.cx(0, 3)

qc.barrier()

qc.t(0)
qc.t(1)
qc.t(2)
qc.t(3)

qc.barrier()

qc.cx(0, 3)
qc.cx(0, 2)
qc.cx(0, 1)

qc.measure_all()

qc.draw(output="mpl")
```

Now assume this circuit shall be mapped to a $4$-qubit architecture defined by the following coupling map:

![Linear 4-qubit Architecture](images/linear_arch.svg)

In _QMAP_ this architecture can be manually defined as follows.

```{code-cell} ipython3
from mqt.qmap.pyqmap import Architecture

arch = Architecture(
    4,
    {
        (0, 1),
        (1, 0),
        (1, 2),
        (2, 1),
        (2, 3),
        (3, 2),
    },
)
```

The quantum circuit `qc` can not be run directly on this architecture since it contains gates that act on qubits not connected on the device architecture.
Naively inserting SWAP gates that permute the logical-to-physical qubit mapping on the fly may yield the following compiled circuit.

```{code-cell} ipython3
---
mystnb:
  remove_code_source: true
---
from qiskit import QuantumCircuit

qc1 = QuantumCircuit(4)
qc1.h(0)
qc1.cx(0, 1)
qc1.swap(0, 1)
qc1.cx(1, 2)
qc1.swap(1, 2)
qc1.cx(2, 3)

qc1.barrier()

qc1.t(0)
qc1.t(1)
qc1.t(2)
qc1.t(3)

qc1.barrier()

qc1.cx(2, 3)
qc1.swap(1, 2)
qc1.cx(1, 2)
qc1.swap(0, 1)
qc1.cx(0, 1)

qc1.measure_all()

qc1.draw(output="mpl")
```

Over the course of the mapping, _four_ SWAP gates have been introduced to satisfy the connectivity constraints of the device's architecture.
Since every additional gate increases the probability of errors, this is a very costly overhead for such a small circuit.

Keeping the number of additionally introduced gates as small as possible is key for ensuring the successful execution of the quantum circuit. Finding an optimal mapping for a quantum circuit is an NP-hard problem <cite data-cite="boteaComplexityQuantumCircuit2018">Botea et al.</cite>.
_QMAP_ offers two dedicated techniques for tackling that problem:

- An _exact_ mapping approach (based on {cite:p}`willeMappingQuantumCircuits2019`, {cite:p}`burgholzer2022limitingSearchSpace`) that guarantees (gate-optimal) solutions and is typically suitable for up to 8 qubits.
- A _heuristic_ mapping approach (based on {cite:p}`zulehnerEfficientMethodologyMapping2019`, {cite:p}`hillmichExlpoitingQuantumTeleportation2021`) that allows to determine efficient mapping solutions in a scalable fashion for up to hundreds of qubits.

## Exact Mapping

The _exact mapper_ implemented in _QMAP_ maps quantum circuits using the _minimal_ number of SWAP gates.
To this end, it encodes the mapping task as a MaxSAT problem and subsequently solves it using the [SMT solver Z3](https://github.com/Z3Prover/z3). Due to the NP-hardness of the mapping task, this approach is only scalable up to roughly eight qubits in most scenarios.

```{note}
On directional architectures, it can be significantly cheaper to surround a CNOT gate with four Hadamard operations (effectively exchanging its control and target) instead of adding a SWAP gate. For these architectures, QMAP minimizes the number of additional SWAP and H gates.
```

Using the exact mapper is as simple as:

```{code-cell} ipython3
from mqt.qmap import compile

qc_mapped, res = compile(qc, arch, method="exact", post_mapping_optimizations=False)

qc_mapped.draw(output="mpl")
```

```{code-cell} ipython3
print(f"Additional SWAPs: {res.output.swaps}")
print(f"Runtime:          {res.time:f}")
```

The resulting solution only requires _two_ SWAP gates for mapping the circuit.

```{note}

The exact mapping method implemented in QMAP is optimal with respect to the number of additional SWAP gates needed for mapping a given circuit.
It is not guaranteed to be optimal with respect to the number of additional gates needed for mapping a given circuit, e.g., any sequence of a SWAP gate and a CNOT gate acting on the same qubits can be simplified to just two CNOT gates.
Such an optimization pass is conducted by default in the `compile` method after the circuit has been mapped.
However, this cost reduction is not accounted for in the SAT formulation at the moment.
```

## Heuristic Mapping

The _heuristic mapper_ implemented in _QMAP_ uses A\*-search to efficiently traverse the immense search space of the mapping problem.
It effectively trades optimality for runtime.
This allows to reliably determine suitable mappings for circuits with up to hundreds of qubits.
Using the heuristic mapper works completely analogous to the exact mapper.

```{code-cell} ipython3
qc_mapped, res = compile(qc, arch, method="heuristic", post_mapping_optimizations=False)

qc_mapped.draw(output="mpl")
```

```{code-cell} ipython3
print(f"Additional SWAPs: {res.output.swaps}")
print(f"Runtime:          {res.time:f}")
```

While this solution is not optimal anymore it only requires one more SWAP gate and even for such a small example the heuristic mapper is orders of magnitudes faster than the exact mapper.
