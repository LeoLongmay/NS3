#!/usr/bin/python3

import argparse
import math
import re
from pathlib import Path

import matplotlib.pyplot as plt
from matplotlib.ticker import FuncFormatter, ScalarFormatter, LogLocator
from cycler import cycler


C = [
    "xkcd:grass green",
    "xkcd:blue",
    "xkcd:purple",
    "xkcd:teal",
    "xkcd:brick red",
    "xkcd:black",
    "xkcd:brown",
    "xkcd:grey",
    "xkcd:orange",
]

LS = [
    "solid",
    "dashed",
    "dotted",
    "dashdot",
]

M = [
    "o",
    "s",
    "x",
    "v",
    "D",
]

ALGO_ORDER = [
    "DCQCN",
    "BiCC",
    "HPCC",
    "Timely",
    "DCTCP",
    "PowerTCP",
    "Gemini",
    "Bifrost",
    # "BBR",
    "THEMIS",
    "LPCC",
]


def setup():
    def lcm(a, b):
        return abs(a * b) // math.gcd(a, b)

    def add_two(c1, c2):
        l = lcm(len(c1), len(c2))
        c1 = c1 * (l // len(c1))
        c2 = c2 * (l // len(c2))
        return c1 + c2

    def add_all(*cyclers):
        cur = None
        for c in cyclers:
            if cur is None:
                cur = c
            else:
                cur = add_two(cur, c)
        return cur

    plt.rc(
        "axes",
        prop_cycle=add_all(
            cycler(color=C),
            cycler(linestyle=LS),
            cycler(marker=M),
        ),
    )
    plt.rc("lines", markersize=5)
    plt.rc("legend", handlelength=3, handleheight=1.5, labelspacing=0.25)
    plt.rcParams["font.size"] = 12
    plt.rcParams["pdf.fonttype"] = 42
    plt.rcParams["ps.fonttype"] = 42


def get_pctl(sorted_values, p):
    if not sorted_values:
        return float("nan")
    idx = int(len(sorted_values) * p)
    if idx >= len(sorted_values):
        idx = len(sorted_values) - 1
    return sorted_values[idx]


def smooth_series(values, window, passes):
    if passes <= 0 or window <= 1 or len(values) < 3:
        return list(values)

    if window % 2 == 0:
        window += 1
    radius = window // 2
    weights = [radius + 1 - abs(i - radius) for i in range(window)]

    current = list(values)
    for _ in range(passes):
        nxt = []
        for idx in range(len(current)):
            wsum = 0.0
            acc = 0.0
            for off, w in enumerate(weights):
                pos = idx + off - radius
                if pos < 0:
                    pos = 0
                elif pos >= len(current):
                    pos = len(current) - 1
                acc += current[pos] * w
                wsum += w
            nxt.append(acc / wsum if wsum > 0 else current[idx])
        current = nxt
    return current


def size2str(steps):
    result = []
    for step in steps:
        if step <= 0:
            result.append("0")
            continue
        if step < 10_000:
            result.append("{:.1f}K".format(step / 1000))
        elif step < 1_000_000:
            result.append("{:.0f}K".format(step / 1000))
        elif step < 1_000_000_000:
            result.append("{:.1f}M".format(step / 1_000_000))
        else:
            result.append("{:.1f}G".format(step / 1_000_000_000))
    return result


def pick_even_indices(length, target_count):
    if length <= 0:
        return []
    if target_count <= 1 or length == 1:
        return [0]
    if length <= target_count:
        return list(range(length))
    idx = []
    for i in range(target_count):
        pos = round(i * (length - 1) / (target_count - 1))
        idx.append(pos)
    return sorted(set(idx))


def parse_fct_file(path: Path):
    rows = []
    with path.open("r", encoding="utf-8") as f:
        for line in f:
            parts = line.strip().split()
            if len(parts) < 8:
                continue
            try:
                size = int(parts[4])
                start_ns = int(parts[5])
                fct_ns = int(parts[6])
                base_ns = int(parts[7])
            except ValueError:
                continue
            rows.append((size, start_ns, fct_ns, base_ns))
    return rows


def get_curve(rows, time_start, time_end, step):
    filtered = []
    for size, start_ns, fct_ns, base_ns in rows:
        if start_ns > time_start and (start_ns + fct_ns) < time_end:
            if base_ns <= 0:
                continue
            slowdown = fct_ns / base_ns
            if slowdown < 1:
                slowdown = 1.0
            filtered.append((size, slowdown))

    if not filtered:
        return None

    filtered.sort(key=lambda x: x[0])
    n = len(filtered)

    xvals = []
    sizes = []
    avg_slow = []
    p99_slow = []
    for i in range(0, 100, step):
        l = int(i * n / 100)
        r = int((i + step) * n / 100)
        bucket = filtered[l:r]
        if not bucket:
            continue
        slowdowns = sorted([x[1] for x in bucket])
        xvals.append(i + step)
        sizes.append(bucket[-1][0])
        avg_slow.append(sum(slowdowns) / len(slowdowns))
        p99_slow.append(get_pctl(slowdowns, 0.99))

    if not xvals:
        return None

    return {"xvals": xvals, "size": sizes, "avg": avg_slow, "p99": p99_slow}


def discover_inputs(mix_dir: Path):
    load_map = {}
    for load_dir in sorted(mix_dir.glob("*%load*")):
        if not load_dir.is_dir():
            continue
        m = re.match(r"^(\d+)%load(?:_\w+)?$", load_dir.name)
        if not m:
            continue
        alg_files = {}
        for alg_dir in sorted(load_dir.iterdir()):
            if not alg_dir.is_dir():
                continue
            if alg_dir.name in {"workload", "out_fig"}:
                continue
            fcts = list(alg_dir.glob("*_out_fct.txt"))
            if not fcts:
                continue
            alg_files[alg_dir.name] = fcts[0]
        if alg_files:
            load_map[load_dir] = alg_files
    return load_map


def plot_metric(
    load_dir: Path,
    curves: dict,
    metric_key: str,
    ylabel: str,
    filename_prefix: str,
    smooth_window: int,
    smooth_passes: int,
):
    fig = plt.figure(figsize=(4, 4))
    ax = fig.add_subplot(111)
    fig.tight_layout()

    ax.set_xlabel("Flow Size (Bytes)", fontsize=12)
    ax.set_ylabel(ylabel, fontsize=12)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    ax.yaxis.set_ticks_position("left")
    ax.xaxis.set_ticks_position("bottom")

    smoothed = {}
    y_values = []
    for name, c in curves.items():
        vals = smooth_series(c[metric_key], smooth_window, smooth_passes)
        smoothed[name] = vals
        for v in vals:
            if v is not None and math.isfinite(v) and v > 0:
                y_values.append(v)
    if not y_values:
        return

    y_min = min(y_values)
    y_max = max(y_values)
    if y_max / y_min >= 20:
        ax.set_yscale("log")
        lower = 0.8
        upper = y_max * 1.1
        if lower >= upper:
            upper = lower * 1.1
        ax.set_ylim(lower, upper)

        ax.yaxis.set_major_locator(LogLocator(base=10.0))

        def pow10_tick(val, _):
            if val <= 0:
                return ""
            e = round(math.log10(val))
            if abs(val - 10 ** e) > 1e-8 * max(val, 1.0):
                return ""
            return rf"$10^{{{int(e)}}}$"

        ax.yaxis.set_major_formatter(FuncFormatter(pow10_tick))
    else:
        ax.set_yscale("linear")
        span = y_max - y_min
        pad = max(span * 0.12, 0.05 * max(1.0, y_max))
        lower = 0.8
        upper = y_max + pad
        if lower >= upper:
            upper = lower + 0.5
        ax.set_ylim(lower, upper)
        sf = ScalarFormatter(useOffset=False)
        sf.set_scientific(False)
        ax.yaxis.set_major_formatter(sf)
        ticks = [t for t in ax.get_yticks() if t >= 1.0]
        if 1.0 not in ticks:
            ticks = [1.0] + ticks
        ax.set_yticks(sorted(set(ticks)))

    plotted = []
    legend_handles = []
    legend_labels = []
    for name in ALGO_ORDER:
        if name not in curves:
            continue
        curve = curves[name]
        metric_vals = smoothed[name]
        line, = ax.plot(
            curve["xvals"],
            metric_vals,
            markersize=1.0,
            linewidth=2.0,
            label=name,
        )
        plotted.append(name)
        legend_handles.append(line)
        legend_labels.append(name)
    for name in sorted(curves.keys()):
        if name in plotted:
            continue
        curve = curves[name]
        metric_vals = smoothed[name]
        line, = ax.plot(
            curve["xvals"],
            metric_vals,
            markersize=1.0,
            linewidth=2.0,
            label=name,
        )
        legend_handles.append(line)
        legend_labels.append(name)

    all_x = sorted({x for curve in curves.values() for x in curve["xvals"]})
    if not all_x:
        return
    x_max = max(all_x)
    ax.set_xscale("linear")
    ax.set_xlim(0.0, x_max * 1.02)

    global_size_by_x = {}
    for x in all_x:
        vals = []
        for curve in curves.values():
            if x in curve["xvals"]:
                pos = curve["xvals"].index(x)
                vals.append(curve["size"][pos])
        if vals:
            global_size_by_x[x] = max(vals)

    tick_idx = pick_even_indices(len(all_x), 8)
    ticks = [all_x[i] for i in tick_idx]
    labels = size2str([global_size_by_x.get(t, 0) for t in ticks])
    ax.set_xticks(ticks)
    ax.set_xticklabels(labels, fontsize=12)
    ax.tick_params(axis="x", rotation=40)

    ax.grid(which="minor", alpha=0.2)
    ax.grid(which="major", alpha=0.5)

    out_dir = load_dir / "out_fig"
    out_dir.mkdir(parents=True, exist_ok=True)

    if legend_handles:
        legend_fig = plt.figure(figsize=(6, 1.2))
        legend_fig.legend(
            legend_handles,
            legend_labels,
            loc="center",
            frameon=False,
            # ncol=min(4, max(1, len(legend_labels))),
            ncol=9,
            fontsize=12,
            handlelength=3,
            columnspacing=0.8,
            labelspacing=0.4,
        )
        legend_fig.savefig(out_dir / "legend_new.pdf", transparent=False, bbox_inches="tight")
        plt.close(legend_fig)

    fig.tight_layout()
    plt.savefig(out_dir / f"{filename_prefix}_new.pdf", transparent=False, bbox_inches="tight")
    plt.savefig(out_dir / f"{filename_prefix}_new.png", transparent=False, bbox_inches="tight", dpi=200)
    plt.close()


def main():
    parser = argparse.ArgumentParser(description="Plot Avg/P99 FCT curves from fct-eval output files")
    parser.add_argument("--mix-dir", default="mix", help="root mix directory")
    parser.add_argument(
        "-sT",
        "--time_limit_begin",
        dest="time_limit_begin",
        action="store",
        type=int,
        default=2_005_000_000,
        help="only consider flows that start after T (ns), default=2005000000",
    )
    parser.add_argument(
        "-fT",
        "--time_limit_end",
        dest="time_limit_end",
        action="store",
        type=int,
        default=6_000_000_000,
        help="only consider flows that finish before T (ns), default=6000000000 (matches run_fct.py simulator_stop=6.0s)",
    )
    parser.add_argument("--step", type=int, default=4, help="percentile step for x-axis buckets")
    parser.add_argument(
        "--smooth-window",
        type=int,
        default=5,
        help="odd moving window size used for curve smoothing",
    )
    parser.add_argument(
        "--smooth-passes",
        type=int,
        default=1,
        help="number of smoothing passes",
    )
    args = parser.parse_args()

    mix_dir = Path(args.mix_dir).resolve()
    if not mix_dir.exists():
        raise FileNotFoundError(f"mix directory not found: {mix_dir}")

    load_map = discover_inputs(mix_dir)
    if not load_map:
        print("No *_out_fct.txt found under mix/*%load/*/")
        return

    for load_dir, alg_files in sorted(load_map.items(), key=lambda x: x[0].name):
        curves = {}
        for alg_name, fct_file in alg_files.items():
            rows = parse_fct_file(fct_file)
            curve = get_curve(rows, args.time_limit_begin, args.time_limit_end, args.step)
            if curve is not None:
                curves[alg_name] = curve

        if not curves:
            print(f"[skip] {load_dir}: no usable rows after time filter")
            continue

        plot_metric(
            load_dir,
            curves,
            "avg",
            "Avg FCT Slowdown",
            "avg_fct",
            args.smooth_window,
            args.smooth_passes,
        )
        plot_metric(
            load_dir,
            curves,
            "p99",
            "P99 FCT Slowdown",
            "p99_fct",
            args.smooth_window,
            args.smooth_passes,
        )
        print(f"[done] {load_dir / 'out_fig'}")


if __name__ == "__main__":
    setup()
    main()
