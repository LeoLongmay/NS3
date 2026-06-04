#!/usr/bin/env python3
"""Deterministic analysis for the LPCC flow-table overhead experiments.

Two table modes:
  --table buffer       LPCC switch-buffer / flow-table-memory vs flow count
  --table maintenance  Goodput / P99 FCT under FLOW_TABLE_MAINTENANCE on/off

Standard library only; never slurps large dumps (streams line by line).
"""
import argparse
import re
from dataclasses import dataclass
from pathlib import Path


@dataclass
class PeakStats:
    buf_bytes: int = 0
    ft_bytes: int = 0
    flow_count: int = 0


@dataclass
class FlowRecord:
    # size_bytes/start_ns retained for future per-flow latency slicing;
    # current analysis paths only consume fct_ns.
    size_bytes: int
    start_ns: int
    fct_ns: int


def parse_flow_table(path):
    """Stream a flow_table.txt and return {switch_id: PeakStats} (peak over time)."""
    stats = {}
    p = Path(path)
    if not p.exists():
        return stats
    with p.open("r", encoding="utf-8", errors="replace") as f:
        for line in f:
            if not line.startswith("rdma: "):
                continue
            parts = line.split()
            # rdma: ts sw buf ft flows egress
            if len(parts) < 6:
                continue
            try:
                sw = int(parts[2]); buf = int(parts[3]); ft = int(parts[4])
                fc = int(parts[5])
            except ValueError:
                continue
            s = stats.setdefault(sw, PeakStats())
            s.buf_bytes = max(s.buf_bytes, buf)
            s.ft_bytes = max(s.ft_bytes, ft)
            s.flow_count = max(s.flow_count, fc)
    return stats


def parse_fct(path):
    """Stream fct.txt -> list[FlowRecord]. Columns: src dst sport dport size start_ns fct_ns ..."""
    out = []
    p = Path(path)
    if not p.exists():
        return out
    with p.open("r", encoding="utf-8", errors="replace") as f:
        for line in f:
            parts = line.split()
            if len(parts) < 7:
                continue
            try:
                out.append(FlowRecord(int(parts[4]), int(parts[5]), int(parts[6])))
            except ValueError:
                continue
    return out


_AGG_RE = re.compile(r"^ToR\s+\d+\s+PortAgg\s+-1\s+throughput\s+([0-9.eE+]+)\b")


def parse_monitor_goodput(path):
    """Mean of the monitored bottleneck-link throughput (bps) over active samples,
    returned in Gbps. 'Active' = throughput > 0 (excludes pre-traffic warmup)."""
    p = Path(path)
    if not p.exists():
        return None
    samples = []
    with p.open("r", encoding="utf-8", errors="replace") as f:
        for line in f:
            m = _AGG_RE.match(line)
            if not m:
                continue
            try:
                v = float(m.group(1))
            except ValueError:
                continue
            if v > 0:
                samples.append(v)
    if not samples:
        return None
    return sum(samples) / len(samples) / 1e9


def p99_ms(fct_ns_values):
    if not fct_ns_values:
        return None
    xs = sorted(fct_ns_values)
    # nearest-rank-ish linear interpolation, matching numpy.percentile default
    k = 0.99 * (len(xs) - 1)
    lo = int(k); hi = min(lo + 1, len(xs) - 1)
    frac = k - lo
    return (xs[lo] + (xs[hi] - xs[lo]) * frac) / 1e6


def buffer_metrics(stats, buffer_mb, switch_id=None):
    if not stats:
        return None
    if switch_id is not None:
        chosen = {switch_id: stats[switch_id]} if switch_id in stats else {}
        if not chosen:
            return None
    else:
        chosen = stats
    peak_buf = max(s.buf_bytes for s in chosen.values())
    peak_ft = max(s.ft_bytes for s in chosen.values())
    peak_flows = max(s.flow_count for s in chosen.values())
    cap_bytes = buffer_mb * 1024 * 1024
    return {
        "peak_buf_mb": peak_buf / 1024 / 1024,
        "buf_util_pct": peak_buf / cap_bytes * 100,
        "peak_ft_kb": peak_ft / 1024,
        "ft_cap_pct": peak_ft / cap_bytes * 100,
        "peak_flows": peak_flows,
    }


_BUF_ROWS = [
    ("Peak switch buffer (MB)", "peak_buf_mb", "{:.2f}"),
    ("Buffer utilization (%)", "buf_util_pct", "{:.2f}"),
    ("Peak flow-table mem (KB)", "peak_ft_kb", "{:.2f}"),
    ("FT mem / buffer cap (%)", "ft_cap_pct", "{:.4f}"),
    ("Peak concurrent flows", "peak_flows", "{:d}"),
]


def _cell(row, key, fmt):
    if row is None or row.get(key) is None:
        return "—"
    val = row[key]
    return fmt.format(int(val)) if fmt.endswith("d}") else fmt.format(val)


def render_buffer_md(cols, rows_by_n, buffer_mb):
    header = "| Metric | " + " | ".join(str(c) for c in cols) + " |"
    sep = "| " + " | ".join(["---"] + ["---:"] * len(cols)) + " |"
    lines = [header, sep]
    for label, key, fmt in _BUF_ROWS:
        cells = [_cell(rows_by_n.get(c), key, fmt) for c in cols]
        lines.append("| " + label + " | " + " | ".join(cells) + " |")
    note = (f"\n_LPCC only; peak over time, max over switches; buffer cap = {buffer_mb} MB._\n"
            "_Note: historical `memory_vs_flows.pdf` applied an undocumented /4 to memory; "
            "values here are true byte conversions (≈4× the old plot) and are canonical._")
    return "\n".join(lines) + "\n" + note


def _mcell(d, key, fmt="{:.2f}"):
    if d is None or d.get(key) is None:
        return "—"
    return fmt.format(d[key])


def render_maintenance_md(cols, data):
    header = "| Metric | Maint. | " + " | ".join(str(c) for c in cols) + " |"
    sep = "| " + " | ".join(["---", "---"] + ["---:"] * len(cols)) + " |"
    lines = [header, sep]
    spec = [("Goodput (Gbps)", "goodput"), ("P99 FCT (ms)", "p99")]
    for label, key in spec:
        for arm, arm_label in (("on", "On"), ("off", "Off")):
            cells = [_mcell(data.get(arm, {}).get(c), key) for c in cols]
            lines.append(f"| {label} | {arm_label} | " + " | ".join(cells) + " |")
    note = ("\n_DCQCN control (maintenance machinery on vs off); goodput = mean bottleneck-link "
            "throughput at node 74. Historical recipe unrecoverable — magnitudes/trends match, "
            "not bit-identical._")
    return "\n".join(lines) + "\n" + note


def _run_dir(root, alg, n, arm=None):
    base = Path(root)
    return (base / arm / str(n)) if arm else (base / alg / str(n))


def build_buffer_table(root, alg, counts, buffer_mb, switch_id):
    rows, missing = {}, []
    for n in counts:
        stats = parse_flow_table(_run_dir(root, alg, n) / "flow_table.txt")
        row = buffer_metrics(stats, buffer_mb, switch_id)
        rows[n] = row
        if row is None:
            missing.append(n)
    return render_buffer_md(counts, rows, buffer_mb), missing


def build_maintenance_table(root, counts):
    data = {"on": {}, "off": {}}
    missing = []
    for arm in ("on", "off"):
        for n in counts:
            rd = _run_dir(root, None, n, arm=arm)
            fcts = [r.fct_ns for r in parse_fct(rd / "fct.txt")]
            good = parse_monitor_goodput(rd / "run.log")
            p99 = p99_ms(fcts)
            if not fcts and good is None:
                missing.append((arm, n)); data[arm][n] = None
            else:
                data[arm][n] = {"goodput": good, "p99": p99}
    return render_maintenance_md(counts, data), missing


def main():
    ap = argparse.ArgumentParser(description="LPCC overhead / buffer analysis")
    ap.add_argument("--table", choices=["buffer", "maintenance"], required=True)
    ap.add_argument("--root", required=True)
    ap.add_argument("--alg", default="lpcc")
    ap.add_argument("--counts", default="64,256,1024,4096,16384")
    ap.add_argument("--buffer-mb", type=int, default=128)
    ap.add_argument("--switch-id", type=int, default=None)
    ap.add_argument("--out", required=True)
    ap.add_argument("--append", action="store_true")
    args = ap.parse_args()
    counts = [int(x) for x in args.counts.split(",") if x.strip()]

    if args.table == "buffer":
        md, missing = build_buffer_table(args.root, args.alg, counts, args.buffer_mb, args.switch_id)
        title = "## LPCC switch-buffer & flow-table memory vs concurrent flows\n\n"
    else:
        md, missing = build_maintenance_table(args.root, counts)
        title = "## Flow-table maintenance overhead (DCQCN control)\n\n"

    block = title + md + "\n"
    out = Path(args.out)
    if args.append and out.exists():
        out.write_text(out.read_text().rstrip() + "\n\n" + block)
    else:
        out.write_text(block)
    print(block)
    if missing:
        print(f"[warn] missing/empty runs (rendered as —): {missing}")


if __name__ == "__main__":
    main()
