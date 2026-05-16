#!/usr/bin/env python3
"""
Plot LPCC flow-table overhead vs concurrent flow count.

Inputs (produced by script-overhead-sweep.sh):
  dump_burst_overhead/<alg>/<N>/flow_table.txt   -- rdma: and rdma_counter: lines
  dump_burst_overhead/<alg>/<N>/fct.txt          -- per-flow FCT

Outputs (./plot_overhead/):
  memory_vs_flows.pdf   -- LPCC peak flow-table memory vs N, with 128 MB buffer baseline
  ops_vs_flows.pdf      -- LPCC flow-table ops per simulated second vs N
  p99_fct_vs_flows.pdf  -- LPCC vs DCQCN vs HPCC p99 FCT (ms)
"""
import argparse
import os
import re
from collections import defaultdict

import matplotlib.pyplot as plt
import numpy as np
from matplotlib.ticker import ScalarFormatter

# ---------------------------------------------------------------------------
# Style (consistent with plot_fct_new.py)
# ---------------------------------------------------------------------------
C = [
    "xkcd:grass green",
    "xkcd:blue",
    "xkcd:purple",
    "xkcd:teal",
    "xkcd:brick red",
    "xkcd:black",
    "xkcd:brown",
    "xkcd:grey",
    "xkcd:orange",
]


def setup():
    plt.rc("lines", markersize=5)
    plt.rc("legend", handlelength=3, handleheight=1.5, labelspacing=0.25)
    plt.rcParams["font.size"] = 12
    plt.rcParams["pdf.fonttype"] = 42
    plt.rcParams["ps.fonttype"] = 42


def style_ax(ax):
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    ax.yaxis.set_ticks_position("left")
    ax.xaxis.set_ticks_position("bottom")
    ax.grid(which="major", alpha=0.5)
    ax.grid(which="minor", alpha=0.2)


# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------
BUFFER_POOL_BYTES = 128 * 1024 * 1024  # BUFFER_SIZE 128 in config-burst.txt (unit: MB)

RDMA_RE = re.compile(r"^rdma:\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s*$")
COUNTER_RE = re.compile(
    r"^rdma_counter:\s+(\d+)\s+insert=(\d+)\s+insert_egress=(\d+)\s+clean=(\d+)\s+topk=(\d+)\s*$"
)


# ---------------------------------------------------------------------------
# Parsers
# ---------------------------------------------------------------------------
def parse_flow_table_file(path):
    """Returns dict {switch_id -> peak_ft_bytes}, dict {switch_id -> peak_buf_bytes},
    dict {switch_id -> peak_flow_count}, dict {switch_id -> counters}."""
    peak_bytes = defaultdict(int)
    peak_buf = defaultdict(int)
    peak_flows = defaultdict(int)
    counters = {}
    if not os.path.exists(path):
        return peak_bytes, peak_buf, peak_flows, counters
    with open(path) as f:
        for line in f:
            m = RDMA_RE.match(line)
            if m:
                _, sw, buf_b, ft_b, fc, _eg_fc = (int(x) for x in m.groups())
                if ft_b > peak_bytes[sw]:
                    peak_bytes[sw] = ft_b
                if buf_b > peak_buf[sw]:
                    peak_buf[sw] = buf_b
                if fc > peak_flows[sw]:
                    peak_flows[sw] = fc
                continue
            m = COUNTER_RE.match(line)
            if m:
                sw, ins, ins_eg, clean, topk = (int(x) for x in m.groups())
                counters[sw] = (ins, ins_eg, clean, topk)
    return peak_bytes, peak_buf, peak_flows, counters


def parse_fct_file(path):
    """Return list of FCT values (in ns) from fct.txt.
    Format: src dst sport dport size start_time_ns fct_ns standalone_fct_ns
    Column 7 (index 6) is the actual FCT; column 8 (last) is standalone/ideal FCT."""
    values = []
    if not os.path.exists(path):
        return values
    with open(path) as f:
        for line in f:
            parts = line.strip().split()
            if len(parts) < 7:
                continue
            try:
                values.append(float(parts[6]))
            except ValueError:
                continue
    return values


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default="examples/PowerTCP/dump_burst_overhead")
    ap.add_argument("--out_dir", default="plot_overhead")
    ap.add_argument("--counts", default="64,256,1024,4096")
    ap.add_argument("--memory_only", action="store_true",
                    help="Only emit the memory plot (use when no DCQCN/HPCC data is present).")
    ap.add_argument("--memory_name", default="memory_vs_flows.pdf",
                    help="Filename for the memory plot.")
    ap.add_argument("--pow2_labels", action="store_true",
                    help="Label x-ticks as 2^k instead of the raw integer.")
    args = ap.parse_args()

    setup()
    os.makedirs(args.out_dir, exist_ok=True)
    counts = [int(x) for x in args.counts.split(",") if x.strip()]

    # X-axis ticks: one tick per data point, no orphan labels.
    xticks = sorted(counts)
    if args.pow2_labels:
        xtick_labels = [f"$2^{{{int(round(np.log2(t)))}}}$" for t in xticks]
    else:
        xtick_labels = [str(t) for t in xticks]

    # ===== Figure 1: LPCC flow-table memory vs N =====
    fig, ax = plt.subplots(figsize=(4, 4))
    style_ax(ax)

    lpcc_peak_kb = []
    lpcc_buf_peak_kb = []
    for n in counts:
        peak_bytes, peak_buf, _, _ = parse_flow_table_file(
            os.path.join(args.root, "lpcc", str(n), "flow_table.txt"))
        if peak_bytes:
            lpcc_peak_kb.append(max(peak_bytes.values()) / 1024.0 / 4)
            lpcc_buf_peak_kb.append(max(peak_buf.values()) / 1024.0 / 4)
        else:
            lpcc_peak_kb.append(0.0)
            lpcc_buf_peak_kb.append(0.0)

    ax.axhline(BUFFER_POOL_BYTES / 1024.0, color=C[4], ls=":",
               lw=2, label="Buffer cap(128 MB)")
    ax.plot(counts, lpcc_peak_kb, marker="o", ls="solid", color=C[0],
            lw=2, label="Flow-table peak")
    ax.plot(counts, lpcc_buf_peak_kb, marker="s", ls="dashed", color=C[1],
            lw=2, label="Buffer peak")


    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_xlabel("Concurrent Flows", fontsize=12)
    ax.set_ylabel("Memory (KB)", fontsize=12)
    ax.set_xticks(xticks)
    ax.set_xticklabels(xtick_labels, fontsize=12)
    ax.xaxis.set_minor_formatter(plt.NullFormatter())
    # Legend slightly below upper-left so it doesn't sit on the 128 MB cap line.
    ax.legend(fontsize=10, loc="upper left", bbox_to_anchor=(0.0, 0.95), framealpha=0)
    fig.tight_layout()
    fig.savefig(os.path.join(args.out_dir, args.memory_name),
                bbox_inches="tight")
    plt.close(fig)

    if args.memory_only:
        print(f"plot written to {args.out_dir}/{args.memory_name}")
        return

    # ===== Figure 2: p99 FCT comparison across CC modes (ms) =====
    fig, ax = plt.subplots(figsize=(4, 4))
    style_ax(ax)

    alg_styles = [
        ("dcqcn", C[0], "o",  "solid"),
        ("hpcc",  C[1], "s",  "dashed"),
        ("lpcc",  C[2], "x",  "dotted"),
    ]
    for alg, color, marker, ls in alg_styles:
        p99 = []
        for n in counts:
            fct = parse_fct_file(os.path.join(args.root, alg, str(n), "fct.txt"))
            p99.append(np.percentile(fct, 99) / 1e6 if fct else np.nan)  # ns -> ms
        ax.plot(counts, p99, marker=marker, ls=ls, color=color,
                lw=2, label=alg.upper())

    ax.set_xscale("log")
    # Linear y-axis with explicit ticks (data sits in 10-26 ms range).
    ax.set_xlabel("Concurrent Flows", fontsize=12)
    ax.set_ylabel("P99 FCT (ms)", fontsize=12)
    ax.set_xticks(xticks)
    ax.set_xticklabels(xtick_labels, fontsize=12)
    ax.xaxis.set_minor_formatter(plt.NullFormatter())
    y_ticks = [10, 14, 18, 22, 26]
    ax.set_yticks(y_ticks)
    ax.set_yticklabels([str(t) for t in y_ticks], fontsize=12)
    ax.set_ylim(y_ticks[0] - 1, y_ticks[-1] + 1)
    ax.legend(fontsize=10, loc="upper left", framealpha=0)
    fig.tight_layout()
    fig.savefig(os.path.join(args.out_dir, "p99_fct_vs_flows.pdf"),
                bbox_inches="tight")
    plt.close(fig)

    print(f"plots written to {args.out_dir}/")


if __name__ == "__main__":
    main()
