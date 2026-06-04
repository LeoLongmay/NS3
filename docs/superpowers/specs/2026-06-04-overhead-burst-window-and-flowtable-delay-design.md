# Design: Burst-window workload change + per-packet flow-table delay model

Date: 2026-06-04
Status: Approved design (pending spec review)
Builds on: `2026-06-04-lpcc-overhead-buffer-reproducible-design.md` (the analyzer + two drivers already exist).

## 1. Motivation

Two follow-up changes to the existing overhead study:

1. **Buffer table (`overhead.md` table 2):** make the workload less of an instantaneous
   spike and extend the run, and add the N=16384 point. Specifically: spread flow start
   uniformly over a small window (0.12–0.13 s) instead of all-at-once, lengthen
   `SIMULATOR_STOP_TIME` to 0.35 s, and sweep N up to 16384.
2. **Maintenance table (`overhead-maint.md`):** the current A/B shows identical On/Off
   because ns-3 functional simulation does not charge flow-table maintenance work to the
   simulated datapath. Add a **per-packet processing-delay model** so the maintenance cost
   is visible in FCT/goodput. The delay is drawn uniformly from **[20 ns, 50 ns]** per
   packet that triggers a flow-table op.

## 2. Decisions (from brainstorming)

| # | Decision |
|---|----------|
| Burst spread | flows start uniformly random in [0.12, 0.13] s (`--start_time 0.12 --rand_start_window 0.01`). |
| Sim time | `SIMULATOR_STOP_TIME 0.35` for all N in the buffer sweep (and the maintenance rerun). |
| Buffer N | `64,256,1024,4096,16384` (16384 re-added). |
| Buffer timeout | raise per-run `timeout` (≥3 h); peaks are reached just after burst start, so even a late timeout still yields valid peak buffer/flow-table/flow-count data. |
| Delay magnitude | per-packet delay ~ **U[20, 50] ns**, drawn per packet that runs a flow-table op. |
| Delay knobs | `FLOW_TABLE_OP_DELAY_MIN_NS` / `FLOW_TABLE_OP_DELAY_MAX_NS`, **default 0/0 = disabled** (backward compatible; buffer/LPCC runs unaffected unless set). |
| Delay injection | at `switch-node.cc:466` egress: defer this packet's `SwitchSend` by the drawn delay; the `off` arm (maintenance disabled) gets no delay. |
| Ordering | per-egress-port FIFO clamp to prevent random-jitter reordering. |
| Reproducibility | `UniformRandomVariable` seeded by the global RNG; runs pin `--RngRun=1`. |
| Maintenance rerun | `script-ab-maintenance.sh` with the new workload, `COUNTS=64,256,1024,4096`; `on` arm sets maintenance=1 + delay 20/50, `off` arm sets maintenance=0. |
| 4096 caveat | 4 GB over the 100 G/~10 ms-RTT DCI link can't drain in 0.35 s; 4096 may still time out → mark unreliable if so. |
| Log slimming (Design D) | `run.log` (stdout) is the multi-GB culprit (95 `std::cout` sites + a per-`delay` per-port throughput monitor under heavy congestion); the data files (`flow_table.txt` etc.) are ~1 MB. Buffer sweep discards stdout; maintenance sweep filters stdout to only the lines the analyzer needs. |

## 3. Design A — buffer-table workload change (driver only, no C++)

Edit `examples/PowerTCP/run-overhead-buffer.sh`:

1. **Flow generation** — add start-window flags:
   ```bash
   python3 "$SCRIPT_DIR/gen-sweep-flows.py" --out_dir "$FLOW_DIR" --sizes "$COUNTS" \
       --bytes 1000000 --start_time 0.12 --rand_start_window 0.01
   ```
   `gen-sweep-flows.py` already supports both flags (`rng.uniform(0, window)` per flow);
   output stays deterministic under fixed seed (42).
2. **Sim time** — replace the per-N `SIM_STOP` ladder with a flat `SIM_STOP=0.35` for every N.
3. **Counts** — default `COUNTS="${COUNTS:-64,256,1024,4096,16384}"`.
4. **Timeout** — raise the per-run `timeout` to `${PER_RUN_TIMEOUT:-10800}` (3 h). Rationale:
   the buffer table reads **peak-over-time** from `flow_table.txt`; peak concurrency/buffer
   is reached shortly after the 0.12–0.13 s burst, long before sim end, so a late timeout on
   a huge N still yields valid peaks. (`flow_table.txt` is written every 100 µs throughout.)
5. **Discard stdout** (Design D) — redirect the binary's stdout to `/dev/null` (keep a small
   stderr log); the buffer table reads only `flow_table.txt`, so the multi-GB `run.log` is
   unnecessary. Essential for N=16384.

No analyzer change: it already renders whatever `--counts` it is given, max-over-switches,
peak-over-time, missing→`—`.

Re-run: restore `overhead.md` to the original maintenance table first (drop the previously
appended buffer section), then run the sweep so a single clean buffer table is appended.

## 4. Design B — per-packet flow-table delay model (C++)

Mirror the existing `FLOW_TABLE_MAINTENANCE` plumbing (config key → global → applied to each
`SwitchNode` in the burst-experiment setup loop).

**4.1 `src/point-to-point/model/switch-node.h`** — add:
- `uint64_t m_flowTableOpDelayMinNs = 0;`
- `uint64_t m_flowTableOpDelayMaxNs = 0;`  (both 0 ⇒ feature disabled)
- `Ptr<UniformRandomVariable> m_flowTableOpDelayRng;` (created in ctor; ns-3 seeds it from the global RNG stream, so `--RngRun` makes it reproducible)
- `std::vector<uint64_t> m_egressPipeFreeNs;` (per egress-device "pipeline free at" timestamp; sized lazily to `GetNDevices()`)
- setter `void SetFlowTableOpDelayNs(uint64_t minNs, uint64_t maxNs);`

**4.2 `src/point-to-point/model/switch-node.cc`** — at the egress block (currently
lines 461–466):
```cpp
bool ranFtOp = false;
if (m_flowTableMaintenance && ch.l3Prot == 0x11 && qIndex != 0) {
    m_flowTable->InsertOrUpdateFlowOnEgress(/* ...unchanged... */);
    ranFtOp = true;
}
if (ranFtOp && m_flowTableOpDelayMaxNs > 0) {
    if (m_egressPipeFreeNs.size() <= idx) m_egressPipeFreeNs.resize(GetNDevices(), 0);
    uint64_t now = Simulator::Now().GetNanoSeconds();
    uint64_t draw = (m_flowTableOpDelayMinNs == m_flowTableOpDelayMaxNs)
        ? m_flowTableOpDelayMinNs
        : m_flowTableOpDelayRng->GetInteger(m_flowTableOpDelayMinNs, m_flowTableOpDelayMaxNs);
    uint64_t tSend = std::max(now + draw, m_egressPipeFreeNs[idx]);   // per-port FIFO clamp
    m_egressPipeFreeNs[idx] = tSend;
    Simulator::Schedule(NanoSeconds(tSend - now),
                        &SwitchNode::DeferredSwitchSend, this, idx, qIndex, p, ch);
} else {
    m_devices[idx]->SwitchSend(qIndex, p, ch);
}
```
Add a small helper `void SwitchNode::DeferredSwitchSend(uint32_t idx, uint32_t qIndex, Ptr<Packet> p, CustomHeader ch)`
that calls `m_devices[idx]->SwitchSend(qIndex, p, ch);` (a method is needed because
`Simulator::Schedule` cannot bind the `m_devices[idx]->` member call directly with a
`CustomHeader` by-value cleanly). `CustomHeader` is passed by value to keep it alive across
the scheduled gap.

Ordering: the per-port clamp guarantees monotonically non-decreasing send times on each
egress port, so packets never reorder on a port; the random draw only adds jitter on top.
Because 20–50 ns ≪ the ~80 ns serialization of a 1 KB packet at 100 G, the clamp rarely
forces extra serialization.

Buffer accounting (ingress/egress admission at lines ~450–452) stays inline/unchanged; the
packet is "charged" at admission and then held in the modeled pipeline for the delay before
egress enqueue — an acceptable processing-delay model.

The ingress op (`InsertOrUpdateFlow`, line ~540 in `SwitchReceiveFromDevice`) is left as-is;
the single egress-side delay represents the aggregate per-packet flow-table work (YAGNI — one
knob, one injection point).

**4.3 `examples/PowerTCP/powertcp-evaluation-burst.cc`** — parse two new config keys
`FLOW_TABLE_OP_DELAY_MIN_NS` / `FLOW_TABLE_OP_DELAY_MAX_NS` into globals (mirroring the
existing `FLOW_TABLE_MAINTENANCE` / `FLOW_TABLE_INACTIVE_THRESHOLD_NS` handling), and in the
switch-setup loop call `sw->SetFlowTableOpDelayNs(min, max)`. Defaults 0/0 when keys absent.

Backward compatibility: with the keys absent/0, behavior is byte-identical to today.

## 5. Design C — maintenance rerun (produces `overhead-maint.md`)

Edit `examples/PowerTCP/script-ab-maintenance.sh`:
- Adopt the new workload: generate flows with `--start_time 0.12 --rand_start_window 0.01`;
  set `SIMULATOR_STOP_TIME 0.35` (flat).
- `COUNTS="${COUNTS:-64,256,1024,4096}"`.
- In the awk config rewrite, add the delay keys per arm:
  - `on` arm: `FLOW_TABLE_MAINTENANCE 1`, `FLOW_TABLE_OP_DELAY_MIN_NS 20`, `FLOW_TABLE_OP_DELAY_MAX_NS 50`.
  - `off` arm: `FLOW_TABLE_MAINTENANCE 0` (delay keys 0/0 or absent — no ops, no delay).
- Keep `--RngRun=1`, monitor node 74, `ENABLE_TRACE 0`, provenance stamp.
- **Filter stdout** to keep `run.log` small (Design D): pipe the binary's output through
  `grep` so only the analyzer-relevant lines survive.
- After the runs, regenerate `overhead-maint.md` (provenance comment + analyzer `--append`).

Expected outcome: `on` now carries an extra 20–50 ns per-packet pipeline delay that `off`
does not, so Goodput/P99 should differ (no longer bit-identical). At low N the difference
is tiny; it should grow with load. If 4096 times out (4 GB can't drain the DCI link in
0.35 s), annotate that column as unreliable and/or exclude it (as before).

## 5.5. Design D — log slimming (disk)

`run.log` (the binary's stdout/stderr) is what ballooned the dump dirs to ~16 GB: 95
`std::cout` sites plus a per-`delay` (`delay = 1.5·minRtt`) per-port throughput monitor,
amplified under heavy 4096-incast congestion → multi-GB per run. The actual data files
(`flow_table.txt` ≈ 1 MB, `fct.txt`/`pfc.txt`/`qlen.txt` small) are tiny. With N=16384 and
SIM 0.35 s, an unfiltered `run.log` could reach 10–30 GB/run (>100 GB across the sweep). No
`.cc` change — fix entirely in the drivers:

**Buffer sweep (`run-overhead-buffer.sh`):** the buffer table reads only `flow_table.txt`, so
`run.log` is unused. Discard stdout, keep a tiny stderr log for diagnosis:
```bash
timeout "$PER_RUN_TIMEOUT" "$BIN" --conf=... --algorithm=9 ... --RngRun=1 \
    > /dev/null 2> "$run_dir/run.err" || echo "    (run timed out or failed)"
```

**Maintenance sweep (`script-ab-maintenance.sh`):** goodput parsing needs only the
`MONITOR_TARGET` and `ToR … PortAgg -1 throughput …` lines. Filter stdout so `run.log` keeps
just those (GBs → a few MB). Use a filter that never fails the pipe and preserves the
binary's exit status:
```bash
set -o pipefail   # near the top of the script
timeout "$PER_RUN_TIMEOUT" "$BIN" --conf=... ... --RngRun=1 2>&1 \
    | grep -E --line-buffered 'MONITOR_TARGET|PortAgg -1 throughput' > "$run_dir/run.log" \
    || echo "    (run timed out or failed)"
```
Note: `grep` exits 1 when it matches nothing, which with `pipefail` would look like failure;
that is acceptable here (the `|| echo` branch just prints a note and the loop continues). The
analyzer already tolerates a `run.log` with no matches (goodput → `None` → `—`).

## 6. Implementation-time verifications

1. `SwitchSend` signature and that a `DeferredSwitchSend` wrapper schedules cleanly with
   `CustomHeader` by value (no dangling refs across the scheduled gap).
2. Exact lines for the `FLOW_TABLE_MAINTENANCE` plumbing in `powertcp-evaluation-burst.cc`
   (config parse + the switch-setup loop) to mirror for the new keys.
3. `UniformRandomVariable::GetInteger(min,max)` inclusivity is acceptable for the model.
4. Confirm an `off`-arm run with the new build is byte-identical to a maintenance=0 run
   built before this change (backward-compat smoke).

## 7. Out of scope / non-goals

- No change to the analyzer (`analyze-overhead.py`) — it already handles arbitrary `--counts`.
- No CLI flag for the delay (config-key only) — YAGNI; the scripts set it via the config.
- The ingress flow-table op is not separately delayed (single aggregate egress delay).
- No attempt to make 16384/4096 *complete* — buffer table needs only peaks; maintenance 4096
  may remain timeout-truncated.

## 8. Testing / verification

- **C++ build:** `./ns3 build examples/PowerTCP/powertcp-evaluation-burst` compiles.
- **Backward-compat smoke:** one short DCQCN run with delay keys absent reproduces prior
  behavior (no delay path taken).
- **Delay-on smoke:** one short DCQCN run with maintenance=1 + delay 20/50 completes; FCT is
  measurably higher than the maintenance=0 run on the same workload (sign check only).
- **Buffer workload smoke:** `COUNTS=64,256 run-overhead-buffer.sh` with the new flags
  produces a buffer table; flow start times in the generated flow file span [0.12, 0.13].
- **No-reorder check:** on the delay-on smoke, confirm no intra-port reordering was introduced
  (the FIFO clamp holds) — spot-check via run completing without RoCE order errors.
- **Log-slimming check (Design D):** after a buffer smoke run, confirm no `run.log` is written
  (stdout went to `/dev/null`) and the run dir is ~MB (just `flow_table.txt` etc.); after a
  maintenance smoke run, confirm `run.log` is a few MB and contains only `MONITOR_TARGET` /
  `PortAgg -1 throughput` lines, and that goodput still parses.

## 9. Deliverables

1. Edited `src/point-to-point/model/switch-node.{h,cc}` (delay knobs + injection + helper).
2. Edited `examples/PowerTCP/powertcp-evaluation-burst.cc` (parse + apply new config keys).
3. Edited `examples/PowerTCP/run-overhead-buffer.sh` (burst window, 0.35 s, N→16384, timeout, stdout→/dev/null per Design D).
4. Edited `examples/PowerTCP/script-ab-maintenance.sh` (new workload + per-arm delay keys + stdout grep-filter per Design D).
5. Regenerated `overhead.md` (buffer table, 5 columns) and `overhead-maint.md` (On/Off now
   differing). 4096/16384 annotated if timeout-truncated.
