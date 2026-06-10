# Path Tracer Baseline — Status

Branch: `pathtracer`

## Implementation

### Chunk 1 — Capture integration: DONE
- `render_checkpointed` in `PathTracer` (evenly-spaced checkpoints 1..pt_spp)
- `CaptureState` with `compare_with_mitsuba` flag and `pt_num_checkpoints`
- Camera JSON save/load; debug UI camera inspector; timestamped output dirs

### Chunk 2 — Mitsuba3 exporter: DONE
- `tools/export_mitsuba.py`: splits OBJ by `g`/`o` group, maps BSDFs (Diffuse/Conductor/Dielectric/MediumShell→null), point-light power→intensity (Φ/4π), area lights as `<emitter type="area">` inside shape, `volpath` integrator, homogeneous media with `sigma_t`/`albedo`, per-shape `interior`/`exterior` medium refs
- `.venv/` with mitsuba 3.8.0; Python tests in `tests/test_export_mitsuba.py`

### Chunk 3 — Mitsuba runner + convergence analysis: DONE
- `tools/run_mitsuba.py`: render scene_mitsuba.xml at each SPP checkpoint → `mit_sppNNNN.exr`
- `tools/convergence_compare.py`: pairs `pt_sppNNNN.exr` + `mit_sppNNNN.exr`, computes RMSE per SPP, saves log-log `convergence.png`
- Both tools invoked from debug UI via `compare_with_mitsuba` capture flag

### Path Tracer correctness (MIS): DONE
- Step 1: point light — `kInv4Pi` in `nee_surface` / `nee_medium`
- Step 2: env light — `prev_specular` guard prevents double-counting
- Step 3: MIS — `power_heuristic` (β=2); `prev_bsdf_pdf` tracked across bounces
- Step 4: area lights — full MIS, `prim_to_area_light_` map, winding-correct Cornell Box light quad
- `shadow_Tr`: multi-segment transmittance marching through Dielectric/MediumShell boundaries; replaces all per-light shadow+Tr code in `nee_surface` and `nee_medium`

## Key decisions
- Shadow rays toward area lights aim from `offset` to sampled point `q` (not `normalize(q-p)` from `offset`) to prevent false self-occlusion when off-centre samples satisfy `dist > dist_to_centroid`.
- `shadow_Tr` treats Diffuse/Conductor prims as opaque (returns 0); Dielectric/MediumShell as transparent boundaries (accumulates Tr, switches medium).
- Env-light NEE MIS weight fixed at 0.5 (cosine-weighted NEE PDF == BSDF PDF for Lambertian).

## Tests: 87/87 passing
