#!/usr/bin/env python3
"""
Integration tests for tools/convergence_compare.py.
Run with:  .venv/bin/python tests/test_convergence_compare.py
"""

import sys
import tempfile
from pathlib import Path

import numpy as np

ROOT = Path(__file__).parent.parent
sys.path.insert(0, str(ROOT / "tools"))

from convergence_compare import find_pairs, load_exr, rmse, main as cc_main


def _write_exr(path: str, width: int = 16, height: int = 16, value: float = 0.5) -> None:
    import mitsuba as mi
    mi.set_variant("scalar_rgb")
    arr = np.full((height, width, 3), value, dtype=np.float32)
    bmp = mi.Bitmap(arr)
    mi.util.write_bitmap(path, bmp, write_async=False)


def test_find_pairs():
    with tempfile.TemporaryDirectory() as tmp:
        out = Path(tmp)
        _write_exr(str(out / "pt_spp0001.exr"))
        _write_exr(str(out / "mit_spp0001.exr"))
        _write_exr(str(out / "pt_spp0004.exr"))
        # no matching mit_spp0004 — should be excluded

        pairs = find_pairs(out)
        assert len(pairs) == 1
        assert pairs[0][0] == 1

    print("test_find_pairs PASSED")


def test_rmse_identical_images():
    with tempfile.TemporaryDirectory() as tmp:
        p = str(Path(tmp) / "img.exr")
        _write_exr(p, value=0.3)
        img = load_exr(p)
        assert rmse(img, img) == 0.0

    print("test_rmse_identical_images PASSED")


def test_rmse_different_images():
    with tempfile.TemporaryDirectory() as tmp:
        pa = str(Path(tmp) / "a.exr")
        pb = str(Path(tmp) / "b.exr")
        _write_exr(pa, value=0.2)
        _write_exr(pb, value=0.8)
        a = load_exr(pa)
        b = load_exr(pb)
        r = rmse(a, b)
        assert r > 0.0 and np.isfinite(r), f"Expected finite positive RMSE, got {r}"

    print("test_rmse_different_images PASSED")


def test_convergence_plot_saved():
    import sys as _sys

    with tempfile.TemporaryDirectory() as tmp:
        out = Path(tmp)
        for spp in [1, 4]:
            _write_exr(str(out / f"pt_spp{spp:04d}.exr"), value=0.5)
            _write_exr(str(out / f"mit_spp{spp:04d}.exr"), value=0.6)

        _sys.argv = ["convergence_compare.py", str(out)]
        cc_main()

        plot = out / "convergence.png"
        assert plot.exists(), "convergence.png not created"
        assert plot.stat().st_size > 0, "convergence.png is empty"

    print("test_convergence_plot_saved PASSED")


if __name__ == "__main__":
    test_find_pairs()
    test_rmse_identical_images()
    test_rmse_different_images()
    test_convergence_plot_saved()
    print("All convergence_compare tests passed.")
