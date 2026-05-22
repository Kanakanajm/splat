# Savepoint Rules
- Use `overview.md` for larger feature-level progress logs.
- Create sub-files for each feature; split into smaller tasks.
- Keep notes concise and structured.
- Preserve key decisions and assumptions.
- Update and compact if a file becomes too verbose.
- Keep human-readable progress summary at bottom while code/implementation related hints on the top.

# Completed

# On-going

| Savepoint | Branch | Summary |
|---|---|---|
| [trace_v1_status.md](trace_v1_status.md) | `trace` | CPU photon tracing pass: multi-bounce diffuse, free-flight media, medium-shell BSDF, GL point visualization. 39/39 tests. Dielectric BSDF + beam vis remaining. |

# Planned

| Savepoint | Branch | Summary |
|---|---|---|
| [scene_config_status.md](scene_config_status.md) | `trace` | JSON sidecar to assign BSDF/medium by object name; replaces hardcoded main.cpp setup. Do this first. |
| [aov_debug_status.md](aov_debug_status.md) | `trace` | ViewState-driven debug AOVs: geometry wireframe/normals/depth/backface, point/beam coloring modes, per-instance and per-medium filters. Depends on SceneConfig. |
| Opacity pass | `opacity` | Frustum slicing / depth peeling for camera-side attenuation maps. See `docs/project/opacity.md`. |
| Splat pass | `splat` | Rasterize beams as billboard quads. See `docs/project/splat.md`. |
