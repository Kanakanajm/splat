#!/usr/bin/env python3
"""
Render a Mitsuba3 scene at each SPP checkpoint, saving mit_sppNNNN.exr files.

Usage:
    python tools/run_mitsuba.py <scene_mitsuba.xml> \
        --spp-list 1,21,41,61,81,256 \
        --output-dir <output_dir>
"""

import argparse
import sys
from pathlib import Path


def render_checkpoints(xml_path: str, spp_list: list[int], output_dir: str) -> None:
    import mitsuba as mi
    mi.set_variant("scalar_rgb")

    out = Path(output_dir)
    out.mkdir(parents=True, exist_ok=True)

    for spp in spp_list:
        scene = mi.load_file(xml_path)
        image = mi.render(scene, spp=spp)
        out_path = str(out / f"mit_spp{spp:04d}.exr")
        mi.util.write_bitmap(out_path, image, write_async=False)
        print(f"Saved: {out_path}")


def main() -> None:
    parser = argparse.ArgumentParser(description="Render Mitsuba3 at SPP checkpoints")
    parser.add_argument("xml_path", help="Path to scene_mitsuba.xml")
    parser.add_argument("--spp-list", required=True,
                        help="Comma-separated SPP values, e.g. 1,21,41,256")
    parser.add_argument("--output-dir", required=True,
                        help="Directory to write mit_sppNNNN.exr files")
    args = parser.parse_args()

    spp_list = [int(s) for s in args.spp_list.split(",")]
    render_checkpoints(args.xml_path, spp_list, args.output_dir)


if __name__ == "__main__":
    main()
