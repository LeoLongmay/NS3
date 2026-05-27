#!/usr/bin/env python3
"""Run the fairness experiment for one or more CC algorithms.

Replaces: script-fairness.sh + results-fairness.sh

Usage examples:
    python3 run_fairness.py --algs GEMINI BBR BICC
    python3 run_fairness.py --algs LPCC --skip-build
    # Run LPCC with a tuned profile:
    python3 run_fairness.py --algs LPCC --profile v17
    # Quick tweak — override a single param:
    python3 run_fairness.py --algs LPCC --profile v17 --override lpccWr=5.0
"""

import argparse
from pathlib import Path

from algo_common import (
    REPO_ROOT,
    SCRIPT_DIR,
    build_core_args,
    build_lpcc_args,
    ensure_build,
    resolve_algorithms,
    resolve_lpcc_params,
    run_cmd,
)

BUILD_TARGET = "examples/PowerTCP/powertcp-evaluation-fairness"
NUM_FLOWS = 4  # fairness experiment monitors 4 competing flows (Src 0..3)


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Run fairness experiment")
    p.add_argument("--algs", nargs="+", default=None,
                   help="algorithms to run (default: all)")
    p.add_argument("--conf", default=str(SCRIPT_DIR / "config-fairness.txt"))
    p.add_argument("--profile", default="v17",
                   help="LPCC parameter profile name (default: v17)")
    p.add_argument("--override", action="append", default=None,
                   help="override LPCC params, e.g. --override lpccWr=5.0 (can be repeated)")
    p.add_argument("--dump-dir", default=str(SCRIPT_DIR / "dump_fairness"))
    p.add_argument("--results-dir", default=str(SCRIPT_DIR / "results_fairness"))
    p.add_argument("--skip-build", action="store_true")
    p.add_argument("--skip-parse", action="store_true")
    return p.parse_args()


# ---------------------------------------------------------------------------
# Result parsing (replaces results-fairness.sh)
# ---------------------------------------------------------------------------

def parse_fairness_results(dump_file: Path, results_dir: Path, alg_lower: str) -> None:
    """Extract per-flow throughput lines from a raw fairness dump."""
    text = dump_file.read_text(encoding="utf-8", errors="replace")
    lines = text.splitlines()

    results_dir.mkdir(parents=True, exist_ok=True)
    for flow_id in range(NUM_FLOWS):
        prefix = f"Src {flow_id} Total"
        matched = [l for l in lines if prefix in l]
        out = results_dir / f"result-{alg_lower}.{flow_id + 1}"
        out.write_text("\n".join(matched) + ("\n" if matched else ""), encoding="utf-8")
        print(f"  [parse] Src {flow_id}: {len(matched)} lines → {out.name}")


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

        cmd = build_core_args(binary, conf_file, alg_conf)

        # LPCC-specific tuning parameters (only applied for the LPCC alg).
        if alg_name == "LPCC" and lpcc_params:
            cmd += build_lpcc_args(lpcc_params)

        print(f"[run] {alg_name} → {dump_file}")
        if alg_name == "LPCC" and lpcc_params:
            print(f"       profile={args.profile}  params={lpcc_params}")
        run_cmd(cmd, cwd=REPO_ROOT, log_path=dump_file)
        print(f"[done] {alg_name}")

        if not args.skip_parse:
            parse_fairness_results(dump_file, results_dir, alg_lower)

    print(f"[all done] dump={dump_dir}  results={results_dir}")


if __name__ == "__main__":
    main()
