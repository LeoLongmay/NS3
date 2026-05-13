#!/usr/bin/env python3
"""
Plot LPCC flow-table overhead vs concurrent flow count.

Inputs (produced by script-overhead-sweep.sh):
  dump_burst_overhead/<alg>/<N>/flow_table.txt   -- rdma: and rdma_counter: lines
  dump_burst_overhead/<alg>/<N>/fct.txt          -- per-flow FCT
  dump_burst_overhead/<alg>/<N>/run.log          -- simulation log

Outputs (./plot_overhead/):
  memory_vs_flows.pdf   -- LPCC peak flow-table memory vs N, with 24MB buffer baseline
  ops_vs_flows.pdf      -- LPCC fast-path ops per simulated second vs N
  p99_fct_vs_flows.pdf  -- LPCC vs DCQCN vs HPCC p99 FCT
"""
import argparse
import os
import re
from collections import defaultdict

import matplotlib.pyplot as plt
import numpy as np


BUFFER_POOL_BYTES = 24 * 1024 * 1024  # see switch-mmu.cc:65
SIM_DURATION_S = 0.25                  # see config-burst.txt SIMULATOR_STOP_TIME

RDMA_RE = re.compile(r"^rdma:\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s*$")
COUNTER_RE = re.compile(
    r"^rdma_counter:\s+(\d+)\s+insert=(\d+)\s+insert_egress=(\d+)\s+clean=(\d+)\s+topk=(\d+)\s*$"
)


def parse_flow_table_file(path):
    """Returns dict {switch_id -> peak_ft_bytes}, dict {switch_id -> peak_flow_count},
    and dict {switch_id -> counters} from a single run."""
    peak_bytes = defaultdict(int)
    peak_flows = defaultdict(int)
    counters = {}
    if not os.path.exists(path):
        return peak_bytes, peak_flows, counters
    with open(path) as f:
        for line in f:
            m = RDMA_RE.match(line)
            if m:
                _, sw, _buf_b, ft_b, fc, _eg_fc = (int(x) for x in m.groups())
                if ft_b > peak_bytes[sw]:
                    peak_bytes[sw] = ft_b
                if fc > peak_flows[sw]:
                    peak_flows[sw] = fc
                continue
            m = COUNTER_RE.match(line)
            if m:
                sw, ins, ins_eg, clean, topk = (int(x) for x in m.groups())
                counters[sw] = (ins, ins_eg, clean, topk)
    return peak_bytes, peak_flows, counters


def parse_fct_file(path):
    """Return list of FCT values (in ns) from fct.txt.
    Format varies by build; this looks for one numeric column at the line end."""
    values = []
    if not os.path.exists(path):
        return values
    with open(path) as f:
        for line in f:
            parts = line.strip().split()
            if not parts:
                continue
            try:
                values.append(float(parts[-1]))
            except ValueError:
                continue
    return values


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default="examples/PowerTCP/dump_burst_overhead")
    ap.add_argument("--out_dir", default="plot_overhead")
    ap.add_argument("--counts", default="64,256,1024,4096,16384")
    args = ap.parse_args()

    os.makedirs(args.out_dir, exist_ok=True)
    counts = [int(x) for x in args.counts.split(",") if x.strip()]

    # Figure 1: LPCC flow-table memory vs N
    fig, ax = plt.subplots(figsize=(6, 4))
    lpcc_peak_kb = []
    lpcc_sum_peak_kb = []
    for n in counts:
        peak_bytes, _, _ = parse_flow_table_file(
            os.path.join(args.root, "lpcc", str(n), "flow_table.txt"))
        if peak_bytes:
            lpcc_peak_kb.append(max(peak_bytes.values()) / 1024.0)
            lpcc_sum_peak_kb.append(sum(peak_bytes.values()) / 1024.0)
        else:
            lpcc_peak_kb.append(0.0)
            lpcc_sum_peak_kb.append(0.0)
    ax.plot(counts, lpcc_peak_kb, "o-", label="LPCC per-switch peak", lw=2)
    ax.plot(counts, lpcc_sum_peak_kb, "s--", label="LPCC fleet peak (sum)", lw=2)
    ax.axhline(BUFFER_POOL_BYTES / 1024.0, color="red", ls=":",
               label="Switch bufferPool (24 MB)")
    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_xlabel("Concurrent flows (N)")
    ax.set_ylabel("Memory (KB)")
    ax.set_title("LPCC flow-table memory vs concurrent flows")
    ax.grid(True, which="both", ls="--", alpha=0.4)
    ax.legend()
    fig.tight_layout()
    fig.savefig(os.path.join(args.out_dir, "memory_vs_flows.pdf"))
    plt.close(fig)

    # Figure 2: LPCC fast-path ops per simulated second
    fig, ax = plt.subplots(figsize=(6, 4))
    ins_per_s = []
    ins_eg_per_s = []
    for n in counts:
        _, _, counters = parse_flow_table_file(
            os.path.join(args.root, "lpcc", str(n), "flow_table.txt"))
        tot_ins = sum(c[0] for c in counters.values())
        tot_ins_eg = sum(c[1] for c in counters.values())
        ins_per_s.append(tot_ins / SIM_DURATION_S)
        ins_eg_per_s.append(tot_ins_eg / SIM_DURATION_S)
    ax.plot(counts, ins_per_s, "o-", label="ingress InsertOrUpdateFlow / s", lw=2)
    ax.plot(counts, ins_eg_per_s, "s-", label="egress InsertOrUpdateFlowOnEgress / s", lw=2)
    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_xlabel("Concurrent flows (N)")
    ax.set_ylabel("Fast-path ops / simulated second (all switches)")
    ax.set_title("LPCC fast-path operation rate vs concurrent flows")
    ax.grid(True, which="both", ls="--", alpha=0.4)
    ax.legend()
    fig.tight_layout()
    fig.savefig(os.path.join(args.out_dir, "ops_vs_flows.pdf"))
    plt.close(fig)

    # Figure 3: p99 FCT comparison across CC modes
    fig, ax = plt.subplots(figsize=(6, 4))
    for alg in ["dcqcn", "hpcc", "lpcc"]:
        p99 = []
        for n in counts:
            fct = parse_fct_file(os.path.join(args.root, alg, str(n), "fct.txt"))
            p99.append(np.percentile(fct, 99) if fct else np.nan)
        ax.plot(counts, p99, "o-", label=alg.upper(), lw=2)
    ax.set_xscale("log")
    ax.set_xlabel("Concurrent flows (N)")
    ax.set_ylabel("p99 FCT (ns)")
    ax.set_title("Switch forwarding performance: p99 FCT vs concurrent flows")
    ax.grid(True, which="both", ls="--", alpha=0.4)
    ax.legend()
    fig.tight_layout()
    fig.savefig(os.path.join(args.out_dir, "p99_fct_vs_flows.pdf"))
    plt.close(fig)

    print(f"plots written to {args.out_dir}/")


if __name__ == "__main__":
    main()
