# Photonic Subcircuit Compiler — Paper Evaluation

This directory holds the **evaluation framework** for the photonic subcircuit
compiler (`mqt.qmap.ph`). It reproduces the benchmark from the paper: it invents
synthetic hardware, runs the compiler against a fixed dual-rail *baseline*
strategy, simulates both on the [Perceval] photonic simulator, and compares
their output distributions (coincidence rate and total-variation distance).

It is **not part of the installed package.** `pip install mqt-qmap[photonics]`
ships only the compiler (which needs just `numpy` and `torch`); the evaluation's
heavier dependencies — Perceval and pandas — are kept out of the wheel and are
installed separately (see [Setup](#setup)).

## What lives where

| Path                                           | Purpose                                                                   |
| ---------------------------------------------- | ------------------------------------------------------------------------- |
| `hardware_model.py`                            | Synthetic beam-splitter reflectivities (`generate_beam_splitter_matrix`). |
| `random_unitary.py`                            | Haar-random target-unitary generation (`get_haar_random_unitary`).        |
| `baseline.py`                                  | Fixed dual-rail baseline strategy the compiler is compared against.       |
| `perceval_simulation.py`                       | Perceval chip construction, lossy simulation, and performance metrics.    |
| `evaluation.py`                                | `evaluate_subcircuit` — runs compiler vs. baseline and returns metrics.   |
| `data_collection.py`                           | Batch parameter sweeps and result aggregation into a pandas `DataFrame`.  |
| `hardware_data/`                               | Measured per-mode transmission coefficients used for the paper results.   |
| `results/`                                     | Output tables/artifacts from evaluation runs.                             |
| `subcircuit_compilation_data_collection.ipynb` | Notebook that reproduces the paper's data collection and plots.           |
| `tests/`                                       | Tests for the evaluation code (run via `nox -s evaluation`).              |

### Relationship to the compiler

The dependency boundary is one-way: the code here imports from `mqt.qmap.ph`,
never the reverse. The compiler takes *measured* hardware parameters plus a
target unitary and returns the phases to program the chip; everything
simulation- and metric-specific lives here.

`eval/ph` is a scripts directory, not an importable package, so its modules
import each other **by bare name** (e.g.
`from evaluation import evaluate_subcircuit`). Run scripts and the notebook from
inside `eval/ph` so these imports resolve, or add `eval/ph` to `sys.path` (the
tests' `conftest.py` does this automatically).

## Setup

The evaluation dependencies are declared as the `evaluation`
[dependency group](https://peps.python.org/pep-0735/) in the project's
`pyproject.toml` (`numpy`, `pandas`, `perceval-quandela`, `torch`). Install them
— together with a build of the compiler — with:

```console
uv sync --group evaluation
```

## Running the evaluation tests

The evaluation tests are intentionally **not** part of the default CI run (they
require Perceval and the Perceval simulations are slow). Run them on demand:

```console
nox -s evaluation
```

This builds the compiler, installs the `evaluation` group, and runs
`pytest eval/ph/tests`. Equivalently, after `uv sync --group evaluation`:

```console
pytest eval/ph/tests
```

## Reproducing the paper results

The full data collection is driven by
`subcircuit_compilation_data_collection.ipynb`. To reproduce a sweep
programmatically, run this from within `eval/ph` (so the bare-name imports
resolve):

```python
from data_collection import build_valid_setups, collect_pipeline_results, export_results_table
from mqt.qmap.ph.subcircuit_compilation import OptimizationConfig

# (num_modes, target_dim) pairs to evaluate; invalid combinations are dropped.
setups = build_valid_setups(num_modes_list=[8, 16], target_dims_list=[2, 4])

df = collect_pipeline_results(
    setups=setups,
    config=OptimizationConfig(),
    num_unitaries_per_setup=10,
    repeats_per_unitary=3,
    phase_errors=[0.0, 0.015, 0.030],
    consider_input_losses=True,
    consider_output_losses=True,
)

export_results_table(df, "results/evaluation.csv")
```

Each row aggregates, per `(num_modes, target_dim, phase_error)`, the mean TVD,
coincidence rate, and compute times for both the proposed compiler and the
baseline, plus their signed differences. The measured transmission coefficients
under `hardware_data/` are used when `consider_input_losses` /
`consider_output_losses` are enabled.

[Perceval]: https://perceval.quandela.net
