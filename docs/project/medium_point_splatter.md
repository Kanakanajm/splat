# Medium Photon Point Splatter (PPS)

GPU splatting of medium photon points (as opposed to the current PBS which splats photon
beams). Analogous to the surface photon point splatter (`surface-splat` branch) but for
volumetric interaction events.

Each medium scatter point is rasterized as a small camera-facing disk (billboard quad)
weighted by a 3D kernel projected onto the screen.

## Algorithm

**Pass 1 — Emit:** same as PBS. Trace photons, record medium scatter events.
Currently, medium scatter points are implicitly encoded as beam start/end positions.
Add explicit `MediumPhotonPoint { position, power, medium_id }` storage (or reuse beam endpoints).

**Pass 2 — Splat (GPU):** upload scatter points as a point VBO. In the GS, expand each point
into a camera-facing quad of radius `r`. In the FS, evaluate:

```
L = K(u_t / r) * Tr(cam → x) * sigma_s * phase(wo) / (π * r²)
```

where `Tr(cam → x)` is read from the existing depth peel maps.

## Key Equation

**Point radiance estimate** at screen pixel viewing scatter point `x` on beam `j`:

```
L = K(u_t / r) * Tr(cam → x) * σ_s * (1/4π) * φ_j / (π r²)
```

This is the 2D projected form of the 3D Epanechnikov kernel (sphere projected to disk).
Compare with the beam version: beam uses a 1D kernel along `u_t` integrated over the
beam length; point uses a 2D disk kernel at a discrete scatter position.

## Testing Progression

| Phase | Scene type | Medium | BSDFs |
|---|---|---|---|
| 1 | Diffuse surface only | None (vacuum) | N/A — volume-only method; skip |
| 2 | Medium only | Homogeneous, isotropic (`g=0`) | None (no surfaces) |
| 3 | Combined | Homogeneous, isotropic | Diffuse only |

Conductor and Dielectric BSDFs are out of scope for all phases.

## Scope

- **Volume only** (no surface component; surface handled by surface-splat or geometry pass).
- **GS expansion**: same pattern as `splat.gs` (point → equilateral triangle or quad).
- **Kernel**: Epanechnikov disk: `K(r) = (3/π)(1 - r²)` for `r ≤ 1`.
- **Transmittance**: read from existing `peel_depth_array_` + `peel_medium_array_`.
- **Output**: additive into the existing HDR FBO; uses the same present pass.
- **Reuse**: depth peel maps, HDR FBO, present pass, `mediaSigmaT[]` / `mediaSigmaS[]` uniforms, capture pipeline.

## Data Structures

### `MediumPhotonPoint`

```cpp
struct MediumPhotonPoint {
    glm::vec3 position;  // world-space medium scatter point
    glm::vec3 power;     // normalized flux (1/N · sigma_s/sigma_t applied in tracer)
    uint32_t  medium_id;
};
```

These are the discrete scatter events currently implicit in beam segments. The tracer
already samples `t_med` from the free-flight distribution; storing this point separately
requires one additional push_back per scatter event.

## Shader Pipeline

```
MediumPhotonPoint VBO (1 pt per scatter event)
  │
  ▼
mpp.vs   — pass position, power, medium_id to GS
  │
  ▼
mpp.gs   — expand point → equilateral triangle on camera plane
  │         tangent frame: (right, up) from camera; circumradius g = (0.5 + √2) * r
  │         per-vertex uv for kernel
  ▼
mpp.fs   — K(uv) * Tr(cam → x) * sigma_s * Inv4Pi * power / (PI * r²)
```

`Tr(cam → x)` uses the same `cameraSideTransmittance()` logic as `beam.fs`.

## Design Decisions

| Decision | Choice | Rationale |
|---|---|---|
| Separate `MediumPhotonPoint` struct | Yes | Decouples from beam storage; beams still needed for PBS comparison |
| Camera-plane billboard (not surface-aligned) | Camera-plane | Volume splats have no surface normal to align to |
| Same GS pattern as surface-splat | Yes | Reuse proven GS expansion; circumradius formula identical |
| Depth peel transmittance | Reuse existing | No additional GPU pass needed |

## Implementation Plan

### New files
- `shaders/mpp.vs` / `mpp.gs` / `mpp.fs` — medium point splat shaders
- Storage extension in `PhotonTracer` (or optionally separate `MediumPhotonPoint` vector)

### Additions to existing files
- `include/scene.hpp` — `upload_medium_points` / `draw_medium_points`
- `src/scene_gl.cpp` — VBO upload + draw
- `src/photon_tracer.cpp` — push_back `MediumPhotonPoint` at each medium scatter event
- `src/main.cpp` — PPS render pass after depth peel, before present
- `src/debug_ui.cpp` — point radius slider for PPS

## Parameters (scene JSON)

```json
"medium_point_splat_r": 0.05
```

## TODOs

| # | Task | Files |
|---|---|---|
| 1 | `MediumPhotonPoint` struct + tracer storage | `include/photon.hpp`, `src/photon_tracer.cpp` |
| 2 | `mpp.{vs,gs,fs}` shaders | `shaders/mpp.vs`, `shaders/mpp.gs`, `shaders/mpp.fs` |
| 3 | `upload_medium_points` / `draw_medium_points` + VBO | `include/scene.hpp`, `src/scene_gl.cpp` |
| 4 | PPS pass in render loop + capture | `src/main.cpp`, `src/debug_ui.cpp` |
| 5 | Tests: kernel normalization, energy sanity | `tests/medium_point_splatter_test.cpp` |
