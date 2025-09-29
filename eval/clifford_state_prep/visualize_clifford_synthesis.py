# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
# Copyright (c) 2025 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""Visualization of Clifford synthesis experiments.

Usage:
    uv run python eval/visualize_clifford_synthesis.py

Outputs:
    paper/figures/
        runtime_vs_gates.pdf/svg/png                # Runtime comparison (faceted by qubits) with shared top legend
        gates_vs_gates.pdf/svg/png                  # Joint gates-vs-gates (faceted by qubits) with shared top legend
        gate_improvement_boxplot.pdf/svg/png        # QMAP vs best baseline (relative gate-count reduction)
"""

from __future__ import annotations

import contextlib
import json
from pathlib import Path

import matplotlib as mpl
import matplotlib.pyplot as plt
import pandas as pd
import seaborn as sns

# Try to enable SciencePlots for journal-quality styling.
# This gracefully falls back to a sensible seaborn style if unavailable.
try:
    import scienceplots  # noqa: F401

    HAVE_SCIENCEPLOTS = True
except Exception:
    HAVE_SCIENCEPLOTS = False

EVAL_DIR = Path(__file__).resolve().parent
RESULTS_JSON = EVAL_DIR / "synthesis_results.json"
FIG_DIR = EVAL_DIR / "figures"
FIG_DIR.mkdir(parents=True, exist_ok=True)

METHOD_LABELS = {
    "ag": "AG (baseline)",
    "greedy": "Greedy (baseline)",
    "qmap_cliff_iter": "QMAP-Iter (Tableau)",
    "qmap_cliff_maxsat": "QMAP-MaxSAT (Tableau)",
    "qmap_circ_iter": "QMAP-Iter (Circuit)",
    "qmap_circ_maxsat": "QMAP-MaxSAT (Circuit)",
}

RUNTIME_COLS = {
    "ag_runtime": "ag",
    "greedy_runtime": "greedy",
    "qmap_cliff_iter_runtime": "qmap_cliff_iter",
    "qmap_cliff_maxsat_runtime": "qmap_cliff_maxsat",
    "qmap_circ_iter_runtime": "qmap_circ_iter",
    "qmap_circ_maxsat_runtime": "qmap_circ_maxsat",
}

GATE_COLS = {
    "ag_gate_count": "ag",
    "greedy_gate_count": "greedy",
    "qmap_cliff_iter_gate_count": "qmap_cliff_iter",
    "qmap_cliff_maxsat_gate_count": "qmap_cliff_maxsat",
    "qmap_circ_iter_gate_count": "qmap_circ_iter",
    "qmap_circ_maxsat_gate_count": "qmap_circ_maxsat",
}


def _set_style() -> None:
    # Use a style that looks at home in LaTeX-based journals and assume LaTeX is available.
    if HAVE_SCIENCEPLOTS:
        plt.style.use(["science", "ieee"])  # IEEE-friendly science plots
        mpl.rcParams.update({
            "text.usetex": True,
            "font.family": "serif",
            "font.serif": ["Times", "Times New Roman", "Computer Modern Roman", "CMU Serif", "DejaVu Serif"],
            "axes.titlesize": 8,
            "axes.labelsize": 8,
            "font.size": 8,
            "xtick.labelsize": 7,
            "ytick.labelsize": 7,
            "legend.fontsize": 7,
            "figure.dpi": 300,
            "savefig.dpi": 300,
            "figure.constrained_layout.use": True,
        })
        # Optional: provide a LaTeX preamble commonly used with scienceplots
        mpl.rcParams.setdefault("text.latex.preamble", r"\usepackage{siunitx}\sisetup{detect-all}")
    else:
        sns.set_theme(context="paper", style="whitegrid")
        mpl.rcParams.update({
            "font.family": "serif",
            "font.serif": ["CMU Serif", "Times New Roman", "Times", "DejaVu Serif"],
            "axes.titlesize": 10,
            "axes.labelsize": 9,
            "xtick.labelsize": 8,
            "ytick.labelsize": 8,
            "legend.fontsize": 8,
            "figure.dpi": 200,
        })


def _read_results() -> pd.DataFrame:
    if RESULTS_JSON.exists():
        with Path(RESULTS_JSON).open(encoding="utf-8") as f:
            data = json.load(f)
        return pd.DataFrame(data)
    msg = (
        f"Results file not found: {RESULTS_JSON}. Please run the evaluation and export to synthesis_results.json first."
    )
    raise FileNotFoundError(msg)


def _long_format(df: pd.DataFrame) -> tuple[pd.DataFrame, pd.DataFrame]:
    # Melt runtimes
    rt_long = (
        df.melt(
            id_vars=["num_qubits", "num_gates", "instance"],
            value_vars=list(RUNTIME_COLS.keys()),
            var_name="metric",
            value_name="runtime_s",
        )
        .dropna(subset=["runtime_s"])  # allow partial results
        .assign(method=lambda d: d["metric"].map(RUNTIME_COLS))
        .drop(columns=["metric"])
    )

    # Melt gate counts
    gc_long = (
        df.melt(
            id_vars=["num_qubits", "num_gates", "instance"],
            value_vars=list(GATE_COLS.keys()),
            var_name="metric",
            value_name="gate_count",
        )
        .dropna(subset=["gate_count"])  # allow partial results
        .assign(method=lambda d: d["metric"].map(GATE_COLS))
        .drop(columns=["metric"])
    )

    # Attach nice labels
    rt_long["method_label"] = rt_long["method"].map(METHOD_LABELS)
    gc_long["method_label"] = gc_long["method"].map(METHOD_LABELS)
    return rt_long, gc_long


def _savefig(fig: mpl.figure.Figure, name: str) -> None:
    exts = ["pdf", "svg", "png"]
    try:
        if mpl.rcParams.get("text.usetex", False):
            exts.append("pgf")  # convenient for direct LaTeX inclusion via \input{}
    except Exception:  # noqa: S110
        pass
    for ext in exts:
        fig.savefig(FIG_DIR / f"{name}.{ext}")  # , bbox_inches="tight")


def fig_runtime_vs_gates(rt_long: pd.DataFrame) -> None:
    """Create a runtime-vs-gates figure (faceted by qubit count) with AG, Greedy, and aggregated QMAP."""
    _set_style()
    hue_order = [
        METHOD_LABELS["ag"],
        METHOD_LABELS["greedy"],
        METHOD_LABELS["qmap_cliff_iter"],
        METHOD_LABELS["qmap_cliff_maxsat"],
        METHOD_LABELS["qmap_circ_iter"],
        METHOD_LABELS["qmap_circ_maxsat"],
    ]
    g = sns.relplot(
        data=rt_long,
        x="num_gates",
        y="runtime_s",
        hue="method_label",
        hue_order=hue_order,
        col="num_qubits",
        kind="line",
        estimator="mean",
        errorbar=("ci", 95),
        palette="deep",
        facet_kws={"sharey": False, "sharex": False},
        markers=False,
        dashes=False,
    )

    # Ensure 3 mm horizontal spacing between subplots (constrained layout)
    mm_in = 3.0 / 25.4
    with contextlib.suppress(Exception):
        g.figure.set_layout_engine("constrained")
    try:
        g.figure.set_constrained_layout_pads(w_pad=mm_in, wspace=0.02)
    except Exception:
        try:
            le = g.figure.get_layout_engine()
            if le and hasattr(le, "set"):
                le.set(w_pad=mm_in, wspace=0.02)
        except Exception:  # noqa: S110
            pass

    # Axis labels close to axes (no figure-level labels)
    g.set_ylabels("Runtime [s]")
    g.set_xlabels("")  # remove per-facet x labels
    try:
        g.figure.supxlabel(r"$|G|$ in target Clifford")
    except Exception:
        g.figure.supxlabel("|G| in target Clifford")

    g.set_titles("{col_name} qubits")
    sns.move_legend(
        g,
        "upper center",
        bbox_to_anchor=(0.5, 1.15),
        ncol=len(hue_order),
        title="Method",
        frameon=False,
    )

    for ax in g.axes.flat:
        ax.grid(True, which="both", axis="both", alpha=0.2)
        ax.xaxis.labelpad = 2
        ax.yaxis.labelpad = 2

    g.figure.set_size_inches(9, 2.5)
    _savefig(g.figure, "runtime_vs_gates")


def fig_gates_vs_gates(gc_long: pd.DataFrame) -> None:
    """Create a joint gates-vs-gates figure (faceted by qubit count) with AG, Greedy, and aggregated QMAP.

    We aggregate all QMAP variants (which share the same gate count by design) into a single
    series labeled "QMAP (proposed)" to avoid redundant lines.
    """
    _set_style()

    # Build aggregated DataFrame with methods: ag, greedy, qmap (aggregated over all qmap_* variants)
    ag_gc = gc_long[gc_long["method"] == "ag"][["num_qubits", "num_gates", "instance", "gate_count"]].copy()
    ag_gc["method"] = "ag"
    ag_gc["method_label"] = METHOD_LABELS["ag"]

    greedy_gc = gc_long[gc_long["method"] == "greedy"][["num_qubits", "num_gates", "instance", "gate_count"]].copy()
    greedy_gc["method"] = "greedy"
    greedy_gc["method_label"] = METHOD_LABELS["greedy"]

    qmap_mask = gc_long["method"].str.startswith("qmap_")
    qmap_gc = gc_long[qmap_mask].groupby(["num_qubits", "num_gates", "instance"], as_index=False)["gate_count"].mean()
    qmap_gc["method"] = "qmap"
    qmap_gc["method_label"] = "QMAP (proposed)"

    df_plot = pd.concat([ag_gc, greedy_gc, qmap_gc], ignore_index=True)

    hue_order = [METHOD_LABELS["ag"], METHOD_LABELS["greedy"], "QMAP (proposed)"]

    # Joint faceted figure with shared legend at the top and a single x-axis label
    g = sns.relplot(
        data=df_plot,
        x="num_gates",
        y="gate_count",
        hue="method_label",
        hue_order=hue_order,
        col="num_qubits",
        kind="line",
        estimator="mean",
        errorbar=("ci", 95),
        palette="deep",
        facet_kws={"sharey": True, "sharex": False},
        markers=False,
        dashes=False,
    )

    # Ensure 3 mm horizontal spacing between subplots (constrained layout)
    mm_in = 3.0 / 25.4
    with contextlib.suppress(Exception):
        g.figure.set_layout_engine("constrained")
    try:
        g.figure.set_constrained_layout_pads(w_pad=mm_in, wspace=0.02)
    except Exception:
        try:
            le = g.figure.get_layout_engine()
            if le and hasattr(le, "set"):
                le.set(w_pad=mm_in, wspace=0.02)
        except Exception:  # noqa: S110
            pass

    g.set_ylabels("")
    with contextlib.suppress(Exception):
        g.figure.supylabel("Resulting total gate count")
    g.set_xlabels("")  # remove per-facet x labels
    try:
        g.figure.supxlabel(r"$|G|$ in target Clifford")
    except Exception:
        g.figure.supxlabel("|G| in target Clifford")
    g.set_titles("{col_name} qubits")

    sns.move_legend(
        g,
        "upper center",
        bbox_to_anchor=(0.5, 1.15),
        ncol=len(hue_order),
        title="Method",
        frameon=False,
    )

    for ax in g.axes.flat:
        ax.grid(True, which="both", axis="both", alpha=0.2)
        ax.xaxis.labelpad = 2
        ax.yaxis.labelpad = 2

    g.figure.set_size_inches(9, 2.5)
    _savefig(g.figure, "gates_vs_gates")


def fig_relative_reduction(gc_long: pd.DataFrame) -> None:
    """Overview boxplot: QMAP gate-count improvement vs. best baseline per number of qubits.

    Aggregates all QMAP variants into one proposed method before computing the relative
    reduction vs. the best baseline (AG/Greedy) on each instance.
    """
    _set_style()
    # Best baseline per instance
    base = (
        gc_long[gc_long["method"].isin(["ag", "greedy"])][
            ["num_qubits", "num_gates", "instance", "method", "gate_count"]
        ]
        .pivot_table(index=["num_qubits", "num_gates", "instance"], columns="method", values="gate_count")
        .rename_axis(None, axis=1)
        .reset_index()
    )
    if not {"ag", "greedy"}.intersection(base.columns):
        return
    base["best_baseline"] = base[[c for c in ["ag", "greedy"] if c in base.columns]].min(axis=1)

    # Aggregate QMAP variants to a single value per instance
    qmap_gc = (
        gc_long[gc_long["method"].str.startswith("qmap_")]
        .groupby(["num_qubits", "num_gates", "instance"], as_index=False)["gate_count"]
        .mean()
        .rename(columns={"gate_count": "qmap_gate_count"})
    )

    merged = base.merge(qmap_gc, on=["num_qubits", "num_gates", "instance"], how="inner")
    if merged.empty:
        return
    merged = merged.assign(
        rel_reduction=lambda d: 100 * (d["best_baseline"] - d["qmap_gate_count"]) / d["best_baseline"]
    )

    # Clip negative improvements at 0 for visualization (do not modify underlying results)
    merged = merged.assign(rel_reduction_plot=lambda d: d["rel_reduction"].clip(lower=0))

    # Determine a y-limit that zooms into the main data region
    try:
        p99 = float(merged["rel_reduction_plot"].quantile(0.99))
        y_max = max(5.0, p99 * 1.05)
    except Exception:
        y_max = None

    plt.figure(figsize=(3.3, 2.5))  # single-column friendly
    ax = sns.boxplot(
        data=merged,
        x="num_qubits",
        y="rel_reduction_plot",
        color=sns.color_palette("deep")[2],
        showfliers=False,
    )
    sns.stripplot(
        data=merged.sample(min(len(merged), 800), random_state=1),
        x="num_qubits",
        y="rel_reduction_plot",
        color="k",
        alpha=0.15,
        size=1.5,
        jitter=0.15,
        legend=False,
    )
    ax.set_xlabel("Number of qubits")
    ax.set_ylabel(r"Rel. reduction vs. best baseline [\%]")
    if y_max is not None:
        ax.set_ylim(0, y_max)
    ax.grid(True, axis="y", alpha=0.2)
    plt.tight_layout()
    _savefig(ax.figure, "gate_improvement_boxplot")


def main() -> None:
    """Main entry point for the script."""
    # Load data
    df = _read_results()

    # Prepare long-format DataFrames
    rt_long, gc_long = _long_format(df)

    # Generate figures
    fig_runtime_vs_gates(rt_long)
    fig_gates_vs_gates(gc_long)
    # Only create relative reduction if baselines exist
    if set(gc_long["method"]).intersection({"ag", "greedy"}):
        fig_relative_reduction(gc_long)


if __name__ == "__main__":
    main()
