#!/usr/bin/env python3
"""Run the workload (mixed traffic) experiment for one or more CC algorithms.

Replaces: script-workload.sh + results-workload.sh

Usage examples:
    python3 run_workload.py --algs DCQCN Bifrost BBR --loads 0.6
    python3 run_workload.py --algs LPCC --loads 0.2 0.4 0.6 0.8 --skip-build
"""

import argparse
from pathlib import Path

from algo_common import (
    REPO_ROOT,
    SCRIPT_DIR,
    build_core_args,
    ensure_build,
    resolve_algorithms,
    run_cmd,
)

BUILD_TARGET = "examples/PowerTCP/powertcp-evaluation-workload"


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Run workload (mixed traffic) experiment")
    p.add_argument("--algs", nargs="+", default=None,
                   help="algorithms to run (default: all)")
    p.add_argument("--conf", default=str(SCRIPT_DIR / "config-workload.txt"))
    p.add_argument("--loads", nargs="+", type=float, default=[0.6],
                   help="link load fractions, e.g. 0.2 0.4 0.6 0.8")
    p.add_argument("--query-request-rate", type=float, default=1,
                   help="Poisson query request rate")
    p.add_argument("--request", type=int, default=1000000,
                   help="query/request size in bytes")
    p.add_argument("--incast", type=int, default=10)
    p.add_argument("--cdf", default=str(SCRIPT_DIR / "traffic_gen" / "cross_datacenter_traffic2.txt"))
    p.add_argument("--start-time", type=float, default=0.1)
    p.add_argument("--end-time", type=float, default=1.3)
    p.add_argument("--flow-launch-end", type=float, default=0.8)
    p.add_argument("--dump-dir", default=str(SCRIPT_DIR / "dump_workload"))
    p.add_argument("--results-dir", default=str(SCRIPT_DIR / "results_workload"))
    p.add_argument("--skip-build", action="store_true")
    p.add_argument("--skip-parse", action="store_true")
    return p.parse_args()


# ---------------------------------------------------------------------------
# Result parsing (replaces results-workload.sh)
# ---------------------------------------------------------------------------

def parse_workload_results(
    dump_file: Path,
    results_dir: Path,
    alg_lower: str,
    load: float,
    req_rate: float,
    query: int,
) -> None:
    """Extract FCT and buffer lines from a raw workload dump."""
    text = dump_file.read_text(encoding="utf-8", errors="replace")
    lines = text.splitlines()
    results_dir.mkdir(parents=True, exist_ok=True)

    tag = f"{alg_lower}-{load}-{int(req_rate)}-{query}"

    # FCT lines: contain both 'FCT' and 'size'
    fct_lines = [l for l in lines if "FCT" in l and "size" in l]
    fct_out = results_dir / f"result-{tag}.fct"
    fct_out.write_text("\n".join(fct_lines) + ("\n" if fct_lines else ""), encoding="utf-8")

    # Buffer lines: contain 'switch 0' and 'qlen'
    buf_lines = [l for l in lines if "switch 0" in l and "qlen" in l]
    buf_out = results_dir / f"result-{tag}.buf"
    buf_out.write_text("\n".join(buf_lines) + ("\n" if buf_lines else ""), encoding="utf-8")

    print(f"  [parse] {tag}: {len(fct_lines)} fct + {len(buf_lines)} buf lines")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> None:
    args = parse_args()
    conf_file = Path(args.conf)
    cdf_file = Path(args.cdf)
    dump_dir = Path(args.dump_dir)
    results_dir = Path(args.results_dir)
    dump_dir.mkdir(parents=True, exist_ok=True)

    binary = ensure_build(BUILD_TARGET, skip=args.skip_build)
    algo_plan = resolve_algorithms(args.algs)

    req_rate = args.query_request_rate
    query = args.request

    for load in args.loads:
        for alg_name, alg_conf in algo_plan:
            alg_lower = alg_name.lower()
            tag = f"{alg_lower}-{load}-{int(req_rate)}-{query}"
            dump_file = dump_dir / f"evaluation-{tag}.out"

            cmd = build_core_args(binary, conf_file, alg_conf)
            cmd += [
                f"--queryRequestRate={req_rate}",
                f"--load={load}",
                f"--START_TIME={args.start_time}",
                f"--END_TIME={args.end_time}",
                f"--FLOW_LAUNCH_END_TIME={args.flow_launch_end}",
                f"--incast={args.incast}",
                f"--cdfFileName={cdf_file}",
                f"--request={query}",
            ]

            print(f"[run] load={load} {alg_name} → {dump_file}")
            run_cmd(cmd, cwd=REPO_ROOT, log_path=dump_file)
            print(f"[done] load={load} {alg_name}")

            if not args.skip_parse:
                parse_workload_results(dump_file, results_dir, alg_lower, load, req_rate, query)

    print(f"[all done] dump={dump_dir}  results={results_dir}")


if __name__ == "__main__":
    main()
