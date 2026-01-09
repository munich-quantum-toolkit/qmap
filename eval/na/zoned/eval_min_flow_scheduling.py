#!/usr/bin/env python
# Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""Script for evaluating the routing-aware zoned neutral atom compiler.

In particular, it compares the minimum cost maximum flow scheduler against
the as-soon-as-possible method.
"""

from __future__ import annotations

import json
import os
import pathlib

from eval_helper import Evaluator, benchmarks, process_benchmark
from mqt.bench import BenchmarkLevel

from mqt.qmap.na.zoned import (
    PlacementAndRoutingAwareCompiler,
    PlacementMethod,
    RoutingAwareCompiler,
    RoutingMethod,
    ZonedNeutralAtomArchitecture,
)


def main() -> None:
    """Main function for evaluating the fast relaxed compiler."""
    # set working directory to script location
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
    asap_compiler = RoutingAwareCompiler(arch, **common_config)
    min_flow_compiler = PlacementAndRoutingAwareCompiler(arch, **common_config)

    evaluator = Evaluator(arch_dict, "results.csv")
    evaluator.print_header()
    pathlib.Path("in").mkdir(exist_ok=True)

    benchmark_list = [
        ("graphstate", (BenchmarkLevel.INDEP, [10])),
    ]
    # benchmark_list = [
    #     ("graphstate", (BenchmarkLevel.INDEP, [60, 80, 100, 120, 140, 160, 180, 200, 500, 1000, 2000, 5000])),
    #     ("qft", (BenchmarkLevel.INDEP, [500, 1000])),
    #     ("qpeexact", (BenchmarkLevel.INDEP, [500, 1000])),
    #     ("wstate", (BenchmarkLevel.INDEP, [500, 1000])),
    #     ("qaoa", (BenchmarkLevel.INDEP, [50, 100, 150, 200])),
    #     ("vqe_two_local", (BenchmarkLevel.INDEP, [50, 100, 150, 200])),
    # ]

    for benchmark, qc in benchmarks(benchmark_list):
        qc.qasm3(f"in/{benchmark}_n{qc.num_qubits}.qasm")
        process_benchmark(asap_compiler, "asap", qc, benchmark, evaluator)
        process_benchmark(min_flow_compiler, "min_flow", qc, benchmark, evaluator)

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
