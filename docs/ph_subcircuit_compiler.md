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

# Photonic Subcircuit Compiler

Linear-optical quantum computing encodes quantum information into the spatial
modes of single photons and processes it through a mesh of Mach-Zehnder
Interferometers (MZIs). Each MZI couples two neighbouring spatial modes via two
beam splitters and a set of phase shifters whose angles collectively implement a
unitary transformation. To compile a target unitary onto such a chip, the
phase-shifter values must be tuned so that the chip's physical transfer matrix
matches the desired operation.

In practice, mode-dependent input and output transmission losses directly limit
the _coincidence rate_, defined as the probability that all photons are detected
simultaneously within the computation zone (the target output modes), rather
than being lost or scattered elsewhere on the chip. Beam-splitter reflectivities
deviating from the ideal 50/50 split further influence this rate. Together,
these imperfections make the choice of how and where to perform an operation on
the chip non-trivial.

MQT QMAP's photonic subcircuit compiler addresses this by finding the routing
path through the chip that minimizes overall photon loss. To do so, a layered
directed acyclic graph (DAG) is constructed from the chip's characterization
data. Edges from the source to candidate input ports are weighted by the
combined input transmission loss of those modes; intermediate edges are weighted
by the bar or cross fidelity of the MZIs the photons traverse during routing;
and final edges to the sink are weighted by the output transmission loss of each
candidate output window. All weights are expressed as $-\log(\text{fidelity})$,
so finding the shortest path through the DAG is equivalent to finding the
routing that maximizes the product of all transmission and routing fidelities
along the photon's path. Once the optimal mode window is identified, a
gradient-based optimizer (Adam) tunes the phase-shifter parameters to compile
the target unitary into that subspace. The result is a set of phases for the
phase shifters of the chip that implement the desired target unitary and the
routing.

:::{note}
Compiling a subcircuit with `compile_subcircuit` needs `torch` and `numpy`,
provided by the optional `photonics` extra. Install it with:

```console
pip install "mqt.qmap[photonics]"
```

:::

## Hardware Model

The chip is a staggered MZI mesh with `chip_dim` spatial modes and `chip_dim`
MZI layers. Layers alternate between _complete_ layers (MZIs coupling modes 0–1,
2–3, …) and _incomplete_ layers (MZIs coupling modes 1–2, 3–4, …). Each MZI
consists of:

- an input beam splitter with reflectivity $r_\text{in}$,
- a phase shifter on each of the two coupled modes,
- an output beam splitter with reflectivity $r_\text{out}$.

The physical imperfections of the chip are captured by three lists:

- **`beam_splitter_reflectivities`** — a list of length `2 * total_mzis`,
  ordered MZI-by-MZI as `[r_in^0, r_out^0, r_in^1, r_out^1, …]`. Ideal chips
  have all values equal to 0.5.
- **`input_transmissions`** — per-mode amplitude transmission coefficients at
  the chip input (fibers, gratings, waveguide tapers). Values in `[0, 1]`,
  normalized so the best mode has coefficient 1.
- **`output_transmissions`** — same for the chip output.

A target unitary of dimension `target_dim` is compiled into a sub-block of the
`chip_dim`-mode chip. Routing selects which `target_dim` modes to use; the
remaining modes act as a waveguide network that steers the photons into the
selected zone.

## Example: Compiling a 4×4 Unitary onto an 8-Mode Chip

### Providing the chip characterization and target unitary

On real hardware the beam-splitter reflectivities and the per-mode input/output
transmissions are **properties of the fabricated chip**, obtained from
calibration measurements; the target unitary is the gate you want to run. In
practice you would load them from your own files, for example:

```python
import numpy as np
import torch

# Measured device characterization:
input_transmissions = np.loadtxt("input_transmissions.txt").tolist()  # length chip_dim
output_transmissions = np.loadtxt("output_transmissions.txt").tolist()  # length chip_dim
beam_splitter_reflectivities = np.loadtxt("beam_splitters.txt").tolist()  # length 2 * total_mzis

# The gate to compile, as a complex (target_dim, target_dim) unitary:
target_unitary = torch.as_tensor(np.load("target_unitary.npy"), dtype=torch.complex128)
```

Note the expected formats: `beam_splitter_reflectivities` is a **flat list**,
not a matrix — one entry per beam splitter, ordered MZI-by-MZI as
`[MZI0_in, MZI0_out, MZI1_in, MZI1_out, …]` (an 8-mode chip has 28 MZIs → 56
values). `input_transmissions` and `output_transmissions` are lists of length
`chip_dim`. The target unitary must be a `torch.Tensor` with a complex dtype;
wrap a NumPy array with `torch.as_tensor(array, dtype=torch.complex128)`.

For this example we have no physical device, so we synthesise representative
random data of the same shapes instead:

```{code-cell} ipython3
import numpy as np
import torch

chip_dim = 8
target_dim = 4

# Placeholder for beam-splitter reflectivities: two values per MZI (in/out),
# ordered MZI-by-MZI, scattered around the ideal 0.5 split. A chip_dim-mode chip
# has chip_dim // 2 MZIs on even layers and chip_dim // 2 - 1 on odd layers.
bs_rng = np.random.default_rng(42)
total_mzis = sum(chip_dim // 2 if layer % 2 == 0 else chip_dim // 2 - 1 for layer in range(chip_dim))
beam_splitter_reflectivities = bs_rng.normal(0.55, 0.04, size=2 * total_mzis).clip(0.0, 1.0).tolist()

# Placeholder for transmissions, normalized so the best mode is 1.0.
hw_rng = np.random.default_rng(9)
input_transmissions = hw_rng.uniform(0.7, 1.0, size=chip_dim)
input_transmissions /= input_transmissions.max()
input_transmissions = input_transmissions.tolist()
output_transmissions = hw_rng.uniform(0.7, 1.0, size=chip_dim)
output_transmissions /= output_transmissions.max()
output_transmissions = output_transmissions.tolist()

# Placeholder for desired unitary: a Haar-random 4x4 unitary via the QR method
# (draw a complex Gaussian, QR-decompose, fix the phases on R's diagonal).
gen = torch.Generator().manual_seed(10)
z = torch.randn(target_dim, target_dim, generator=gen, dtype=torch.complex128)
q, r = torch.linalg.qr(z)
target_unitary = q * (torch.diagonal(r) / torch.diagonal(r).abs()).unsqueeze(0)

print("beam_splitter_reflectivities:", len(beam_splitter_reflectivities), "values (flat, 2 * total_mzis)")
print("input_transmissions :", [round(t, 3) for t in input_transmissions])
print("output_transmissions:", [round(t, 3) for t in output_transmissions])
```

### Compile the subcircuit

`compile_subcircuit` runs the routing search and the phase-shifter optimization
and returns a {py:class}`~mqt.qmap.ph.subcircuit_compilation.CompilationResult`.
It needs only the chip characterization and the target unitary.

```{code-cell} ipython3
from mqt.qmap.ph.subcircuit_compilation import OptimizationConfig, compile_subcircuit

torch.manual_seed(0)  # only seeds the phase-shifter initialization
config = OptimizationConfig(max_iterations=500)

result = compile_subcircuit(
    beam_splitter_reflectivities=beam_splitter_reflectivities,
    input_transmissions=input_transmissions,
    output_transmissions=output_transmissions,
    target_unitary=target_unitary,
    config=config,
)
```

### Inspect the compiled result

The result carries everything needed to drive the chip:

```{code-cell} ipython3
print("Phase-shifter settings (count):", len(result.phases))          # chip_dim**2, column-major
print("Inject photons at input modes :", result.input_ports)          # target_dim // 2 mode indices
print("Detect photons at output modes:", result.output_ports)         # target_dim mode indices
print(f"Final fidelity loss           : {result.loss:.2e}")
```

`result.phases` is a flat list of `chip_dim**2` phase-shifter angles in
column-major (layer-by-layer) order — the value for spatial mode `r` in MZI
layer `c` is at index `c * chip_dim + r`. These are the values you program onto
the chip. `result.input_ports` and `result.output_ports` are both lists of
physical mode indices — which modes to inject photons into and which modes to
read out. The router chose these to minimize photon loss.

## The compilation result

{py:class}`~mqt.qmap.ph.subcircuit_compilation.CompilationResult` bundles:

| Field          | Meaning                                                                                               |
| -------------- | ----------------------------------------------------------------------------------------------------- |
| `phases`       | `(chip_dim, chip_dim)` tensor of phase-shifter angles to program (rows = modes, columns = MZI layers) |
| `input_ports`  | the `target_dim // 2` physical modes to inject photons into (lower mode of each dual-rail pair)       |
| `output_ports` | the `target_dim` physical modes of the computation zone, where the output is measured                 |
| `loss`         | final fidelity loss of the optimization (see below)                                                   |
| `compute_time` | wall-clock seconds for routing + optimization                                                         |

### Fidelity loss

The fidelity loss is the optimizer's objective:

$$\mathcal{L} = 1 - \frac{|\operatorname{Tr}(U_\text{target}^\dagger \, U_\text{chip})|^2}{N^2}$$

where $N$ is the number of compared columns. A loss near zero means the chip's
effective unitary closely matches the target in the routed subspace. It is a
noise-free quantity computed directly from the phase-shifter parameters.

## Configuration

The optimization behavior is controlled by
{py:class}`~mqt.qmap.ph.subcircuit_compilation.OptimizationConfig`:

```{code-cell} ipython3
from mqt.qmap.ph.subcircuit_compilation import OptimizationConfig

config = OptimizationConfig(
    lr=0.05,                          # Adam learning rate (initial)
    threshold=1e-6,                   # Stop when fidelity loss drops below this
    max_iterations=10000,             # Hard iteration cap
    exclude_edge_phase_shifters=False, # Exclude the two corner phase shifters
    optimize_routing_parameters=True, # Allow routing MZIs one free parameter
)
```

| Parameter                     | Default | Effect                                                                           |
| ----------------------------- | ------- | -------------------------------------------------------------------------------- |
| `lr`                          | `0.05`  | Initial Adam learning rate; a scheduler halves it on plateau                     |
| `threshold`                   | `1e-6`  | Early exit once fidelity loss falls below this value                             |
| `max_iterations`              | `10000` | Maximum gradient steps regardless of convergence                                 |
| `exclude_edge_phase_shifters` | `False` | Drop the phase shifters at the two chip corners (reduces parameter count by 2)   |
| `optimize_routing_parameters` | `True`  | Give each routing MZI one free parameter to compensate small reflectivity errors |

In practice, 300–500 iterations are sufficient for `chip_dim = 8` and
`target_dim = 4` with a good initial learning rate. For larger chips or noisier
hardware, increasing `max_iterations` and reducing `lr` can improve convergence.
