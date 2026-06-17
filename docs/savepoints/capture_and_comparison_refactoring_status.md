# Capture & Comparison Refactoring Status

Branch: `integr/splat-all`

## Goal
Separate comparison responsibility from SplatApp. Let SplatApp focus on interactive debugging and generating images from multiple methods per capture run. Improve Python comparison tooling.

## Design Decisions
- **Beam Splat removed** entirely (obsolete, superseded by Vol Splat)
- **Multi-method capture**: sequential execution, all methods share one timestamped output folder
- **Shared SPP schedule**: PT, PM, and Mitsuba use the same `shared_max_spp` / `shared_num_checkpoints`; SS/VS/PVS have their own pass-count checkpoints
- **Python env**: user-managed (venv or conda); C++ resolves via `$SPLAT_PYTHON` → `.venv/bin/python3` → `python3`
- **Output folder**: always `output/{scene}_{timestamp}/`; `camera.json` always written; no user-specified output path
- **Manifest (Chunk B)**: `manifest.json` written after all methods complete listing rendered methods + files
- **Python tools**: `requirements.txt` in `tools/`; setup documented in `README.md` only (no setup script)
- **Comparison**: `convergence_compare.py` reads `manifest.json`; exports CSV + JSON raw data; outputs PDF + PNG

## Chunks

### Chunk A — CaptureState + UI + main.cpp dispatch ✅ DONE

**Files changed:**
- `include/debug_ui.hpp`: New `CaptureState` struct
- `src/debug_ui.cpp`: New `drawCapturePanel()`
- `src/main.cpp`: New sequential capture dispatch

**`CaptureState` new fields:**
- `capture_pt`, `capture_pm`, `capture_ss`, `capture_vs`, `capture_pvs`, `capture_mitsuba` — independent booleans (were mutually-exclusive radio)
- `shared_max_spp`, `shared_num_checkpoints` — shared SPP schedule for PT/PM/Mitsuba
- `pt_max_depth` — PT only
- `pm_n_photons`, `pm_r_surf`, `pm_r_vol`, `pm_max_cam_depth`, `pm_max_emit_depth` — PM only
- `ss_total_photons`, `ss_photons_per_pass`, `ss_max_emit_depth`, `ss_h`, `ss_exposure`, `ss_num_checkpoints`
- `vs_total_photons`, `vs_photons_per_pass`, `vs_max_emit_depth`, `vs_beam_radius`, `vs_exposure`, `vs_num_checkpoints`
- `pvs_total_photons`, `pvs_photons_per_pass`, `pvs_max_emit_depth`, `pvs_h`, `pvs_beam_radius`, `pvs_exposure`, `pvs_num_checkpoints`
- `mit_max_depth` — Mitsuba only (SPP from shared schedule)
- `current_method[64]` — displayed in UI while rendering
- Removed: `output_path`, `total_photons`, `photons_per_pass` (old shared), `use_*` flags, `compare_reference`, `ss_compare_pm`, `vs_compare_pm`, `ss_pm_spp`, `vs_pm_spp`, `pt_spp`, `pm_spp`, `pt_num_checkpoints`, `pm_num_checkpoints`, `pm_compare_depth`, `ss_pm_save_checkpoints`, `vs_pm_save_checkpoints`

**UI changes (`drawCapturePanel`):**
- Independent checkboxes per method; each expands inline settings when checked
- SPP checkpoint block (shared) shown only when PT/PM/Mitsuba is checked
- PM shows effective depth hint (`emit + cam`)
- Mitsuba shows tip matching PM effective depth when both checked
- Render button disabled when nothing selected; label shows `"Rendering {method}..."` during run

**main.cpp capture dispatch:**
- Replaced `if/else if` chain with sequential `if` blocks
- Shared preamble: creates output dir, saves `camera.json`, defines `save_exr`/`run` lambdas, builds shared SPP checkpoint list + `spp_list` string
- Python resolution: `$SPLAT_PYTHON` → `.venv/bin/python3` → `python3`
- GPU beam state restored once after all splatter methods (SS/VS/PVS), not per-method
- Removed entire Beam Splat capture block (~145 lines)
- Removed: single-render paths, `compare_reference` branching, venv-hardcoded python path

---

### Chunk B — manifest.json ✅ DONE

After all method blocks complete (just before `cs.is_running = false`), write `manifest.json` in the output dir.

**Schema:**
```json
{
  "timestamp": "20260617_143022",
  "scene": "assets/models/.../CornellBox.obj",
  "camera": "camera.json",
  "methods": [
    {
      "type": "pt",
      "params": { "max_spp": 64, "max_depth": 8 },
      "checkpoints": "spp",
      "files": ["pt_spp0001.exr", "pt_spp0064.exr"]
    },
    {
      "type": "pm",
      "params": { "max_spp": 64, "n_photons": 100000, "r_surf": 0.05, "r_vol": 0.05,
                  "max_cam_depth": 3, "max_emit_depth": 5 },
      "checkpoints": "spp",
      "files": ["pm_spp0001.exr", "pm_spp0064.exr"]
    },
    {
      "type": "ss",
      "params": { "total_photons": 1000000, "photons_per_pass": 100000,
                  "max_emit_depth": 20, "h": 0.01, "exposure": 1.0 },
      "checkpoints": "pass",
      "files": ["ss_pass0001.exr", "ss_pass0010.exr"]
    },
    {
      "type": "mitsuba",
      "params": { "max_spp": 64, "max_depth": 8 },
      "checkpoints": "spp",
      "files": ["mit_spp0001.exr", "mit_spp0064.exr"]
    }
  ]
}
```

**Implementation location:** `src/main.cpp`, after the `if (cs.capture_mitsuba)` block, before `cs.current_method[0] = '\0'`.

Use `nlohmann/json` (already a dependency in the project) to build and write the JSON.

Each method block should accumulate its file list into a `std::vector<std::string>` that gets passed into the manifest builder. The simplest approach: collect filenames in the checkpoint lambdas.

---

### Chunk C — Python Tool Improvements ✅ DONE

**Files to create/modify:**
- `tools/requirements.txt` — new file
- `tools/convergence_compare.py` — major refactor
- `README.md` — new file

#### `tools/requirements.txt`
```
mitsuba
numpy
matplotlib
```

#### `tools/convergence_compare.py` refactor (as implemented)
- `--mode sample|convergence` (default: auto-detect from manifest)
- Reads `manifest.json` if present; falls back to filename heuristics
- `sample` mode: paired-checkpoint RMSE between two SPP-based methods (PT vs Mitsuba, PT vs PM, PM vs Mitsuba)
- `convergence` mode: checkpoint series vs a fixed reference image (any method vs a pre-rendered EXR)
- `--ref-image <path>`: use any EXR from a previous run as reference (forces convergence mode); avoids re-running Mitsuba. Auto-selects method from manifest (priority: pvs > vs > ss > pt > pm); override with `--methods`
- Exports `convergence_data.csv`, `convergence_data.json`, `convergence.pdf`, `convergence.png`

**CLI:**
```
python3 tools/convergence_compare.py <output_dir> [--mode sample|convergence]
    [--ref <method>] [--ref-image <path>] [--methods <a> <b>]
```

#### `README.md`
Sections:
1. Project overview (one paragraph)
2. Build instructions (CMake configure + build commands)
3. Python environment setup:
   - Option A: venv — `python3 -m venv .venv && source .venv/bin/activate && pip install -r tools/requirements.txt`
   - Option B: conda — `conda create -n splat python=3.11 && conda activate splat && pip install -r tools/requirements.txt`
   - Note: activate environment before launching the app, OR set `SPLAT_PYTHON=/path/to/python3`
4. Running the app
5. Running comparison scripts manually

---

### Chunk D — CLI capture mode ⬜ DEFERRED

---

## Output Folder Structure (reference)
```
output/{scene}_{YYYYMMDD_HHMMSS}/
├── camera.json
├── manifest.json
├── pt_spp0001.exr
├── pt_spp0064.exr
├── pm_spp0001.exr
├── pm_spp0064.exr
├── ss_pass0001.exr
├── ss_pass0010.exr
├── vs_pass0001.exr
├── vs_pass0010.exr
├── pvs_pass0001.exr
├── pvs_pass0010.exr
├── mit_spp0001.exr           (if Mitsuba selected)
├── mit_spp0064.exr
├── scene_mitsuba.xml          (if Mitsuba selected)
├── groups/                    (if Mitsuba selected)
├── convergence_data.csv       (after convergence_compare.py)
├── convergence_data.json      (after convergence_compare.py)
├── convergence.pdf            (after convergence_compare.py)
└── convergence.png            (after convergence_compare.py)
```

## Key Files
| File | Role |
|------|------|
| `include/debug_ui.hpp` | `CaptureState` struct definition |
| `src/debug_ui.cpp` | `drawCapturePanel()` ImGui UI |
| `src/main.cpp` | Capture dispatch (lines ~320–450 after refactor) |
| `tools/export_mitsuba.py` | OBJ → Mitsuba XML export |
| `tools/run_mitsuba.py` | Mitsuba checkpoint renderer |
| `tools/convergence_compare.py` | RMSE comparison + plot (needs Chunk C refactor) |
| `tools/requirements.txt` | Python dependencies (to create in Chunk C) |
| `README.md` | Setup documentation (to create in Chunk C) |
