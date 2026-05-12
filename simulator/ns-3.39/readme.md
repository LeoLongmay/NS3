# FCT Experiment Pipeline README

This document explains how to use the following files together in this project:

- `run_fct.py`
- `traffic_gen/traffic_gen.py`
- `scratch/fct-eval.cc`
- `plot_fct.py`

It is written for repeatable congestion-control comparison experiments in the `simulator/ns-3.39` workspace.

---

## 1. Overview

The workflow is:

1. **Generate workload flows** (`traffic_gen.py`)
2. **Build and run ns-3 simulation for selected algorithms** (`run_fct.py` -> `fct-eval.cc`)
3. **Collect FCT outputs** (`*_out_fct.txt`)
4. **Plot Avg/P99 slowdown curves** (`plot_fct.py`)

`run_fct.py` is the main entry point. It calls both traffic generation and simulator execution.

---

## 2. File Responsibilities

### 2.1 `run_fct.py`

Purpose:
- One-command batch execution for multiple load points and multiple CC algorithms.
- Auto-generates per-algorithm config files under `mix/<load>%load/<ALG>/config.txt`.
- Builds `scratch/fct-eval` and runs `build/scratch/ns3.39-fct-eval-default`.

Key behavior:
- Uses one generated workload file per load:
  - `mix/<load>%load/workload/flows_<load>%load.txt`
- Runs algorithms according to internal `ALGO_MATRIX` mapping.
- Writes logs to `mix/<load>%load/<ALG>/ns3.log`.

### 2.2 `traffic_gen/traffic_gen.py`

Purpose:
- Generate the flow list file used by simulator (`FLOW_FILE`).

Current logic (important):
- Fixed random seed (`FIXED_RANDOM_SEED = 1024`) for reproducibility.
- Requires `-n 16` (current mode assumes hosts 0..15).
- Generates **cross-DC-only traffic**:
  - `0..7 -> 8..15`
  - `8..15 -> 0..7`
- No intra-DC flows are generated.

Output format:
- First line: total flow count.
- Remaining lines:
  - `<src> <dst> 3 <size_bytes> <start_time_sec>`

### 2.3 `scratch/fct-eval.cc`

Purpose:
- Core simulation program:
  - parses config file,
  - builds topology,
  - installs transport/CC stack,
  - loads flows,
  - runs simulation,
  - dumps FCT outputs.

Key output:
- `FCT_OUTPUT_FILE` path from config, each line:
  - `src dst sport dport flow_size start_ns fct_ns standalone_fct_ns`

Notes:
- Runtime command-line options can override config fields (used by `run_fct.py`).
- Switch and host CC parameters are set here.

### 2.4 `plot_fct.py`

Purpose:
- Parse all `mix/*%load/*_out_fct.txt` results and generate plots.

Current outputs per load:
- `mix/<load>%load/out_fig/avg_fct.pdf`
- `mix/<load>%load/out_fig/avg_fct.png`
- `mix/<load>%load/out_fig/p99_fct.pdf`
- `mix/<load>%load/out_fig/p99_fct.png`
- `mix/<load>%load/out_fig/legend.pdf` (legend saved separately)

Plot semantics:
- X-axis: flow-size percentile buckets.
- Y-axis: slowdown (`fct_ns / standalone_fct_ns`, clamped to >= 1).
- Supports time-window filtering (`-sT`, `-fT`).

---

## 3. Prerequisites

- Python 3
- ns-3.39 build environment available
- Topology and CDF files exist at configured paths

Recommended run directory:

```bash
cd /home/master01/CC_Exp/simulator/ns-3.39
```

---

## 4. Typical Usage

### 4.1 Run one load with selected algorithms

```bash
python3 run_fct.py --loads 40 --algs DCQCN HPCC LPCC GEMINI BBR
```

### 4.2 Run default matrix (all configured algorithms)

```bash
python3 run_fct.py
```

### 4.3 Skip rebuild when binary already exists

```bash
python3 run_fct.py --loads 40 --skip-build
```

### 4.4 Plot results

```bash
python3 plot_fct.py --mix-dir mix
```

Optional plot time window:

```bash
python3 plot_fct.py --mix-dir mix -sT 2005000000 -fT 10000000000 --step 5
```

---

## 5. `run_fct.py` Arguments

Common options:

- `--topology`: topology file path
- `--cdf`: flow-size CDF for traffic generation
- `--loads`: load list, e.g. `40 80`
- `--algs`: algorithm list (case-insensitive)
- `--sim-time`: traffic generation window (seconds)
- `--seed-base`: base random seed
- `--traffic-script`: traffic generator path
- `--base-conf`: base config template
- `--output-root`: output root directory (`mix` by default)
- `--skip-build`: skip `./ns3 build scratch/fct-eval`

Example:

```bash
python3 run_fct.py \
  --topology /home/master01/CC_Exp/examples/PowerTCP/topology_simple.txt \
  --cdf /home/master01/CC_Exp/simulator/ns-3.39/traffic_gen/tempcdf.txt \
  --loads 40 \
  --algs LPCC GEMINI \
  --sim-time 1.0
```

---

## 6. Generated Output Layout

For load `40` and algorithm `LPCC`:

- `mix/40%load/workload/flows_40%load.txt`
- `mix/40%load/LPCC/config.txt`
- `mix/40%load/LPCC/ns3.log`
- `mix/40%load/LPCC/LPCC_out_fct.txt`
- `mix/40%load/LPCC/LPCC_out_trace.txt`
- `mix/40%load/LPCC/LPCC_out_pfc.txt`
- `mix/40%load/LPCC/LPCC_out_qlen.txt`

Plots:
- `mix/40%load/out_fig/*`

---

## 7. Reproducibility Notes

- `traffic_gen.py` uses fixed seed (`1024`) by default.
- `run_fct.py` also passes per-run random seeds to simulator (`--randomSeed=...`).
- Keep topology/CDF/config paths unchanged for strict reproducibility.

---

## 8. Current Traffic Model Constraints

Current `traffic_gen.py` mode is intentionally constrained:

- `nhost` must be exactly `16`.
- Only cross-DC flows are generated.
- Host groups are hardcoded:
  - DC-A: `0..7`
  - DC-B: `8..15`

If your topology host indexing changes, update this logic first.

---

## 9. LPCC Parameter Profiles

LPCC effective parameter injection point:

- `scratch/fct-eval.cc` (LPCC block near `cc_mode == 9`)
- Parameters are applied via `rdmaHw->SetAttribute(...)` and `sw->SetAttribute("Epsilon", ...)`.

### 9.1 Previously used LPCC settings (before 1ms RTT tuning)

- `epsilon = 262144` (256 KB)
- `lpccThetaUs = 5000` (5 ms)
- `lpccIncreaseIntervalUs = 250` (250 us)
- `lpccBeta = 0.10`
- `lpccWr = 2.0`
- `lpccKr = 0.20`
- `lpccFcnpInterval = 100 us`

### 9.2 Current LPCC settings for ~1ms WAN RTT

- `epsilon = 131072` (128 KB)
- `lpccThetaUs = 1000` (1 ms)
- `lpccIncreaseIntervalUs = 100` (100 us)
- `lpccBeta = 0.06`
- `lpccWr = 1.5`
- `lpccKr = 0.12`
- `lpccFcnpInterval = 50 us`

Rationale:

- Shorter RTT requires faster congestion-window cadence (`theta`, increase interval).
- Smaller threshold and gentler reduction caps (`wr`, `kr`, `beta`) reduce overshoot and queue persistence in low-RTT long-haul links.

---

## 10. Troubleshooting

### 10.1 `CalledProcessError` from `run_fct.py`

Check:
- `mix/<load>%load/<ALG>/ns3.log`
- whether `build/scratch/ns3.39-fct-eval-default` exists
- whether topology and flow file paths in generated `config.txt` are valid

### 10.2 Too few lines in `*_out_fct.txt`

Possible causes:
- simulation stop time too short for long flows
- heavy congestion causing many unfinished flows
- runtime failure (see `ns3.log` end for `SimulationSeconds` line)

### 10.3 Plot looks noisy (sharp spikes)

Reasons:
- small sample size per percentile bucket
- high-tail metric (P99) with too few flows

Mitigation:
- increase `--step` (e.g. 10)
- extend simulation/runtime to finish more flows
- compare with consistent flow-completion subsets

---

## 11. Suggested End-to-End Command Set

```bash
cd /home/master01/CC_Exp/simulator/ns-3.39

# 1) run experiments
python3 run_fct.py --loads 40 --algs DCQCN HPCC LPCC GEMINI BBR

# 2) plot
python3 plot_fct.py --mix-dir mix
```
