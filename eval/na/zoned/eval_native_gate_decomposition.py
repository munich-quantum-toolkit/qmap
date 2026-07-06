#!/usr/bin/env -S uv run --script --quiet
# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

# /// script
# dependencies = [
#   "mqt.bench==2.1.0",
#   "mqt.qmap @ git+https://github.com/cda-tum/mqt-qmap@e6048e0fce5ed10468dde74aa1571a274da5d8df",
#   "qiskit==2.4.2",
# ]
# [tool.uv]
# exclude-newer = "2026-07-06T23:59:59Z"
# ///

"""Script for evaluating the routing-aware native gate zoned neutral atom compiler.

In particular, it runs the native gate compiler to produce hardware compliant output.
It records central metrics of the compilation runs and the generated code. It compares
two different settings to evaluate the effectiveness of the theta optimization.
"""

from __future__ import annotations

import json
import os
import pathlib

from eval_framework import BenchmarkLevel, Evaluator, benchmarks, process_benchmark

from mqt.qmap.na.zoned import (
    PlacementMethod,
    RoutingAwareCompiler,
    RoutingAwareNativeGateCompiler,
    RoutingMethod,
    ZonedNeutralAtomArchitecture,
)


def main() -> None:
    """Main function for evaluating the native gate compiler."""
    # set the working directory to the script location
    os.chdir(pathlib.Path(pathlib.Path(__file__).resolve()).parent)
    print("\033[32m[INFO]\033[0m Reading in architecture...")
    with pathlib.Path("square_architecture.json").open(encoding="utf-8") as f:
        arch_dict = json.load(f)
    arch = ZonedNeutralAtomArchitecture.from_json_file("square_architecture.json")
    arch.to_namachine_file("arch.namachine")
    print("\033[32m[INFO]\033[0m Done")
    common_config = {
        "log_level": "error",
        "max_filling_factor": 0.9,
        "use_window": True,
        "window_min_width": 16,
        "window_ratio": 1.0,
        "window_share": 0.8,
        "placement_method": PlacementMethod.ids,
        "deepening_factor": 0.01,
        "deepening_value": 0.0,
        "lookahead_factor": 0.4,
        "reuse_level": 5.0,
        "trials": 4,
        "queue_capacity": 100,
        "routing_method": RoutingMethod.relaxed,
        "prefer_split": 1.0,
        "warn_unsupported_gates": False,
    }
    baseline = RoutingAwareCompiler(arch, **common_config)
    setting1 = RoutingAwareNativeGateCompiler(arch, **common_config)
    setting2 = RoutingAwareNativeGateCompiler(arch, **common_config, theta_opt_schedule=True)

    evaluator = Evaluator(arch_dict, "results.csv")
    evaluator.print_header()
    pathlib.Path("in").mkdir(exist_ok=True)

    benchmark_list = [
        ("graphstate", (BenchmarkLevel.INDEP, [20, 100])),
        ("qft", (BenchmarkLevel.INDEP, [20, 100])),
        ("qpeexact", (BenchmarkLevel.INDEP, [20, 100])),
        ("wstate", (BenchmarkLevel.INDEP, [20, 100])),
        ("qaoa", (BenchmarkLevel.INDEP, [20, 100])),
        ("vqe_two_local", (BenchmarkLevel.INDEP, [20, 100])),
    ]

    for benchmark, qc in benchmarks(benchmark_list):
        qc.qasm3(f"in/{benchmark}_n{qc.num_qubits}.qasm")
        process_benchmark(baseline, "baseline", qc, benchmark, evaluator)
        process_benchmark(setting1, "setting1", qc, benchmark, evaluator)
        process_benchmark(setting2, "setting2", qc, benchmark, evaluator)

    print(
        "\033[32m[INFO]\033[0m =============================================================\n"
        "\033[32m[INFO]\033[0m Now, \n"
        "\033[32m[INFO]\033[0m    - the results are located in `results.csv`,\n"
        "\033[32m[INFO]\033[0m    - the input circuits in the QASM format are located in\n"
        "\033[32m[INFO]\033[0m      the `in` directory,\n"
        "\033[32m[INFO]\033[0m    - the compiled circuits in the naviz format are located\n"
        "\033[32m[INFO]\033[0m      in the `out` directory separated for each compiler and\n"
        "\033[32m[INFO]\033[0m      setting, and\n"
        "\033[32m[INFO]\033[0m    - the architecture specification compatible with NAViz is\n"
        "\033[32m[INFO]\033[0m      located in `arch.namachine`\n"
        "\033[32m[INFO]\033[0m \n"
        "\033[32m[INFO]\033[0m The generated `.naviz` files can be animated with the\n"
        "\033[32m[INFO]\033[0m MQT NAViz tool."
    )


if __name__ == "__main__":
    main()
