#!/usr/bin/env python3

import argparse
import sys
from pathlib import Path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Clean historical FCT outputs under mix/<load>%load."
    )
    parser.add_argument(
        "--load",
        type=int,
        required=True,
        help="Load percentage, e.g. 40 means mix/40%%load",
    )
    parser.add_argument(
        "--algo",
        type=str,
        required=True,
        help='Algorithm directory name (e.g. DCQCN, LPCC) or "all".',
    )
    return parser.parse_args()


def remove_files_recursively(root: Path) -> int:
    removed = 0
    for path in root.rglob("*"):
        if path.is_file() or path.is_symlink():
            path.unlink()
            removed += 1
    return removed


def main() -> int:
    args = parse_args()
    ns3_root = Path(__file__).resolve().parent
    mix_root = ns3_root / "mix"
    load_dir = mix_root / f"{args.load}%load"

    if not load_dir.exists() or not load_dir.is_dir():
        print(f"[error] load directory not found: {load_dir}", file=sys.stderr)
        return 1

    if args.algo.lower() == "all":
        removed = remove_files_recursively(load_dir)
        print(f"[ok] removed {removed} files under: {load_dir}")
        return 0

    target_dir = load_dir / args.algo
    if not target_dir.exists() or not target_dir.is_dir():
        print(f"[error] algorithm directory not found: {target_dir}", file=sys.stderr)
        return 1

    removed = remove_files_recursively(target_dir)
    print(f"[ok] removed {removed} files under: {target_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
