# AOV Debug Visualization — Savepoint

Branch: `trace`. Depends on SceneConfig being done first.
Motivated by the light-leaking bug in the tallBox medium (Cornell box scene).

## Goal

A structured set of debug AOVs across geometry, photon points, and photon beams,
driven by a clean `ViewState`-based UI so passes can be added without touching
the render loop structure.

## Architecture

### ViewState (central state bag)

Owned by `DebugUi`, read by `main.cpp` each frame:

```cpp
struct ViewState {
    // Geometry
    bool showGeometry = false;
    enum class GeomAov { None, Diffuse, Normal, Depth, Backface } geomAov = GeomAov::None;
    std::vector<bool> instanceVisible;

    // Photon Points
    bool showPoints = true;
    enum class PointAov { InstanceId, BsdfKind, BounceDepth } pointAov = PointAov::InstanceId;
    std::vector<bool> instancePointsVisible;

    // Photon Beams
    bool showBeams = true;
    enum class BeamAov { MediumId, T, BounceDepth, Length } beamAov = BeamAov::MediumId;
    std::vector<bool> mediumBeamsVisible;
};
```

### Shader strategy

- One geometry shader pair `shaders/geom.{vs,fs}` with `uniform int aov_mode`.
- Existing `shaders/point.{vs,fs}` extended with `uniform int aov_mode` for both points and beams.
- Mode integer matches the `enum class` ordinal.

### GL geometry

`Scene` gains `upload_geometry()` / `draw_geometry(shader)` in `scene_gl.cpp`,
mirroring the existing `upload_points` / `upload_beams` pattern.

### PhotonPoint / PhotonBeam additions

Both structs gain `uint32_t bounce_depth`, filled by the tracer at store time.

## AOVs

### Geometry
| AOV | Description |
|---|---|
| Wireframe | `glPolygonMode` overlay, per-instance show/hide |
| Normal | World-space normal → fake RGB |
| Depth | Linear depth, remapped to [0,1] |
| Backface | Faces with `dot(N, view) < 0` highlighted; useful for winding bugs |
| Diffuse | Flat instance color (placeholder until material system) |

### Photon Points
| AOV | Description |
|---|---|
| InstanceId | Already implemented |
| BsdfKind | Color by `Diffuse`/`MediumShell`/etc. |
| BounceDepth | Color by which bounce produced the point |
| Per-instance filter | ImGui checkbox per instance to hide its points |

### Photon Beams
| AOV | Description |
|---|---|
| MediumId | Already implemented |
| T | Lerp parameter along beam (start→end); color encodes scatter position |
| BounceDepth | Color by which bounce produced the beam |
| Length | Color by segment length; escaped beams are anomalously long |
| Per-medium filter | ImGui checkbox per medium to hide its beams |

### Deferred
- **Single-photon step mode** — step through one photon path at a time in ImGui;
  most targeted tool for the leaking bug but requires a retrace API.

## Tasks

| # | Task | Files |
|---|---|---|
| ~~1~~ | ~~`ViewState` + multi-panel `DebugUi` scaffold~~ ✅ | `include/debug_ui.hpp`, `src/debug_ui.cpp`, `src/main.cpp` |
| ~~2~~ | ~~Geometry pass: upload + wireframe + per-instance show/hide~~ ✅ | `include/scene.hpp`, `src/scene_gl.cpp`, `shaders/geom.{vs,fs}` |
| ~~3~~ | ~~Geometry AOV modes: Normal, Depth, Backface~~ ✅ | `shaders/geom.fs` |
| 4 | Photon point AOVs + per-instance filter | `include/photon.hpp`, `src/photon_tracer.cpp`, `src/scene_gl.cpp`, `shaders/point.{vs,fs}` |
| 5 | Photon beam AOVs + per-medium filter | `include/photon.hpp`, `src/photon_tracer.cpp`, `src/scene_gl.cpp`, `shaders/point.{vs,fs}` |

## Notes

- `ViewState` filter vectors use `std::vector<bool>`, which returns proxies not real `bool&`.
  ImGui `Checkbox` requires `bool*`, so all filter loops copy to a local `bool v`, then write back on change.
- `medium_count` passed to `draw()` is a fixed upper bound (8); the filter vector resizes to fit actual usage.
- Geometry VBO layout: `[x,y,z, nx,ny,nz]` per vertex; face normals computed from cross product at upload time.
- Per-instance draw calls (one `glDrawArrays` per instance) allow skipping hidden instances without re-uploading.
- `geom.fs` `aov_mode` must match `ViewState::GeomAov` enum ordinals exactly (bug-prone — keep in sync).
- None and Diffuse both show flat instance palette color; Diffuse will diverge when a material system is added.

## Status: On-going (3/5 tasks done)
