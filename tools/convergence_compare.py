#!/usr/bin/env python3
"""
Compute per-checkpoint RMSE and save a log-log convergence plot.

Auto-detects comparison mode from files present in output_dir:

  vs_pass*.exr + pm_spp*.exr   → VS vs PM final  (each VS checkpoint vs last PM)
  ss_pass*.exr + pm_spp*.exr   → SS vs PM final  (each SS checkpoint vs last PM)
  pt_spp*.exr  + mit_spp*.exr  → PT vs Mitsuba   (paired by SPP)
  pm_spp*.exr  + mit_spp*.exr  → PM vs Mitsuba   (paired by SPP)
  pm_spp*.exr  + pt_spp*.exr   → PM vs PathTracer (paired by SPP)

Usage:
    python tools/convergence_compare.py <output_dir>
"""

import re
import sys
from pathlib import Path

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt


def load_exr(path: str) -> np.ndarray:
    import mitsuba as mi
    mi.set_variant("scalar_rgb")
    bmp = mi.Bitmap(path)
    return np.array(bmp.convert(mi.Bitmap.PixelFormat.RGB, mi.Struct.Type.Float32, False))


def rmse(a: np.ndarray, b: np.ndarray) -> float:
    return float(np.sqrt(np.mean((a - b) ** 2)))


def find_spp_files(output_dir: Path, prefix: str) -> dict[int, Path]:
    pat = re.compile(rf"{re.escape(prefix)}(\d+)\.exr")
    return {int(m.group(1)): p
            for p in output_dir.glob(f"{prefix}*.exr")
            if (m := pat.fullmatch(p.name))}


# Alias: pass-indexed files use the same filename pattern.
find_pass_files = find_spp_files


def find_pairs(a: dict, b: dict) -> list[tuple[int, Path, Path]]:
    common = sorted(set(a) & set(b))
    return [(spp, a[spp], b[spp]) for spp in common]


def plot_rmse(pairs: list, xlabel: str, label: str, out: Path, title: str) -> None:
    xs, ys = [], []
    for spp, pa, pb in pairs:
        r = rmse(load_exr(str(pa)), load_exr(str(pb)))
        print(f"spp={spp:4d}  RMSE={r:.6f}")
        xs.append(spp)
        ys.append(r)

    fig, ax = plt.subplots()
    ax.loglog(xs, ys, marker="o", label=label)
    ax.set_xlabel(xlabel)
    ax.set_ylabel("RMSE")
    ax.set_title(title)
    ax.legend()
    ax.grid(True, which="both", ls="--", alpha=0.5)
    plot_path = out / "convergence.png"
    fig.savefig(str(plot_path), dpi=150, bbox_inches="tight")
    plt.close(fig)
    print(f"Saved: {plot_path}")


def main() -> None:
    import argparse
    parser = argparse.ArgumentParser(description="Convergence RMSE comparison")
    parser.add_argument("output_dir", help="Directory containing EXR checkpoints")
    args = parser.parse_args()

    out = Path(args.output_dir)

    vs  = find_pass_files(out, "vs_pass")
    ss  = find_pass_files(out, "ss_pass")
    pt  = find_spp_files(out, "pt_spp")
    mit = find_spp_files(out, "mit_spp")
    pm  = find_spp_files(out, "pm_spp")

    if vs and pm:
        ref_path = pm[max(pm.keys())]
        pairs = [(pass_n, path, ref_path) for pass_n, path in sorted(vs.items())]
        plot_rmse(pairs, "VS pass", "VS vs PM (final)", out,
                  "Volume Splat convergence vs PM reference")
    elif ss and pm:
        # Each SS checkpoint vs the final (highest-SPP) PM image as reference.
        ref_path = pm[max(pm.keys())]
        pairs = [(pass_n, path, ref_path) for pass_n, path in sorted(ss.items())]
        plot_rmse(pairs, "SS pass", "SS vs PM (final)", out,
                  "Surface Splat convergence vs PM reference")
    elif pm and pt:
        pairs = find_pairs(pm, pt)
        if not pairs:
            print("No matching pm_spp / pt_spp pairs found.", file=sys.stderr)
            sys.exit(1)
        plot_rmse(pairs, "SPP", "PM vs PathTracer", out, "Photon Mapper vs Path Tracer")
    elif pm and mit:
        pairs = find_pairs(pm, mit)
        if not pairs:
            print("No matching pm_spp / mit_spp pairs found.", file=sys.stderr)
            sys.exit(1)
        plot_rmse(pairs, "SPP", "PM vs Mitsuba", out, "Photon Mapper vs Mitsuba")
    elif pt and mit:
        pairs = find_pairs(pt, mit)
        if not pairs:
            print("No matching pt_spp / mit_spp pairs found.", file=sys.stderr)
            sys.exit(1)
        plot_rmse(pairs, "SPP", "Ours vs Mitsuba3", out, "Convergence comparison")
    else:
        print("No recognised EXR checkpoint pairs found in " + str(out), file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
