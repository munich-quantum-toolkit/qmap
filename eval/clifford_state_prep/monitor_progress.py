# Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
# Copyright (c) 2025 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT

"""Terminal UI to continuously monitor progress of Clifford synthesis evaluations.

Similar to `htop`, this script refreshes a table that shows, for each
(num_qubits, num_gates) pair, how many instances have completed per method.

Usage:
python3 eval/monitor_progress.py           refresh every 2s (default)
python3 eval/monitor_progress.py --interval 5
python3 eval/monitor_progress.py --once    print table once and exit
python3 eval/monitor_progress.py --db /path/to/synthesis_results.db
"""

from __future__ import annotations

import argparse
import datetime as dt
import sys
import time
from pathlib import Path

from sqlalchemy import Column, Float, Integer, create_engine
from sqlalchemy.orm import Session, declarative_base

# Define a local SQLAlchemy mapping to avoid importing evaluation module
# (to prevent side effects such as creating new DBs on import)
Base = declarative_base()


class SynthesisResult(Base):  # type: ignore[misc]
    """Database model for synthesis results."""

    __tablename__ = "synthesis_results"

    id = Column(Integer, primary_key=True)
    num_qubits = Column(Integer)
    num_gates = Column(Integer)
    instance = Column(Integer)

    ag_runtime = Column(Float, nullable=True)
    ag_gate_count = Column(Integer, nullable=True)

    greedy_runtime = Column(Float, nullable=True)
    greedy_gate_count = Column(Integer, nullable=True)

    qmap_cliff_iter_runtime = Column(Float, nullable=True)
    qmap_cliff_iter_gate_count = Column(Integer, nullable=True)

    qmap_cliff_maxsat_runtime = Column(Float, nullable=True)
    qmap_cliff_maxsat_gate_count = Column(Integer, nullable=True)

    qmap_circ_iter_runtime = Column(Float, nullable=True)
    qmap_circ_iter_gate_count = Column(Integer, nullable=True)

    qmap_circ_maxsat_runtime = Column(Float, nullable=True)
    qmap_circ_maxsat_gate_count = Column(Integer, nullable=True)


METHODS: list[tuple[str, any]] = [
    ("ag", SynthesisResult.ag_runtime),
    ("greedy", SynthesisResult.greedy_runtime),
    ("clifford_iter", SynthesisResult.qmap_cliff_iter_runtime),
    ("clifford_maxsat", SynthesisResult.qmap_cliff_maxsat_runtime),
    ("circuit_iter", SynthesisResult.qmap_circ_iter_runtime),
    ("circuit_maxsat", SynthesisResult.qmap_circ_maxsat_runtime),
]


def build_progress_table(session: Session) -> str:
    """Build a table showing progress for each (qubits, gates) pair."""
    # Gather distinct (qubits, gates)
    pairs = session.query(SynthesisResult.num_qubits, SynthesisResult.num_gates).distinct().all()
    if not pairs:
        return "No circuits found in database. Waiting for data..."

    qubits_to_gates: dict[int, list[int]] = {}
    for q, g in pairs:
        qubits_to_gates.setdefault(int(q), set()).add(int(g))  # type: ignore[arg-type]
    for q in list(qubits_to_gates.keys()):
        qubits_to_gates[q] = sorted(qubits_to_gates[q])  # type: ignore[index]

    headers = ["Qubits", "Gates"] + [m[0] for m in METHODS]
    widths = [len(h) for h in headers]

    # Precompute totals and counts to determine widths
    precomputed: dict[tuple[int, int], dict[str, tuple[int, int]]] = {}
    method_names = [m[0] for m in METHODS]

    for q in sorted(qubits_to_gates.keys()):
        for g in qubits_to_gates[q]:
            total = session.query(SynthesisResult).filter_by(num_qubits=q, num_gates=g).count()
            cell_counts: dict[str, tuple[int, int]] = {}
            for name, col in METHODS:
                completed = (
                    session.query(SynthesisResult).filter_by(num_qubits=q, num_gates=g).filter(col.isnot(None)).count()
                )
                cell_counts[name] = (completed, total)
                widths_idx = 2 + method_names.index(name)
                widths[widths_idx] = max(widths[widths_idx], len(f"{completed}/{total}"))
            precomputed[q, g] = cell_counts
            widths[0] = max(widths[0], len(str(q)))
            widths[1] = max(widths[1], len(str(g)))

    # Compute global totals (per-method and overall) and update widths
    total_global = session.query(SynthesisResult).count()
    totals_per_method: dict[str, tuple[int, int]] = {}
    for name, col in METHODS:
        completed_global = session.query(SynthesisResult).filter(col.isnot(None)).count()
        totals_per_method[name] = (completed_global, total_global)
        idx = 2 + method_names.index(name)
        widths[idx] = max(widths[idx], len(f"{completed_global}/{total_global}"))
    widths[0] = max(widths[0], len("TOTAL"))

    def fmt_row(cells: list[str]) -> str:
        return " | ".join(c.ljust(w) for c, w in zip(cells, widths, strict=False))

    sep = "-+-".join("-" * w for w in widths)
    lines: list[str] = []
    lines.extend((fmt_row(headers), sep))
    for q in sorted(qubits_to_gates.keys()):
        first_row = True
        for g in qubits_to_gates[q]:
            row = [str(q) if first_row else "", str(g)]
            for name in method_names:
                c, t = precomputed[q, g][name]
                row.append(f"{c}/{t}")
            lines.append(fmt_row(row))
            first_row = False

    # Totals row per method
    lines.append(sep)
    total_row = ["TOTAL", ""]
    for name in method_names:
        c, t = totals_per_method[name]
        total_row.append(f"{c}/{t}")
    lines.append(fmt_row(total_row))

    return "\n".join(lines)


def clear_screen() -> None:
    """Clear the terminal screen."""
    # ANSI clear screen and move cursor to home
    sys.stdout.write("\x1b[2J\x1b[H")
    sys.stdout.flush()


def parse_args() -> argparse.Namespace:
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser(description="Monitor Clifford synthesis evaluation progress.")
    default_db = Path(__file__).resolve().parent / "synthesis_results.db"
    parser.add_argument(
        "--db",
        type=Path,
        default=default_db,
        help=f"Path to SQLite DB file (default: {default_db})",
    )
    parser.add_argument(
        "--interval",
        type=float,
        default=2.0,
        help="Refresh interval in seconds (default: 2.0)",
    )
    parser.add_argument(
        "--once",
        action="store_true",
        help="Print the table once and exit.",
    )
    return parser.parse_args()


def main() -> int:
    """Main entry point for the script."""
    args = parse_args()
    db_path: Path = args.db

    if args.once and not db_path.exists():
        print(f"Database not found: {db_path}")
        return 1

    try:
        while True:
            if not db_path.exists():
                clear_screen()
                print("Clifford Synthesis Progress Monitor")
                print(f"DB: {db_path}")
                print()
                print("Waiting for database to be created ...")
                time.sleep(max(args.interval, 0.5))
                continue

            # Use absolute path to avoid accidentally creating DB elsewhere
            engine = create_engine(f"sqlite:///{db_path}", echo=False)
            with Session(engine) as session:
                try:
                    table = build_progress_table(session)
                except Exception as e:
                    table = f"Error while reading progress: {e!r}"

            timestamp = dt.datetime.now().strftime("%Y-%m-%d %H:%M:%S")

            clear_screen()
            print("Clifford Synthesis Progress Monitor")
            print(f"DB: {db_path}")
            print(f"Updated: {timestamp}")
            print()
            print(table)
            print()
            print("Press Ctrl+C to quit. Use --interval to change refresh rate.")

            if args.once:
                return 0

            time.sleep(max(args.interval, 0.25))
    except KeyboardInterrupt:
        return 130


if __name__ == "__main__":
    raise SystemExit(main())
