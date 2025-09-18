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

# Zoned Neutral Atom Compiler

Successful quantum computation requires advanced software, especially compilers that optimize quantum algorithms for
hardware execution.
Zoned neutral atom architectures execute operations in designated spatially separated zones.
The zones facilitate higher coherence times and overall fidelity of the quantum computation.
However, the zones also require the rearrangement of atoms during the quantum computation.
MQT QMAP provides two tools to compile a quantum circuit to target-specific instructions for zoned quantum computing architectures based on neutral atoms:

- a reuse-aware compiler based on {cite:p}`linReuseAwareCompilationZoned2025`, and
- a routing-aware compiler based on {cite:p}`stadeRoutingAwarePlacement2025`.

:::{note}
The second, i.e., routing-aware compiler also implements the reuse-aware compilation approach.
Specifically, it exchanges the placement component of the reuse-aware compiler with a routing-aware placer.
Hence, in the following, the first compiler is referred to as the _routing-agnostic compiler_ and the second one as the _routing-aware compiler_.
:::

## Example: GHZ State on Neutral Atom Architecture

In this example, we will demonstrate how to use the zoned neutral atom compiler to generate a sequence of
target-specific instructions for a quantum circuit.
For this purpose, we employ a circuit that prepares a Greenberger-Horne-Zeilinger (GHZ) state of 8 qubits.
Compared to the usual GHZ circuit that applies the CX-gates in a chain, this circuit applies the CX-gates in a tree-like
manner.
First, it brings one qubit into maximal superposition and then applies the first CX-gate as usual.
Afterward, it applies two CX-gates in parallel controlled on the qubits already in superposition instead of applying them
serially.

```{code-cell} ipython3
from qiskit import QuantumCircuit
from numpy import pi

qc = QuantumCircuit(8)
qc.h(0)
qc.cx(0, 4)
qc.cx(0, 2)
qc.cx(4, 6)
qc.cx(0, 1)
qc.cx(2, 3)
qc.cx(4, 5)
qc.cx(6, 7)

qc.draw(output="mpl")
```

This circuit is not compatible with the native gate set of the zoned neutral atom architecture that consists of global
ry-gates, local rz-gates, and controlled z-gates.
The following circuit is equivalent to the previous one but decomposes the circuit into the native gate set of the
zoned neutral atom architecture.

```{note}
Even though other single-qubit gates may not be supported by the hardware, the compiler can handle arbitrary
single-qubit gates. It will translate them to generic u3 gates and include them in the output.
```

```{code-cell} ipython3
from qiskit import QuantumCircuit
from numpy import pi

def global_ry(theta, num_qubits):
    """:returns: a global ry gate"""
    qc = QuantumCircuit(num_qubits)
    qc.ry(theta, range(num_qubits))
    return qc.to_gate(label = f"Ry({theta:.3f})")


qc = QuantumCircuit(8)
qc.append(global_ry(-pi/4, 8), range(8))
qc.z(range(8))
qc.append(global_ry(pi/4, 8), range(8))
qc.cz(0, 4)
qc.append(global_ry(-pi/4, 8), range(8))
qc.z(4)
qc.append(global_ry(pi/4, 8), range(8))
qc.cz(0, 2)
qc.cz(4, 6)
qc.append(global_ry(-pi/4, 8), range(8))
qc.z([2, 6])
qc.append(global_ry(pi/4, 8), range(8))
qc.cz(0, 1)
qc.cz(2, 3)
qc.cz(4, 5)
qc.cz(6, 7)
qc.append(global_ry(-pi/4, 8), range(8))
qc.z([1,3,5,7])
qc.append(global_ry(pi/4, 8), range(8))

qc.draw(output="mpl")
```

On the considered architecture, the single-qubit gates, i.e., the global ry and local rz-gates can be executed everywhere.
However, the controlled z-gates can only be executed between nearby atoms in the so-called entanglement zone.
This entanglement zone is spatially separated from the storage zone, where all atoms not involved in a cz-gate are
stored.

```{image} images/zones.pdf
:alt: Zoned Neutral Atom Architecture
:width: 80%
:align: center
```

To find an optimized sequence of target-specific instructions, we use one of the zoned neutral atom compilers.
Each compiler requires first a specification of the architecture.

```{code-cell} ipython3
from mqt.qmap.na.zoned import ZonedNeutralAtomArchitecture

arch = ZonedNeutralAtomArchitecture.from_json_string("""{
  "name": "Architecture with one entanglement and one storage zone",
  "operation_duration": {"rydberg_gate": 0.36, "single_qubit_gate": 52, "atom_transfer": 15},
  "operation_fidelity": {"rydberg_gate": 0.995, "single_qubit_gate": 0.9997, "atom_transfer": 0.999},
  "qubit_spec": {"T": 1.5e6},
  "storage_zones": [{
    "zone_id": 0,
    "slms": [{"id": 0, "site_separation": [3, 3], "r": 20, "c": 100, "location": [0, 0]}],
    "offset": [0, 0],
    "dimension": [297, 57]
  }],
  "entanglement_zones": [{
    "zone_id": 0,
    "slms": [
      {"id": 1, "site_separation": [12, 10], "r": 7, "c": 20, "location": [35, 67]},
      {"id": 2, "site_separation": [12, 10], "r": 7, "c": 20, "location": [37, 67]}
    ],
    "offset": [35, 67],
    "dimension": [230, 60]
  }],
  "aods": [{"id": 0, "site_separation": 2, "r": 100, "c": 100}],
  "rydberg_range": [[[30, 62], [270, 132]]]
}""")
```

In the following, we will first create a compiler with default settings.
Those can later be fine-tuned to fit the needs of the user, see further down.

```{code-cell} ipython3
from mqt.qmap.na.zoned import RoutingAgnosticCompiler, RoutingAwareCompiler

compiler = RoutingAwareCompiler(arch)
# or if you want to use the routing-agnostic compiler:
# compiler = RoutingAgnosticCompiler(arch)
```

Now, the created compiler can be used to compile the circuit from above.
The output is in the `.naviz` format that can be read by the `MQT NAViz` tool
at [github.com/cda-tum/mqt-naviz](https://github.com/cda-tum/mqt-naviz).
This tool allows visualizing the resulting quantum computation.
To import the architecture used for the compilation into MQT NAViz, you can use the [`to_namachine_file`](#mqt.qmap.na.zoned.ZonedNeutralAtomArchitecture.to_namachine_file) method to export the architecture to the `.namachine` format accepted by MQT NAViz.

```{code-cell} ipython3
from mqt.core import load

circ = load(qc)
code = compiler.compile(circ)
print(code)
```

```{note}
The A* search in the placer of the routing-aware compiler is quite memory intensive.
Right now, the maximum number of nodes considered in the A* search is limited to 50M.
If this limit is hit, you will get an error message. You can freely adapt this limit
by setting the argument `max_nodes` in the constructor of the `RoutingAwareCompiler`, see below.
```

Above, we have used the default settings for the compiler.
However, the different stages of the compiler can also be configured, e.g., the deepening factor of the A\*-placer:

```{code-cell} ipython3
compiler = RoutingAwareCompiler(arch, deepening_factor = 0.6)

circ = load(qc)
code = compiler.compile(circ)
print(code)
```
