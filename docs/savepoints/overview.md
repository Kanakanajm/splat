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

# On-going

| Savepoint | Branch | Summary |
|---|---|---|
| [trace_v2_status.md](trace_v2_status.md) | `trace` | RGB power tracking, RR, Conductor/Dielectric BSDFs, power AOVs. 0/7 tasks done. |
| Opacity pass | `opacity` | Frustum slicing / depth peeling for camera-side attenuation maps. See `docs/project/opacity.md`. |
| Splat pass | `splat` | Rasterize beams as billboard quads. See `docs/project/splat.md`. |
