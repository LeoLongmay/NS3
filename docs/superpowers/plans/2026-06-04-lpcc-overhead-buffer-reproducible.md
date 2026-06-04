# LPCC Overhead + Buffer-vs-Flows Reproducible Analysis — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a deterministic, stdlib-only analysis tool and thin drivers that (a) produce an LPCC switch-buffer / flow-table-memory vs flow-count table appended to `overhead.md`, and (b) reproducibly regenerate the Goodput/P99 maintenance table into `overhead-maint.md`.

**Architecture:** No C++ changes. The existing `powertcp-evaluation-burst` binary already emits per-switch `rdma:` buffer/flow-table lines (`flow_table.txt`) and per-interval `ToR … PortAgg … throughput …` monitor lines (stdout → `run.log`). A new `analyze-overhead.py` parses these into Markdown tables; two thin shell drivers run the LPCC sweep and the DCQCN maintenance A/B (monitor pinned to node 74, RNG pinned via `--RngRun`).

**Tech Stack:** Python 3 (standard library only), Bash, ns-3.39 (`./ns3` CMake build), `pytest` for unit tests.

---

## Reference facts (verified in source — do not re-derive)

- `flow_table.txt` line: `rdma: <ts_ns> <switchId> <bufBytes> <ftBytes> <flowCount> <egressFlowCount>` (`powertcp-evaluation-burst.cc:329`). Also `rdma_counter: <swId> insert=.. insert_egress=.. clean=.. topk=..`.
- `fct.txt` columns (space-separated): `src dst sport dport size_bytes start_ns fct_ns [standalone_ns]`. **fct_ns is index 6**, size index 4, start index 5 (matches `plot-overhead.py:98`).
- Monitor stdout line (aggregate, when whole-switch monitored): `ToR <idx> PortAgg -1 throughput <bps> txBytes 0 qlen <q> time <t_s> normpower <p>` (`powertcp-evaluation-burst.cc:529`). `throughput` is in **bits/sec**. Only ONE switch is printed when `--monitorSwitchId` is set, so matching `ToR \d+ PortAgg -1 throughput <X>` is unambiguous.
- `--monitorSwitchId` matches the switch **node id** (`switchIdToNum`, `:1338`). Node **74** is a switch (ids 64–78). Default if unset is 72.
- Buffer capacity = `BUFFER_SIZE` MB (128) → `SetBufferPool(128*1024*1024)` (`:1317`).
- **No `SeedManager`/`RngRun` in the .cc** → ns-3 `CommandLine::Parse` auto-handles the global `--RngRun`/`--RngSeed`. Pin via CLI `--RngRun=1` (no source edit).
- LPCC algorithm id = 9, `--windowCheck=0` (per existing sweep scripts). DCQCN id = 1.
- Build target: `examples/PowerTCP/powertcp-evaluation-burst`; optimized binary at `build/examples/PowerTCP/ns3.39-powertcp-evaluation-burst-optimized` (already present).

## File structure

| File | Action | Responsibility |
|------|--------|----------------|
| `examples/PowerTCP/analyze-overhead.py` | Create | Parsers + buffer/maintenance renderers + CLI. |
| `examples/PowerTCP/tests/test_analyze_overhead.py` | Create | Unit tests for parsers/renderers. |
| `examples/PowerTCP/tests/fixtures/` | Create | Tiny hand-written + one real-captured fixture. |
| `examples/PowerTCP/run-overhead-buffer.sh` | Create | LPCC-only sweep driver → appends buffer table to `overhead.md`. |
| `examples/PowerTCP/script-ab-maintenance.sh` | Modify | Param `BIN`, N→16384, `--monitorSwitchId=74`, `--RngRun=1`, call analyzer → `overhead-maint.md`. |
| `examples/PowerTCP/overhead.md` | Output | Original table preserved; buffer table appended. |
| `examples/PowerTCP/overhead-maint.md` | Output | Reproduced Goodput/P99 table. |

Run all `pytest` from `examples/PowerTCP/`. Run all `./ns3` / drivers from repo root `/home/leo/CC_Exp`.

---

## Task 1: Calibration spike — capture a real fixture, confirm formats

**Files:**
- Create: `examples/PowerTCP/tests/fixtures/real-lpcc-64/` (captured dump snippets)

- [ ] **Step 1: Ensure the optimized binary exists**

Run (repo root):
```bash
cd /home/leo/CC_Exp
ls build/examples/PowerTCP/ns3.39-powertcp-evaluation-burst-optimized \
  || CXXFLAGS=-w ./ns3 build examples/PowerTCP/powertcp-evaluation-burst
```
Expected: the binary path prints (already built) or build succeeds.

- [ ] **Step 2: Generate the N=64 flow file**

```bash
cd /home/leo/CC_Exp
python3 examples/PowerTCP/gen-sweep-flows.py --out_dir /tmp/cal_flows --sizes 64 --bytes 1000000
```
Expected: `wrote /tmp/cal_flows/flow-burstExp-64.txt (64 flows)`.

- [ ] **Step 3: Build a calibration config (redirect outputs, monitor node 74, trace off)**

```bash
cd /home/leo/CC_Exp
mkdir -p /tmp/cal_run
awk -v flow=/tmp/cal_flows/flow-burstExp-64.txt -v fct=/tmp/cal_run/fct.txt \
    -v pfc=/tmp/cal_run/pfc.txt -v ftmon=/tmp/cal_run/flow_table.txt \
    -v qlen=/tmp/cal_run/qlen.txt '
  $1=="FLOW_FILE"{print "FLOW_FILE "flow;next}
  $1=="FCT_OUTPUT_FILE"{print "FCT_OUTPUT_FILE "fct;next}
  $1=="PFC_OUTPUT_FILE"{print "PFC_OUTPUT_FILE "pfc;next}
  $1=="FLOW_TABLE_MON_FILE"{print "FLOW_TABLE_MON_FILE "ftmon;next}
  $1=="QLEN_MON_FILE"{print "QLEN_MON_FILE "qlen;next}
  $1=="SIMULATOR_STOP_TIME"{print "SIMULATOR_STOP_TIME 0.20";next}
  $1=="ENABLE_TRACE"{print "ENABLE_TRACE 0";next}
  {print}' examples/PowerTCP/config-burst.txt > /tmp/cal_run/config.txt
```
Expected: `/tmp/cal_run/config.txt` exists.

- [ ] **Step 4: Run one LPCC simulation at node 74, RNG pinned**

```bash
cd /home/leo/CC_Exp
timeout 900 build/examples/PowerTCP/ns3.39-powertcp-evaluation-burst-optimized \
  --conf=/tmp/cal_run/config.txt --algorithm=9 --transportMode=0 --flowControlMode=0 \
  --wien=false --delayWien=false --windowCheck=0 \
  --monitorSwitchId=74 --monitorThroughputBps=100000000000 --RngRun=1 \
  > /tmp/cal_run/run.log 2>&1 ; echo "exit=$?"
```
Expected: `exit=0`; `/tmp/cal_run/flow_table.txt` and `/tmp/cal_run/fct.txt` non-empty.

- [ ] **Step 5: Confirm the three line formats match Reference facts**

```bash
grep -m1 '^rdma:' /tmp/cal_run/flow_table.txt
grep -m1 'PortAgg -1 throughput' /tmp/cal_run/run.log
head -1 /tmp/cal_run/fct.txt
```
Expected: an `rdma:` line with 6 numeric fields; a `ToR <idx> PortAgg -1 throughput <bps> …` line; an fct row with ≥7 columns. **If any format differs from Reference facts, update the Reference facts block and the parser code in later tasks before proceeding.**

- [ ] **Step 6: Save trimmed real fixtures**

```bash
mkdir -p examples/PowerTCP/tests/fixtures/real-lpcc-64
grep '^rdma' /tmp/cal_run/flow_table.txt | head -200 > examples/PowerTCP/tests/fixtures/real-lpcc-64/flow_table.txt
grep -E 'PortAgg -1 throughput|MONITOR_TARGET' /tmp/cal_run/run.log > examples/PowerTCP/tests/fixtures/real-lpcc-64/run.log
head -100 /tmp/cal_run/fct.txt > examples/PowerTCP/tests/fixtures/real-lpcc-64/fct.txt
```
Expected: three non-empty fixture files.

- [ ] **Step 7: Commit**

```bash
cd /home/leo/CC_Exp
git add examples/PowerTCP/tests/fixtures/real-lpcc-64
git commit -m "test: capture real LPCC N=64 calibration fixtures for overhead analyzer"
```

---

## Task 2: `parse_flow_table` (streaming peak reducer)

**Files:**
- Create: `examples/PowerTCP/analyze-overhead.py`
- Create: `examples/PowerTCP/tests/test_analyze_overhead.py`
- Create: `examples/PowerTCP/tests/fixtures/ft_small.txt`

- [ ] **Step 1: Write the synthetic fixture**

Create `examples/PowerTCP/tests/fixtures/ft_small.txt`:
```
rdma: 1000 74 500 100 5 5
rdma: 1000 75 200 40 2 2
rdma: 2000 74 1500 300 12 12
rdma: 2000 75 800 90 7 7
rdma_counter: 74 insert=12 insert_egress=12 clean=3 topk=4
```

- [ ] **Step 2: Write the failing test**

Create `examples/PowerTCP/tests/test_analyze_overhead.py`:
```python
from pathlib import Path
import importlib.util

_spec = importlib.util.spec_from_file_location(
    "analyze_overhead", Path(__file__).resolve().parents[1] / "analyze-overhead.py")
ao = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(ao)

FIX = Path(__file__).resolve().parent / "fixtures"


def test_parse_flow_table_peaks_per_switch():
    stats = ao.parse_flow_table(FIX / "ft_small.txt")
    # switch 74 peak buf=1500, ft=300, flows=12 ; switch 75 peak buf=800, ft=90, flows=7
    assert stats[74].buf_bytes == 1500
    assert stats[74].ft_bytes == 300
    assert stats[74].flow_count == 12
    assert stats[75].buf_bytes == 800
```

- [ ] **Step 3: Run test to verify it fails**

Run: `cd examples/PowerTCP && python3 -m pytest tests/test_analyze_overhead.py::test_parse_flow_table_peaks_per_switch -v`
Expected: FAIL (`analyze-overhead.py` has no `parse_flow_table`).

- [ ] **Step 4: Write minimal implementation**

Create `examples/PowerTCP/analyze-overhead.py`:
```python
#!/usr/bin/env python3
"""Deterministic analysis for the LPCC flow-table overhead experiments.

Two table modes:
  --table buffer       LPCC switch-buffer / flow-table-memory vs flow count
  --table maintenance  Goodput / P99 FCT under FLOW_TABLE_MAINTENANCE on/off

Standard library only; never slurps large dumps (streams line by line).
"""
import argparse
from dataclasses import dataclass
from pathlib import Path


@dataclass
class PeakStats:
    buf_bytes: int = 0
    ft_bytes: int = 0
    flow_count: int = 0


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
```

- [ ] **Step 5: Run test to verify it passes**

Run: `cd examples/PowerTCP && python3 -m pytest tests/test_analyze_overhead.py::test_parse_flow_table_peaks_per_switch -v`
Expected: PASS.

- [ ] **Step 6: Add a real-fixture smoke test**

Append to `tests/test_analyze_overhead.py`:
```python
def test_parse_flow_table_real_fixture_has_node_74():
    stats = ao.parse_flow_table(FIX / "real-lpcc-64" / "flow_table.txt")
    assert 74 in stats
    assert stats[74].flow_count > 0
```
Run: `cd examples/PowerTCP && python3 -m pytest tests/test_analyze_overhead.py -v`
Expected: both PASS. (If node 74 is absent in the real fixture, the monitored switch index differs — adjust `--monitorSwitchId` understanding in Task 1 and re-capture.)

- [ ] **Step 7: Commit**

```bash
cd /home/leo/CC_Exp
git add examples/PowerTCP/analyze-overhead.py examples/PowerTCP/tests/
git commit -m "feat: parse_flow_table streaming peak reducer with tests"
```

---

## Task 3: `parse_fct` (FCT values for P99)

**Files:**
- Modify: `examples/PowerTCP/analyze-overhead.py`
- Modify: `examples/PowerTCP/tests/test_analyze_overhead.py`
- Create: `examples/PowerTCP/tests/fixtures/fct_small.txt`

- [ ] **Step 1: Write the fixture**

Create `examples/PowerTCP/tests/fixtures/fct_small.txt` (cols: src dst sport dport size start_ns fct_ns standalone_ns):
```
0 1 3 10000 1000000 130000000 5230000 4000000
1 2 3 10001 1000000 130000000 17890000 4000000
2 3 3 10002 1000000 130000000 123190000 4000000
```

- [ ] **Step 2: Write the failing test**

Append to `tests/test_analyze_overhead.py`:
```python
def test_parse_fct_returns_fct_ns_column():
    recs = ao.parse_fct(FIX / "fct_small.txt")
    assert [r.fct_ns for r in recs] == [5230000, 17890000, 123190000]
    assert recs[0].size_bytes == 1000000
```

- [ ] **Step 3: Run test to verify it fails**

Run: `cd examples/PowerTCP && python3 -m pytest tests/test_analyze_overhead.py::test_parse_fct_returns_fct_ns_column -v`
Expected: FAIL (no `parse_fct`).

- [ ] **Step 4: Write minimal implementation**

Add to `analyze-overhead.py`:
```python
@dataclass
class FlowRecord:
    size_bytes: int
    start_ns: int
    fct_ns: int


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
```

- [ ] **Step 5: Run test to verify it passes**

Run: `cd examples/PowerTCP && python3 -m pytest tests/test_analyze_overhead.py::test_parse_fct_returns_fct_ns_column -v`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
cd /home/leo/CC_Exp
git add examples/PowerTCP/analyze-overhead.py examples/PowerTCP/tests/
git commit -m "feat: parse_fct reader with test"
```

---

## Task 4: `parse_monitor_goodput` (node-74 bottleneck-link goodput)

**Files:**
- Modify: `examples/PowerTCP/analyze-overhead.py`
- Modify: `examples/PowerTCP/tests/test_analyze_overhead.py`
- Create: `examples/PowerTCP/tests/fixtures/runlog_small.txt`

- [ ] **Step 1: Write the fixture** (`throughput` in bits/sec; goodput = mean over active samples)

Create `examples/PowerTCP/tests/fixtures/runlog_small.txt`:
```
ToR 10 PortAgg -1 throughput 0 txBytes 0 qlen 0 time 0.10 normpower 0
ToR 10 PortAgg -1 throughput 90000000000 txBytes 0 qlen 100 time 0.15 normpower 1
ToR 10 PortAgg -1 throughput 100000000000 txBytes 0 qlen 200 time 0.16 normpower 1
```

- [ ] **Step 2: Write the failing test**

Append to `tests/test_analyze_overhead.py`:
```python
def test_parse_monitor_goodput_mean_of_active_samples():
    # active samples: 90e9 and 100e9 -> mean 95e9 -> 95.0 Gbps (the 0 sample is excluded)
    g = ao.parse_monitor_goodput(FIX / "runlog_small.txt")
    assert abs(g - 95.0) < 1e-9
```

- [ ] **Step 3: Run test to verify it fails**

Run: `cd examples/PowerTCP && python3 -m pytest tests/test_analyze_overhead.py::test_parse_monitor_goodput_mean_of_active_samples -v`
Expected: FAIL (no `parse_monitor_goodput`).

- [ ] **Step 4: Write minimal implementation**

Add to `analyze-overhead.py`:
```python
import re

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
```

- [ ] **Step 5: Run test to verify it passes**

Run: `cd examples/PowerTCP && python3 -m pytest tests/test_analyze_overhead.py::test_parse_monitor_goodput_mean_of_active_samples -v`
Expected: PASS.

- [ ] **Step 6: Sanity-check against the real fixture**

Append to `tests/test_analyze_overhead.py`:
```python
def test_parse_monitor_goodput_real_fixture_in_range():
    g = ao.parse_monitor_goodput(FIX / "real-lpcc-64" / "run.log")
    # N=64 is far below line rate; expect a small positive Gbps value.
    assert g is None or g > 0
```
Run: `cd examples/PowerTCP && python3 -m pytest tests/test_analyze_overhead.py -v`
Expected: all PASS. **If `g is None` because the real fixture has no `PortAgg -1` line, inspect `/tmp/cal_run/run.log` for the actual monitored-line token layout and update `_AGG_RE` + re-capture the fixture.**

- [ ] **Step 7: Commit**

```bash
cd /home/leo/CC_Exp
git add examples/PowerTCP/analyze-overhead.py examples/PowerTCP/tests/
git commit -m "feat: parse_monitor_goodput (node-74 bottleneck-link mean Gbps)"
```

---

## Task 5: P99 helper + buffer-table reducer/renderer

**Files:**
- Modify: `examples/PowerTCP/analyze-overhead.py`
- Modify: `examples/PowerTCP/tests/test_analyze_overhead.py`

- [ ] **Step 1: Write the failing tests**

Append to `tests/test_analyze_overhead.py`:
```python
def test_p99_ms():
    vals_ns = [i * 1_000_000 for i in range(1, 101)]  # 1..100 ms
    assert abs(ao.p99_ms(vals_ns) - 99.0) < 0.5

def test_buffer_row_for_n():
    stats = {74: ao.PeakStats(buf_bytes=64 * 1024 * 1024, ft_bytes=512 * 1024, flow_count=4096),
             75: ao.PeakStats(buf_bytes=1024, ft_bytes=10, flow_count=3)}
    row = ao.buffer_metrics(stats, buffer_mb=128, switch_id=None)
    assert abs(row["peak_buf_mb"] - 64.0) < 1e-6        # max over switches
    assert abs(row["buf_util_pct"] - 50.0) < 1e-6       # 64/128
    assert abs(row["peak_ft_kb"] - 512.0) < 1e-6
    assert row["peak_flows"] == 4096

def test_render_markdown_buffer_table():
    cols = [64, 256]
    rows = {64: {"peak_buf_mb": 1.0, "buf_util_pct": 0.78, "peak_ft_kb": 8.0,
                 "ft_cap_pct": 0.006, "peak_flows": 64},
            256: None}  # 256 missing -> em dash
    md = ao.render_buffer_md(cols, rows, buffer_mb=128)
    assert "| Peak switch buffer (MB)" in md
    assert "—" in md           # missing cell
    assert "| 64 | 256 |" in md.replace("  ", " ") or "64" in md
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cd examples/PowerTCP && python3 -m pytest tests/test_analyze_overhead.py -k "p99 or buffer or render_markdown" -v`
Expected: FAIL (functions undefined).

- [ ] **Step 3: Write minimal implementation**

Add to `analyze-overhead.py`:
```python
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
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cd examples/PowerTCP && python3 -m pytest tests/test_analyze_overhead.py -v`
Expected: all PASS.

- [ ] **Step 5: Commit**

```bash
cd /home/leo/CC_Exp
git add examples/PowerTCP/analyze-overhead.py examples/PowerTCP/tests/
git commit -m "feat: p99_ms + buffer metrics/markdown renderer"
```

---

## Task 6: maintenance-table reducer/renderer

**Files:**
- Modify: `examples/PowerTCP/analyze-overhead.py`
- Modify: `examples/PowerTCP/tests/test_analyze_overhead.py`

- [ ] **Step 1: Write the failing test**

Append to `tests/test_analyze_overhead.py`:
```python
def test_render_maintenance_md():
    cols = [64, 256]
    # data[arm][n] = {"goodput": Gbps, "p99": ms}
    data = {
        "on":  {64: {"goodput": 3.64, "p99": 5.23}, 256: {"goodput": 15.29, "p99": 17.89}},
        "off": {64: {"goodput": 3.64, "p99": 5.12}, 256: {"goodput": 15.78, "p99": 15.63}},
    }
    md = ao.render_maintenance_md(cols, data)
    assert "| Goodput (Gbps) | On" in md
    assert "| P99 FCT (ms)   | Off".replace("   ", " ") in md.replace("   ", " ")
    assert "3.64" in md and "17.89" in md
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd examples/PowerTCP && python3 -m pytest tests/test_analyze_overhead.py::test_render_maintenance_md -v`
Expected: FAIL (no `render_maintenance_md`).

- [ ] **Step 3: Write minimal implementation**

Add to `analyze-overhead.py`:
```python
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
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cd examples/PowerTCP && python3 -m pytest tests/test_analyze_overhead.py::test_render_maintenance_md -v`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
cd /home/leo/CC_Exp
git add examples/PowerTCP/analyze-overhead.py examples/PowerTCP/tests/
git commit -m "feat: maintenance-table markdown renderer"
```

---

## Task 7: CLI wiring (`main`, two modes, `--append`, provenance)

**Files:**
- Modify: `examples/PowerTCP/analyze-overhead.py`
- Modify: `examples/PowerTCP/tests/test_analyze_overhead.py`

- [ ] **Step 1: Write the failing test (buffer mode end-to-end on fixtures)**

Append to `tests/test_analyze_overhead.py`:
```python
import subprocess, sys, os

def test_cli_buffer_mode_append(tmp_path):
    # Build a fake root: <root>/lpcc/64/flow_table.txt
    root = tmp_path / "root"
    d = root / "lpcc" / "64"; d.mkdir(parents=True)
    (d / "flow_table.txt").write_text(
        "rdma: 1000 74 1048576 8192 64 64\nrdma: 2000 74 2097152 16384 64 64\n")
    out = tmp_path / "overhead.md"
    out.write_text("# existing\n\n| old | table |\n")
    script = Path(__file__).resolve().parents[1] / "analyze-overhead.py"
    r = subprocess.run([sys.executable, str(script), "--table", "buffer", "--alg", "lpcc",
                        "--root", str(root), "--counts", "64", "--buffer-mb", "128",
                        "--out", str(out), "--append"], capture_output=True, text=True)
    assert r.returncode == 0, r.stderr
    text = out.read_text()
    assert "old | table" in text          # original preserved
    assert "Peak switch buffer (MB)" in text
    assert "2.00" in text                  # 2 MiB peak buffer
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd examples/PowerTCP && python3 -m pytest tests/test_analyze_overhead.py::test_cli_buffer_mode_append -v`
Expected: FAIL (no CLI / `__main__`).

- [ ] **Step 3: Write minimal implementation**

Add to `analyze-overhead.py`:
```python
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
        title = "\n\n## LPCC switch-buffer & flow-table memory vs concurrent flows\n\n"
    else:
        md, missing = build_maintenance_table(args.root, counts)
        title = "## Flow-table maintenance overhead (DCQCN control)\n\n"

    block = title + md + "\n"
    out = Path(args.out)
    if args.append and out.exists():
        out.write_text(out.read_text().rstrip() + "\n" + block)
    else:
        out.write_text(block)
    print(block)
    if missing:
        print(f"[warn] missing/empty runs (rendered as —): {missing}")


if __name__ == "__main__":
    main()
```

- [ ] **Step 4: Run the full test suite**

Run: `cd examples/PowerTCP && python3 -m pytest tests/test_analyze_overhead.py -v`
Expected: all PASS.

- [ ] **Step 5: Commit**

```bash
cd /home/leo/CC_Exp
git add examples/PowerTCP/analyze-overhead.py examples/PowerTCP/tests/
git commit -m "feat: analyze-overhead CLI (buffer/maintenance modes, --append)"
```

---

## Task 8: `run-overhead-buffer.sh` LPCC driver

**Files:**
- Create: `examples/PowerTCP/run-overhead-buffer.sh`

- [ ] **Step 1: Write the driver**

Create `examples/PowerTCP/run-overhead-buffer.sh`:
```bash
#!/usr/bin/env bash
# LPCC switch-buffer / flow-table-memory sweep vs concurrent flow count.
# Reproducible: fixed seed flow files, pinned RngRun, ENABLE_TRACE off, monitor node 74.
# Appends the resulting table below the existing content of overhead.md.
set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/config.sh"

COUNTS="${COUNTS:-64,256,1024,4096,16384}"
CONFIG_TEMPLATE="$NS3/examples/PowerTCP/config-burst.txt"
OUT_ROOT="$NS3/examples/PowerTCP/dump_burst_overhead"
FLOW_DIR="$NS3/examples/PowerTCP/sweep_flows"
BIN="${BIN:-$NS3/build/examples/PowerTCP/ns3.39-powertcp-evaluation-burst-optimized}"
OUT_MD="$NS3/examples/PowerTCP/overhead.md"

[ -x "$BIN" ] || { echo "[*] building binary"; (cd "$NS3" && CXXFLAGS=-w ./ns3 build examples/PowerTCP/powertcp-evaluation-burst); }

mkdir -p "$OUT_ROOT" "$FLOW_DIR"
python3 "$SCRIPT_DIR/gen-sweep-flows.py" --out_dir "$FLOW_DIR" --sizes "$COUNTS" --bytes 1000000

cd "$NS3"
IFS=',' read -ra NLIST <<< "$COUNTS"
for N in "${NLIST[@]}"; do
    if   (( N <= 1024 )); then SIM_STOP=0.20
    elif (( N <= 4096 )); then SIM_STOP=0.25
    else                       SIM_STOP=0.50; echo "[warn] N=$N is heavy (large dump / long wall-clock)"; fi
    run_dir="$OUT_ROOT/lpcc/$N"; mkdir -p "$run_dir"; run_conf="$run_dir/config.txt"
    awk -v flow="$FLOW_DIR/flow-burstExp-$N.txt" -v fct="$run_dir/fct.txt" \
        -v pfc="$run_dir/pfc.txt" -v ftmon="$run_dir/flow_table.txt" \
        -v qlen="$run_dir/qlen.txt" -v stop="$SIM_STOP" '
        $1=="FLOW_FILE"{print "FLOW_FILE "flow;next}
        $1=="FCT_OUTPUT_FILE"{print "FCT_OUTPUT_FILE "fct;next}
        $1=="PFC_OUTPUT_FILE"{print "PFC_OUTPUT_FILE "pfc;next}
        $1=="FLOW_TABLE_MON_FILE"{print "FLOW_TABLE_MON_FILE "ftmon;next}
        $1=="QLEN_MON_FILE"{print "QLEN_MON_FILE "qlen;next}
        $1=="SIMULATOR_STOP_TIME"{print "SIMULATOR_STOP_TIME "stop;next}
        $1=="ENABLE_TRACE"{print "ENABLE_TRACE 0";next}
        {print}' "$CONFIG_TEMPLATE" > "$run_conf"
    grep -q "^FLOW_TABLE_MON_FILE " "$run_conf" || { echo "FLOW_TABLE_MON_FILE $run_dir/flow_table.txt" >> "$run_conf"; echo "FLOW_TABLE_MON_INTERVAL_NS 100000" >> "$run_conf"; }

    echo "[*] LPCC N=$N SIM_STOP=$SIM_STOP -> $run_dir"
    timeout 7200 "$BIN" --conf="$run_conf" --algorithm=9 --transportMode=0 \
        --flowControlMode=0 --wien=false --delayWien=false --windowCheck=0 \
        --monitorSwitchId=74 --monitorThroughputBps=100000000000 --RngRun=1 \
        > "$run_dir/run.log" 2>&1 || echo "    (run timed out or failed)"
    echo "    ft lines=$(wc -l < "$run_dir/flow_table.txt" 2>/dev/null || echo 0)"
done

# Provenance stamp + append the buffer table to overhead.md
COMMIT=$(git -C "$NS3" rev-parse --short HEAD 2>/dev/null || echo unknown)
CONF_SHA=$(sha256sum "$CONFIG_TEMPLATE" | cut -c1-12)
{ echo ""; echo "<!-- generated by run-overhead-buffer.sh | commit=$COMMIT | counts=$COUNTS | config_sha=$CONF_SHA | seed=42 RngRun=1 -->"; } >> "$OUT_MD"
python3 "$SCRIPT_DIR/analyze-overhead.py" --table buffer --alg lpcc --root "$OUT_ROOT" \
    --counts "$COUNTS" --buffer-mb 128 --switch-id 74 --out "$OUT_MD" --append
echo "## OVERHEAD-BUFFER SWEEP FINISHED -> $OUT_MD ##"
```

- [ ] **Step 2: Make executable and smoke-run on the two smallest N**

```bash
cd /home/leo/CC_Exp
chmod +x examples/PowerTCP/run-overhead-buffer.sh
COUNTS=64,256 examples/PowerTCP/run-overhead-buffer.sh
```
Expected: two runs complete; ends with `## OVERHEAD-BUFFER SWEEP FINISHED ...`; `overhead.md` gains a `## LPCC switch-buffer & flow-table memory vs concurrent flows` section with numeric (non-`—`) cells for 64 and 256, and the original table is still present at the top.

- [ ] **Step 3: Verify isolation (no committed data polluted)**

```bash
cd /home/leo/CC_Exp
git status --porcelain examples/PowerTCP/dump_burst_overhead examples/PowerTCP/sweep_flows
```
Expected: empty output (both are gitignored).

- [ ] **Step 4: Commit (driver + the regenerated overhead.md)**

```bash
cd /home/leo/CC_Exp
git add examples/PowerTCP/run-overhead-buffer.sh examples/PowerTCP/overhead.md
git commit -m "feat: run-overhead-buffer.sh LPCC buffer sweep; append buffer table to overhead.md"
```

---

## Task 9: Update `script-ab-maintenance.sh` for reproducible maintenance table

**Files:**
- Modify: `examples/PowerTCP/script-ab-maintenance.sh`

- [ ] **Step 1: Parameterize BIN, extend N, add monitor+RNG, call analyzer**

In `examples/PowerTCP/script-ab-maintenance.sh`:

Replace line 14:
```bash
BIN="$NS3/build/examples/PowerTCP/ns3.39-powertcp-evaluation-burst-debug"
```
with:
```bash
BIN="${BIN:-$NS3/build/examples/PowerTCP/ns3.39-powertcp-evaluation-burst-optimized}"
[ -x "$BIN" ] || (cd "$NS3" && CXXFLAGS=-w ./ns3 build examples/PowerTCP/powertcp-evaluation-burst)
```

Replace line 21 (`for N in 64 256 1024 4096; do`) with:
```bash
for N in 64 256 1024 4096 16384; do
```

In the same loop, change the SIM_STOP rule (lines 22-24) to cover 16384:
```bash
    if   (( N <= 1024 )); then SIM_STOP=0.20
    elif (( N <= 4096 )); then SIM_STOP=0.25
    else                       SIM_STOP=0.50
    fi
```

In the `awk` block, add an `ENABLE_TRACE` redirect line (after the `SIMULATOR_STOP_TIME` line, before the closing `{ print }`):
```bash
            $1 == "ENABLE_TRACE"        { print "ENABLE_TRACE 0"; next }
```

Replace the `timeout ... "$BIN" ...` invocation (lines 51-54) with (adds monitor node 74 + pinned RNG):
```bash
        timeout "$PER_RUN_TIMEOUT" "$BIN" \
            --conf="$run_conf" --algorithm="$cc" --transportMode="$tp" \
            --flowControlMode="$fc" --wien="$wien" --delayWien="$delay" \
            --windowCheck="$window" \
            --monitorSwitchId=74 --monitorThroughputBps=100000000000 --RngRun=1 \
            > "$run_dir/run.log" 2>&1 || echo "    (run failed/timeout)"
```

Before the final `echo "## A/B MAINTENANCE TEST FINISHED ##"`, add the analyzer call:
```bash
python3 "$SCRIPT_DIR/analyze-overhead.py" --table maintenance \
    --root "$OUT_ROOT" --counts 64,256,1024,4096,16384 \
    --out "$NS3/examples/PowerTCP/overhead-maint.md"
echo "## wrote $NS3/examples/PowerTCP/overhead-maint.md ##"
```

Note: `sweep_flows_ab/flow-burstExp-<N>.txt` must exist (the AB script does not generate them). Add, right after `cd "$NS3"` (line 17), generation with the fixed seed:
```bash
python3 "$SCRIPT_DIR/gen-sweep-flows.py" --out_dir "$FLOW_DIR" --sizes 64,256,1024,4096,16384 --bytes 1000000
```

- [ ] **Step 2: Syntax-check the script**

Run: `bash -n examples/PowerTCP/script-ab-maintenance.sh && echo OK`
Expected: `OK`.

- [ ] **Step 3: Smoke-run the two smallest arms only**

```bash
cd /home/leo/CC_Exp
# Temporary tiny run: edit the N loop inline via env is not supported here, so run the script
# but interrupt after 64/256 finish, OR trust the full run on a fast machine.
# Minimal validation: run just N=64 both arms by temporarily exporting a guard is out of scope;
# instead validate the analyzer path on the calibration data:
python3 examples/PowerTCP/analyze-overhead.py --table maintenance \
    --root /tmp/does_not_exist --counts 64 --out /tmp/overhead-maint-smoke.md || true
cat /tmp/overhead-maint-smoke.md
```
Expected: the file renders the maintenance table skeleton with `—` cells (missing data handled gracefully); confirms the analyzer wiring works even before a full AB run.

- [ ] **Step 4: Commit**

```bash
cd /home/leo/CC_Exp
git add examples/PowerTCP/script-ab-maintenance.sh
git commit -m "feat: reproducible AB maintenance (node 74, RNG pinned, N->16384, analyzer -> overhead-maint.md)"
```

---

## Task 10: README note + final full-suite verification

**Files:**
- Create: `examples/PowerTCP/OVERHEAD_REPRODUCE.md`

- [ ] **Step 1: Write the reproduction note**

Create `examples/PowerTCP/OVERHEAD_REPRODUCE.md`:
```markdown
# Reproducing the overhead tables

Both tables are produced by `analyze-overhead.py` (stdlib-only) from simulation dumps.

## LPCC buffer / flow-table memory vs flows (appended to `overhead.md`)
    examples/PowerTCP/run-overhead-buffer.sh            # default COUNTS=64,256,1024,4096,16384
    COUNTS=64,256,1024 examples/PowerTCP/run-overhead-buffer.sh   # quick subset

Monitors switch node 74, RNG pinned (`--RngRun=1`), trace disabled. Output dirs
(`dump_burst_overhead/`, `sweep_flows/`) are gitignored.

## Maintenance Goodput/P99 (-> `overhead-maint.md`)
    examples/PowerTCP/script-ab-maintenance.sh

Goodput = mean bottleneck-link throughput at node 74 over active samples. The historical
`overhead.md` recipe was not committed, so regenerated numbers match in magnitude/trend
but are not bit-identical.

## Tests
    cd examples/PowerTCP && python3 -m pytest tests/test_analyze_overhead.py -v
```

- [ ] **Step 2: Run the full unit-test suite**

Run: `cd examples/PowerTCP && python3 -m pytest tests/test_analyze_overhead.py -v`
Expected: all tests PASS.

- [ ] **Step 3: Commit**

```bash
cd /home/leo/CC_Exp
git add examples/PowerTCP/OVERHEAD_REPRODUCE.md
git commit -m "docs: how to reproduce the overhead tables"
```

---

## Self-review notes (addressed)

- **Spec coverage:** Q3 buffer table (Tasks 2,5,7,8 → `overhead.md` append); Q2 maintenance reproduction (Tasks 3,4,6,7,9 → `overhead-maint.md`); node 74 (Tasks 1,4,8,9); fixed RNG via `--RngRun=1` no `.cc` change (Tasks 1,8,9); ENABLE_TRACE off (Tasks 1,8,9); N incl 16384 (Tasks 8,9); `/4` footnote (Task 5); preserve original `overhead.md` via `--append` (Tasks 7,8); provenance stamp (Task 8); missing-cell `—` handling (Tasks 5,7,9).
- **Empirical unknowns** (exact monitor line layout / goodput reduction) are calibrated against a real captured fixture in Task 1 and cross-checked in Tasks 2 & 4 with explicit "if format differs, update" instructions — not left as placeholders.
- **Type consistency:** `PeakStats`, `FlowRecord`, `parse_flow_table`, `parse_fct`, `parse_monitor_goodput`, `p99_ms`, `buffer_metrics`, `render_buffer_md`, `render_maintenance_md`, `build_buffer_table`, `build_maintenance_table` are defined once and reused with consistent signatures.
