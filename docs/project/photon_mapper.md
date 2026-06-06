# Photon Mapper (PPM — Point Gather)

CPU gather-side photon mapping for participating media. Covers both surface (PPM) and
volume (volumetric PPM) radiance estimates.

Reference implementation: `clefairy/src/integrators/photon.cpp`.

## Algorithm

Two-pass method:

**Pass 1 — Emit (light-side):** same algorithm as the existing `PhotonTracer`.
Store surface photon points and medium photon points in separate lists.
Build a kd-tree (or BVH) for each.

**Pass 2 — Gather (camera-side):** trace camera rays. At each medium scatter event or
diffuse surface hit, query the corresponding photon map for nearby photons within radius `r`.

```
for each pixel (i, j):
    ray = camera.generate_ray(i, j)
    gather(ray, medium=vacuum)

gather(ray, medium):
    t_hit = BVH.intersect(ray)
    if medium is non-vacuum:
        t_med = -ln(ξ) / sigma_t
    else:
        t_med = ∞

    if t_med < t_hit:                        // medium gather
        x = ray(t_med)
        weight *= Tr(ray.origin → x)
        nearby = vol_map.radius_search(x, r_vol)
        L += weight * sigma_s * phase * sum(phi_j) * k_vol(r)
        sample new direction; continue gather

    if surface is diffuse:                   // surface gather
        weight *= Tr(ray.origin → hit)
        nearby = surf_map.radius_search(hit, r_surf)
        L += weight * sum(f_r * phi_j) * k_surf(r)
        (stop or continue with Russian roulette)
```

## Testing Progression

| Phase | Scene type | Medium | BSDFs |
|---|---|---|---|
| 1 | Diffuse surface only | None (vacuum) | Diffuse only |
| 2 | Medium only | Homogeneous, isotropic (`g=0`) | None (no surfaces) |
| 3 | Combined | Homogeneous, isotropic | Diffuse only |

Conductor and Dielectric BSDFs are out of scope for all phases.

## Scope

- **Surface photon map**: kd-tree gather at diffuse surface hits (`bounce_depth >= 1`).
- **Volume photon map**: kd-tree gather at camera-side medium scatter points.
- **Phase function**: isotropic (`1 / (4π)`).
- **Kernel**: Epanechnikov for both surface (`1/πr²`) and volume (`3/(4/3 π r³)`).
- **BSDFs**: Diffuse only. Conductor and Dielectric excluded.
- **Output**: EXR via capture pipeline.
- **Reuse**: `PhotonTracer` for the emit pass; existing BVH + medium + BSDF infrastructure.

## Key Equations

**Surface radiance estimate:**
```
L_surface(x, ω_o) = (1 / (π r²)) Σ_j f_r(x, ω_j, ω_o) · φ_j
```

**Volume radiance estimate** at camera-side medium scatter point `x`:
```
L_vol(x, ω_o) = σ_s(x) · (3 / (4π r³)) Σ_j p(ω_j, ω_o) · φ_j
```

where `φ_j = photon.power` (already normalized by `1/N` in emit pass).

## Data Structures

### `PhotonKdTree`
- 3D kd-tree for radius searches. Build once after emit; query per pixel.
- Interface: `build(vector<PhotonPoint>)`, `radius_search(pos, r) → vector<PhotonPoint>`.
- Can reuse `tinybvh` or implement a minimal kd-tree (≤ 200 lines).

## Implementation Plan

### New files
- `include/photon_map.hpp` + `src/photon_map.cpp` — `PhotonKdTree`
- `include/photon_mapper.hpp` + `src/photon_mapper.cpp` — `PhotonMapper` class

### `PhotonMapper` interface
```cpp
class PhotonMapper {
public:
    PhotonMapper(const Scene& scene, int n_photons, float r_surf, float r_vol, int max_cam_depth);
    void render(std::vector<float>& out, int width, int height);
private:
    void emit();
    glm::vec3 gather(const Ray& ray, Medium* medium, int depth);
};
```

### Reuse
- `PhotonTracer::trace_one()` (or replicate emit logic) for light pass.
- `PhotonPoint` struct — already has `position`, `power`, `incoming_dir`.
- Capture pipeline for EXR export.

## Parameters (scene JSON)

```json
"photon_mapper": {
    "n_photons":     100000,
    "r_surf":        0.05,
    "r_vol":         0.05,
    "max_cam_depth": 3
}
```

## TODOs

| # | Task | Files |
|---|---|---|
| 1 | `PhotonKdTree` — build + radius search | `include/photon_map.hpp`, `src/photon_map.cpp` |
| 2 | `PhotonMapper` — emit pass (reuse tracer logic) | `include/photon_mapper.hpp`, `src/photon_mapper.cpp` |
| 3 | `PhotonMapper` — gather pass (surface + volume) | `src/photon_mapper.cpp` |
| 4 | Wire into capture mode | `src/main.cpp`, `src/debug_ui.cpp` |
| 5 | Tests: single-slab transmittance, single-scatter energy | `tests/photon_mapper_test.cpp` |
