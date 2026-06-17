# Splat — Hybrid Photon Beam Splatter

A hybrid CPU+OpenGL renderer for participating media that supports Surface Splat (SS), Volume Splat (VS), and Combined Splat (PVS) methods alongside Path Tracing (PT), Photon Mapping (PM), and Mitsuba3 reference renders. Captures multi-method output in a single run and generates RMSE convergence plots for comparison.

## Build

```sh
cmake -S . -B build \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=1 \
  -G Ninja

cmake --build build -j8
```

## Python Environment Setup

The app resolves the Python interpreter via `$SPLAT_PYTHON` → `.venv/bin/python3` → `python3`.
Activate your environment before launching the app, or set `SPLAT_PYTHON` to point to your interpreter.

**Option A — venv**
```sh
python3 -m venv .venv
source .venv/bin/activate
pip install -r tools/requirements.txt
```

**Option B — conda**
```sh
conda create -n splat python=3.11
conda activate splat
pip install -r tools/requirements.txt
```

**Option C — explicit path**
```sh
export SPLAT_PYTHON=/path/to/python3
```

## Running the App

```sh
./build/SplatApp assets/models/<scene>/<scene>.obj
```

Use the Capture panel in the debug UI to select methods, configure parameters, and click Render.
Output goes to `output/<scene>_<timestamp>/` and includes `manifest.json` listing all rendered files.

## Running Comparison Scripts Manually

```sh
# Auto-detect mode and comparison pair from manifest.json:
python3 tools/convergence_compare.py output/<scene>_<timestamp>/

# Explicit mode:
python3 tools/convergence_compare.py output/<scene>_<timestamp>/ --mode convergence

# Explicit methods:
python3 tools/convergence_compare.py output/<scene>_<timestamp>/ --methods pvs mitsuba

# Specify reference for convergence mode:
python3 tools/convergence_compare.py output/<scene>_<timestamp>/ --mode convergence --ref mitsuba

# Reuse a pre-rendered reference EXR (skips re-running Mitsuba):
python3 tools/convergence_compare.py output/<scene>_<timestamp>/ \
    --ref-image output/<scene>_<gt_timestamp>/mit_spp1024.exr
```

`--ref-image` accepts any EXR path (absolute or relative). It forces convergence mode and auto-selects a method from the manifest (priority: pvs > vs > ss > pt > pm). Use `--methods <m>` to override which method is compared.

Outputs written to the same directory: `convergence_data.csv`, `convergence_data.json`, `convergence.pdf`, `convergence.png`.
