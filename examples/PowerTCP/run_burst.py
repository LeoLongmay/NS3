#!/usr/bin/env python3
"""Run the burst (incast) experiment for one or more CC algorithms.

Replaces: script-burst.sh + results-burst.sh

Usage examples:
    # Run LPCC with the current best profile:
    python3 run_burst.py --algs LPCC --profile v17

    # Quick tweak — override a single parameter:
    python3 run_burst.py --algs LPCC --profile v17 --override lpccThetaUs=1500

    # Run multiple algorithms:
    python3 run_burst.py --algs DCQCN HPCC LPCC

    # Skip compilation (binary already built):
    python3 run_burst.py --algs LPCC --skip-build
"""

import argparse
import re
from pathlib import Path

from algo_common import (
    ALGO_MATRIX,
    REPO_ROOT,
    SCRIPT_DIR,
    build_core_args,
    build_lpcc_args,
    ensure_build,
    resolve_algorithms,
    resolve_lpcc_params,
    run_cmd,
)

BUILD_TARGET = "examples/PowerTCP/powertcp-evaluation-burst"


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Run burst (incast) experiment")
    p.add_argument("--algs", nargs="+", default=None,
                   help="algorithms to run (default: all)")
    p.add_argument("--conf", default=str(SCRIPT_DIR / "config-burst.txt"))
    p.add_argument("--profile", default="v17",
                   help="LPCC parameter profile name (default: v17)")
    p.add_argument("--override", nargs="*", default=None,
                   help="override LPCC params, e.g. lpccThetaUs=1500")
    p.add_argument("--dump-dir", default=str(SCRIPT_DIR / "dump_burst"))
    p.add_argument("--results-dir", default=str(SCRIPT_DIR / "results_burst"))
    p.add_argument("--monitor-switch-id", type=int, default=72)
    p.add_argument("--monitor-throughput-bps", type=int, default=400_000_000_000)
    p.add_argument("--skip-build", action="store_true")
    p.add_argument("--skip-parse", action="store_true")
    return p.parse_args()


# ---------------------------------------------------------------------------
# Result parsing (replaces results-burst.sh)
# ---------------------------------------------------------------------------

def parse_burst_results(dump_file: Path, output_file: Path) -> None:
    """Extract monitored ToR/Port lines from a raw burst dump file."""
    text = dump_file.read_text(encoding="utf-8", errors="replace")
    lines = text.splitlines()

    # Find MONITOR_TARGET line to determine ToR index and port.
    monitor_tor = None
    monitor_port = None
    for line in lines:
        if line.startswith("MONITOR_TARGET"):
            parts = line.split()
            for i, tok in enumerate(parts):
                if tok == "ToRIdx" and i + 1 < len(parts):
                    monitor_tor = parts[i + 1]
                elif tok == "MonitorPort" and i + 1 < len(parts):
                    monitor_port = parts[i + 1]
            break

    # Build grep pattern.
    if monitor_tor and monitor_tor != "-1" and (not monitor_port or monitor_port == "-1"):
        prefix = f"ToR {monitor_tor} PortAgg -1 "
    elif monitor_tor and monitor_port and monitor_tor != "-1" and monitor_port != "-1":
        prefix = f"ToR {monitor_tor} Port {monitor_port} "
    else:
        print(f"  [warn] MONITOR_TARGET not found in {dump_file.name}, fallback ToR 0 Port 0")
        prefix = "ToR 0 Port 0 "

    matched = [l for l in lines if l.startswith(prefix)]
    output_file.parent.mkdir(parents=True, exist_ok=True)
    output_file.write_text("\n".join(matched) + ("\n" if matched else ""), encoding="utf-8")
    print(f"  [parse] {len(matched)} lines → {output_file}")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> None:
    args = parse_args()
    conf_file = Path(args.conf)
    dump_dir = Path(args.dump_dir)
    results_dir = Path(args.results_dir)
    dump_dir.mkdir(parents=True, exist_ok=True)

    binary = ensure_build(BUILD_TARGET, skip=args.skip_build)
    algo_plan = resolve_algorithms(args.algs)

    # Pre-resolve LPCC params once (shared across all LPCC runs).
    lpcc_params = resolve_lpcc_params(args.profile, args.override)

    for alg_name, alg_conf in algo_plan:
        alg_lower = alg_name.lower()
        dump_file = dump_dir / f"evaluation-{alg_lower}.out"

        # Build command.
        cmd = build_core_args(binary, conf_file, alg_conf)

        # LPCC-specific tuning parameters.
        if alg_name == "LPCC" and lpcc_params:
            cmd += build_lpcc_args(lpcc_params)

        # Monitoring parameters.
        cmd += [
            f"--monitorSwitchId={args.monitor_switch_id}",
            f"--monitorThroughputBps={args.monitor_throughput_bps}",
        ]

        print(f"[run] {alg_name} → {dump_file}")
        run_cmd(cmd, cwd=REPO_ROOT, log_path=dump_file)
        print(f"[done] {alg_name}")

        # Post-process results.
        if not args.skip_parse:
            result_file = results_dir / f"result-{alg_lower}.burst"
            parse_burst_results(dump_file, result_file)

    print(f"[all done] dump={dump_dir}  results={results_dir}")


if __name__ == "__main__":
    main()
