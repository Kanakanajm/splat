For each photon beam, build a camera-facing billboard quad and rasterize them. **TODO: more details**

---

## V1 — Surface Photon Splatting

Rasterize stored `PhotonPoint`s as kernel-weighted splats on their hit surfaces.
Based on Stürzlinger & Bastos (2001), §2.2.

### Algorithm

**Preprocessing (once per trace):**
- Compute kernel texture (64×64 Epanechnikov kernel, uploaded to GPU).
- Filter photon points to `bounce_depth >= 1` (indirect only).
- Upload filtered points as a point-cloud VBO (1 vertex per photon).

**Render pass (per frame, after geometry depth prepass):**
- Depth test on, depth write **off**.
- Additive blending (`GL_ONE, GL_ONE`).
- Draw point VBO through VS → GS → FS pipeline.
- Restore blend/depth state.

### Pipeline

```
PhotonPoint VBO (1 pt/photon)
  │
  ▼
splat.vs   — pass-through; forward attributes
  │
  ▼
splat.gs   — expand point → equilateral triangle on surface plane
  │           tangent frame from normal; circumradius g = (0.5 + √2)·h
  │           per-vertex UV = 0.5 + tangent_offset / (2h)
  ▼
splat.fs   — sample kernel texture; evaluate diffuse BRDF; additive output
               fragColor = k(uv) × power × (bsdf_color / π)
```

### Key equations

**Kernel bandwidth:** global fixed `h` (scene config `splat_h`, default 0.1).

**Radiance estimate (eq. 2 in paper):**
```
L_o(x, ω_o) = (1/h²) Σ φ_j · f_r(x, ω_i, ω_o) · k((x − x_j)/h)
```

**Triangle circumradius:**
```
g = (0.5 + √2) · h
```

**Kernel function (Epanechnikov):**
```
k(r) = (3/π)(1 − r²)  for r ≤ 1,  0 otherwise
r = ‖uv − 0.5‖ / 0.5
```

### `PhotonPoint` fields required

| Field | Use |
|---|---|
| `position` | Triangle center in world space |
| `normal` | Orient triangle on surface plane; tangent frame |
| `incoming_dir` | BRDF evaluation (ω_i) |
| `power` | Photon flux |
| `bsdf_id` → `bsdf.color` | Diffuse reflectance tint |
| `bounce_depth` | Filter: only bounce ≥ 1 uploaded |

### Occlusion

Depth test (read-only) prevents splats from appearing on surfaces occluded from camera.
No per-surface stencil masking. Junction bleeding (coplanar surface edges) is acceptable
for V1; a normal G-buffer check can be added later if artifacts are visible.

### Design decisions

| Decision | Choice | Rationale |
|---|---|---|
| Triangle construction | Geometry Shader | 1→3 expansion is GS sweet spot; avoids CPU build and 3× VBO |
| Kernel bandwidth | Global fixed `h` | Per-surface h = C√(A/n) requires surface area computation; deferred |
| Photon filter | `bounce_depth >= 1` | Direct illumination handled separately |
| Stencil masking | None | Depth test sufficient; stencil N-draw-call cost not justified |
| BRDF | Diffuse only | `f_r = color/π`; glossy BRDF deferred |
| Framebuffer precision | Float (HDR) | Additive accumulation requires enough range |
