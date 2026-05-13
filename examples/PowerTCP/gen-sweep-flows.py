#!/usr/bin/env python3
"""
Generate flow files with N concurrent flows for the LPCC flow-table overhead sweep.

Flow file format (matches flow-burstExp.txt):
    line 1: <n_flows>
    next N lines: <src> <dst> <pg> <dport> <size_bytes> <start_time_s>

Hosts: 0..63 (per topology.txt).
"""
import argparse
import os


def gen(n_flows: int, out_path: str, size: int, start_time: float, n_hosts: int = 64) -> None:
    os.makedirs(os.path.dirname(out_path) or ".", exist_ok=True)
    with open(out_path, "w") as f:
        f.write(f"{n_flows}\n")
        for i in range(n_flows):
            src = i % n_hosts
            dst = (src + 1 + (i // n_hosts)) % n_hosts
            if dst == src:
                dst = (src + 1) % n_hosts
            pg = 3
            dport = 10000 + i
            f.write(f"{src} {dst} {pg} {dport} {size} {start_time}\n")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out_dir", default="examples/PowerTCP/sweep_flows",
                    help="output directory for flow files")
    ap.add_argument("--sizes", default="64,256,1024,4096,16384",
                    help="comma-separated flow counts")
    ap.add_argument("--bytes", type=int, default=1_000_000,
                    help="per-flow size in bytes (1MB default = flow completes inside sim)")
    ap.add_argument("--start_time", type=float, default=0.13,
                    help="flow start time in seconds")
    args = ap.parse_args()

    counts = [int(x) for x in args.sizes.split(",") if x.strip()]
    os.makedirs(args.out_dir, exist_ok=True)
    for n in counts:
        out = os.path.join(args.out_dir, f"flow-burstExp-{n}.txt")
        gen(n, out, args.bytes, args.start_time)
        print(f"wrote {out} ({n} flows)")


if __name__ == "__main__":
    main()
