# MQT QMAP's Zoned Neutral Atom Compiler Evaluation

This document describes how to use the evaluation script for the zoned neutral atom compiler.
The script automates the process of running QMAP on a set of benchmark circuits and collecting performance metrics.

## Prerequisites

Before running the evaluation script, ensure you have the following installed:

- uv 0.9.13

## Usage

The main evaluation script is `evaluate_ids_relaxed_routing.py`. You can directly run it from the command line.

```bash
evaluate_ids_relaxed_routing.py
```

## Output

The script produces a CSV file named `results.csv` in the directory of the script.
Additionally, all input circuits are written to the `in` directory as QASM files.
Furthermore, the compiled files are written to the `out` directory ordered by compiler and settings.
