#!/usr/bin/env python3
"""
Export a Splat scene (.obj + .json sidecar + camera.json) to a Mitsuba3 XML scene.

The exporter:
  - Splits OBJ mesh groups into per-group sub-OBJs (absolute vertex indices).
  - Maps Splat BSDFs to Mitsuba3 material types.
  - Converts point-light power (total flux W) to Mitsuba3 intensity (W/sr).
  - Writes a single self-contained XML that references the sub-OBJs by relative path.

Usage:
    python tools/export_mitsuba.py <scene.obj> <camera.json> \\
        --output-dir <dir> [--spp N] [--max-depth N]

Output layout inside --output-dir:
    scene_mitsuba.xml       Mitsuba3 scene file
    groups/<group>.obj      one OBJ per mesh group with absolute indices
"""

import argparse
import json
import math
from pathlib import Path
import xml.etree.ElementTree as ET

_PI = math.pi


# ── OBJ utilities ─────────────────────────────────────────────────────────────

def split_obj_groups(obj_path):
    """Parse *obj_path* and return {group_name: obj_text} with absolute indices.

    Each value is a ready-to-write OBJ string containing only the vertices and
    faces that belong to that group.  Relative (negative) face indices in the
    source file are converted to absolute 1-based indices.
    """
    verts, norms, texes = [], [], []
    groups = []
    current = None

    with open(obj_path) as f:
        for line in f:
            s = line.strip()
            if s[:2] in ('v ', 'v\t'):
                verts.append(s)
            elif s[:3] in ('vn ', 'vn\t'):
                norms.append(s)
            elif s[:3] in ('vt ', 'vt\t'):
                texes.append(s)
            elif s.startswith('g '):
                current = {'name': s[2:].strip(), 'faces': []}
                groups.append(current)
            elif s.startswith('f ') and current is not None:
                abs_toks = []
                for tok in s.split()[1:]:
                    parts = tok.split('/')
                    def _abs(x, n):
                        if not x:
                            return x
                        i = int(x)
                        return str(i if i > 0 else n + i + 1)
                    new = [_abs(parts[0], len(verts))]
                    if len(parts) > 1:
                        new.append(_abs(parts[1], len(texes)) if parts[1] else '')
                    if len(parts) > 2:
                        new.append(_abs(parts[2], len(norms)) if parts[2] else '')
                    abs_toks.append('/'.join(new))
                current['faces'].append(abs_toks)

    result = {}
    for grp in groups:
        if not grp['faces']:
            continue

        used_v  = set()
        used_vt = set()
        used_vn = set()
        for face in grp['faces']:
            for tok in face:
                parts = tok.split('/')
                used_v.add(int(parts[0]))
                if len(parts) > 1 and parts[1]:
                    used_vt.add(int(parts[1]))
                if len(parts) > 2 and parts[2]:
                    used_vn.add(int(parts[2]))

        v_map  = {old: new for new, old in enumerate(sorted(used_v),  1)}
        vt_map = {old: new for new, old in enumerate(sorted(used_vt), 1)}
        vn_map = {old: new for new, old in enumerate(sorted(used_vn), 1)}

        lines = []
        for old in sorted(used_v):
            lines.append(verts[old - 1])
        for old in sorted(used_vt):
            lines.append(texes[old - 1])
        for old in sorted(used_vn):
            lines.append(norms[old - 1])
        lines.append(f'g {grp["name"]}')
        for face in grp['faces']:
            new_toks = []
            for tok in face:
                parts = tok.split('/')
                nv  = v_map[int(parts[0])]
                nvt = vt_map[int(parts[1])] if len(parts) > 1 and parts[1] else None
                nvn = vn_map[int(parts[2])] if len(parts) > 2 and parts[2] else None
                if nvt is not None or nvn is not None:
                    new_toks.append(f'{nv}/{nvt or ""}/{nvn or ""}')
                else:
                    new_toks.append(str(nv))
            lines.append('f ' + ' '.join(new_toks))

        result[grp['name']] = '\n'.join(lines) + '\n'

    return result


# ── BSDF mapping ──────────────────────────────────────────────────────────────

def _rgb_str(color):
    return ' '.join(f'{c:.6f}' for c in color)


def bsdf_to_xml_elem(bsdf_def, bsdf_id=None):
    """Return a Mitsuba3 <bsdf> ET.Element for a Splat BSDF definition dict."""
    kind  = bsdf_def.get('kind', 'Diffuse')
    color = bsdf_def.get('color', [0.8, 0.8, 0.8])

    elem = ET.Element('bsdf')
    if bsdf_id:
        elem.set('id', bsdf_id)

    if kind == 'Diffuse':
        elem.set('type', 'diffuse')
        ET.SubElement(elem, 'rgb', name='reflectance', value=_rgb_str(color))

    elif kind == 'Conductor':
        elem.set('type', 'conductor')
        ET.SubElement(elem, 'string', name='material', value='none')

    elif kind == 'Dielectric':
        ior = bsdf_def.get('ior', 1.5)
        elem.set('type', 'dielectric')
        ET.SubElement(elem, 'float', name='int_ior', value=str(ior))

    else:
        # MediumShell or unknown → opaque white diffuse placeholder
        elem.set('type', 'diffuse')
        ET.SubElement(elem, 'rgb', name='reflectance', value='0.8 0.8 0.8')

    return elem


# ── XML helpers ───────────────────────────────────────────────────────────────

def _indent(elem, level=0):
    """In-place pretty-print indentation (Python 3.9+ ET.indent alternative)."""
    pad = '\n' + '    ' * level
    if len(elem):
        if not elem.text or not elem.text.strip():
            elem.text = pad + '    '
        for child in elem:
            _indent(child, level + 1)
        if not child.tail or not child.tail.strip():
            child.tail = pad
    if level and (not elem.tail or not elem.tail.strip()):
        elem.tail = pad


# ── Main export ───────────────────────────────────────────────────────────────

def export_mitsuba(obj_path, camera_json_path, output_dir, spp=256, max_depth=8):
    """Export scene to Mitsuba3 XML and return the path to the written XML file."""
    obj_path   = Path(obj_path).resolve()
    output_dir = Path(output_dir)
    groups_dir = output_dir / 'groups'
    output_dir.mkdir(parents=True, exist_ok=True)
    groups_dir.mkdir(exist_ok=True)

    scene_json_path = obj_path.with_suffix('.json')
    with open(scene_json_path) as f:
        scene = json.load(f)
    with open(camera_json_path) as f:
        cam = json.load(f)

    # Split OBJ by group, write sub-OBJs
    group_objs = split_obj_groups(obj_path)
    for name, content in group_objs.items():
        (groups_dir / f'{name}.obj').write_text(content)

    # Build XML tree
    root = ET.Element('scene', version='3.0.0')

    # Integrator
    integ = ET.SubElement(root, 'integrator', type='path')
    ET.SubElement(integ, 'integer', name='max_depth', value=str(max_depth))

    # Sensor
    sensor = ET.SubElement(root, 'sensor', type='perspective')
    ET.SubElement(sensor, 'float',  name='fov',      value=f'{math.degrees(cam["fov_y"]):.4f}')
    ET.SubElement(sensor, 'string', name='fov_axis', value='y')

    tf = ET.SubElement(sensor, 'transform', name='to_world')
    e, t, u = cam['eye'], cam['target'], cam['up']
    ET.SubElement(tf, 'lookat',
                  origin=f'{e[0]} {e[1]} {e[2]}',
                  target=f'{t[0]} {t[1]} {t[2]}',
                  up=f'{u[0]} {u[1]} {u[2]}')

    sampler = ET.SubElement(sensor, 'sampler', type='independent')
    ET.SubElement(sampler, 'integer', name='sample_count', value=str(spp))

    film = ET.SubElement(sensor, 'film', type='hdrfilm')
    ET.SubElement(film, 'integer', name='width',        value=str(cam['width']))
    ET.SubElement(film, 'integer', name='height',       value=str(cam['height']))
    ET.SubElement(film, 'string',  name='pixel_format', value='rgb')

    # Lights
    def _add_point_light(light_def):
        emitter = ET.SubElement(root, 'emitter', type='point')
        pos = light_def['position']
        ET.SubElement(emitter, 'point', name='position',
                      value=f'{pos[0]} {pos[1]} {pos[2]}')
        # Convert total radiant flux (W) → radiant intensity (W/sr): I = Φ / 4π
        power     = light_def.get('power', [1.0, 1.0, 1.0])
        intensity = [v / (4.0 * _PI) for v in power]
        ET.SubElement(emitter, 'rgb', name='intensity',
                      value=_rgb_str(intensity))

    # Pre-collect area lights for embedding inside their shape below
    area_lights_by_instance = {
        ld['instance']: ld
        for ld in scene.get('lights', [])
        if ld.get('kind') == 'area'
    }

    for light_def in ([scene['light']] if 'light' in scene else []) + scene.get('lights', []):
        kind = light_def.get('kind', 'point')
        if kind == 'point':
            _add_point_light(light_def)
        # 'area' lights are embedded inside their shape element below

    if 'env_light' in scene:
        el = scene['env_light']
        radiance = el.get('color', el.get('power', [1.0, 1.0, 1.0]))
        emitter = ET.SubElement(root, 'emitter', type='constant')
        ET.SubElement(emitter, 'rgb', name='radiance', value=_rgb_str(radiance))

    # Named BSDFs (defined once, referenced by shapes)
    bsdfs = scene.get('bsdfs', {})
    for bsdf_name, bsdf_def in bsdfs.items():
        root.append(bsdf_to_xml_elem(bsdf_def, bsdf_id=bsdf_name))

    # Shapes (one per OBJ group)
    instances = scene.get('instances', {})
    for grp_name in group_objs:
        shape = ET.SubElement(root, 'shape', type='obj')
        ET.SubElement(shape, 'string', name='filename', value=f'groups/{grp_name}.obj')

        assigned = instances.get(grp_name, {}).get('bsdf') if grp_name in instances else None
        if assigned and assigned in bsdfs:
            ET.SubElement(shape, 'ref', id=assigned)
        else:
            bsdf_el = ET.SubElement(shape, 'bsdf', type='diffuse')
            ET.SubElement(bsdf_el, 'rgb', name='reflectance', value='0.8 0.8 0.8')

        if grp_name in area_lights_by_instance:
            al = area_lights_by_instance[grp_name]
            em = ET.SubElement(shape, 'emitter', type='area')
            ET.SubElement(em, 'rgb', name='radiance', value=_rgb_str(al['emission']))

    _indent(root)
    out_path = output_dir / 'scene_mitsuba.xml'
    ET.ElementTree(root).write(str(out_path), encoding='unicode', xml_declaration=True)
    print(f'Exported: {out_path}')
    return str(out_path)


def main():
    p = argparse.ArgumentParser(description='Export Splat scene to Mitsuba3 XML')
    p.add_argument('scene_obj',    help='Path to scene .obj file')
    p.add_argument('camera_json',  help='Path to camera.json')
    p.add_argument('--output-dir', required=True)
    p.add_argument('--spp',        type=int, default=256)
    p.add_argument('--max-depth',  type=int, default=8)
    args = p.parse_args()
    export_mitsuba(args.scene_obj, args.camera_json, args.output_dir,
                   spp=args.spp, max_depth=args.max_depth)


if __name__ == '__main__':
    main()
