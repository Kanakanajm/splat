# Savepoint Rules
- Use `overview.md` for larger feature-level progress logs.
- Create sub-files for each feature; split into smaller tasks.
- Keep notes concise and structured.
- Preserve key decisions and assumptions.
- Update and compact if a file becomes too verbose.
- Keep human-readable progress summary at bottom while code/implementation related hints on the top.

# Completed

| Savepoint | Branch | Summary |
|---|---|---|
| [scene_config_status.md](scene_config_status.md) | `trace` | JSON sidecar to assign BSDF/medium by object name; replaces hardcoded main.cpp setup. |
| [trace_v1_status.md](trace_v1_status.md) | `trace` | CPU photon tracing pass: multi-bounce diffuse, free-flight media, medium-shell BSDF, beam/point GL visualization. 41/41 tests. |
| [aov_debug_status.md](aov_debug_status.md) | `trace` | ViewState-driven debug AOVs: geometry wireframe/normals/depth/backface/diffuse, point/beam coloring modes, per-instance/per-medium/bounce-depth filters. 5/5 tasks done. |
| [trace_v2_status.md](trace_v2_status.md) | `trace` | RGB power tracking, RR, Conductor/Dielectric BSDFs, power AOVs. 7/7 tasks done. |
| [power_sanity_status.md](power_sanity_status.md) | `trace` | Energy-conservation sanity checks for V2 power tracking: surface albedo decay, beam albedo decay, combined lossless scene. 5/5 tests passing, visually verified. |
| [depth_peel_status.md](depth_peel_status.md) | `vol-splat` | Depth-peel pass producing `peel_depth_array_` + `peel_medium_array_` for camera-side transmittance. 8/8 tests passing. |
| [billboard_gs_status.md](billboard_gs_status.md) | `vol-splat` | Geometry shader expands beam lines into camera-facing billboard quads; `vUt`/`vT` interpolated for radiance estimate. 66/66 tests passing. |
| [splat_v1_status.md](splat_v1_status.md) | `surface-splat` | Surface photon point splatting: GS-expanded Epanechnikov kernel splats, depth-tested indirect illumination. 5/5 tasks done. |
| [splat_v15_status.md](splat_v15_status.md) | `surface-splat` | Throughput (instancing reverted), direct shadow map done; stencil surface bleeding deferred. Merged to main (PR #13). |
| [capture_status.md](capture_status.md) | `capture` | Multi-pass offline capture: sub-pass trace → GPU accumulation → EXR export via tinyexr. Merged to main (PR #14). |
| [vol_splat_beam_status.md](vol_splat_beam_status.md) | `vol-splat` | Full beam radiance estimate (Tr camera-side + beam-side, phase, kernel); HDR pipeline; env/multi-light; capture integration. |

# On-going

| Savepoint | Branch | Summary |
|---|---|---|
| [path_tracer_status.md](path_tracer_status.md) | `pathtracer` | PT baseline complete: MIS (point/env/area lights), `shadow_Tr`, Mitsuba exporter (volpath, media, area lights), Mitsuba runner + RMSE convergence plot. 87/87 tests. |
