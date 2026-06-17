#!/usr/bin/env python3
"""
RMSE comparison and convergence plot for splat output directories.

Reads manifest.json when present; falls back to filename heuristics.

  sample mode     — paired-checkpoint RMSE between two SPP-based methods (pt/pm/mitsuba)
  convergence mode — splatter checkpoint series vs a single reference image (ss/vs/pvs vs pm/mitsuba)

Usage:
    python3 tools/convergence_compare.py <output_dir> [--mode sample|convergence]
                                         [--ref <method>] [--methods <a> <b>]
"""

import argparse
import csv
import json
import re
import sys
from pathlib import Path
from typing import Optional

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt


# ── EXR / RMSE helpers ────────────────────────────────────────────────────────

def load_exr(path: str) -> np.ndarray:
    import mitsuba as mi
    mi.set_variant("scalar_rgb")
    bmp = mi.Bitmap(path)
    return np.array(bmp.convert(mi.Bitmap.PixelFormat.RGB, mi.Struct.Type.Float32, False))


def rmse(a: np.ndarray, b: np.ndarray) -> float:
    return float(np.sqrt(np.mean((a - b) ** 2)))


# ── File discovery ────────────────────────────────────────────────────────────

def find_indexed_files(output_dir: Path, prefix: str) -> dict[int, Path]:
    pat = re.compile(rf"{re.escape(prefix)}(\d+)\.exr")
    return {int(m.group(1)): p
            for p in output_dir.glob(f"{prefix}*.exr")
            if (m := pat.fullmatch(p.name))}


_PREFIXES: dict[str, str] = {
    "pt":      "pt_spp",
    "pm":      "pm_spp",
    "mitsuba": "mit_spp",
    "ss":      "ss_pass",
    "vs":      "vs_pass",
    "pvs":     "pvs_pass",
}
_CP_KIND: dict[str, str] = {
    "pt": "spp", "pm": "spp", "mitsuba": "spp",
    "ss": "pass", "vs": "pass", "pvs": "pass",
}
_SPLATTER_METHODS = {"ss", "vs", "pvs"}

_SAMPLE_PRIORITY = [
    ("pt", "mitsuba"),
    ("pm", "mitsuba"),
    ("pt", "pm"),
]
_CONVERGENCE_PRIORITY = [
    ("pvs", "mitsuba"), ("vs", "mitsuba"), ("ss", "mitsuba"),
    ("pvs", "pm"),      ("vs", "pm"),      ("ss", "pm"),
]


def discover_heuristic(output_dir: Path) -> dict[str, dict]:
    result = {}
    for name, prefix in _PREFIXES.items():
        files = find_indexed_files(output_dir, prefix)
        if files:
            result[name] = {"files": files, "params": {}, "checkpoints": _CP_KIND[name]}
    return result


# ── Manifest parsing ──────────────────────────────────────────────────────────

def load_manifest(output_dir: Path) -> Optional[dict]:
    p = output_dir / "manifest.json"
    return json.loads(p.read_text()) if p.exists() else None


def manifest_to_method_data(manifest: dict, output_dir: Path) -> dict[str, dict]:
    result = {}
    for entry in manifest.get("methods", []):
        mtype = entry["type"]
        prefix = _PREFIXES.get(mtype)
        if prefix is None:
            continue
        pat = re.compile(rf"{re.escape(prefix)}(\d+)\.exr")
        files = {int(m.group(1)): output_dir / fname
                 for fname in entry.get("files", [])
                 if (m := pat.fullmatch(fname))}
        result[mtype] = {
            "files": files,
            "params": entry.get("params", {}),
            "checkpoints": entry.get("checkpoints", _CP_KIND.get(mtype, "spp")),
        }
    return result


# ── Auto-detection ────────────────────────────────────────────────────────────

def auto_detect(available: set[str]) -> Optional[tuple[str, str, str]]:
    for a, b in _SAMPLE_PRIORITY:
        if a in available and b in available:
            return "sample", a, b
    for a, b in _CONVERGENCE_PRIORITY:
        if a in available and b in available:
            return "convergence", a, b
    return None


# ── Computation ───────────────────────────────────────────────────────────────

def compute_sample(files_a: dict[int, Path],
                   files_b: dict[int, Path]) -> list[tuple[int, float]]:
    common = sorted(set(files_a) & set(files_b))
    if not common:
        return []
    results = []
    for idx in common:
        r = rmse(load_exr(str(files_a[idx])), load_exr(str(files_b[idx])))
        print(f"  checkpoint={idx:4d}  RMSE={r:.6f}")
        results.append((idx, r))
    return results


def compute_convergence(files_a: dict[int, Path],
                        ref_path: Path) -> list[tuple[int, float]]:
    ref = load_exr(str(ref_path))
    results = []
    for idx in sorted(files_a):
        r = rmse(load_exr(str(files_a[idx])), ref)
        print(f"  checkpoint={idx:4d}  RMSE={r:.6f}")
        results.append((idx, r))
    return results


# ── Export ────────────────────────────────────────────────────────────────────

def export_csv(output_dir: Path, cp_label: str, data: list[tuple[int, float]]) -> None:
    p = output_dir / "convergence_data.csv"
    with open(p, "w", newline="") as f:
        csv.writer(f).writerows([[cp_label, "rmse"], *data])
    print(f"Saved: {p}")


def export_json(output_dir: Path, mode: str, name_a: str, name_b: str,
                cp_label: str, data: list[tuple[int, float]],
                params_a: dict, params_b: dict) -> None:
    p = output_dir / "convergence_data.json"
    out = {
        "mode": mode,
        "method_a": {"type": name_a, "params": params_a},
        "method_b": {"type": name_b, "params": params_b},
        "checkpoint_axis": cp_label,
        "data": [{"checkpoint": idx, "rmse": r} for idx, r in data],
    }
    p.write_text(json.dumps(out, indent=2) + "\n")
    print(f"Saved: {p}")


def export_plot(output_dir: Path, cp_label: str, data: list[tuple[int, float]],
                name_a: str, name_b: str, title: str) -> None:
    xs = [d[0] for d in data]
    ys = [d[1] for d in data]
    fig, ax = plt.subplots(figsize=(6, 4))
    ax.loglog(xs, ys, marker="o", label=f"{name_a} vs {name_b}")
    ax.set_xlabel(cp_label)
    ax.set_ylabel("RMSE")
    ax.set_title(title)
    ax.legend()
    ax.grid(True, which="both", ls="--", alpha=0.5)
    fig.tight_layout()
    for ext in ("pdf", "png"):
        p = output_dir / f"convergence.{ext}"
        fig.savefig(str(p), dpi=150, bbox_inches="tight")
        print(f"Saved: {p}")
    plt.close(fig)


# ── Main ──────────────────────────────────────────────────────────────────────

def main() -> None:
    parser = argparse.ArgumentParser(description="Convergence RMSE comparison")
    parser.add_argument("output_dir")
    parser.add_argument("--mode", choices=["sample", "convergence"])
    parser.add_argument("--ref", metavar="METHOD",
                        help="Reference method for convergence mode (pt/pm/mitsuba)")
    parser.add_argument("--ref-image", metavar="PATH",
                        help="Pre-rendered EXR to use as reference (implies convergence mode)")
    parser.add_argument("--methods", nargs=2, metavar=("A", "B"),
                        help="Two methods to compare (overrides auto-detection)")
    args = parser.parse_args()

    out = Path(args.output_dir)
    manifest = load_manifest(out)
    method_data = (manifest_to_method_data(manifest, out) if manifest
                   else discover_heuristic(out))

    available = set(method_data)
    if not available:
        print("No checkpoint files found in " + str(out), file=sys.stderr)
        sys.exit(1)

    name_a, name_b = (args.methods if args.methods else (None, None))
    mode = args.mode

    # --ref-image bypasses method lookup: forces convergence against an external EXR.
    if args.ref_image:
        ref_image_path = Path(args.ref_image)
        if not ref_image_path.exists():
            print(f"Reference image not found: {ref_image_path}", file=sys.stderr)
            sys.exit(1)
        mode = "convergence"
        if name_a is None:
            # Auto-pick the splatter method from the manifest.
            for candidate in ("pvs", "vs", "ss", "pt", "pm"):
                if candidate in available:
                    name_a = candidate
                    break
            if name_a is None:
                print("Cannot auto-detect method; use --methods to specify one.",
                      file=sys.stderr)
                sys.exit(1)
        name_b = ref_image_path.stem
        info_a = method_data[name_a]
        files_a = info_a["files"]
        if not files_a:
            print(f"No files for method '{name_a}'", file=sys.stderr)
            sys.exit(1)
        print(f"Mode: convergence  |  {name_a} vs {ref_image_path.name} (external reference)")
        cp_label = info_a["checkpoints"]
        data = compute_convergence(files_a, ref_image_path)
        params_a = info_a["params"]
        params_b = {}
        title = f"{name_a} convergence vs {ref_image_path.name}"
    else:
        if name_a and name_b:
            if mode is None:
                mode = "convergence" if name_a in _SPLATTER_METHODS else "sample"
        else:
            detected = auto_detect(available)
            if detected is None:
                print("Cannot auto-detect comparison pair from: " + ", ".join(sorted(available)),
                      file=sys.stderr)
                sys.exit(1)
            det_mode, det_a, det_b = detected
            mode = mode or det_mode
            name_a = name_a or det_a
            name_b = name_b or det_b

        for name in (name_a, name_b):
            if name not in method_data:
                print(f"Method '{name}' not found in {out}", file=sys.stderr)
                sys.exit(1)

        info_a = method_data[name_a]
        files_a = info_a["files"]
        if not files_a:
            print(f"No files for method '{name_a}'", file=sys.stderr)
            sys.exit(1)

        print(f"Mode: {mode}  |  {name_a} vs {name_b}")

        if mode == "sample":
            files_b = method_data[name_b]["files"]
            data = compute_sample(files_a, files_b)
            cp_label = info_a["checkpoints"]
            params_a = info_a["params"]
            params_b = method_data[name_b]["params"]
            title = f"{name_a} vs {name_b} — paired {cp_label}"
        else:
            ref_name = args.ref or name_b
            if ref_name not in method_data or not method_data[ref_name]["files"]:
                print(f"Reference method '{ref_name}' not available", file=sys.stderr)
                sys.exit(1)
            ref_files = method_data[ref_name]["files"]
            ref_path = ref_files[max(ref_files)]
            print(f"Reference: {ref_path.name}")
            cp_label = info_a["checkpoints"]
            data = compute_convergence(files_a, ref_path)
            params_a = info_a["params"]
            params_b = method_data[ref_name]["params"]
            name_b = ref_name
            title = f"{name_a} convergence vs {name_b} (reference)"

    if not data:
        print("No matching checkpoints found.", file=sys.stderr)
        sys.exit(1)

    export_csv(out, cp_label, data)
    export_json(out, mode, name_a, name_b, cp_label, data, params_a, params_b)
    export_plot(out, cp_label, data, name_a, name_b, title)


if __name__ == "__main__":
    main()
