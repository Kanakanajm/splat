#!/usr/bin/env python3
"""
Integration tests for tools/run_mitsuba.py.
Run with:  .venv/bin/python tests/test_run_mitsuba.py
"""

import json
import math
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).parent.parent
sys.path.insert(0, str(ROOT / "tools"))

from export_mitsuba import export_mitsuba
from run_mitsuba import render_checkpoints

CORNELL_OBJ = ROOT / "assets" / "models" / "cornell-box" / "CornellBox-Original_fixed.obj"


def _write_camera(tmp_dir: str, width: int = 64, height: int = 64) -> str:
    cam = {
        "eye":    [0, 1, 4],
        "target": [0, 1, 0],
        "up":     [0, 1, 0],
        "fov_y":  math.radians(38.0),
        "width":  width,
        "height": height,
    }
    p = Path(tmp_dir) / "camera.json"
    p.write_text(json.dumps(cam))
    return str(p)


def test_render_creates_exr_files():
    with tempfile.TemporaryDirectory() as tmp:
        cam_path = _write_camera(tmp)
        xml_path = export_mitsuba(str(CORNELL_OBJ), cam_path, tmp, spp=4, max_depth=4)

        spp_list = [1, 2, 4]
        render_checkpoints(xml_path, spp_list, tmp)

        for spp in spp_list:
            exr = Path(tmp) / f"mit_spp{spp:04d}.exr"
            assert exr.exists(), f"Missing: {exr.name}"
            assert exr.stat().st_size > 0, f"Empty: {exr.name}"

    print("test_render_creates_exr_files PASSED")


def test_render_correct_count():
    with tempfile.TemporaryDirectory() as tmp:
        cam_path = _write_camera(tmp)
        xml_path = export_mitsuba(str(CORNELL_OBJ), cam_path, tmp, spp=4, max_depth=4)

        spp_list = [1, 4]
        render_checkpoints(xml_path, spp_list, tmp)

        created = list(Path(tmp).glob("mit_spp*.exr"))
        assert len(created) == len(spp_list), f"Expected {len(spp_list)} files, got {len(created)}"

    print("test_render_correct_count PASSED")


if __name__ == "__main__":
    test_render_creates_exr_files()
    test_render_correct_count()
    print("All run_mitsuba tests passed.")
