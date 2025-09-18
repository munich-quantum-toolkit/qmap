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

# Neutral Atom Logical State Preparation

All quantum computers are prone to errors.
This is the motivation of employing error correction during a quantum computation.
To this end, a (logical) qubit on the algorithmic level is encoded into a shared and highly entangled state of multiple physical qubits.
Before the actual computation can start, those physical qubits need to be prepared in a state that represents the logical zero state.

For that, we provide a tool based on {cite:p}`stadeOptimalStatePreparation2024` that takes a state preparation circuit and generates an optimal sequence of operations tailored to the zoned neutral atom architecture.
Thereby, the circuit consists of one initial layer of Hadamard gates on all qubits that initialize the physical qubits in the plus state.
Those are followed by a set of entangling (CZ) gates that generate a so-called graph state.
The final logical state is achieved by applying additional Hadamard gates on selected qubits.

Below we demonstrate how the optimal schedule can be retrieved for the Steane-code, the smallest 2D color code.
First, we create the state preparation circuit for the Steane-code as a `qiskit.QuantumCircuit`.

```{code-cell} ipython3
from qiskit import QuantumCircuit

qc = QuantumCircuit(7)
qc.h(range(7))
qc.cz(0, 3)
qc.cz(0, 4)
qc.cz(1, 2)
qc.cz(1, 5)
qc.cz(1, 6)
qc.cz(2, 3)
qc.cz(2, 4)
qc.cz(3, 5)
qc.cz(4, 6)
qc.h(0)
qc.h(2)
qc.h(5)
qc.h(6)

qc.draw(output="mpl")
```

We solve the problem of optimal state preparation with an SMT solver (Z3).
Therefore, we encode the problem into an SMT-model.
To construct the SMT model, the solver takes the entangling operations (CZ) as a list of qubit pairs.

```{code-cell} ipython3
from mqt.core import load
from mqt.qmap.na.state_preparation import get_ops_for_solver

circ = load(qc)
ops = get_ops_for_solver(circ, "z", 1)  # We extract the 'Z' gates with '1' control, i.e., CZ gates
ops
```

Now, we are ready to initialize the solver and to generate the optimal sequence of operations.
The parameters of the solver describe an architecture with two storage zones with each two rows, one zone above the entangling zone and one below.
The entangling zone itself consists of three rows and the architecture model has three columns.
Within each interaction site, atoms can be offset by two sites in every direction.
The considered architecture offers two AOD columns and three AOD rows.

We instruct the solver to generate a sequence consisting of four stages.
Thereby, we do not fix the number of transfer stages.
The last two boolean arguments, specify that the solver needs not to maintain the order of operations and must shield idling qubits in the storage zone.
For further details on the employed abstraction of the 2D plane in the solver, please refer to the corresponding article {cite:p}`stadeOptimalStatePreparation2024`.

```{code-cell} ipython3
from mqt.qmap.na.state_preparation import NAStatePreparationSolver

solver = NAStatePreparationSolver(3, 7, 2, 3, 2, 2, 2, 2, 2, 4)
result = solver.solve(ops, 7, 4, None, False, True)
```

To inspect the result, it can be exported to the human-readable JSON format by invoking the method `result.json()`
In this example, we take another approach and generate code from the result.
For that, we call the function `generate_code` with the respective arguments.

```{code-cell} ipython3
from mqt.qmap.na.state_preparation import generate_code

code = generate_code(circ, result)
print(code)
```
