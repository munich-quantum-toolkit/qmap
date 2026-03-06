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

from eval_helper import (
    Evaluator,
    benchmarks,
    benchmarks_from_file_name,
    process_benchmark,
)
from mqt.bench import BenchmarkLevel

from mqt.qmap.na.zoned import (
    PlacementAndRoutingAwareCompiler,
    PlacementMethod,
    RoutingAgnosticCompiler,
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
        "reuse_level": 30.0,  # 5.0,
        "trials": 4,
        "queue_capacity": 100,
        "routing_method": RoutingMethod.relaxed,
        "prefer_split": 1.0,
        "warn_unsupported_gates": False,
    }
    RoutingAgnosticCompiler(arch)
    asap_compiler = RoutingAwareCompiler(arch, **common_config)
    min_flow_compiler = PlacementAndRoutingAwareCompiler(arch, **common_config)

    evaluator = Evaluator(arch_dict, "results.csv")
    evaluator.print_header()
    pathlib.Path("in").mkdir(exist_ok=True)
    use_mqt_bnech = False

    if use_mqt_bnech:
        benchmark_list = [
            # ("graphstate", (BenchmarkLevel.INDEP, [10])),
            ("qaoa", (BenchmarkLevel.INDEP, [40, 50, 60, 70, 80, 90, 100, 150, 200])),
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
    else:
        benchmark_list = []
        # folder_path: pathlib.Path = pathlib.Path("rand_tt")
        folder_path: pathlib.Path = pathlib.Path("qlut-qasm")
        # folder_path: pathlib.Path = pathlib.Path("test")
        if pathlib.Path.is_dir(folder_path):
            # Find all .qasm files in the folder
            benchmark_list = sorted([str(f) for f in pathlib.Path.iterdir(folder_path) if f.suffix == ".qasm"])
        for benchmark, qc in benchmarks_from_file_name(benchmark_list):
            benchmark_str = benchmark.split("/")[-1].split(".")[0]
            qc.qasm3(f"in/{benchmark_str}.qasm")
            # process_benchmark(zac_compiler, "zac", qc, benchmark_str, evaluator)
            process_benchmark(asap_compiler, "asap", qc, benchmark_str, evaluator)
            process_benchmark(min_flow_compiler, "min_flow", qc, benchmark_str, evaluator)

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
