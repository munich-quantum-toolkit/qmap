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

# Synthesis of Clifford Circuits

Executing quantum circuits on a quantum computer requires compilation to representations that conform to all restrictions imposed by the device.
Due to device's limited coherence times and gate fidelity, the compilation process has to be optimized as much as possible.
To this end, an algorithm's description first has to be _synthesized_ using the device's gate library.
In addition, circuits have to be _mapped_ to the target quantum device to satisfy its connectivity constraints.
Even though Clifford circuits form a finite subgroup of all quantum circuits -- one that is not even universal for quantum computing -- the search space for these problems grows exponentially with respect to the number of considered qubits.

The _Clifford synthesis approach_ in QMAP can be used to produce optimal Clifford circuits based on the methods proposed in {cite:p}`peham2023DepthOptimalSynthesis}.
To this end, it encodes the underlying task as a satisfiability (SAT) problem and solves it using the [SMT solver Z3](https://github.com/Z3Prover/z3) in conjunction with a binary search scheme.

The following gives a brief overview on Clifford circuits and how QMAP can be used for their synthesis.

## Clifford Circuits

Clifford circuits, i.e., circuits generated from the set $\{H, S, \mathit{CNOT}\}$, form an important subclass of quantum circuits.
This is due to several factors

- According to the Gottesman-Knill theorem, they can be simulated in polynomial time and space on classical computers using the _stabilizer_ formalism.
- They can be used to describe several quantum phenomena such as superposition, entanglement, superdense coding, and teleportation.
- Many error correcting codes rely on them.

Quantum states that can be obtained from the all-zero basis state $|0\dots 0\rangle$ by applying Clifford operations are called stabilizer states.
The name originates from the fact that such a state is uniquely and efficiently described by the set of operators that generate the group of its stabilizers.
Specifically, any _n_-qubit stabilizer state can be described by a set of _n_ Pauli strings $\pm P_{i,0}P_{i,1}P_{i,2}\dots P_{i,n-1}$, with $P_{i,j}\in\{I, X, Y, Z\}$ and $i, j\in 0,\dots, n-1$.
Hence, two bits per qubit are needed to identify the Pauli operator, as well as one additional bit for the phase, which leads to a total of $n(2n+1)` bits needed to uniquely describe a particular stabilizer state.

The stabilizer representation of a quantum state is conveniently described by a _tableau_:

$$
    \begin{bmatrix}
        x_{0,0}   & \cdots & x_{0,n-1}   & z_{0,0}    & \cdots & z_{0,n-1}   & r_0    \\
        \vdots    & \ddots &  \vdots         & \vdots     & \ddots &    \vdots         & \vdots \\
        x_{n-1,0} & \cdots & x_{n-1,n-1} & z_{n-1,0} & \cdots & z_{n-1,n-1} & r_{n-1}  \\
    \end{bmatrix}
$$

Here, the binary variables $x_{ij}$ and $z_{ij}$ specify whether the Pauli term $P_{i,j}$ is $X$ or $Z$, respectively.
Since $Y = iXZ$, setting $x_{ij} = z_{ij} = 1$ corresponds to $P_{i,j}=Y$.
Finally, $r_i$ describes whether the generator has a negative phase.

Consider the following quantum circuit:

```{code-cell} ipython3
from qiskit import QuantumCircuit

qc = QuantumCircuit(2)
qc.h(0)
qc.cx(0, 1)
qc.h(0)
qc.h(1)

qc.draw(output="mpl")
```

Then, the corresponding stabilizer tableau is

```
0 0 | 1 1 | 0
1 1 | 0 0 | 0
```

which corresponds to the stabilizers

```{code-cell} ipython3
stabilizers = ["+ZZ", "+XX"]
```

The stabilizer tableau does not fix a unitary operator uniquely. As stated above, the stabilizers only fix the state that is obtained by applying a unitary to the all-zero basis state. The following circuit also produces the same state as the one above:

```{code-cell} ipython3
from qiskit import QuantumCircuit

qc_alt = QuantumCircuit(2)
qc_alt.z(0)
qc_alt.h(0)
qc_alt.cx(0, 1)
qc_alt.h(0)
qc_alt.h(1)

qc_alt.draw(output="mpl")
```

But the first circuit has the unitary

$$
\frac{1}{\sqrt{2}}\begin{bmatrix}1&0&1&0\\0&1&0&1\\0&1&0&-1\\1&0&-1&0\end{bmatrix}
$$

whereas the second circuit has the unitary

$$
\frac{1}{\sqrt{2}}\begin{bmatrix}1&0&1&0\\0&-1&0&-1\\0&-1&0&1\\1&0&-1&0\end{bmatrix}
$$

To fix the unitary one needs to also take the _destabilizers_ into account. The destabilizers are also Pauli strings that together with the stabilizers generate the entire Pauli group.

The destabilizers of the first circuit are

```{code-cell} ipython3
destabilizers = ["+IX", "+ZI"]
```

The destabilizers of the second circuit are

```{code-cell} ipython3
destabilizers_alt = ["-IX", "+ZI"]
```

## Using QMAP for Optimal Synthesis

_QMAP_ can be used in a multitude of ways to efficiently synthesize Clifford circuits:

### Starting from an initial circuit `qc`

```{code-cell} ipython3
from qiskit import QuantumCircuit

from mqt import qmap

qc = QuantumCircuit(2)
qc.h(0)
qc.cx(0, 1)
qc.h(0)
qc.h(1)

qc_opt, results = qmap.optimize_clifford(circuit=qc, use_maxsat=True, include_destabilizers=True)

qc_opt.draw(output="mpl")
```

The `include_destabilizers` flag guarantees that the unitary of the circuit is preserved during optimization.

By default _QMAP_ generates optimal Clifford circuits with respect to the target metric.
This might lead to runtime problems when trying to optimize larger circuits.
When optimizing for depth, _QMAP_ provides a heuristic that splits the circuits into several independent parts and optimizes them separately.
This allows optimizing larger circuits while not guaranteeing that the depth-optimal circuit is synthesized.

The heuristic synthesizer can be used as follows:

```{code-cell} ipython3
from qiskit import QuantumCircuit

from mqt import qmap

qc = QuantumCircuit(2)
qc.x(0)
qc.cx(0, 1)
qc.x(0)
qc.s(1)
qc.x(1)
qc.cx(1, 0)
qc.x(1)

qc_opt, results = qmap.optimize_clifford(
    circuit=qc, heuristic=True, split_size=3, include_destabilizers=True, target_metric="depth"
)

qc_opt.draw(output="mpl")
```

The parameter `split_size` determines how many layers of the circuit are optimized individually.

In this example the synthesized circuit does not have optimal depth as can be checked by running the optimal synthesis method:

```{code-cell} ipython3
from qiskit import QuantumCircuit

from mqt import qmap

qc = QuantumCircuit(2)
qc.x(0)
qc.cx(0, 1)
qc.x(0)
qc.s(1)
qc.x(1)
qc.cx(1, 0)
qc.x(1)

qc_opt, results = qmap.optimize_clifford(circuit=qc, heuristic=False, include_destabilizers=True, target_metric="depth")

qc_opt.draw(output="mpl")
```

However, the heuristic still gives a good depth reduction in many cases.

+++

### Starting from a functional description

```{code-cell} ipython3
from mqt.qmap import synthesize_clifford
from mqt.qmap.clifford_synthesis import Tableau

tableau = Tableau("['+ZZ', '+XX']")
qc_synth, results = synthesize_clifford(tableau)

qc_synth.draw(output="mpl")
```

The synthesis method offers lots of configuration options to fine-tune the synthesis procedure, e.g., changing the target metric.

See {func}`~mqt.qmap.clifford_synthesis.synthesize_clifford` and {func}`~mqt.qmap.clifford_synthesis.optimize_clifford` for more information.
