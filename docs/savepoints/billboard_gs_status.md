# Billboard Geometry Shader — Savepoint

Branch: `vol-splat`. Depends on depth_peel_status being complete.
Scope defined in `docs/project/splat.md` § TODO 3.

## Goal

Construct camera-aligned billboard quads from photon beam line segments using a
geometry shader, so each beam is rasterized as a wide strip on screen. This is
the geometry stage of the splatting pipeline; the radiance estimate in the
fragment shader is TODO 4.

## Architecture

### Shader pipeline

| Stage | File | Role |
|---|---|---|
| VS | `shaders/beam.vs` | Pass world-space position + per-beam data to GS |
| GS | `shaders/beam.gs` | Expand each line into a camera-facing quad |
| FS | `shaders/beam.fs` | Existing AOV display; receives `vUt` from GS |

### Billboard construction (`beam.gs`)

Input primitive: `lines` (start + end vertex of each beam).
Output: `triangle_strip, max_vertices = 4`.

```
u = normalize(beam_dir × (-cameraDir))   // tangent perpendicular to beam and camera
corners: p0 ± r·u  (t=0),  p1 ± r·u  (t=1)
```

Degenerate beams (zero length, or beam parallel to camera) are silently dropped.

### Interpolated fragment data

| Variable | Range | Used for |
|---|---|---|
| `vT` | [0, 1] | along-beam parameter; `vT * vLength` = distance from start |
| `vUt` | [−r, +r] | perpendicular distance from beam axis; kernel argument |

All other per-beam attributes (`vMediumId`, `vBounceDepth`, `vLength`, `vSigmaT`, `vPower`)
are constant per beam and carried from the nearest endpoint vertex.

### `Shader` class extension

Added 3-argument constructor:
```cpp
Shader(const char* vertexPath, const char* geometryPath, const char* fragmentPath);
```

### `main.cpp` wiring

```cpp
Shader beamShader("shaders/beam.vs", "shaders/beam.gs", "shaders/beam.fs");
// per-frame before draw_beams:
beamShader.setVec3("cameraDir", camera.Front.x, camera.Front.y, camera.Front.z);
beamShader.setFloat("beamRadius", 0.05f);
```

`beamRadius` is hardcoded for now; will become a UI-adjustable parameter in TODO 4.

## Tasks

| # | Task | Files |
|---|---|---|
| ~~1~~ | ~~`Shader` 3-stage constructor~~ ✅ | `include/shader.hpp`, `src/shader.cpp` |
| ~~2~~ | ~~`beam.vs` outputs world-space `gWorldPos`, renames outputs `g*`~~ ✅ | `shaders/beam.vs` |
| ~~3~~ | ~~`beam.gs` billboard geometry shader~~ ✅ | `shaders/beam.gs` |
| ~~4~~ | ~~`beam.fs` adds `vUt` input~~ ✅ | `shaders/beam.fs` |
| ~~5~~ | ~~`main.cpp` wiring~~ ✅ | `src/main.cpp` |

## Status: Complete (5/5 tasks done, 66/66 tests passing)

Next: TODO 4 — full radiance estimate in fragment shader:
`k_r(u_t) * sigma_s * Phi * Tr(v_t) * Tr(w_t) * phase_func / sin(w, v)`
using `vUt`, `vT`, and the depth peel transmittance maps.
