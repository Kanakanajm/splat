# Photon Beam Mapper (PBM — Beam Gather)

CPU gather-side photon beam method. The camera ray is treated as a query "beam"; it
intersects with stored photon beams (line segments) and accumulates radiance from each
crossing.

Reference implementation: `clefairy/src/integrators/beam.cpp`.

## Algorithm

Same two-pass structure as PPM, but the volume gather uses ray-vs-segment intersection
instead of sphere queries.

**Pass 1 — Emit (light-side):** same as PPM / existing `PhotonTracer`.
Store photon beams (`PhotonBeam { start, end, power, medium_id }`).
Build a BVH of beam segments (capsule BVH or AABB BVH with per-segment refinement).

**Pass 2 — Gather (camera-side):**

```
for each pixel (i, j):
    ray = camera.generate_ray(i, j)
    t_hit = BVH.intersect(scene, ray)         // opaque surface

    // Volume gather: integrate along camera ray segment [0, t_hit]
    for each beam j that intersects ray within radius r:
        t_cam  = closest point on ray to beam j
        t_beam = closest point on beam j to ray
        u_t    = perpendicular distance (ray ↔ beam)
        if u_t > r: skip

        L += K(u_t) * Tr(cam → t_cam) * sigma_s * p(w_cam, w_beam)
             * Tr(beam_start → t_beam) * phi_j / sin(w_cam, w_beam)

    // Surface gather (same as PPM)
    if surface is diffuse:
        gather surface photons at hit
```

## Testing Progression

| Phase | Scene type | Medium | BSDFs |
|---|---|---|---|
| 1 | Diffuse surface only | None (vacuum) | Diffuse only |
| 2 | Medium only | Homogeneous, isotropic (`g=0`) | None (no surfaces) |
| 3 | Combined | Homogeneous, isotropic | Diffuse only |

Conductor and Dielectric BSDFs are out of scope for all phases.

## Scope

- **Volume**: beam-vs-ray gather using BVH of beam segments; Epanechnikov kernel on perpendicular distance.
- **Surface**: same as PPM surface gather (kd-tree radius search).
- **Phase function**: isotropic.
- **BSDFs**: Diffuse only. Conductor and Dielectric excluded.
- **Output**: EXR via capture pipeline.
- **Reuse**: `PhotonTracer` emit pass; `PhotonBeam` struct; existing BVH/scene infrastructure.

## Key Equation

**Beam radiance estimate** along camera ray segment:

```
L_vol = Σ_j K(u_t,j / r) * Tr(cam → t_cam,j) * σ_s * (1/4π)
        * Tr(beam_start_j → t_beam,j) * φ_j / (r * sin(θ_j))
```

where `sin(θ_j) = |ray.dir × beam_j.dir|`, matching the PBS fragment shader formula.

## Data Structures

### `BeamBvh`
- BVH of axis-aligned bounding boxes over beam capsules (sphere-swept AABBs).
- Interface: `build(vector<PhotonBeam>)`, `query_ray(ray, t_max, radius, callback)`.
- The callback receives `(t_cam, t_beam, u_t)` per intersecting beam.
- `tinybvh` can be used as the AABB accelerator; capsule test in the leaf callback.

## Implementation Plan

### New files
- `include/beam_bvh.hpp` + `src/beam_bvh.cpp` — `BeamBvh`
- `include/beam_mapper.hpp` + `src/beam_mapper.cpp` — `BeamMapper` class

### `BeamMapper` interface
```cpp
class BeamMapper {
public:
    BeamMapper(const Scene& scene, int n_photons, float r_beam, float r_surf, int max_cam_depth);
    void render(std::vector<float>& out, int width, int height);
private:
    void emit();
    glm::vec3 gather(const Ray& ray, Medium* medium, int depth);
};
```

### Reuse
- `PhotonTracer` emit pass (beams already recorded).
- `PhotonKdTree` from PPM for the surface photon gather.
- Capture pipeline for EXR export.

## Parameters (scene JSON)

```json
"beam_mapper": {
    "n_photons":     100000,
    "r_beam":        0.05,
    "r_surf":        0.05,
    "max_cam_depth": 1
}
```

## TODOs

| # | Task | Files |
|---|---|---|
| 1 | `BeamBvh` — build + ray query with capsule test | `include/beam_bvh.hpp`, `src/beam_bvh.cpp` |
| 2 | `BeamMapper` — emit pass (reuse tracer) | `include/beam_mapper.hpp`, `src/beam_mapper.cpp` |
| 3 | `BeamMapper` — gather pass (beam + surface) | `src/beam_mapper.cpp` |
| 4 | Wire into capture mode | `src/main.cpp`, `src/debug_ui.cpp` |
| 5 | Tests: beam query correctness, single-beam energy | `tests/beam_mapper_test.cpp` |
