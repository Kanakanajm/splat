#!/usr/bin/env python3
"""
Integration tests for tools/export_mitsuba.py.
Run with:  .venv/bin/python tests/test_export_mitsuba.py
Requires mitsuba to be installed in the project venv.
"""

import json
import math
import sys
import tempfile
from pathlib import Path

# Resolve project root relative to this file
ROOT = Path(__file__).parent.parent
sys.path.insert(0, str(ROOT / 'tools'))

from export_mitsuba import export_mitsuba, split_obj_groups, bsdf_to_xml_elem

import mitsuba as mi
mi.set_variant('scalar_rgb')

CORNELL_OBJ = ROOT / 'assets' / 'models' / 'cornell-box' / 'CornellBox-Original_fixed.obj'


def _write_camera(tmp_dir, fov_deg=38.0, width=128, height=128):
    cam = {
        'eye':    [0, 1, 4],
        'target': [0, 1, 0],
        'up':     [0, 1, 0],
        'fov_y':  math.radians(fov_deg),
        'width':  width,
        'height': height,
    }
    path = str(Path(tmp_dir) / 'camera.json')
    with open(path, 'w') as f:
        json.dump(cam, f)
    return path


# ── split_obj_groups ──────────────────────────────────────────────────────────

def test_split_obj_groups_returns_all_groups():
    groups = split_obj_groups(CORNELL_OBJ)
    assert len(groups) >= 4, f'Expected >= 4 groups, got {len(groups)}: {list(groups)}'
    assert 'floor' in groups
    assert 'leftWall'  in groups
    assert 'rightWall' in groups


def test_split_obj_groups_absolute_indices():
    groups = split_obj_groups(CORNELL_OBJ)
    for name, content in groups.items():
        for line in content.splitlines():
            if line.startswith('f '):
                for tok in line.split()[1:]:
                    idx = int(tok.split('/')[0])
                    assert idx > 0, f'Negative index {idx} in group {name}'


def test_split_obj_groups_mitsuba_loadable():
    """Each per-group OBJ must load in Mitsuba3 without error."""
    groups = split_obj_groups(CORNELL_OBJ)
    with tempfile.TemporaryDirectory() as tmp:
        for name, content in groups.items():
            obj_path = str(Path(tmp) / f'{name}.obj')
            Path(obj_path).write_text(content)
            scene = mi.load_dict({'type': 'scene', name: {
                'type': 'obj', 'filename': obj_path,
                'bsdf': {'type': 'diffuse',
                         'reflectance': {'type': 'rgb', 'value': [0.8, 0.8, 0.8]}},
            }})
            assert len(scene.shapes()) == 1, f'Expected 1 shape for group {name}'


# ── bsdf_to_xml_elem ──────────────────────────────────────────────────────────

def test_bsdf_diffuse_xml():
    import xml.etree.ElementTree as ET
    el = bsdf_to_xml_elem({'kind': 'Diffuse', 'color': [0.5, 0.2, 0.1]}, 'my-bsdf')
    assert el.get('type') == 'diffuse'
    assert el.get('id') == 'my-bsdf'
    rgb = el.find('rgb')
    assert rgb is not None and '0.5' in rgb.get('value', '')


def test_bsdf_conductor_xml():
    import xml.etree.ElementTree as ET
    el = bsdf_to_xml_elem({'kind': 'Conductor', 'color': [1, 1, 1]})
    assert el.get('type') == 'conductor'


def test_bsdf_dielectric_xml():
    el = bsdf_to_xml_elem({'kind': 'Dielectric', 'ior': 1.5})
    assert el.get('type') == 'dielectric'
    ior_el = el.find('float')
    assert ior_el is not None and ior_el.get('value') == '1.5'


# ── export_mitsuba (integration) ──────────────────────────────────────────────

def test_export_creates_xml_and_groups():
    with tempfile.TemporaryDirectory() as tmp:
        cam = _write_camera(tmp)
        xml_path = export_mitsuba(str(CORNELL_OBJ), cam, tmp, spp=4, max_depth=4)
        assert Path(xml_path).exists(), 'scene_mitsuba.xml not created'
        groups = list((Path(tmp) / 'groups').glob('*.obj'))
        assert len(groups) >= 4, f'Expected >= 4 group OBJs, got {len(groups)}'


def test_export_loads_in_mitsuba():
    with tempfile.TemporaryDirectory() as tmp:
        cam = _write_camera(tmp)
        xml_path = export_mitsuba(str(CORNELL_OBJ), cam, tmp, spp=4, max_depth=4)
        scene = mi.load_file(xml_path)
        assert len(scene.shapes())   > 0, 'No shapes in exported scene'
        assert len(scene.emitters()) > 0, 'No emitters in exported scene'


def test_export_correct_total_face_count():
    """Total faces across all Mitsuba3 shapes must equal the face count from the source OBJ groups."""
    with tempfile.TemporaryDirectory() as tmp:
        cam = _write_camera(tmp)
        xml_path = export_mitsuba(str(CORNELL_OBJ), cam, tmp, spp=4, max_depth=4)
        groups = split_obj_groups(CORNELL_OBJ)

        # Count triangles after fan-triangulation (n-vert polygon → n-2 triangles)
        expected_faces = sum(
            sum(len(line.split()) - 3  # n_verts - 2, minus the 'f' token
                for line in content.splitlines() if line.startswith('f '))
            for content in groups.values()
        )

        scene = mi.load_file(xml_path)
        actual_faces = sum(s.face_count() for s in scene.shapes())
        assert actual_faces == expected_faces, \
            f'Face count mismatch: Mitsuba {actual_faces} vs source {expected_faces}'


# ── runner ────────────────────────────────────────────────────────────────────

if __name__ == '__main__':
    tests = [
        test_split_obj_groups_returns_all_groups,
        test_split_obj_groups_absolute_indices,
        test_split_obj_groups_mitsuba_loadable,
        test_bsdf_diffuse_xml,
        test_bsdf_conductor_xml,
        test_bsdf_dielectric_xml,
        test_export_creates_xml_and_groups,
        test_export_loads_in_mitsuba,
        test_export_correct_total_face_count,
    ]
    failed = 0
    for t in tests:
        try:
            t()
            print(f'  PASS  {t.__name__}')
        except Exception as e:
            print(f'  FAIL  {t.__name__}: {e}')
            failed += 1
    print()
    if failed:
        print(f'{failed}/{len(tests)} tests FAILED')
        sys.exit(1)
    else:
        print(f'All {len(tests)} tests passed.')
