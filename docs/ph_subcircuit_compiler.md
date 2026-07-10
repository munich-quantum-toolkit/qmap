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
simultaneously at the correct output ports. Beam-splitter reflectivities
deviating from the ideal 50/50 split further influence this rate. Together,
these imperfections make the choice of how and where to perform an operation on
the chip non-trivial.

MQT QMAP's photonic subcircuit compiler addresses this by finding the routing
path through the chip that minimises overall photon loss. To do so, a layered
directed acyclic graph (DAG) is constructed from the chip's characterisation
data. Edges from the source to candidate input ports are weighted by the
combined input transmission loss of those modes; intermediate edges are weighted
by the bar or cross fidelity of the MZIs the photons traverse during routing;
and final edges to the sink are weighted by the output transmission loss of each
candidate output window. All weights are expressed as $-\log(\text{fidelity})$,
so finding the shortest path through the DAG is equivalent to finding the
routing that maximises the product of all transmission and routing fidelities
along the photon's path. Once the optimal mode window is identified, a
gradient-based optimiser (Adam) tunes the phase-shifter parameters to compile
the target unitary into that subspace. The result is a set of phases for the
phase shifters of the chip that implement the desired target unitary and the
routing.

:::{note}
The photonic subcircuit compiler requires the optional `photonics` dependency
group, which includes `torch`, `perceval-quandela`, `numpy`, and `pandas`.
Install it with:

```console
pip install "mqt.qmap[photonics]"
```

:::

## Hardware Model

The chip is a staggered MZI mesh with `chip_dim` spatial modes and `chip_dim`
MZI layers. Layers alternate between _full_ layers (MZIs coupling modes 0–1,
2–3, …) and _half_ layers (MZIs coupling modes 1–2, 3–4, …). Each MZI consists
of:

- an input beam splitter with reflectivity $r_\text{in}$,
- a phase shifter on each of the two coupled modes,
- an output beam splitter with reflectivity $r_\text{out}$.

The physical imperfections of the chip are captured by three arrays:

- **`beam_splitter_reflectivities`** — a 1D array of length `2 * total_mzis`,
  ordered MZI-by-MZI as `[r_in^0, r_out^0, r_in^1, r_out^1, …]`. Ideal chips
  have all values equal to 0.5.
- **`input_transmissions`** — per-mode amplitude transmission coefficients at
  the chip input (fibres, gratings, waveguide tapers). Values in `[0, 1]`,
  normalised so the best mode has coefficient 1.
- **`output_transmissions`** — same for the chip output.

A target unitary of dimension `target_dim` is compiled into a sub-block of the
`chip_dim`-mode chip. Routing selects which `target_dim` modes to use; the
remaining modes act as a waveguide network that steers the photons into the
selected zone.

## Example: Compiling a 4×4 Unitary onto an 8-Mode Chip

### Providing the chip characterisation and target unitary

On real hardware the beam-splitter reflectivities and the per-mode input/output
transmissions are **properties of the fabricated chip**, obtained
from a calibration measurements; the target unitary is the gate you want to run.
In practice you would load them from your own files, for example:

```python
import numpy as np
import torch

# Measured device characterisation:
input_transmissions = np.loadtxt("input_transmissions.txt")       # shape (chip_dim,)
output_transmissions = np.loadtxt("output_transmissions.txt")     # shape (chip_dim,)
beam_splitter_reflectivities = np.loadtxt("beam_splitters.txt")   # shape (2 * total_mzis,)

# The gate to compile, as a complex (target_dim, target_dim) unitary:
target_unitary = torch.as_tensor(np.load("target_unitary.npy"), dtype=torch.complex128)
```

Note the expected formats: `beam_splitter_reflectivities` is a
**flat 1-D array**, not a matrix — one entry per beam splitter, ordered
MZI-by-MZI as `[MZI0_in, MZI0_out, MZI1_in, MZI1_out, …]` (an 8-mode chip has 28
MZIs → 56 values). `input_transmissions` and `output_transmissions` are 1-D
arrays of length `chip_dim`. The target unitary must be a `torch.Tensor` with a
complex dtype; wrap a NumPy array with
`torch.as_tensor(array, dtype=torch.complex128)`.

For this example we have no physical device, so we synthesise representative
random data of the same shapes instead:

```{code-cell} ipython3
import numpy as np
import torch
from mqt.qmap.ph.graph import generate_beam_splitter_matrix
from mqt.qmap.ph.unitary_to_phase_compilation import get_haar_random_unitary

chip_dim = 8
target_dim = 4

# Placeholder for beam-splitter reflectivities (statistically distributed around 0.5).
beam_splitter_reflectivities = generate_beam_splitter_matrix(
    chip_size=chip_dim, ideal_bs=False, rng=np.random.default_rng(42)
)

# Placeholder for transmissions, normalised so the best mode is 1.0.
hw_rng = np.random.default_rng(9)
input_transmissions = hw_rng.uniform(0.7, 1.0, size=chip_dim)
input_transmissions /= input_transmissions.max()
output_transmissions = hw_rng.uniform(0.7, 1.0, size=chip_dim)
output_transmissions /= output_transmissions.max()

# Placeholder for desired unitary: a Haar-random 4x4 unitary.
target_unitary = get_haar_random_unitary(
    target_dim, torch.Generator().manual_seed(10), dtype=torch.complex128
)

print("beam_splitter_reflectivities:", beam_splitter_reflectivities.shape, "(flat, 2 * total_mzis)")
print("input_transmissions :", np.round(input_transmissions, 3))
print("output_transmissions:", np.round(output_transmissions, 3))
```

### Compile the subcircuit

`compile_subcircuit` runs the routing search and the phase-shifter optimisation
and returns a {py:class}`~mqt.qmap.ph.subcircuit_compilation.CompilationResult`.
It needs only the chip characterisation and the target unitary.

```{code-cell} ipython3
from mqt.qmap.ph.subcircuit_compilation import OptimizationConfig, compile_subcircuit

torch.manual_seed(0)  # only seeds the phase-shifter initialisation
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
print("Phase-shifter settings (shape):", tuple(result.phases.shape))  # (chip_dim, chip_dim)
print("Inject photons in input state :", result.input_ports)          # length chip_dim (dual-rail)
print("Detect photons at output modes:", result.output_ports)         # target_dim modes
print(f"Final fidelity loss           : {result.loss:.2e}")
```

`result.phases` is a `(chip_dim, chip_dim)` tensor of phase-shifter angles (rows
are spatial modes, columns are MZI layers) — the values you program onto the
chip. `result.input_ports` tells you which modes to inject photons into and
`result.output_ports` which modes to read out. The router chose these to
minimise photon loss.

### Evaluate in simulation (optional)

For chips small enough to simulate classically, you can benchmark the compiled
result against the fixed-placement baseline with
{py:func}`~mqt.qmap.ph.subcircuit_compilation.evaluate_subcircuit`. This runs a
full Perceval simulation — including optional phase noise and the transmission
losses — and reports the coincidence rate and Total Variation Distance for both
strategies. It is a benchmarking tool, not part of driving real hardware.

```{code-cell} ipython3
from mqt.qmap.ph.baseline import embed_target_unitary_into_chip
from mqt.qmap.ph.subcircuit_compilation import evaluate_subcircuit

# The baseline optimiser compares against the target embedded in a chip-sized identity.
target_unitary_embedded = embed_target_unitary_into_chip(
    target_unitary.numpy(), chip_dim=chip_dim, target_dim=target_dim
)

run = evaluate_subcircuit(
    result,
    beam_splitter_reflectivities=beam_splitter_reflectivities,
    input_transmissions=input_transmissions,
    output_transmissions=output_transmissions,
    target_unitary=target_unitary,
    target_unitary_embedded=target_unitary_embedded,
    phase_error=0.01,     # std-dev of Gaussian phase noise added in the simulation
    config=config,
    phase_noise_seed=0,   # fix the noise draw for a reproducible number
)

print(f"Proposed  coincidence rate: {run.performance['coincidence_rate']:.3f}  TVD: {run.performance['tvd']:.4f}")
print(f"Baseline  coincidence rate: {run.baseline_performance['coincidence_rate']:.3f}  TVD: {run.baseline_performance['tvd']:.4f}")
```

Because the routed placement uses better-coupled modes than the fixed
first-`target_dim` window, the proposed compiler reaches a higher coincidence
rate than the baseline here.

## The compilation result

{py:class}`~mqt.qmap.ph.subcircuit_compilation.CompilationResult` bundles:

| Field | Meaning |
| --- | --- |
| `phases` | `(chip_dim, chip_dim)` tensor of phase-shifter angles to program (rows = modes, columns = MZI layers) |
| `input_ports` | length-`chip_dim` binary vector marking the modes to inject photons into (dual-rail) |
| `output_ports` | the `target_dim` physical modes of the computation zone, where the output is measured |
| `loss` | final fidelity loss of the optimisation (see below) |
| `compute_time` | wall-clock seconds for routing + optimisation |

### Fidelity loss

The fidelity loss is the optimiser's objective:

$$\mathcal{L} = 1 - \frac{|\operatorname{Tr}(U_\text{target}^\dagger \, U_\text{chip})|^2}{N^2}$$

where $N$ is the number of compared columns. A loss near zero means the chip's
effective unitary closely matches the target in the routed subspace. It is a
noise-free quantity computed directly from the phase-shifter parameters.

## Simulation metrics

When you benchmark a compiled result with `evaluate_subcircuit`, two figures of
merit are reported — for both the proposed compiler and the baseline.

### Coincidence rate

The coincidence rate is the probability that all `target_dim // 2` photons
arrive simultaneously at the correct set of output modes, as detected after the
chip's output coupling. It is the primary figure of merit for a photonic quantum
gate:

$$\text{CR} = P(\text{all photons detected in target output modes})$$

It rises with higher, more uniform input and output transmissions, which is
exactly why routing the photons to the best-coupled modes improves it — and why
the proposed compiler outperforms the fixed-placement baseline whenever the
transmissions vary across modes.

### Total Variation Distance (TVD)

The TVD quantifies how closely the measured coincidence output distribution
matches the ideal distribution produced by a perfect version of the target
unitary:

$$\text{TVD} = \frac{1}{2} \sum_s \lvert p_\text{chip}(s) - p_\text{ideal}(s) \rvert$$

A TVD of 0 indicates a perfect match; 1 indicates completely disjoint
distributions. In the presence of phase noise and beam-splitter imperfections,
the TVD is a sensitive indicator of compilation quality.

## Routing vs. Baseline

The baseline strategy always injects photons into the first `target_dim` modes
and optimises phase-shifter parameters for that fixed placement. No routing
graph is consulted, which is equivalent to ignoring the hardware's spatial
variation in transmission quality.

The proposed compiler instead builds a weighted layered graph where each edge
cost combines the MZI routing fidelity (bar or cross operation) with the
transmission coefficients of the modes involved, and finds the minimum-cost path
through this DAG with a shortest-path sweep. The path defines a _movement mask_:
a `(chip_dim, chip_dim)` integer tensor that labels each MZI cell as either a
routing element (fixed bar or cross state) or a computation element (free phase
parameter). The optimiser then tunes only the computation-zone parameters, while
the routing zone is held at its optimal fixed state.

When all transmissions are equal (lossless, perfectly uniform chip), both
strategies are equivalent and the routing step adds no benefit. The advantage
grows as the spread of transmission coefficients increases.

## Configuration

The optimisation behaviour is controlled by
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

| Parameter | Default | Effect |
| --- | --- | --- |
| `lr` | `0.05` | Initial Adam learning rate; a scheduler halves it on plateau |
| `threshold` | `1e-6` | Early exit once fidelity loss falls below this value |
| `max_iterations` | `10000` | Maximum gradient steps regardless of convergence |
| `exclude_edge_phase_shifters` | `False` | Drop the phase shifters at the two chip corners (reduces parameter count by 2) |
| `optimize_routing_parameters` | `True` | Give each routing MZI one free parameter to compensate small reflectivity errors |

In practice, 300–500 iterations are sufficient for `chip_dim = 8` and
`target_dim = 4` with a good initial learning rate. For larger chips or noisier
hardware, increasing `max_iterations` and reducing `lr` can improve convergence.

## Batch Data Collection

A single compile-and-evaluate run reflects one hardware realisation and one
target unitary. To characterise the compiler's _average_ advantage — and to
reproduce the results reported in the accompanying paper — a batch sweep over
many hardware configurations, target unitaries, and phase-error levels is
provided as a standalone script under
[`eval/ph/`](https://github.com/munich-quantum-toolkit/qmap/tree/main/eval/ph),
alongside the `subcircuit_compilation_data_collection.ipynb` notebook that
drives it. It is kept out of the installed package because it depends on the
exact per-mode transmission files used for the submission
(`eval/ph/hardware_data/`).

Its `collect_pipeline_results` function sweeps all combinations and returns a
consolidated {py:class}`pandas.DataFrame` with one row per
`(num_modes, target_dim, phase_error)` group, averaged over all unitaries and
repeats:

```python
from data_collection import Setup, collect_pipeline_results  # local module in eval/ph/

df = collect_pipeline_results(
    setups=[Setup(num_modes=24, target_dim=4), Setup(num_modes=48, target_dim=4)],
    config=OptimizationConfig(max_iterations=1000),
    num_unitaries_per_setup=4,
    repeats_per_unitary=1,
    phase_errors=[0.0, 0.015, 0.03],
    input_losses=True,   # loads eval/ph/hardware_data/{num_modes}_mode_input_transmissions.txt
    output_losses=True,  # loads the matching output-transmission file
    ideal_beam_splitters=False,
)
```

The `coincidence_rate_difference` column of the result directly quantifies the
routing advantage over the baseline for each configuration.
