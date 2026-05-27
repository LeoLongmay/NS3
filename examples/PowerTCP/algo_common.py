#!/usr/bin/env python3
"""Shared algorithm definitions, LPCC tuning profiles, and utilities.

All experiment runners (run_burst.py, run_fairness.py, run_workload.py,
run_fct.py) import from this module to avoid duplicating algorithm metadata.
"""

import subprocess
from pathlib import Path
from typing import Dict, List, Optional, Tuple

SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parents[1]                        # /home/master01/CC_Exp  (ns-3 root for burst/fairness/workload)
NS3_ROOT = REPO_ROOT / "simulator" / "ns-3.39"           # ns-3 root for fct-eval

# ---------------------------------------------------------------------------
# Algorithm matrix: name → core CLI parameters shared by every experiment
# ---------------------------------------------------------------------------
ALGO_MATRIX: Dict[str, dict] = {
    "DCQCN":    {"algorithm": 1,  "transportMode": 0, "flowControlMode": 0, "windowCheck": 0, "wien": "false", "delayWien": "false"},
    "DCTCP":    {"algorithm": 8,  "transportMode": 0, "flowControlMode": 0, "windowCheck": 1, "wien": "false", "delayWien": "false"},
    "HPCC":     {"algorithm": 3,  "transportMode": 0, "flowControlMode": 0, "windowCheck": 1, "wien": "false", "delayWien": "false"},
    "Timely":   {"algorithm": 7,  "transportMode": 0, "flowControlMode": 0, "windowCheck": 0, "wien": "false", "delayWien": "false"},
    "PowerTCP": {"algorithm": 3,  "transportMode": 0, "flowControlMode": 0, "windowCheck": 1, "wien": "true",  "delayWien": "false"},
    "LPCC":     {"algorithm": 9,  "transportMode": 0, "flowControlMode": 0, "windowCheck": 1, "wien": "false", "delayWien": "false"},
    "BICC":     {"algorithm": 12, "transportMode": 0, "flowControlMode": 0, "windowCheck": 0, "wien": "false", "delayWien": "false"},
    "THEMIS":   {"algorithm": 13, "transportMode": 0, "flowControlMode": 0, "windowCheck": 0, "wien": "false", "delayWien": "false"},
    "GEMINI":   {"algorithm": 11, "transportMode": 0, "flowControlMode": 0, "windowCheck": 1, "wien": "false", "delayWien": "false"},
    "Bifrost":  {"algorithm": 1,  "transportMode": 0, "flowControlMode": 1, "windowCheck": 0, "wien": "false", "delayWien": "false"},
    "BBR":      {"algorithm": 0,  "transportMode": 1, "flowControlMode": 0, "windowCheck": 0, "wien": "false", "delayWien": "false"},
}

# ---------------------------------------------------------------------------
# LPCC tuning profiles — extracted from script-burst.sh comment history.
#
# Usage:
#   python3 run_burst.py --algs LPCC --profile v17
#   python3 run_burst.py --algs LPCC --profile v17 --override lpccThetaUs=1500
#
# Each profile is a complete, self-contained dict of --lpcc* CLI flag values.
# Profiles marked FAILED are kept for reproducibility / comparison.
# ---------------------------------------------------------------------------
LPCC_PROFILES: Dict[str, dict] = {
    # Use C++ Attribute defaults (no --lpcc* flags passed).
    "default": {},

    # v6: early tuning, epsilon 1.5MB
    "v6": {
        "lpccEpsilon": 1500000,
        "lpccThetaUs": 1500,
        "lpccFcnpMinIntervalUs": 500,
        "lpccPerFlowFcnpCooldownUs": 800,
        "lpccFcnpTopK": 3,
        "lpccFcnpTopKHigh": 10,
        "lpccFcnpKHighThreshBytes": 4000000,
        "lpccIncreaseIntervalUs": 65,
        "lpccIncreaseFactor": 0.10,
        "lpccWr": 1.0,
        "lpccKr": 0.12,
    },

    # v7: theta 3ms for smooth steady state
    "v7": {
        "lpccEpsilon": 2000000,
        "lpccThetaUs": 3000,
        "lpccFcnpMinIntervalUs": 600,
        "lpccPerFlowFcnpCooldownUs": 1500,
        "lpccFcnpTopK": 3,
        "lpccFcnpTopKHigh": 10,
        "lpccFcnpKHighThreshBytes": 10000000,
        "lpccIncreaseIntervalUs": 65,
        "lpccIncreaseFactor": 0.10,
        "lpccWr": 2.0,
        "lpccKr": 0.12,
    },

    # v8: theta 2ms, topK 4→13
    "v8": {
        "lpccEpsilon": 2000000,
        "lpccThetaUs": 2000,
        "lpccFcnpMinIntervalUs": 400,
        "lpccPerFlowFcnpCooldownUs": 1200,
        "lpccFcnpTopK": 4,
        "lpccFcnpTopKHigh": 13,
        "lpccFcnpKHighThreshBytes": 5000000,
        "lpccIncreaseIntervalUs": 65,
        "lpccIncreaseFactor": 0.10,
        "lpccWr": 2.0,
        "lpccKr": 0.12,
    },

    # v9: back to v7 theta, keep v8 kHigh=13
    "v9": {
        "lpccEpsilon": 2000000,
        "lpccThetaUs": 3000,
        "lpccFcnpMinIntervalUs": 500,
        "lpccPerFlowFcnpCooldownUs": 1500,
        "lpccFcnpTopK": 3,
        "lpccFcnpTopKHigh": 13,
        "lpccFcnpKHighThreshBytes": 5000000,
        "lpccIncreaseIntervalUs": 65,
        "lpccIncreaseFactor": 0.10,
        "lpccWr": 2.0,
        "lpccKr": 0.12,
    },

    # v10: nearly hit target (400G/<10MB at t=0-11ms & t=70-79ms), but
    # cyclic collapse-recovery because per-FCNP cut (~48%) > AI growth.
    # Root cause: hidden defaults QueueTargetRatio=0.375, DropCapHigh=0.5,
    # AiSuppressMultiplier=15.
    "v10": {
        "lpccEpsilon": 4000000,
        "lpccThetaUs": 1000,
        "lpccFcnpMinIntervalUs": 200,
        "lpccPerFlowFcnpCooldownUs": 400,
        "lpccFcnpTopK": 5,
        "lpccFcnpTopKHigh": 24,
        "lpccFcnpKHighThreshBytes": 8000000,
        "lpccIncreaseIntervalUs": 80,
        "lpccIncreaseFactor": 0.04,
        "lpccWr": 4.0,
        "lpccKr": 0.08,
    },

    # v11 [FAILED]: route B with both qTgtRatio AND dropCapHigh relaxed.
    # Buffer saturated at 134MB. Two simultaneous weakenings made cuts
    # too gentle for the 23-flow incast burst.
    "v11": {
        "lpccEpsilon": 4000000,
        "lpccThetaUs": 1000,
        "lpccFcnpMinIntervalUs": 200,
        "lpccPerFlowFcnpCooldownUs": 400,
        "lpccFcnpTopK": 5,
        "lpccFcnpTopKHigh": 18,
        "lpccFcnpKHighThreshBytes": 8000000,
        "lpccIncreaseIntervalUs": 80,
        "lpccIncreaseFactor": 0.04,
        "lpccWr": 2.0,
        "lpccKr": 0.08,
        "lpccQueueTargetRatio": 1.5,
        "lpccDropCapHigh": 0.3,
        "lpccAiSuppressMultiplier": 5,
    },

    # v12: incast hit 400G+ with qlen <50MB, settled qlen <1.3MB but
    # throughput stuck at 246G — AI gain too small to recoup gentle
    # steady-state FCNP cuts in the qRatio < 1 interpolation band.
    "v12": {
        "lpccEpsilon": 4000000,
        "lpccThetaUs": 1000,
        "lpccFcnpMinIntervalUs": 200,
        "lpccPerFlowFcnpCooldownUs": 400,
        "lpccFcnpTopK": 5,
        "lpccFcnpTopKHigh": 24,
        "lpccFcnpKHighThreshBytes": 8000000,
        "lpccIncreaseIntervalUs": 80,
        "lpccIncreaseFactor": 0.04,
        "lpccWr": 4.0,
        "lpccKr": 0.08,
        "lpccQueueTargetRatio": 1.5,
        "lpccDropCapHigh": 0.5,
        "lpccAiSuppressMultiplier": 5,
    },

    # v13 [FAILED]: incFactor 0.10 was too aggressive — AI explosion
    # refilled buffer to 134MB cap. (= v12 + IncreaseFactor 0.04→0.10)
    "v13": {
        "lpccEpsilon": 4000000,
        "lpccThetaUs": 1000,
        "lpccFcnpMinIntervalUs": 200,
        "lpccPerFlowFcnpCooldownUs": 400,
        "lpccFcnpTopK": 5,
        "lpccFcnpTopKHigh": 24,
        "lpccFcnpKHighThreshBytes": 8000000,
        "lpccIncreaseIntervalUs": 80,
        "lpccIncreaseFactor": 0.10,
        "lpccWr": 4.0,
        "lpccKr": 0.08,
        "lpccQueueTargetRatio": 1.5,
        "lpccDropCapHigh": 0.5,
        "lpccAiSuppressMultiplier": 5,
    },

    # v17 [CURRENT]: bisect qTgtRatio between v14 (1.5) and v16 (3.0).
    # qTgtRatio=2.0 → target 8MB, dropCapHigh at qlen>=16MB.
    # lpccIncreaseFactor: 0.06 → 0.01 to reduce AIMD oscillation amplitude
    # (mult term explosively grew m_rate +6%/tick = +120%/ms when queue empty).
    "v17": {
        "lpccEpsilon": 4000000,
        "lpccThetaUs": 1000,
        "lpccFcnpMinIntervalUs": 200,
        "lpccPerFlowFcnpCooldownUs": 400,
        "lpccFcnpTopK": 5,
        "lpccFcnpTopKHigh": 24,
        "lpccFcnpKHighThreshBytes": 8000000,
        "lpccIncreaseIntervalUs": 80,
        "lpccIncreaseFactor": 0.001,
        "lpccWr": 4.0,
        "lpccKr": 0.08,
        "lpccQueueTargetRatio": 2.0,
        "lpccDropCapHigh": 0.5,
        "lpccAiSuppressMultiplier": 5,
    },
}


# ---------------------------------------------------------------------------
# Utility functions
# ---------------------------------------------------------------------------

def resolve_algorithms(
    selected: Optional[List[str]],
    matrix: Dict[str, dict] = None,
) -> List[Tuple[str, dict]]:
    """Resolve algorithm names (case-insensitive) to (canonical_name, conf) pairs."""
    if matrix is None:
        matrix = ALGO_MATRIX
    all_names = list(matrix.keys())
    if not selected:
        return [(name, matrix[name]) for name in all_names]

    by_lower = {name.lower(): name for name in all_names}
    uniq: List[str] = []
    seen: set = set()
    for raw in selected:
        key = raw.lower()
        if key not in by_lower:
            valid = ", ".join(all_names)
            raise ValueError(f"Unknown algorithm '{raw}'. Valid values: {valid}")
        canon = by_lower[key]
        if canon not in seen:
            seen.add(canon)
            uniq.append(canon)
    return [(name, matrix[name]) for name in uniq]


def run_cmd(cmd: List[str], cwd: Path, log_path: Path = None) -> None:
    """Run a command, optionally capturing stdout+stderr to *log_path*."""
    if log_path is None:
        subprocess.run(cmd, cwd=cwd, check=True)
        return
    with log_path.open("w", encoding="utf-8") as logf:
        subprocess.run(cmd, cwd=cwd, check=True, stdout=logf, stderr=subprocess.STDOUT)


def build_core_args(
    binary: Path,
    conf_file: Path,
    alg_conf: dict,
) -> List[str]:
    """Build the 7 CLI arguments common to every experiment binary."""
    return [
        str(binary),
        f"--conf={conf_file}",
        f"--algorithm={alg_conf['algorithm']}",
        f"--transportMode={alg_conf['transportMode']}",
        f"--flowControlMode={alg_conf['flowControlMode']}",
        f"--windowCheck={alg_conf['windowCheck']}",
        f"--wien={alg_conf['wien']}",
        f"--delayWien={alg_conf['delayWien']}",
    ]


def resolve_lpcc_params(
    profile_name: str,
    overrides: Optional[List[str]] = None,
) -> dict:
    """Merge a named LPCC profile with optional ``key=value`` overrides."""
    if profile_name not in LPCC_PROFILES:
        valid = ", ".join(LPCC_PROFILES.keys())
        raise ValueError(f"Unknown LPCC profile '{profile_name}'. Valid: {valid}")
    params = dict(LPCC_PROFILES[profile_name])
    if overrides:
        for token in overrides:
            if "=" not in token:
                raise ValueError(f"Override must be key=value, got '{token}'")
            key, val = token.split("=", 1)
            # Auto-cast numeric values.
            try:
                val = int(val)
            except ValueError:
                try:
                    val = float(val)
                except ValueError:
                    pass
            params[key] = val
    return params


def build_lpcc_args(params: dict) -> List[str]:
    """Convert an LPCC parameter dict to a list of ``--key=value`` strings."""
    return [f"--{k}={v}" for k, v in params.items()]


def ensure_build(target: str, skip: bool = False, ns3_root: Path = None) -> Path:
    """Build an ns-3 target and return the binary path.

    *target* is relative to the ns-3 root, e.g.
    ``"examples/PowerTCP/powertcp-evaluation-burst"`` or
    ``"scratch/fct-eval"``.

    *ns3_root* selects which ns-3 installation to use.  Defaults to
    ``REPO_ROOT`` (burst/fairness/workload live there).  Pass ``NS3_ROOT``
    for targets under ``simulator/ns-3.39`` (e.g. fct-eval).
    """
    if ns3_root is None:
        ns3_root = REPO_ROOT
    if not skip:
        run_cmd(["./ns3", "build", target], cwd=ns3_root)

    # ns-3.39 build output pattern: build/<dir>/ns3.39-<basename>-<variant>
    parts = target.rsplit("/", 1)
    subdir = parts[0] if len(parts) > 1 else "scratch"
    basename = parts[-1]

    # Prefer optimized > default > debug. Also support release (no suffix).
    for variant in ("optimized", "default", "debug"):
        candidate = ns3_root / "build" / subdir / f"ns3.39-{basename}-{variant}"
        if candidate.exists():
            return candidate
    candidate = ns3_root / "build" / subdir / f"ns3.39-{basename}"
    if candidate.exists():
        return candidate
    raise FileNotFoundError(
        f"Built binary not found for target '{target}' "
        f"under {ns3_root / 'build' / subdir}"
    )
