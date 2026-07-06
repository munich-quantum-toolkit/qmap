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
#   "mqt.qmap==3.5.0",
#   "qiskit==2.4.2",
# ]
# [tool.uv]
# exclude-newer = "2025-12-16T12:59:59Z"
# ///

"""Script for evaluating the routing-aware zoned neutral atom compiler.

In particular, it compares the iterative diving search method against
the A* search method. Additionally, it evaluates the impact of relaxed
routing.
"""

from __future__ import annotations

import json
import os
import pathlib

from eval_framework import BenchmarkLevel, Evaluator, benchmarks, process_benchmark

from mqt.qmap.na.zoned import PlacementMethod, RoutingAwareCompiler, RoutingMethod, ZonedNeutralAtomArchitecture


def main() -> None:
    """Main function for evaluating the fast relaxed compiler."""
    # set the working directory to the script location
    os.chdir(pathlib.Path(pathlib.Path(__file__).resolve()).parent)
    print("\033[32m[INFO]\033[0m Reading in architecture...")
    with pathlib.Path("square_architecture.json").open(encoding="utf-8") as f:
        arch_dict = json.load(f)
    arch = ZonedNeutralAtomArchitecture.from_json_file("square_architecture.json")
    arch.to_namachine_file("arch.namachine")
    print("\033[32m[INFO]\033[0m Done")
    astar_compiler = RoutingAwareCompiler(
        arch,
        log_level="error",
        max_filling_factor=0.9,
        use_window=True,
        window_min_width=16,
        window_ratio=1.0,
        window_share=0.8,
        placement_method=PlacementMethod.astar,
        deepening_factor=0.9,
        deepening_value=8.0,
        lookahead_factor=0.2,
        reuse_level=5.0,
        max_nodes=int(1e7),
        routing_method=RoutingMethod.strict,
        warn_unsupported_gates=False,
    )
    common_ids_config = {
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
        "warn_unsupported_gates": False,
    }
    ids_compiler = RoutingAwareCompiler(arch, **common_ids_config, routing_method=RoutingMethod.strict)
    relaxed_compiler = RoutingAwareCompiler(
        arch, **common_ids_config, routing_method=RoutingMethod.relaxed, prefer_split=1.0
    )

    evaluator = Evaluator(arch_dict, "results.csv")
    evaluator.print_header()
    pathlib.Path("in").mkdir(exist_ok=True)

    benchmark_list = [
        ("graphstate", (BenchmarkLevel.INDEP, [60, 80, 100, 120, 140, 160, 180, 200, 500, 1000, 2000, 5000])),
        ("qft", (BenchmarkLevel.INDEP, [500, 1000])),
        ("qpeexact", (BenchmarkLevel.INDEP, [500, 1000])),
        ("wstate", (BenchmarkLevel.INDEP, [500, 1000])),
        ("qaoa", (BenchmarkLevel.INDEP, [50, 100, 150, 200])),
        ("vqe_two_local", (BenchmarkLevel.INDEP, [50, 100, 150, 200])),
    ]

    for benchmark, qc in benchmarks(benchmark_list):
        qc.qasm3(f"in/{benchmark}_n{qc.num_qubits}.qasm")
        process_benchmark(astar_compiler, "astar", qc, benchmark, evaluator)
        process_benchmark(ids_compiler, "ids", qc, benchmark, evaluator)
        process_benchmark(relaxed_compiler, "relaxed", qc, benchmark, evaluator)

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
