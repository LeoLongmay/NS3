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
