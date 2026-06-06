# Splat V1.5 — Savepoint

Branch: `surface-splat`. Builds on `splat_v1_status.md`.

## Goal

Three targeted fixes to the V1 pipeline before V2 feature work begins:
1. **Throughput** — investigated instancing as GS replacement. Reverted; no gain. GS retained.
2. **Surface bleeding** — stencil mask per surface to prevent splat triangles crossing boundaries.
3. **Direct lighting shadows** — shadow map in `geom.fs` Phong pass (depth from light POV + PCF).

---

## Task 1: Instancing (Reverted — No Performance Gain)

### What was tried

Replaced GS with `glDrawArraysInstanced`:
- Static unit triangle VBO (3 × 2D tangent-space corners).
- Per-instance VBO: position, tangent frame (t1, t2), cos_theta, power, bsdf_color — precomputed on CPU at upload time so VS does only cheap linear ops.
- No GS. GPU backface culling handles back-facing triangles via winding order.

CPU backface cull with per-frame VBO re-upload was also tried but caused severe FPS drops on camera movement (~6 FPS vs 125 FPS) due to ~52 MB PCIe re-upload per frame for N=1M.

### Result

No measurable FPS improvement over GS at N=1M photons. GS at ~125 FPS, instancing at ~125 FPS. The GS 1→3 fixed expansion is well-optimised by the driver for this use case.

### Conclusion

GS pipeline retained as-is. Throughput task deferred — only worth revisiting if N scales beyond ~5M or a different GPU shows a bottleneck here. The fillrate (h² × N fragments) is the more likely next bottleneck, addressable by reducing `h`.

---

## Task 2: Surface Bleeding — Stencil Mask

### Motivation

Splat triangles extend past surface boundaries at corners. With large `h` the triangle covers adjacent surfaces and the depth test accepts those fragments, producing light leaking across edges.

### Implementation

Two-phase draw per frame, limited to ≤ 255 instances (8-bit stencil):

**Phase 1 — geometry stencil stamp** (color/depth write off):
- For each instance i: draw its geometry with `GL_ALWAYS` stencil func, `GL_REPLACE` op, value = i+1.
- Depth test `GL_LEQUAL` ensures only the front-most surface writes its stencil at each pixel.

**Phase 2 — per-instance splat draw** (stencil read-only):
- For each instance i: `GL_EQUAL, i+1` stencil test; splat fragments on other surfaces fail and are discarded.

`upload_splats` now groups photons by `instance_id` into contiguous VBO ranges so each instance's photons can be drawn with a single `glDrawArrays` call.

### Files Changed

| File | Change |
|---|---|
| `include/scene.hpp` | `draw_splats` takes `geom_shader`; add `splat_ranges_` member. |
| `src/scene_gl.cpp` | `upload_splats` groups by instance; `draw_splats` two-phase stencil. |
| `src/main.cpp` | `GLFW_STENCIL_BITS 8` hint; `GL_STENCIL_BUFFER_BIT` in clear; pass `geomShader` to `draw_splats`. |

### Status: Done — visual verification pending

---

## Task 3: Direct Lighting Shadows

### Implementation

Omnidirectional shadow map via depth cubemap (LearnOpenGL point shadows approach).

**Shadow pass** — rendered once before the render loop (scene is static):
- 1024×1024 `GL_TEXTURE_CUBE_MAP` depth texture + FBO with `glFramebufferTexture`.
- `shadow.gs` broadcasts each triangle to all 6 faces via `gl_Layer`; `shadow.fs` stores linear depth as `length(fragPos - lightPos) / farPlane`.
- `SHADOW_FAR = 25.0f`.

**`geom.fs`** — `shadowFactor()` samples the cubemap with `fragPos - lightPos`, compares linear distance against stored depth (bias = 0.05). Multiplies into the diffuse term. Gated by `uniform bool useShadow`.

**Debug UI** — "Shadow" checkbox in Geometry panel (active whenever geometry is shown).

### Files Changed

| File | Change |
|---|---|
| `shaders/shadow.vs` | New — world-space position pass-through |
| `shaders/shadow.gs` | New — 6-face cubemap broadcast |
| `shaders/shadow.fs` | New — linear depth write |
| `shaders/geom.fs` | `shadowFactor()` + `useShadow` uniform; applied in diffuse branch |
| `include/debug_ui.hpp` | `useShadow` field in `ViewState` |
| `src/debug_ui.cpp` | Shadow checkbox in `drawGeometryPanel` |
| `src/main.cpp` | Cubemap FBO setup + once-only shadow pass; shadow uniforms in geometry loop |

### Status: Done — visual verification pending

---

## Status

| # | Task | Status |
|---|---|---|
| 1 | Instancing + CPU backface cull | Reverted — no gain over GS |
| 2 | Surface bleeding (stencil) | **Deferred** — depth test prevents splats on *other* surfaces; same-surface junction bleeding (edge artifacts on the same wall) is still present but not addressed. Stencil approach shelved for now. |
| 3 | Direct lighting shadows | **Done** — merged to main (PR #13) |

**Overall: Closed.** Shadow map complete; stencil deferred to future work.
