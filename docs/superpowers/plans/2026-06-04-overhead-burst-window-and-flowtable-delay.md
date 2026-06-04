# Burst-window workload + per-packet flow-table delay model — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Spread the burst workload over 0.12–0.13 s + extend sim to 0.35 s + add N=16384 to the LPCC buffer table; add a configurable per-packet flow-table-op datapath delay (U[20,50] ns) so the DCQCN maintenance A/B shows a real (if tiny) On/Off difference; and slim the multi-GB `run.log` so 16384 is feasible on disk.

**Architecture:** Two C++ edits (a new per-port deferred-send delay in `SwitchNode`, wired through two new `config-burst.txt` keys in the burst example) plus two shell-driver edits (workload flags + log redirection). The Python analyzer is unchanged. Backward compatible: with the new keys absent/0, behavior is byte-identical.

**Tech Stack:** ns-3.39 C++ (`./ns3` CMake build), Bash drivers, the existing stdlib `analyze-overhead.py` + its pytest suite.

---

## Reference facts (verified — do not re-derive)

- Spec: `docs/superpowers/specs/2026-06-04-overhead-burst-window-and-flowtable-delay-design.md`.
- **Egress flow-table op + send** in `src/point-to-point/model/switch-node.cc` lines 460–466:
  ```cpp
  			m_bytes[inDev][idx][qIndex] += p->GetSize();
  			if (m_flowTableMaintenance && ch.l3Prot == 0x11 && qIndex != 0) {
  				m_flowTable->InsertOrUpdateFlowOnEgress(Ipv4Address(ch.sip), Ipv4Address(ch.dip),
  				                                        ch.udp.sport, ch.udp.dport, idx, qIndex, ch.udp.pg,
  				                                        p->GetSize());
  			}
  			m_devices[idx]->SwitchSend(qIndex, p, ch);
  ```
- `SwitchSend` signature: `bool QbbNetDevice::SwitchSend(uint32_t qIndex, Ptr<Packet> packet, CustomHeader &ch)` (non-const ref → a by-value local binds fine).
- `switch-node.h`: flow-table members at lines 111–113; setters at 256–258 (`SetFlowTableMaintenance` is inline). `pCnt` is the port-count array dim used for `m_txBytes[pCnt]` etc. `#include "ns3/nstime.h"` already present; `std::max<uint16_t>` is already used in switch-node.cc (so `<algorithm>` is available).
- `SwitchNode::SwitchNode()` ctor at `switch-node.cc:185` (initializes per-port arrays in `for ... pCnt` loops).
- `SetFlowTableInactiveThresholdNs` impl pattern at `switch-node.cc:236`.
- burst.cc globals at lines 105–107 (`flow_table_maintenance` etc.); config parse for `FLOW_TABLE_MAINTENANCE` at lines 949–951; applied in switch-setup loop at lines 1264–1267 (`sw->SetFlowTableMaintenance(...)`).
- `gen-sweep-flows.py` already supports `--start_time` and `--rand_start_window` (uniform start in `[start_time, start_time+window]`); seed default 42.
- `UniformRandomVariable` is seeded from the global RNG stream → `--RngRun=1` makes draws reproducible.

## File structure

| File | Action | Responsibility |
|------|--------|----------------|
| `src/point-to-point/model/switch-node.h` | Modify | Declare delay knobs, RNG, per-port pipeline-free array, setter, `DeferredSwitchSend`. |
| `src/point-to-point/model/switch-node.cc` | Modify | Ctor init; setter impl; `DeferredSwitchSend`; egress deferred-send injection. |
| `examples/PowerTCP/powertcp-evaluation-burst.cc` | Modify | Parse `FLOW_TABLE_OP_DELAY_MIN_NS`/`MAX_NS`; apply to each switch. |
| `examples/PowerTCP/run-overhead-buffer.sh` | Modify | Burst window flags, flat SIM 0.35, N→16384, timeout 10800, stdout→/dev/null. |
| `examples/PowerTCP/script-ab-maintenance.sh` | Modify | New workload, per-arm delay keys, `pipefail` + grep-filtered stdout. |

Run all `./ns3` and drivers from repo root `/home/leo/CC_Exp`. Commit with the inline identity (git is not globally configured): `git -c user.name="Leo" -c user.email="NadineBlakevrg@californiamail.com" commit -m "..."`.

---

## Task 1: SwitchNode — delay knobs, RNG, deferred-send injection (C++)

**Files:**
- Modify: `src/point-to-point/model/switch-node.h` (members ~111–113, setters ~256–258)
- Modify: `src/point-to-point/model/switch-node.cc` (ctor ~185, after `SetFlowTableInactiveThresholdNs` ~242, egress block 460–466)

- [ ] **Step 1: Declare members in `switch-node.h`** — replace the three flow-table member lines (111–113):
```cpp
	    uint64_t m_flowTableInactiveThresholdNs{50000}; // default 50us; overridable from config
	    uint64_t m_flowTableCleanIntervalNs{50000};     // default 50us; overridable from config
	    bool m_flowTableMaintenance{true};              // false skips Insert*/Clean* for A/B no-op tests
```
with:
```cpp
	    uint64_t m_flowTableInactiveThresholdNs{50000}; // default 50us; overridable from config
	    uint64_t m_flowTableCleanIntervalNs{50000};     // default 50us; overridable from config
	    bool m_flowTableMaintenance{true};              // false skips Insert*/Clean* for A/B no-op tests
	    // Per-packet flow-table-op datapath delay model (0/0 => disabled, backward compatible).
	    uint64_t m_flowTableOpDelayMinNs{0};
	    uint64_t m_flowTableOpDelayMaxNs{0};
	    Ptr<UniformRandomVariable> m_flowTableOpDelayRng;
	    uint64_t m_egressPipeFreeNs[pCnt];              // per-egress-port FIFO clamp for the delay
```

- [ ] **Step 2: Declare setter + deferred-send in `switch-node.h`** — after the `SetFlowTableMaintenance` line (258):
```cpp
	void SetFlowTableMaintenance(bool on) { m_flowTableMaintenance = on; }
```
add:
```cpp
	void SetFlowTableOpDelayNs(uint64_t minNs, uint64_t maxNs);
	void DeferredSwitchSend(uint32_t idx, uint32_t qIndex, Ptr<Packet> p, CustomHeader ch);
```

- [ ] **Step 3: Add include for the RNG** — at the top of `switch-node.h`, after `#include "ns3/nstime.h"` (line 11):
```cpp
#include "ns3/random-variable-stream.h"
```

- [ ] **Step 4: Initialize in the ctor (`switch-node.cc`, inside `SwitchNode::SwitchNode()` ~185)** — after the line `m_mmu->SetSwitchId(m_id);` (line 189) add:
```cpp
	m_flowTableOpDelayRng = CreateObject<UniformRandomVariable>();
	for (uint32_t i = 0; i < pCnt; i++)
		m_egressPipeFreeNs[i] = 0;
```

- [ ] **Step 5: Add setter + DeferredSwitchSend impl** — in `switch-node.cc`, immediately after the `SetFlowTableInactiveThresholdNs` function (ends ~line 242 with its closing `}`), add:
```cpp
void SwitchNode::SetFlowTableOpDelayNs(uint64_t minNs, uint64_t maxNs) {
	m_flowTableOpDelayMinNs = minNs;
	m_flowTableOpDelayMaxNs = maxNs;
}

void SwitchNode::DeferredSwitchSend(uint32_t idx, uint32_t qIndex, Ptr<Packet> p, CustomHeader ch) {
	m_devices[idx]->SwitchSend(qIndex, p, ch);
}
```

- [ ] **Step 6: Inject the deferred send at the egress block** — replace lines 460–466:
```cpp
			m_bytes[inDev][idx][qIndex] += p->GetSize();
			if (m_flowTableMaintenance && ch.l3Prot == 0x11 && qIndex != 0) {
				m_flowTable->InsertOrUpdateFlowOnEgress(Ipv4Address(ch.sip), Ipv4Address(ch.dip),
				                                        ch.udp.sport, ch.udp.dport, idx, qIndex, ch.udp.pg,
				                                        p->GetSize());
			}
			m_devices[idx]->SwitchSend(qIndex, p, ch);
```
with:
```cpp
			m_bytes[inDev][idx][qIndex] += p->GetSize();
			bool ranFtOp = false;
			if (m_flowTableMaintenance && ch.l3Prot == 0x11 && qIndex != 0) {
				m_flowTable->InsertOrUpdateFlowOnEgress(Ipv4Address(ch.sip), Ipv4Address(ch.dip),
				                                        ch.udp.sport, ch.udp.dport, idx, qIndex, ch.udp.pg,
				                                        p->GetSize());
				ranFtOp = true;
			}
			if (ranFtOp && m_flowTableOpDelayMaxNs > 0 && idx < pCnt) {
				uint64_t now = Simulator::Now().GetNanoSeconds();
				uint64_t draw = (m_flowTableOpDelayMinNs >= m_flowTableOpDelayMaxNs)
					? m_flowTableOpDelayMaxNs
					: m_flowTableOpDelayRng->GetInteger((uint32_t)m_flowTableOpDelayMinNs,
					                                    (uint32_t)m_flowTableOpDelayMaxNs);
				uint64_t tSend = std::max(now + draw, m_egressPipeFreeNs[idx]); // per-port FIFO clamp
				m_egressPipeFreeNs[idx] = tSend;
				Simulator::Schedule(NanoSeconds(tSend - now),
				                    &SwitchNode::DeferredSwitchSend, this, idx, qIndex, p, ch);
			} else {
				m_devices[idx]->SwitchSend(qIndex, p, ch);
			}
```

- [ ] **Step 7: Build**

Run: `cd /home/leo/CC_Exp && CXXFLAGS=-w ./ns3 build examples/PowerTCP/powertcp-evaluation-burst 2>&1 | tail -15`
Expected: build completes (`Finished executing` / no compile errors). This recompiles the point-to-point module + relinks (a few minutes). If it fails, read the error and fix the edit (common: missing include, `pCnt` scope, `GetInteger` arg types).

- [ ] **Step 8: Commit**

```bash
cd /home/leo/CC_Exp
git add src/point-to-point/model/switch-node.h src/point-to-point/model/switch-node.cc
git -c user.name="Leo" -c user.email="NadineBlakevrg@californiamail.com" commit -m "feat(switch): per-packet flow-table-op datapath delay (deferred egress send, FIFO-clamped)"
```

---

## Task 2: Wire the two config keys in the burst example (C++)

**Files:**
- Modify: `examples/PowerTCP/powertcp-evaluation-burst.cc` (globals ~107, parse ~951, apply ~1267)

- [ ] **Step 1: Add globals** — after line 107 (`int flow_table_maintenance = 1; ...`) add:
```cpp
uint64_t flow_table_op_delay_min_ns = 0; // per-packet flow-table-op datapath delay (0/0 = disabled)
uint64_t flow_table_op_delay_max_ns = 0;
```

- [ ] **Step 2: Parse the keys** — in the config-parse chain, after the `FLOW_TABLE_MAINTENANCE` branch (lines 949–951), insert two more `else if` branches:
```cpp
		} else if (key.compare("FLOW_TABLE_OP_DELAY_MIN_NS") == 0) {
			conf >> flow_table_op_delay_min_ns;
			std::cout << "FLOW_TABLE_OP_DELAY_MIN_NS\t\t\t\t" << flow_table_op_delay_min_ns << '\n';
		} else if (key.compare("FLOW_TABLE_OP_DELAY_MAX_NS") == 0) {
			conf >> flow_table_op_delay_max_ns;
			std::cout << "FLOW_TABLE_OP_DELAY_MAX_NS\t\t\t\t" << flow_table_op_delay_max_ns << '\n';
```
(Insert the two `} else if ...` blocks so they sit between the existing `FLOW_TABLE_MAINTENANCE` block and the following `} else if (key.compare("MULTI_RATE")...`.)

- [ ] **Step 2.5: Add a sentinel so missing keys are explicit** — none needed; globals default to 0/0 (disabled), which matches "absent".

- [ ] **Step 3: Apply to each switch** — in the switch-setup loop, after line 1267 (`sw->SetFlowTableMaintenance(flow_table_maintenance != 0);`) add:
```cpp
			sw->SetFlowTableOpDelayNs(flow_table_op_delay_min_ns, flow_table_op_delay_max_ns);
```

- [ ] **Step 4: Build**

Run: `cd /home/leo/CC_Exp && CXXFLAGS=-w ./ns3 build examples/PowerTCP/powertcp-evaluation-burst 2>&1 | tail -15`
Expected: build completes with no errors.

- [ ] **Step 5: Commit**

```bash
cd /home/leo/CC_Exp
git add examples/PowerTCP/powertcp-evaluation-burst.cc
git -c user.name="Leo" -c user.email="NadineBlakevrg@californiamail.com" commit -m "feat(burst): config keys FLOW_TABLE_OP_DELAY_MIN/MAX_NS wired to SwitchNode"
```

---

## Task 3: Smoke — delay path active, deterministic, backward compatible

Verifies the new code without the driver edits, using throwaway `/tmp` configs.

**Files:** none modified (verification only).

- [ ] **Step 1: Generate a small spread-burst flow file**

```bash
cd /home/leo/CC_Exp
python3 examples/PowerTCP/gen-sweep-flows.py --out_dir /tmp/d3_flows --sizes 256 --bytes 1000000 \
    --start_time 0.12 --rand_start_window 0.01
awk 'NR>1{print $6}' /tmp/d3_flows/flow-burstExp-256.txt | sort -n | sed -n '1p;$p'
```
Expected: first/last start times both within [0.120000000, 0.130000000] (confirms the burst window).

- [ ] **Step 2: Build three configs (off / on-nodelay / on-delay)** — DCQCN, monitor 74, trace off, SIM 0.35:

```bash
cd /home/leo/CC_Exp
mkbase () { # $1=outfile $2=maint $3=dmin $4=dmax
  awk -v flow=/tmp/d3_flows/flow-burstExp-256.txt -v dir="$5" -v maint="$2" -v dmin="$3" -v dmax="$4" '
    $1=="FLOW_FILE"{print "FLOW_FILE "flow;next}
    $1=="FCT_OUTPUT_FILE"{print "FCT_OUTPUT_FILE "dir"/fct.txt";next}
    $1=="PFC_OUTPUT_FILE"{print "PFC_OUTPUT_FILE "dir"/pfc.txt";next}
    $1=="FLOW_TABLE_MON_FILE"{print "FLOW_TABLE_MON_FILE "dir"/flow_table.txt";next}
    $1=="QLEN_MON_FILE"{print "QLEN_MON_FILE "dir"/qlen.txt";next}
    $1=="SIMULATOR_STOP_TIME"{print "SIMULATOR_STOP_TIME 0.35";next}
    $1=="ENABLE_TRACE"{print "ENABLE_TRACE 0";next}
    $1=="FLOW_TABLE_MAINTENANCE"{print "FLOW_TABLE_MAINTENANCE "maint;next}
    {print}
  ' examples/PowerTCP/config-burst.txt > "$1"
  grep -q "^FLOW_TABLE_MAINTENANCE " "$1" || echo "FLOW_TABLE_MAINTENANCE $2" >> "$1"
  echo "FLOW_TABLE_OP_DELAY_MIN_NS $3" >> "$1"
  echo "FLOW_TABLE_OP_DELAY_MAX_NS $4" >> "$1"
}
for v in off onnd ond; do mkdir -p /tmp/d3_$v; done
mkbase /tmp/d3_off/config.txt  0 0  0  /tmp/d3_off
mkbase /tmp/d3_onnd/config.txt 1 0  0  /tmp/d3_onnd
mkbase /tmp/d3_ond/config.txt  1 20 50 /tmp/d3_ond
echo "configs built"
```

- [ ] **Step 3: Run the three (DCQCN, RngRun=1), discard stdout**

```bash
cd /home/leo/CC_Exp
BIN=build/examples/PowerTCP/ns3.39-powertcp-evaluation-burst-optimized
for v in off onnd ond; do
  timeout 1800 $BIN --conf=/tmp/d3_$v/config.txt --algorithm=1 --transportMode=0 \
    --flowControlMode=0 --wien=false --delayWien=false --windowCheck=0 \
    --monitorSwitchId=74 --monitorThroughputBps=100000000000 --RngRun=1 \
    > /dev/null 2> /tmp/d3_$v/run.err ; echo "$v exit=$? fct=$(wc -l < /tmp/d3_$v/fct.txt 2>/dev/null)"
done
```
Expected: all three `exit=0` with non-zero fct line counts.

- [ ] **Step 4: Compare P99 FCT at full precision**

```bash
cd /home/leo/CC_Exp
python3 - <<'PY'
import importlib.util, pathlib
s=importlib.util.spec_from_file_location("ao","examples/PowerTCP/analyze-overhead.py")
ao=importlib.util.module_from_spec(s); s.loader.exec_module(ao)
for v in ["off","onnd","ond"]:
    fc=[r.fct_ns for r in ao.parse_fct(pathlib.Path(f"/tmp/d3_{v}/fct.txt"))]
    print(v, "p99_ms=", None if not fc else round(ao.p99_ms(fc),6), "n=",len(fc))
PY
```
Expected:
- `off` and `onnd` P99 **identical** (delay disabled ⇒ original datapath; backward compatible).
- `ond` P99 **>= onnd** (the 20–50 ns per-packet delay only adds latency). The delta may be sub-millisecond (20–50 ns is tiny vs ms-scale FCT) — a `>=` with a strictly-different value at µs precision confirms the delay path is active. If `ond == onnd` exactly, the delay path was NOT taken — investigate the Task 1 injection guard before proceeding.

- [ ] **Step 5: Determinism check (rerun `ond`, expect identical fct)**

```bash
cd /home/leo/CC_Exp
BIN=build/examples/PowerTCP/ns3.39-powertcp-evaluation-burst-optimized
mkdir -p /tmp/d3_ond2
sed 's#/tmp/d3_ond#/tmp/d3_ond2#g' /tmp/d3_ond/config.txt > /tmp/d3_ond2/config.txt
timeout 1800 $BIN --conf=/tmp/d3_ond2/config.txt --algorithm=1 --transportMode=0 \
  --flowControlMode=0 --wien=false --delayWien=false --windowCheck=0 \
  --monitorSwitchId=74 --monitorThroughputBps=100000000000 --RngRun=1 > /dev/null 2>&1
diff <(sort /tmp/d3_ond/fct.txt) <(sort /tmp/d3_ond2/fct.txt) && echo "DETERMINISTIC-OK"
```
Expected: `DETERMINISTIC-OK` (identical fct under fixed RngRun).

- [ ] **Step 6: Record the result** — no commit (no files changed). Report the three P99 values and the OK checks. If backward-compat (`off==onnd`) or determinism fails, STOP and report.

---

## Task 4: Edit `run-overhead-buffer.sh` (Design A + D)

**Files:**
- Modify: `examples/PowerTCP/run-overhead-buffer.sh`

- [ ] **Step 1: Default counts + 3 h timeout** — change the `COUNTS` default line to include 16384 and add a timeout var. Replace:
```bash
COUNTS="${COUNTS:-64,256,1024,4096,16384}"
```
(if it already reads `64,256,1024,4096,16384`, leave as-is) and ensure a `PER_RUN_TIMEOUT="${PER_RUN_TIMEOUT:-10800}"` line exists near the other variable definitions (add it just after the `OUT_MD=` line if absent).

- [ ] **Step 2: Spread-burst flow generation** — change the `gen-sweep-flows.py` call to add the window flags:
```bash
python3 "$SCRIPT_DIR/gen-sweep-flows.py" --out_dir "$FLOW_DIR" --sizes "$COUNTS" --bytes 1000000 \
    --start_time 0.12 --rand_start_window 0.01
```

- [ ] **Step 3: Flat SIM_STOP=0.35** — replace the per-N SIM_STOP ladder:
```bash
    if   (( N <= 1024 )); then SIM_STOP=0.20
    elif (( N <= 4096 )); then SIM_STOP=0.25
    else                       SIM_STOP=0.50; echo "[warn] N=$N is heavy (large dump / long wall-clock)"; fi
```
with:
```bash
    SIM_STOP=0.35
    (( N > 4096 )) && echo "[warn] N=$N is heavy (long wall-clock; peaks still captured early)"
```

- [ ] **Step 4: Discard stdout (Design D) + use the timeout var** — replace the run invocation:
```bash
    timeout 7200 "$BIN" --conf="$run_conf" --algorithm=9 --transportMode=0 \
        --flowControlMode=0 --wien=false --delayWien=false --windowCheck=0 \
        --monitorSwitchId=74 --monitorThroughputBps=100000000000 --RngRun=1 \
        > "$run_dir/run.log" 2>&1 || echo "    (run timed out or failed)"
```
with:
```bash
    timeout "$PER_RUN_TIMEOUT" "$BIN" --conf="$run_conf" --algorithm=9 --transportMode=0 \
        --flowControlMode=0 --wien=false --delayWien=false --windowCheck=0 \
        --monitorSwitchId=74 --monitorThroughputBps=100000000000 --RngRun=1 \
        > /dev/null 2> "$run_dir/run.err" || echo "    (run timed out or failed)"
```

- [ ] **Step 5: Syntax check**

Run: `bash -n examples/PowerTCP/run-overhead-buffer.sh && echo OK`
Expected: `OK`.

- [ ] **Step 6: Smoke (N=64,256) — restore overhead.md first, then run**

```bash
cd /home/leo/CC_Exp
head -6 examples/PowerTCP/overhead.md > /tmp/oh.md && mv /tmp/oh.md examples/PowerTCP/overhead.md
COUNTS=64,256 examples/PowerTCP/run-overhead-buffer.sh
```
Expected: ends `## OVERHEAD-BUFFER SWEEP FINISHED ...`; `overhead.md` gains the LPCC buffer section with numeric 64/256 cells; **no `run.log`** under `dump_burst_overhead/lpcc/*/` (only `run.err`, `flow_table.txt`, etc.).

- [ ] **Step 7: Verify Design A + D effects**

```bash
cd /home/leo/CC_Exp
echo "start times in window?"; awk 'NR>1{print $6}' examples/PowerTCP/sweep_flows/flow-burstExp-256.txt | sort -n | sed -n '1p;$p'
echo "no giant run.log?"; ls examples/PowerTCP/dump_burst_overhead/lpcc/256/ ; du -sh examples/PowerTCP/dump_burst_overhead/lpcc/256/
tail -12 examples/PowerTCP/overhead.md
```
Expected: start times within [0.12, 0.13]; dir is small (no multi-GB run.log); buffer table present.

- [ ] **Step 8: Commit**

```bash
cd /home/leo/CC_Exp
git add examples/PowerTCP/run-overhead-buffer.sh examples/PowerTCP/overhead.md
git -c user.name="Leo" -c user.email="NadineBlakevrg@californiamail.com" commit -m "feat(buffer sweep): spread burst 0.12-0.13s, SIM 0.35s, N->16384, discard stdout (Design A+D)"
```

---

## Task 5: Edit `script-ab-maintenance.sh` (Design C + D)

**Files:**
- Modify: `examples/PowerTCP/script-ab-maintenance.sh`

- [ ] **Step 1: Enable pipefail** — directly after the `set -e` line near the top, add:
```bash
set -o pipefail
```

- [ ] **Step 2: Counts default to 64,256,1024,4096** — ensure the counts line reads:
```bash
COUNTS="${COUNTS:-64,256,1024,4096}"
```
(it currently reads `64,256,1024,4096,16384` — change it to drop 16384 for the maintenance rerun, since 16384 cannot complete and FCT would be meaningless).

- [ ] **Step 3: Spread-burst flow generation** — change the `gen-sweep-flows.py` call to:
```bash
python3 "$SCRIPT_DIR/gen-sweep-flows.py" --out_dir "$FLOW_DIR" --sizes "$COUNTS" --bytes 1000000 \
    --start_time 0.12 --rand_start_window 0.01
```

- [ ] **Step 4: Flat SIM_STOP=0.35** — replace the SIM_STOP ladder inside the loop:
```bash
    if   (( N <= 1024 )); then SIM_STOP=0.20
    elif (( N <= 4096 )); then SIM_STOP=0.25
    else                       SIM_STOP=0.50
    fi
```
with:
```bash
    SIM_STOP=0.35
```

- [ ] **Step 5: Per-arm delay keys in the awk rewrite** — the awk block already rewrites `FLOW_TABLE_MAINTENANCE` from `maint_val`. Add the two delay keys to the generated config, per arm, right after the awk redirect (after the `grep -q "^FLOW_TABLE_MAINTENANCE " ...` line). Insert:
```bash
        if [[ "$maint" == "on" ]]; then dmin=20; dmax=50; else dmin=0; dmax=0; fi
        echo "FLOW_TABLE_OP_DELAY_MIN_NS $dmin" >> "$run_conf"
        echo "FLOW_TABLE_OP_DELAY_MAX_NS $dmax" >> "$run_conf"
```

- [ ] **Step 6: Filter stdout (Design D)** — replace the run invocation:
```bash
        timeout "$PER_RUN_TIMEOUT" "$BIN" \
            --conf="$run_conf" --algorithm="$cc" --transportMode="$tp" \
            --flowControlMode="$fc" --wien="$wien" --delayWien="$delay" \
            --windowCheck="$window" \
            --monitorSwitchId=74 --monitorThroughputBps=100000000000 --RngRun=1 \
            > "$run_dir/run.log" 2>&1 || echo "    (run failed/timeout)"
```
with:
```bash
        timeout "$PER_RUN_TIMEOUT" "$BIN" \
            --conf="$run_conf" --algorithm="$cc" --transportMode="$tp" \
            --flowControlMode="$fc" --wien="$wien" --delayWien="$delay" \
            --windowCheck="$window" \
            --monitorSwitchId=74 --monitorThroughputBps=100000000000 --RngRun=1 2>&1 \
            | grep -E --line-buffered 'MONITOR_TARGET|PortAgg -1 throughput' > "$run_dir/run.log" \
            || echo "    (run failed/timeout or no monitor lines)"
```

- [ ] **Step 7: Syntax check**

Run: `bash -n examples/PowerTCP/script-ab-maintenance.sh && echo OK`
Expected: `OK`.

- [ ] **Step 8: Smoke (COUNTS=64, both arms)**

```bash
cd /home/leo/CC_Exp
COUNTS=64 examples/PowerTCP/script-ab-maintenance.sh
echo "== run.log small & filtered? =="
du -sh examples/PowerTCP/dump_burst_overhead_ab/on/64/run.log
head -2 examples/PowerTCP/dump_burst_overhead_ab/on/64/run.log
echo "== on config has delay keys? =="
grep FLOW_TABLE_OP_DELAY examples/PowerTCP/dump_burst_overhead_ab/on/64/config.txt
echo "== maint table =="
cat examples/PowerTCP/overhead-maint.md
```
Expected: ends `## wrote .../overhead-maint.md ##`; `run.log` is small (KB–MB) and contains only `MONITOR_TARGET`/`PortAgg` lines; on-arm config shows `FLOW_TABLE_OP_DELAY_MIN_NS 20` / `MAX_NS 50`; `overhead-maint.md` has numeric Goodput/P99 for the 64 column (On/P99 may be ≥ Off/P99 by a tiny amount, or equal at 2-decimal display — note which).

- [ ] **Step 9: Remove the partial 64-only artifact; commit only the script**

```bash
cd /home/leo/CC_Exp
rm -f examples/PowerTCP/overhead-maint.md
git add examples/PowerTCP/script-ab-maintenance.sh
git -c user.name="Leo" -c user.email="NadineBlakevrg@californiamail.com" commit -m "feat(maint A/B): spread burst + SIM 0.35, per-arm flow-table op delay, filtered stdout (Design C+D)"
```
(The full `overhead-maint.md` is regenerated when the user runs the complete `COUNTS=64,256,1024,4096` sweep — heavy; left to the user as before.)

---

## Self-review notes (addressed)

- **Spec coverage:** Design A → Task 4 (burst window, SIM 0.35, N→16384, timeout); Design B → Tasks 1+2 (knobs, RNG, FIFO-clamped deferred egress send, config wiring, default-0 backward compat); Design C → Task 5 (new workload, per-arm delay keys, counts 64–4096); Design D → Task 4 (buffer stdout→/dev/null) + Task 5 (maintenance grep-filter + pipefail). Verifications (§6 of spec): build (Tasks 1,2), backward-compat + delay-active + determinism (Task 3), log slimming (Tasks 4,5 smokes), burst window (Tasks 3,4).
- **Placeholder scan:** every code step shows full edits; no TBD/TODO. The one judgement call (FCT delta may be sub-ms at 20–50 ns) is called out explicitly with a concrete pass/fail rule (`off==onnd` for backward-compat; `ond>=onnd` and strictly differs at µs precision for delay-active).
- **Type/name consistency:** `m_flowTableOpDelayMinNs/MaxNs`, `m_flowTableOpDelayRng`, `m_egressPipeFreeNs`, `SetFlowTableOpDelayNs`, `DeferredSwitchSend`, globals `flow_table_op_delay_min_ns/max_ns`, config keys `FLOW_TABLE_OP_DELAY_MIN_NS/MAX_NS` — used identically across Tasks 1, 2, 3, 5.
- **Note:** Maintenance counts drop 16384 (can't complete); buffer counts keep 16384 (only peaks needed). This matches the spec's 4096/16384 caveat.
