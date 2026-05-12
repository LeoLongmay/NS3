#!/usr/bin/env python3
import argparse
import subprocess
from pathlib import Path
from typing import List, Tuple


ALGO_MATRIX = {
    "DCQCN": {"algorithm": 1, "transportMode": 0, "flowControlMode": 0, "windowCheck": 0, "wien": "false", "delayWien": "false"},
    "DCTCP": {"algorithm": 8, "transportMode": 0, "flowControlMode": 0, "windowCheck": 1, "wien": "false", "delayWien": "false"},
    "HPCC": {"algorithm": 3, "transportMode": 0, "flowControlMode": 0, "windowCheck": 1, "wien": "false", "delayWien": "false"},
    "Timely": {"algorithm": 7, "transportMode": 0, "flowControlMode": 0, "windowCheck": 0, "wien": "false", "delayWien": "false"},
    "PowerTCP": {"algorithm": 3, "transportMode": 0, "flowControlMode": 0, "windowCheck": 1, "wien": "true", "delayWien": "false"},
    "LPCC": {"algorithm": 9, "transportMode": 0, "flowControlMode": 0, "windowCheck": 0, "wien": "false", "delayWien": "false"},
    "BICC": {"algorithm": 12, "transportMode": 0, "flowControlMode": 0, "windowCheck": 0, "wien": "false", "delayWien": "false"},
    "GEMINI": {"algorithm": 11, "transportMode": 0, "flowControlMode": 0, "windowCheck": 1, "wien": "false", "delayWien": "false"},
    "Bifrost": {"algorithm": 1, "transportMode": 0, "flowControlMode": 1, "windowCheck": 0, "wien": "false", "delayWien": "false"},
    "BBR": {"algorithm": 0, "transportMode": 1, "flowControlMode": 0, "windowCheck": 0, "wien": "false", "delayWien": "false"},
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run fct-eval for 9 CC algorithms under multiple loads")
    parser.add_argument("--topology", default="/home/master01/CC_Exp/examples/PowerTCP/topology_simple.txt")
    parser.add_argument("--cdf", default="/home/master01/CC_Exp/simulator/ns-3.39/traffic_gen/tempcdf.txt")
    parser.add_argument("--loads", nargs="+", type=int, default=[40, 80], help="load percentages, e.g., 40 80")
    parser.add_argument(
        "--algs",
        nargs="+",
        default=None,
        help="algorithm names to run (case-insensitive), e.g., DCQCN HPCC BBR",
    )
    parser.add_argument("--sim-time", type=float, default=2.0, help="traffic generation time window in seconds")
    parser.add_argument("--seed-base", type=int, default=1)
    parser.add_argument("--traffic-script", default="traffic_gen/traffic_gen.py")
    parser.add_argument("--base-conf", default="examples/PowerTCP/config-workload.txt")
    parser.add_argument("--output-root", default="mix")
    parser.add_argument("--skip-build", action="store_true")
    return parser.parse_args()


def resolve_algorithms(selected: List[str]) -> List[Tuple[str, dict]]:
    all_names = list(ALGO_MATRIX.keys())
    if not selected:
        return [(name, ALGO_MATRIX[name]) for name in all_names]

    by_lower = {name.lower(): name for name in all_names}
    picked = []
    for raw in selected:
        key = raw.lower()
        if key not in by_lower:
            valid = ", ".join(all_names)
            raise ValueError(f"Unknown algorithm '{raw}'. Valid values: {valid}")
        canon = by_lower[key]
        picked.append(canon)

    # Keep user order but remove duplicates.
    uniq = []
    seen = set()
    for name in picked:
        if name in seen:
            continue
        seen.add(name)
        uniq.append(name)
    return [(name, ALGO_MATRIX[name]) for name in uniq]


def parse_rate_bps(raw_rate: str) -> int:
    token = raw_rate.strip()
    if not token:
        raise ValueError("Empty rate token")
    suffix = token[-1].upper()
    if suffix in {"G", "M", "K"}:
        value = float(token[:-1])
        scale = {"G": 1e9, "M": 1e6, "K": 1e3}[suffix]
        return int(round(value * scale))
    return int(round(float(token)))


def format_rate_for_traffic_gen(rate_bps: int) -> str:
    if rate_bps % 1_000_000_000 == 0:
        return f"{rate_bps // 1_000_000_000}G"
    if rate_bps % 1_000_000 == 0:
        return f"{rate_bps // 1_000_000}M"
    if rate_bps % 1_000 == 0:
        return f"{rate_bps // 1_000}K"
    return str(rate_bps)


def load_topology_metadata(topology_path: Path) -> Tuple[int, int, List[int], List[Tuple[int, int, int]]]:
    lines: List[str] = []
    with topology_path.open("r", encoding="utf-8") as f:
        lines = [line.strip() for line in f if line.strip()]
    if not lines:
        raise ValueError(f"Empty topology file: {topology_path}")

    parts = lines[0].split()
    if len(parts) not in (3, 4):
        raise ValueError(f"Unsupported topology header format: {lines[0]}")
    node_num = int(parts[0])
    switch_num = int(parts[1])
    link_num = int(parts[-1])
    if node_num <= switch_num:
        raise ValueError(f"Invalid topology header node/switch counts: {lines[0]}")
    if len(lines) < 2:
        raise ValueError(f"Topology missing switch-id line: {topology_path}")

    switch_ids = [int(x) for x in lines[1].split()]
    if len(switch_ids) != switch_num:
        raise ValueError(
            f"Switch-id count mismatch in topology: expected {switch_num}, got {len(switch_ids)}"
        )

    link_lines = lines[2 : 2 + link_num]
    if len(link_lines) != link_num:
        raise ValueError(
            f"Link line count mismatch in topology: expected {link_num}, got {len(link_lines)}"
        )

    links: List[Tuple[int, int, int]] = []
    for line in link_lines:
        fields = line.split()
        if len(fields) < 3:
            raise ValueError(f"Invalid link line in topology: {line}")
        src = int(fields[0])
        dst = int(fields[1])
        rate_bps = parse_rate_bps(fields[2])
        if src < 0 or src >= node_num or dst < 0 or dst >= node_num:
            raise ValueError(f"Link endpoint out of range [0,{node_num - 1}]: {line}")
        links.append((src, dst, rate_bps))
    return node_num, switch_num, switch_ids, links


def infer_host_bandwidth(
    node_num: int, switch_ids: List[int], links: List[Tuple[int, int, int]]
) -> str:
    switch_set = set(switch_ids)
    host_rates = []
    for src, dst, rate_bps in links:
        src_is_host = src not in switch_set
        dst_is_host = dst not in switch_set
        if src_is_host != dst_is_host:
            host_rates.append(rate_bps)
    if not host_rates:
        raise ValueError("Cannot infer host-link bandwidth from topology links")
    # Conservative pick for traffic generation when host links differ.
    return format_rate_for_traffic_gen(min(host_rates))


def build_ecn_maps(links: List[Tuple[int, int, int]]) -> Tuple[str, str, str]:
    rates = sorted({rate_bps for _, _, rate_bps in links})
    if not rates:
        raise ValueError("No link rates found for ECN map generation")

    kmin_pairs = []
    kmax_pairs = []
    pmax_pairs = []
    for rate in rates:
        kmin = max(1, int(round(rate / 250_000_000.0)))
        kmax = 4 * kmin
        kmin_pairs.append((rate, kmin))
        kmax_pairs.append((rate, kmax))
        pmax_pairs.append((rate, 0.2))

    kmin_tokens = " ".join(f"{r} {v}" for r, v in kmin_pairs)
    kmax_tokens = " ".join(f"{r} {v}" for r, v in kmax_pairs)
    pmax_tokens = " ".join(f"{r} {v}" for r, v in pmax_pairs)
    return (
        f"KMIN_MAP {len(kmin_pairs)} {kmin_tokens}",
        f"KMAX_MAP {len(kmax_pairs)} {kmax_tokens}",
        f"PMAX_MAP {len(pmax_pairs)} {pmax_tokens}",
    )


def replace_or_append(lines: List[str], key: str, value: str) -> List[str]:
    prefix = key + " "
    replaced = False
    out = []
    for line in lines:
        if line.startswith(prefix):
            out.append(f"{key} {value}")
            replaced = True
        else:
            out.append(line)
    if not replaced:
        out.append(f"{key} {value}")
    return out


def build_conf(
    base_conf_lines: List[str],
    topology: Path,
    flow_file: Path,
    fct_file: Path,
    trace_file: Path,
    trace_out: Path,
    pfc_out: Path,
    qlen_out: Path,
    simulator_stop_time: float,
    flowgen_start_time: float,
    flowgen_stop_time: float,
    random_seed: int,
    ecn_maps: Tuple[str, str, str],
) -> str:
    lines = list(base_conf_lines)
    lines = replace_or_append(lines, "TOPOLOGY_FILE", str(topology))
    lines = replace_or_append(lines, "FLOW_FILE", str(flow_file))
    lines = replace_or_append(lines, "TRACE_FILE", str(trace_file))
    lines = replace_or_append(lines, "TRACE_OUTPUT_FILE", str(trace_out))
    lines = replace_or_append(lines, "FCT_OUTPUT_FILE", str(fct_file))
    lines = replace_or_append(lines, "PFC_OUTPUT_FILE", str(pfc_out))
    lines = replace_or_append(lines, "QLEN_MON_FILE", str(qlen_out))
    lines = replace_or_append(lines, "SIMULATOR_STOP_TIME", f"{simulator_stop_time:.6f}")
    lines = replace_or_append(lines, "FLOWGEN_START_TIME", f"{flowgen_start_time:.6f}")
    lines = replace_or_append(lines, "FLOWGEN_STOP_TIME", f"{flowgen_stop_time:.6f}")
    lines = replace_or_append(lines, "QLEN_MON_START", str(int(flowgen_start_time * 1e9)))
    lines = replace_or_append(lines, "QLEN_MON_END", str(int(flowgen_stop_time * 1e9)))
    lines = replace_or_append(lines, "RANDOM_SEED", str(random_seed))
    lines = replace_or_append(lines, "KMIN_MAP", ecn_maps[0].split(" ", 1)[1])
    lines = replace_or_append(lines, "KMAX_MAP", ecn_maps[1].split(" ", 1)[1])
    lines = replace_or_append(lines, "PMAX_MAP", ecn_maps[2].split(" ", 1)[1])
    return "\n".join(lines).rstrip() + "\n"


def run_cmd(cmd: List[str], cwd: Path, log_path: Path = None) -> None:
    if log_path is None:
        subprocess.run(cmd, cwd=cwd, check=True)
        return
    with log_path.open("w", encoding="utf-8") as logf:
        subprocess.run(cmd, cwd=cwd, check=True, stdout=logf, stderr=subprocess.STDOUT)


def stretch_flow_start_times(
    flow_file: Path,
    old_start_sec: float,
    old_stop_sec: float,
    new_stop_sec: float,
) -> None:
    if old_stop_sec <= old_start_sec:
        raise ValueError("old flow generation window is invalid")
    if new_stop_sec <= old_start_sec:
        raise ValueError("new flow generation window is invalid")

    scale = (new_stop_sec - old_start_sec) / (old_stop_sec - old_start_sec)
    lines = flow_file.read_text(encoding="utf-8").splitlines()
    if not lines:
        raise ValueError(f"Empty flow file: {flow_file}")

    # Keep header as-is; only stretch per-flow start times.
    out = [lines[0]]
    for raw in lines[1:]:
        if not raw.strip():
            continue
        parts = raw.split()
        if len(parts) not in (5, 6):
            raise ValueError(f"Unexpected flow row format: {raw}")
        old_t = float(parts[-1])
        new_t = old_start_sec + (old_t - old_start_sec) * scale
        parts[-1] = f"{new_t:.9f}"
        out.append(" ".join(parts))
    flow_file.write_text("\n".join(out) + "\n", encoding="utf-8")


def main() -> None:
    args = parse_args()
    ns3_root = Path(__file__).resolve().parent
    topology_path = Path(args.topology)
    cdf_path = Path(args.cdf)
    traffic_script = Path(args.traffic_script)
    base_conf_path = Path(args.base_conf)
    output_root = Path(args.output_root)

    if not topology_path.exists():
        raise FileNotFoundError(f"Topology not found: {topology_path}")
    if not cdf_path.exists():
        raise FileNotFoundError(f"CDF not found: {cdf_path}")
    if not (ns3_root / traffic_script).exists():
        raise FileNotFoundError(f"Traffic generator script not found: {traffic_script}")
    if not (ns3_root / base_conf_path).exists():
        raise FileNotFoundError(f"Base config not found: {base_conf_path}")

    node_num, switch_num, switch_ids, links = load_topology_metadata(topology_path)
    nhost = node_num - switch_num
    bandwidth = infer_host_bandwidth(node_num, switch_ids, links)
    ecn_maps = build_ecn_maps(links)
    algo_plan = resolve_algorithms(args.algs)

    if not args.skip_build:
        run_cmd(["./ns3", "build", "scratch/fct-eval"], cwd=ns3_root)

    binary = ns3_root / "build" / "scratch" / "ns3.39-fct-eval-default"
    if not binary.exists():
        raise FileNotFoundError(f"Built binary not found: {binary}")

    base_conf_lines = (ns3_root / base_conf_path).read_text(encoding="utf-8").splitlines()

    # Extended injection-window mode (keep total traffic bytes unchanged):
    # 1) Generate baseline workload in [flowgen_start, baseline_flowgen_stop].
    # 2) Stretch all flow start times linearly to [flowgen_start, flowgen_stop].
    flowgen_start = 2.0
    baseline_flowgen_stop = 3.0
    flowgen_stop = 4.0
    simulator_stop = 6.0

    for load in args.loads:
        load_tag = f"{load}%load"
        load_dir = ns3_root / output_root / load_tag
        workload_dir = load_dir / "workload"
        workload_dir.mkdir(parents=True, exist_ok=True)

        flow_file = workload_dir / f"flows_{load_tag}.txt"
        traffic_cmd = [
            "python3",
            str(ns3_root / traffic_script),
            "-c",
            str(cdf_path),
            "-n",
            str(nhost),
            "-l",
            f"{load / 100.0}",
            "-b",
            bandwidth,
            "-t",
            f"{baseline_flowgen_stop - flowgen_start}",
            # Rollback logic (old version, do not delete):
            # f"{2.0}",
            "-o",
            str(flow_file),
        ]
        run_cmd(traffic_cmd, cwd=ns3_root)
        stretch_flow_start_times(flow_file, flowgen_start, baseline_flowgen_stop, flowgen_stop)

        # Rollback logic (old version, do not delete):
        # flowgen_start = 2.0
        # flowgen_stop = flowgen_start + 2.0
        # simulator_stop = flowgen_stop + args.sim_time

        for idx, (alg_name, conf) in enumerate(algo_plan):
            alg_dir = load_dir / alg_name
            alg_dir.mkdir(parents=True, exist_ok=True)

            fct_file = alg_dir / f"{alg_name}_out_fct.txt"
            trace_file = alg_dir / f"{alg_name}_in_trace.txt"
            trace_out = alg_dir / f"{alg_name}_out_trace.txt"
            pfc_out = alg_dir / f"{alg_name}_out_pfc.txt"
            qlen_out = alg_dir / f"{alg_name}_out_qlen.txt"
            conf_file = alg_dir / "config.txt"
            run_log = alg_dir / "ns3.log"

            conf_text = build_conf(
                base_conf_lines=base_conf_lines,
                topology=topology_path,
                flow_file=flow_file,
                fct_file=fct_file,
                trace_file=trace_file,
                trace_out=trace_out,
                pfc_out=pfc_out,
                qlen_out=qlen_out,
                    simulator_stop_time=simulator_stop,
                    flowgen_start_time=flowgen_start,
                    flowgen_stop_time=flowgen_stop,
                    random_seed=args.seed_base + load * 100 + idx,
                    ecn_maps=ecn_maps,
                )
            conf_file.write_text(conf_text, encoding="utf-8")

            run_cmd(
                [
                    str(binary),
                    f"--conf={conf_file}",
                    f"--algorithm={conf['algorithm']}",
                    f"--transportMode={conf['transportMode']}",
                    f"--flowControlMode={conf['flowControlMode']}",
                    f"--windowCheck={conf['windowCheck']}",
                    f"--wien={conf['wien']}",
                    f"--delayWien={conf['delayWien']}",
                    f"--randomSeed={args.seed_base + load * 100 + idx}",
                ],
                cwd=ns3_root,
                log_path=run_log,
            )
            print(f"[done] load={load}% alg={alg_name} out={fct_file}")

    print(f"[all done] outputs at: {ns3_root / output_root}")


if __name__ == "__main__":
    main()
