# PowerTCP Simulations

All experiment scripts live in this directory (`examples/PowerTCP/`). Each experiment type has a unified Python runner that handles simulation, result parsing, and is ready for plotting in one step.

## Supported Algorithms

DCQCN, DCTCP, HPCC, Timely, PowerTCP, LPCC, BICC, GEMINI, Bifrost, BBR

Algorithm definitions are centralized in `algo_common.py`. All runners share the same `ALGO_MATRIX`, so adding or modifying an algorithm only requires a single edit.

## Incast (Burst)

- **Simulations + parse:** Run `python3 run_burst.py --algs LPCC DCQCN` to launch incast simulations. The raw output is written to `dump_burst/`, and parsed results are automatically extracted to `results_burst/`. No separate parsing step needed.
- **Generate plots:** Run `python3 plot-burst.py` to generate figures (pdf and png) from `results_burst/`.
- **LPCC tuning profiles:** Use `--profile v17` to select a saved parameter set, or `--override lpccThetaUs=1500` to tweak a single parameter without editing code.

```bash
# Run with current best LPCC params
python3 run_burst.py --algs LPCC --profile v17

# Quick parameter tweak
python3 run_burst.py --algs LPCC --profile v17 --override lpccThetaUs=1500

# Skip recompilation if binary is already built
python3 run_burst.py --algs LPCC DCQCN --skip-build
```

## Fairness

- **Simulations + parse:** Run `python3 run_fairness.py --algs GEMINI BBR BICC` to launch fairness tests. Raw output goes to `dump_fairness/`, per-flow throughput results are extracted to `results_fairness/`.
- **Generate plots:** Run `python3 plot-fairness.py` to generate figures (pdf and png).

```bash
python3 run_fairness.py --algs GEMINI BBR BICC
python3 run_fairness.py --algs LPCC --skip-build
```

## Workload

- **Simulations + parse:** Run `python3 run_workload.py --algs DCQCN --loads 0.6` to launch workload tests. Raw output goes to `dump_workload/`, FCT and buffer results are extracted to `results_workload/`.
- **Generate plots:** Run `python3 plot-workload.py` to generate figures (pdf and png).

```bash
# Single load
python3 run_workload.py --algs DCQCN Bifrost BBR --loads 0.6

# Multiple loads
python3 run_workload.py --algs DCQCN LPCC --loads 0.2 0.4 0.6 0.8

# Custom parameters
python3 run_workload.py --algs DCQCN --loads 0.6 --start-time 0.1 --end-time 1.3 --flow-launch-end 0.8
```

## FCT (Flow Completion Time)

- **Simulations:** Run `python3 run_fct.py --algs DCQCN LPCC --loads 40 60` to generate workload traffic and run FCT evaluation. Results are written to `mix/<load>%load/<algorithm>/`.
- **Generate plots:** Run `python3 plot_fct_new.py --mix-dir mix` to generate avg/p99 FCT slowdown figures under `mix/<load>%load/out_fig/`.
- **Clean results:** Run `python3 clean_fct.py --load 40 --algo LPCC` to remove specific result files.

```bash
python3 run_fct.py --algs DCQCN LPCC --loads 40 60 --skip-build
python3 plot_fct_new.py --mix-dir mix
```

## LPCC Parameter Profiles

LPCC tuning profiles are defined in `algo_common.py` under `LPCC_PROFILES`. Each profile is a complete, named parameter set that can be selected via `--profile`:

| Profile | Status | Description |
|---------|--------|-------------|
| `default` | -- | Use C++ Attribute defaults |
| `v6`  | saved  | Early tuning, epsilon 1.5MB |
| `v7`  | saved  | Theta 3ms, smooth steady state |
| `v8`  | saved  | Theta 2ms, topK 4-13 |
| `v9`  | saved  | Back to v7 theta, keep v8 kHigh |
| `v10` | saved  | Nearly hit target, cyclic collapse-recovery |
| `v11` | FAILED | Route B dual relaxation, buffer saturated |
| `v12` | saved  | Incast 400G+ qlen<50MB, throughput stuck 246G |
| `v13` | FAILED | IncreaseFactor 0.10 too aggressive |
| `v17` | saved  | Current best, qTgtRatio=2.0 bisect |

To "fix" a new set of tuned parameters, add a new entry to `LPCC_PROFILES` in `algo_common.py`.

## Project Structure

```
examples/PowerTCP/
    algo_common.py              Shared: ALGO_MATRIX, LPCC_PROFILES, utility functions
    run_burst.py                Burst (incast) experiment runner
    run_fairness.py             Fairness experiment runner
    run_workload.py             Workload (mixed traffic) experiment runner
    run_fct.py                  FCT experiment runner
    plot-burst.py               Burst plotting
    plot-fairness.py            Fairness plotting
    plot-workload.py            Workload plotting
    plot_fct_new.py             FCT plotting (current version)
    plot_fct.py                 FCT plotting (legacy version)
    clean_fct.py                Clean FCT result files
    config-burst.txt            Burst simulation config
    config-fairness.txt         Fairness simulation config
    config-workload.txt         Workload / FCT simulation config
    topology.txt                Full topology
    topology_simple.txt         DCI topology (16 hosts, 11 switches)
    flow-burstExp.txt           Burst flow definition (23 flows)
    flow-fairnessExp.txt        Fairness flow definition
    traffic_gen/                Traffic generation scripts and CDF files
    dump_burst/                 Raw burst simulation output
    dump_fairness/              Raw fairness simulation output
    dump_workload/              Raw workload simulation output
    results_burst/              Parsed burst results (.burst)
    results_fairness/           Parsed fairness results (.1-.4)
    results_workload/           Parsed workload results (.fct, .buf)
    mix/                        FCT results and plots
    powertcp-evaluation-*.cc    C++ simulation source files
```

## Legacy Shell Scripts

The original shell-based scripts are still present but no longer the primary entry point:

- `script-burst.sh` / `results-burst.sh` -> use `run_burst.py` instead
- `script-fairness.sh` / `results-fairness.sh` -> use `run_fairness.py` instead
- `script-workload.sh` / `results-workload.sh` -> use `run_workload.py` instead
- `config.sh` -> no longer needed; paths are resolved automatically by `algo_common.py`
