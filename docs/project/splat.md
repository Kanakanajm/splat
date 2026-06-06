# Medium Photon Beam Splatting
Photon beams stored are directly splatted onto the screen by building a camera facing billboard.
Suppose the view plane's normal (the direction that the camera is looking at) we call it `w_n` and a photon beam direction `v`
we will get the third axis by `u = v x (-w_n)`. The length of the billboard is the length of the photon beam. 
We pass in the start and end point of each beam into the geometry shader and emit a quad with the start and end point extended in the direction of `u` (and `-u`) with a distance of `r` set by user.

After construction such billboards, we rasterize them and blend their radiance contributes. 
At the fragment shader, we evaluate the equation `k_r(u_t) * scattering_coefficient * photon_power * Tr(v_t) * Tr(w_t) * phase_func(w, v) / sin(w, v)` where `k_r` is the kernel function, `u_t` is the distance of the point on the billboard (project the fragment back) to the line (in direction of `u`), `Tr(w_t)` is the transmittance towards the camera side calculated with the help of transmittance map (see `docs/project/opacity.md`), `Tr(v_t)` is the transmittance along the photon beam (from the start of the beam) for homogenous medium this is trivial and the phase function (isotropic for now).

The depth test is disabled and blending is used to accumulate the radiance.

The camera-side transmittance is calculated with depth-peeling for now on branch `depthpeel`. It is not directly usable since the `main.cpp` there is to test things out.

# TODOs
1. ~~Organize `depthpeel` branch to export the functionality related to transmittance map which will be uploaded as a depth texture array and medium texture array. The evaluation details please refer to `docs/project/opacity.md`~~
3. ~~Construct camera-aligned billboard in a geometry shader~~
4. ~~Finish off the rendering pipeline with the final radiance estimate.~~

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
