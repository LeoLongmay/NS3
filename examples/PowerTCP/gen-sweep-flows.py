#!/usr/bin/env python3
"""
Generate flow files with N concurrent flows for the LPCC flow-table overhead sweep.

Flow file format (matches flow-burstExp.txt):
    line 1: <n_flows>
    next N lines: <src> <dst> <pg> <dport> <size_bytes> <start_time_s>

Hosts: 0..63 (per topology.txt).

Supports two modes:
  --bytes N        : uniform flow size (original behavior)
  --cdf <file.txt> : sample flow sizes from a CDF file (e.g. websearch.txt)
"""
from __future__ import annotations

import argparse
import os
import random


def load_cdf(path: str) -> list[tuple[float, float]]:
    """Load a CDF file: each line is '<size_bytes> <cumulative>'.
    Auto-detects whether the cumulative column is fraction (0-1) or percent (0-100).
    Returns sorted list of (cdf_fraction, size_bytes)."""
    raw = []
    with open(path) as f:
        for line in f:
            parts = line.strip().split()
            if len(parts) < 2:
                continue
            try:
                size = float(parts[0])
                cum = float(parts[1])
            except ValueError:
                continue
            raw.append((cum, size))
    if not raw:
        return []
    max_cum = max(c for c, _ in raw)
    scale = 100.0 if max_cum > 1.5 else 1.0  # percent vs fraction
    points = [(c / scale, s) for c, s in raw]
    points.sort()
    return points


def sample_cdf(cdf: list[tuple[float, float]], rng: random.Random) -> int:
    """Sample one flow size from the CDF via linear interpolation."""
    u = rng.random()
    # Find the two CDF points bracketing u
    for i in range(len(cdf)):
        if cdf[i][0] >= u:
            if i == 0:
                return max(1, int(cdf[i][1]))
            f0, s0 = cdf[i - 1]
            f1, s1 = cdf[i]
            if f1 == f0:
                return max(1, int(s1))
            t = (u - f0) / (f1 - f0)
            return max(1, int(s0 + t * (s1 - s0)))
    return max(1, int(cdf[-1][1]))


def gen(n_flows: int, out_path: str, size: int, start_time: float,
        cdf: list[tuple[float, float]] | None = None, seed: int = 42,
        n_hosts: int = 64, rand_start_window: float = 0.0) -> None:
    os.makedirs(os.path.dirname(out_path) or ".", exist_ok=True)
    rng = random.Random(seed)
    with open(out_path, "w") as f:
        f.write(f"{n_flows}\n")
        for i in range(n_flows):
            src = i % n_hosts
            dst = (src + 1 + (i // n_hosts)) % n_hosts
            if dst == src:
                dst = (src + 1) % n_hosts
            pg = 3
            # Keep dport in valid range when N > 55000 by wrapping.
            dport = 10000 + (i % 55000)
            flow_size = sample_cdf(cdf, rng) if cdf else size
            t_start = start_time
            if rand_start_window > 0.0:
                t_start = start_time + rng.uniform(0.0, rand_start_window)
            f.write(f"{src} {dst} {pg} {dport} {flow_size} {t_start:.9f}\n")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out_dir", default="examples/PowerTCP/sweep_flows",
                    help="output directory for flow files")
    ap.add_argument("--sizes", default="64,256,1024,4096,16384",
                    help="comma-separated flow counts")
    ap.add_argument("--bytes", type=int, default=1_000_000,
                    help="per-flow size in bytes (uniform mode)")
    ap.add_argument("--cdf", default=None,
                    help="path to CDF file for heterogeneous flow sizes")
    ap.add_argument("--seed", type=int, default=42,
                    help="random seed for CDF sampling")
    ap.add_argument("--start_time", type=float, default=0.13,
                    help="flow start time in seconds")
    ap.add_argument("--rand_start_window", type=float, default=0.0,
                    help="if > 0, randomize start in [start_time, start_time + window]")
    args = ap.parse_args()

    cdf = load_cdf(args.cdf) if args.cdf else None
    if cdf:
        print(f"Loaded CDF from {args.cdf} ({len(cdf)} points)")

    counts = [int(x) for x in args.sizes.split(",") if x.strip()]
    os.makedirs(args.out_dir, exist_ok=True)
    for n in counts:
        out = os.path.join(args.out_dir, f"flow-burstExp-{n}.txt")
        gen(n, out, args.bytes, args.start_time, cdf=cdf, seed=args.seed,
            rand_start_window=args.rand_start_window)
        print(f"wrote {out} ({n} flows)")


if __name__ == "__main__":
    main()
