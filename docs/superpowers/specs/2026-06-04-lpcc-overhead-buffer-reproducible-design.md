# Design: Reproducible LPCC flow-table overhead analysis + buffer-vs-flows table

Date: 2026-06-04
Status: Approved design (pending spec review)

## 1. Background & problem

The PowerTCP example (`examples/PowerTCP`) studies the cost of the per-switch RDMA
flow table that LPCC requires. Two existing artifacts capture this:

- `overhead.md` — a Markdown table of **Goodput** and **P99 FCT** under
  `FLOW_TABLE_MAINTENANCE` On vs Off, columns N ∈ {64, 256, 1024, 4096, 16384}.
  Produced by `script-ab-maintenance.sh` (runs DCQCN as a control, since DCQCN does
  not read the flow table, isolating the pure forwarding cost of the maintenance
  machinery).
- `plot_overhead/*.pdf` — plots including `memory_vs_flows.pdf` (LPCC flow-table peak
  memory + buffer peak vs N) from `plot-overhead.py`.

### Gaps that motivate this work

1. **Not reproducible.** The raw simulation data lives in `dump_*/`, `sweep_*/` which
   are `.gitignore`d (lines 39–41) and are absent from disk. More importantly, **no
   committed code computes goodput / wall-time or assembles `overhead.md`** — that
   analysis was done ad-hoc. The `overhead.md` table cannot be regenerated today.
2. **`overhead.md`'s 16384 column has no script source** — `script-ab-maintenance.sh`
   tops out at N=4096; `script-overhead-sweep-extra.sh` tops out at 8192. The 16384
   point was produced by a manual one-off run.
3. **Buffer usage is not tabulated.** The flow-table overhead story omits how much
   switch packet buffer is consumed and how the flow-table's own memory scales with
   the number of concurrent flows.

### Data model (verified in source)

`monitor_flow_table()` in `powertcp-evaluation-burst.cc:317` already emits, every
`FLOW_TABLE_MON_INTERVAL_NS` (100 µs), **one line per switch**:

```
rdma: <ts_ns> <switchId> <bufBytes> <ftBytes> <flowCount> <egressFlowCount>
```

- `bufBytes` = `SwitchMmu::GetTotalUsedBuffer()` — packet-buffer occupancy, summed
  over all ports×queues (`switch-mmu.cc:1104`). Capacity = `BUFFER_SIZE` MB (128 MB
  in `config-burst.txt`) via `SetBufferPool`.
- `ftBytes` = `RDMAFlowTable::GetTotalMemoryUsage()` — the flow table's own
  control-plane memory: map + LRU bytes (`rdma-flow-table.cc:258`). A **separate**
  memory from the packet buffer.
- `flowCount` / `egressFlowCount` — tracked-flow counts.

Plus a one-shot `rdma_counter: <swId> insert=.. insert_egress=.. clean=.. topk=..`.

**Key consequence:** both buffer metrics the user wants are already in the dump. No
C++ changes are required; the missing piece is deterministic analysis + a reproducible
driver.

## 2. Goals (and non-goals)

In scope:
- **Q3:** a Markdown table of switch buffer occupancy + flow-table memory vs N, for
  **LPCC only**.
- **Q2:** make the `overhead.md` Goodput/P99 maintenance table reproducible from
  committed code.

Non-goals (explicitly out of scope):
- No changes to any `.cc`/`.h` simulation source.
- No new per-queue/per-port buffer breakdown or data-plane buffer attribution.
- No plot/PDF regeneration (deliverable is Markdown tables only).
- wall-time reproduction is **excluded** (it is compile-profile dependent; we run the
  optimized binary).

## 3. Decisions log (from brainstorming)

| # | Decision |
|---|----------|
| Approach | A — reuse existing sweep/AB shell scripts; add one analysis script + thin drivers. |
| Buffer scope | Both switch total buffer occupancy **and** flow-table control memory. |
| Algorithms | Buffer table: **LPCC only**. |
| Deliverable | **Markdown table** (no plot, no CSV). |
| Switch口径 | Max over all switches (bottleneck), matching `plot-overhead.py`'s `max()`; `--switch-id` to pin one. |
| Time aggregation | **Peak** over time. |
| `/4` factor | Report **true byte conversions** (no `/4`); footnote that historical `memory_vs_flows.pdf` applied an undocumented `/4`. |
| Buffer table output | New file `overhead-buffer.md`. |
| Binary | optimized (metrics are profile-independent); `BIN` parameterized. |
| Dump slimming | Set `ENABLE_TRACE 0` in per-run config to avoid multi-GB `mix.tr`. |
| N list | Default `64,256,1024,4096,16384` (16384 **included**); `--counts` configurable; warn for large N. |
| Goodput口径 | **Bottleneck-link monitor point** (reuse burst throughput monitor, e.g. `--monitorSwitchId=72`), to match the historical ~100G-ceiling semantics. |
| Reproducibility caveat | Historical `overhead.md` numbers' exact recipe is unrecoverable; the new tool uses explicit definitions — same magnitude/trend expected, **not bit-identical**. |

## 4. Architecture

Three new/edited components, no C++ touched:

```
analyze-overhead.py        (NEW)  deterministic, stdlib-only analysis
run-overhead-buffer.sh     (NEW)  thin LPCC-only driver -> overhead-buffer.md  (Q3)
script-ab-maintenance.sh   (EDIT) parameterize BIN, extend N to 16384,
                                  enable throughput monitor, call analyzer (Q2)
```

`analyze-overhead.py` has two table modes sharing the same parsers:

```
analyze-overhead.py --table buffer      --alg lpcc --root dump_burst_overhead    --out overhead-buffer.md
analyze-overhead.py --table maintenance --root dump_burst_overhead_ab            --out overhead.md
```

## 5. Phase 1 — `analyze-overhead.py` skeleton

Location: `examples/PowerTCP/analyze-overhead.py`. Python 3, standard library only.

CLI:
```
--table {buffer,maintenance}
--root  <dump root dir>
--alg   lpcc                 (buffer mode)
--counts 64,256,1024,4096,16384
--buffer-mb 128              (capacity, for utilization %)
--switch-id <int>            (optional; default: max over all switches)
--out   <path.md>
```

Parser units (single responsibility, unit-testable on tiny fixtures):

1. `parse_flow_table(path) -> {sw_id: PeakStats(buf_bytes, ft_bytes, flow_count)}`
   - **Streams** `rdma:` lines (never slurps; dumps can be large). For each switch
     keeps the running max of `bufBytes`, `ftBytes`, `flowCount` over time.
   - Also captures the final `rdma_counter:` values (kept for possible future use).
2. `parse_fct(path) -> list[FlowRecord(size_bytes, start_ns, fct_ns)]`
   - Streams `fct.txt`; column index 6 is `fct_ns`, column 4 is size, column 5 is
     start. Used by maintenance mode (P99) and as a fallback.
3. `parse_monitor_throughput(path) -> goodput_gbps`
   - Reads the burst throughput-monitor output for the monitored switch/port and
     reduces it to a single goodput number. **Exact field + reduction (steady-state
     mean vs peak) to be pinned during implementation** against the real monitor
     output (`PrintResults` / `MONITOR_TARGET` rows).
4. `reduce_*` + `render_markdown(rows, counts) -> str`

Cross-cutting rules:
- **Switch口径:** reduce per-switch peaks by `max()` over all switches unless
  `--switch-id` pins one.
- **Time aggregation:** peak over the run.
- **Missing/empty runs:** emit `—` for that cell and print a `[warn]` summary at the
  end. Never coerce missing data to `0`.
- Writes `--out` and echoes the table to stdout.

## 6. Phase 2 — buffer table schema (`--table buffer`, LPCC)

```
| Metric                    |   64 |  256 | 1024 | 4096 | 16384 |
| ------------------------- | ---: | ---: | ---: | ---: | ----: |
| Peak switch buffer (MB)   |      |      |      |      |       |
| Buffer utilization (%)    |      |      |      |      |       |
| Peak flow-table mem (KB)  |      |      |      |      |       |
| FT mem / buffer cap (%)   |      |      |      |      |       |
| Peak concurrent flows     |      |      |      |      |       |
```

| Row | Source | Formula | Unit |
|-----|--------|---------|------|
| Peak switch buffer | `bufBytes` | max over time×switches, `/1024/1024` | MB |
| Buffer utilization | `bufBytes` | `peak_buf / (buffer_mb·1MB) · 100` | % |
| Peak flow-table mem | `ftBytes` | max over time×switches, `/1024` | KB |
| FT mem / buffer cap | `ftBytes` | `peak_ft / (buffer_mb·1MB) · 100` | % |
| Peak concurrent flows | `flowCount` | max over time×switches | flows |

Intent: rows 1–2 = data-plane buffer pressure; rows 3–4 = control-plane flow-table
footprint (and how negligible it is vs the 128 MB pool); row 5 = context / alignment
sanity. Output file: `examples/PowerTCP/overhead-buffer.md`, with a footnote:

> Historical `memory_vs_flows.pdf` divided memory by an extra, undocumented factor of
> 4; values here are true byte conversions (≈4× the old plot). This table is canonical.

## 7. Phase 3 — driver `run-overhead-buffer.sh` (Q3 reproducibility)

LPCC-only, one command, from a clean checkout regenerates `overhead-buffer.md`.

```
source config.sh                       # NS3=/home/leo/CC_Exp
BIN=${BIN:-$NS3/build/.../ns3.39-powertcp-evaluation-burst-optimized}
[ -x "$BIN" ] || ./ns3 build examples/PowerTCP/powertcp-evaluation-burst
python3 gen-sweep-flows.py --out_dir sweep_flows --sizes $COUNTS --bytes 1000000   # seed=42
for N in $COUNTS:
    run_dir=dump_burst_overhead/lpcc/$N
    awk-rewrite config-burst.txt -> $run_dir/config.txt:
        FLOW_FILE          -> sweep_flows/flow-burstExp-$N.txt
        FCT/PFC/FLOW_TABLE_MON/QLEN -> $run_dir/*
        SIMULATOR_STOP_TIME-> (N<=1024:0.20 / N<=4096:0.25 / else:0.50)
        ENABLE_TRACE       -> 0
    timeout $T "$BIN" --conf=$run_dir/config.txt --algorithm=9 --windowCheck=0 ... > run.log 2>&1
python3 analyze-overhead.py --table buffer --alg lpcc --root dump_burst_overhead \
        --counts $COUNTS --out overhead-buffer.md
```

Properties:
- **Isolation:** all output under `dump_burst_overhead/` and `sweep_flows/`, both
  `.gitignore`d → no repo pollution.
- **Determinism:** flow files use `seed=42`. **RNG seed verification:** confirm the
  simulation fixes `RngSeed`/`RngRun`; if not, set it so buffer-occupancy peaks are
  reproducible (flow-table memory/flow-count peaks are already determined by the
  deterministic flow file).
- **Provenance stamp** prepended to `overhead-buffer.md`: git commit, date, `$COUNTS`,
  `sha256(config-burst.txt)`, seed.
- **N list:** default `64,256,1024,4096,16384`; `--counts` override. Print a wall-time
  warning for N > 8192 (16384 × 1 MB ≈ 16 GB of traffic under incast contention is a
  heavy run, though `flow_table.txt` itself stays small).

## 8. Phase 4 — reproduce `overhead.md` (Q2, maintenance table)

Extend `analyze-overhead.py` with `--table maintenance`: reads
`dump_burst_overhead_ab/{on,off}/<N>/` and emits the Goodput / P99 table (rows =
metric × Maint On/Off, columns = N).

| Metric | Source | Definition |
|--------|--------|-----------|
| P99 FCT (ms) | `fct.txt` col idx 6 (`fct_ns`) | `percentile(fcts, 99) / 1e6` (matches old `plot-overhead.py`). |
| Goodput (Gbps) | burst throughput monitor | Bottleneck-link monitored throughput at `--monitorSwitchId` (reduction pinned in impl). |

Driver edits to `script-ab-maintenance.sh`:
- Parameterize `BIN` (default optimized).
- Extend N to include 16384.
- **Enable the throughput monitor** (pass `--monitorSwitchId` / `--monitorThroughputBps`,
  e.g. node 72 as in `run_burst.py`) so goodput is measured at the bottleneck link.
- After the runs, call `analyze-overhead.py --table maintenance`.

Honesty caveat (also printed in the table footnote): the historical `overhead.md`
recipe is unrecoverable; regenerated numbers use the explicit definitions above —
expected same order of magnitude and trend, **not bit-identical**.

## 9. Implementation-time verifications (must confirm before claiming done)

1. ns-3 RNG seed is fixed (else set it) — for reproducible buffer-occupancy peaks.
2. Exact burst-monitor output field + the goodput reduction (steady-state mean vs
   peak) against real `PrintResults`/`MONITOR_TARGET` output.
3. Confirm the `/4` in `plot-overhead.py:154-155` is indeed unjustified before
   footnoting (optionally fix the plot later — not bound to this design).

## 10. Testing strategy

- Unit-test each parser on tiny hand-written fixtures (a few `rdma:` lines, a few
  `fct.txt` rows, a small monitor snippet) — assert peak reduction, missing-cell
  handling, unit conversions.
- Smoke-run the buffer driver on the two smallest N (64, 256) end-to-end and confirm
  `overhead-buffer.md` is produced with non-`—` cells and a provenance header.

## 11. Deliverables

1. `examples/PowerTCP/analyze-overhead.py` (two table modes, stdlib-only).
2. `examples/PowerTCP/run-overhead-buffer.sh` (LPCC buffer driver → `overhead-buffer.md`).
3. Edited `examples/PowerTCP/script-ab-maintenance.sh` (parameterized, N→16384,
   monitor enabled, calls analyzer → regenerates `overhead.md`).
4. `examples/PowerTCP/overhead-buffer.md` (the new buffer table).
5. Unit-test fixtures + a short README note on how to reproduce both tables.
