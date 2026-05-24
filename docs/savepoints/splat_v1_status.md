# Splat V1 — Savepoint

Branch: `surface-splat`. Branched from `main` (includes all trace functionality). Depends on trace_v2 and power_sanity being complete.
Scope defined in `docs/project/splat.md` § V1.

## Goal

Rasterize stored `PhotonPoint`s as kernel-weighted splats on their hit surfaces,
producing an indirect-illumination image contribution. Based on Stürzlinger & Bastos (2001).

## Architecture

### `PhotonPoint` — new fields

```cpp
struct PhotonPoint {
    bvhvec3  position;
    bvhvec3  normal;        // geometric normal at hit  ← new
    bvhvec3  incoming_dir;  // photon travel dir before hit  ← new
    bvhvec3  power;
    uint32_t bsdf_id;
    uint32_t instance_id;
    uint32_t bounce_depth;
};
```

### Kernel texture

64×64 `GL_R32F` texture. Epanechnikov kernel:
```
k(r) = (3/π)(1 − r²)  for r ≤ 1,  0 outside
r = 2 · ‖uv − 0.5‖
```
Sampler: `GL_CLAMP_TO_BORDER`, border = 0.

### Shader pipeline: VS → GS → FS

**VBO layout (1 point per photon, bounce_depth ≥ 1 only):**
```
position (vec3) | normal (vec3) | incoming_dir (vec3) | power (vec3) | bsdf_color (vec3)
```

**`splat.gs`** — `layout(points) in`, `layout(triangle_strip, max_vertices=3) out`:
- Compute `(t1, t2)` tangent frame from normal.
- `g = (0.5 + sqrt(2.0)) * h`
- Vertices on surface plane:
  ```
  v0 = pos + g * t1
  v1 = pos + g * (−0.5·t1 + √3/2·t2)
  v2 = pos + g * (−0.5·t1 − √3/2·t2)
  ```
- Per-vertex UV: `uv_i = 0.5 + tangent_offset / (2·h)`

**`splat.fs`:**
```glsl
float k    = texture(kernelTex, vUV).r;
vec3  brdf = vBsdfColor / PI;
fragColor  = vec4(k * vPower * brdf, 1.0);
```

### Render pass

After geometry depth prepass (depth buffer already filled):
1. Depth test on, depth write **off**
2. Additive blend (`GL_ONE, GL_ONE`)
3. `draw_splats(shader, cam_pos, h)`
4. Restore state

### Scene config

`splat_h` float added to JSON sidecar (default `0.1`). Also exposed as UI slider.

## Design Decisions

**Geometry Shader for triangle construction** — 1→3 fixed expansion is the GS sweet spot.
Avoids CPU build step and 3× VBO. Switch to instancing if GS throughput becomes a bottleneck.

**No stencil masking** — depth test handles occluded surfaces. Per-surface stencil requires
N draw calls and is not justified. Junction bleeding acceptable for V1; normal G-buffer check
can be added later.

**Diffuse BRDF only** — `f_r = color/π`. Glossy/conductor BRDF deferred to V2.

**Indirect photons only** — `bounce_depth >= 1` filtered on upload. Direct illumination
is handled by the existing geometry/Phong pass.

**Global fixed `h`** — per-surface `h = C√(A/n)` deferred (requires per-instance area computation).

## Tasks

| # | Task | Files |
|---|---|---|
| ~~1~~ | ~~Add `normal` + `incoming_dir` to `PhotonPoint`; update `PhotonTracer` to record them~~ ✅ | `include/photon.hpp`, `src/photon_tracer.cpp`, `tests/photon_tracer_test.cpp` |
| ~~2~~ | ~~Kernel texture — CPU build + GL upload utility~~ ✅ | `include/kernel_texture.hpp`, `src/kernel_texture.cpp`, `tests/kernel_texture_test.cpp` |
| 3 | Splat shaders — `splat.vs`, `splat.gs`, `splat.fs` | `shaders/splat.{vs,gs,fs}` |
| 4 | Scene GL — `upload_splats` / `draw_splats`; wire `splat_h` config + UI slider | `include/scene.hpp`, `src/scene_gl.cpp`, `src/scene_config.cpp`, `src/debug_ui.cpp` |
| 5 | Render loop — depth-prepass → splat pass ordering; HDR framebuffer if needed | `src/main.cpp` |

## Status: In Progress (2/5 tasks done)
