# Medium Photon Beam Splatting — Savepoint

Branch: `vol-splat`. Depends on depth_peel and billboard_gs being complete.
Scope defined in `docs/project/splat.md` § TODO 4 and `docs/project/opacity.md`.

## Goal

Complete the full beam radiance estimate in the fragment shader and wire up all supporting
infrastructure: HDR accumulation pipeline, linear-space blending, env light, multiple lights,
power normalization, opaque surface pass in the splat, and capture-mode integration.

## Architecture

### Radiance Estimate (`beam.fs` — TODO 4)

For each rasterized billboard fragment the beam shader evaluates:

```
L = k_r(u_t) * sigma_s * Phi * Tr(v_t) * Tr(w_t) * phase(w, v) / sin(w, v)
```

| Term | Source |
|---|---|
| `k_r(u_t)` | Epanechnikov kernel on perpendicular distance `vUt` from beam axis |
| `sigma_s` | Per-medium scattering coefficient (uniform array `mediaSigmaS`) |
| `Phi` | Per-beam power `vPower` (RGB, scaled by `1/N` in tracer) |
| `Tr(v_t)` | Beer-Lambert along beam: `exp(-sigma_t * vT * vLength)` |
| `Tr(w_t)` | Camera-side transmittance via depth peel maps (`cameraSideTransmittance()`) |
| `phase(w,v)` | Isotropic: `1 / (4π)` |
| `sin(w, v)` | `length(cross(beamDir, cameraDir))` — geometric sin factor |

`cameraSideTransmittance()` walks the depth peel layers pixel-by-pixel, accumulating
Beer-Lambert attenuation per medium segment using `mediaSigmaT[]`.

### HDR Pipeline and Tone Mapping

Replaced direct-to-framebuffer output with a two-FBO pipeline:
- **HDR FBO** (`hdrFbo`): `GL_RGB16F`, receives all additive contributions.
- **Present pass** (`present.vs` / `present.fs`): full-screen quad that reads HDR FBO,
  applies exposure scaling and sRGB gamma, outputs to default framebuffer.
- Linear-space blending: all splatting uses `GL_ONE, GL_ONE` into the HDR FBO; gamma is applied once at presentation.

### Supporting Features

| Feature | Details |
|---|---|
| **Env light** | `EnvLight` class; background radiance sampled via cube-map or constant color |
| **Multiple lights** | `PhotonTracer` samples uniformly from `std::vector<PointLight>` |
| **Power normalization** | Each photon initialized with `light.power / N`; beam FS receives pre-normalized power |
| **Medium stack** | Bit-simulated medium stack (`medium_stack.glsl`) for correct depth-peel traversal |
| **Opaque surfaces in splat/capture** | Geometry drawn into HDR FBO before beam pass to occlude beams behind surfaces |
| **Background in capture** | Env light contribution rendered into accumulation FBO |

### Files Changed (beyond depth_peel + billboard_gs)

| File | Change |
|---|---|
| `shaders/beam.fs` | Full radiance estimate; `cameraSideTransmittance()`; all peel uniforms |
| `shaders/quad.vs` / `quad.fs` | Full-screen quad pass for HDR presentation |
| `shaders/present.fs` | Exposure + gamma tone mapping |
| `include/scene.hpp` | HDR FBO + accumulation FBO management |
| `src/main.cpp` | HDR pipeline; opaque/background pass; capture integration |
| `src/scene_gl.cpp` | `upload_beams` includes power per beam |
| `src/photon_tracer.cpp` | Power normalization fix (`1/N`); env light + multi-light support |
| `src/debug_ui.cpp` | Beam radius slider; exposure control; capture panel |
| `assets/models/cornell-box/` | New Cornell box with homogeneous medium scene |

## Status: Complete — on-going branch `vol-splat`

Next steps: implement baseline methods (PT, PPM, PBM, PPS) for quality/speed comparison.
See `docs/project/baselines.md`.
