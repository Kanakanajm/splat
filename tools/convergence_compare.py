#!/usr/bin/env python3
"""
Compute per-checkpoint RMSE between pt_sppNNNN.exr and mit_sppNNNN.exr,
then save a log-log convergence plot as convergence.png.

Usage:
    python tools/convergence_compare.py <output_dir>
"""

import argparse
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


def find_pairs(output_dir: Path) -> list[tuple[int, Path, Path]]:
    pt_files = {int(m.group(1)): p
                for p in output_dir.glob("pt_spp*.exr")
                if (m := re.fullmatch(r"pt_spp(\d+)\.exr", p.name))}
    mit_files = {int(m.group(1)): p
                 for p in output_dir.glob("mit_spp*.exr")
                 if (m := re.fullmatch(r"mit_spp(\d+)\.exr", p.name))}
    common = sorted(set(pt_files) & set(mit_files))
    return [(spp, pt_files[spp], mit_files[spp]) for spp in common]


def main() -> None:
    parser = argparse.ArgumentParser(description="Convergence RMSE comparison")
    parser.add_argument("output_dir", help="Directory containing pt/mit EXR files")
    args = parser.parse_args()

    out = Path(args.output_dir)
    pairs = find_pairs(out)
    if not pairs:
        print("No matching pt_sppNNNN.exr / mit_sppNNNN.exr pairs found.", file=sys.stderr)
        sys.exit(1)

    spps, rmses_pt, rmses_mit = [], [], []
    for spp, pt_path, mit_path in pairs:
        pt_img  = load_exr(str(pt_path))
        mit_img = load_exr(str(mit_path))
        r = rmse(pt_img, mit_img)
        print(f"spp={spp:4d}  RMSE={r:.6f}")
        spps.append(spp)
        rmses_pt.append(r)

    fig, ax = plt.subplots()
    ax.loglog(spps, rmses_pt, marker="o", label="Ours vs Mitsuba3")
    ax.set_xlabel("SPP")
    ax.set_ylabel("RMSE")
    ax.set_title("Convergence comparison")
    ax.legend()
    ax.grid(True, which="both", ls="--", alpha=0.5)

    plot_path = out / "convergence.png"
    fig.savefig(str(plot_path), dpi=150, bbox_inches="tight")
    plt.close(fig)
    print(f"Saved: {plot_path}")


if __name__ == "__main__":
    main()
