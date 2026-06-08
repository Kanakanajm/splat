# Path Tracer (PT)

Reference ground-truth CPU renderer. Used as the quality baseline for all other methods.
Implemented in the same framework to guarantee scene parity.

Reference implementation: `clefairy/src/integrators/pathtracer.cpp`.

## Validation Against Mitsuba

Before using PT as a baseline for the other methods, validate it against
[Mitsuba 3](https://www.mitsuba-renderer.org/) on the same scene parameters.

**Comparison metrics:**
- **Convergence plot** — MSE or RMSE vs. sample count (log-log); both should converge at O(1/√N).
- **Pixel-level error** — false-color error image (|PT − Mitsuba| / Mitsuba) at fixed SPP.
- **Scene parity check** — match extinction coefficients, phase function (isotropic `g=0`),
  and light power between the splat JSON scene and the Mitsuba XML scene before comparing.

**Workflow:**
1. Export scene to a Mitsuba 3 XML file (manually or via a small helper script).
2. Render with Mitsuba `volpath` integrator at high SPP as ground truth.
3. Render PT at matched SPP; compute per-channel RMSE.
4. If RMSE < threshold (e.g. 1%) at convergence, PT is validated.

## Algorithm

Per pixel, traces a camera ray and recursively samples the scene until hitting a light or
terminating via Russian roulette.

```
for each pixel (i, j):
    ray = camera.generate_ray(i, j)
    Lo  = Li(ray)
    image[i][j] += Lo / spp
```

**`Li(ray)` — volumetric path tracing:**

Let `medium` = camera's starting medium (initially vacuum).

```
weight = 1; Lo = 0
for depth = 0..maxDepth:
    t_hit = BVH.intersect(ray)                    // ∞ if miss
    if medium is non-vacuum:
        t_med = -ln(ξ) / sigma_t                  // free-flight sample
    else:
        t_med = ∞

    if t_med < t_hit:                             // medium scatter
        weight *= sigma_s / sigma_t               // single-scatter albedo
        Lo     += weight * phase(wo, wi) * direct_light_estimate(x)
        sample new direction from phase function
        ray.origin = x = ray(t_med)
    else:                                         // surface hit or miss
        if miss: Lo += Le * weight; break
        weight *= Tr(ray.origin → hit)            // Beer-Lambert
        if diffuse surface: Lo += weight * direct_light_estimate(hit)
        sample BSDF direction; weight *= bsdf_weight
        update medium on refraction; advance ray
    Russian roulette on weight
```

## Testing Progression

| Phase | Scene type | Medium | BSDFs |
|---|---|---|---|
| 1 | Diffuse surface only | None (vacuum) | Diffuse only |
| 2 | Medium only | Homogeneous, isotropic (`g=0`) | None (no surfaces) |
| 3 | Combined | Homogeneous, isotropic | Diffuse only |

Conductor and Dielectric BSDFs are out of scope for all phases.

## Scope

- **Medium**: homogeneous, isotropic phase function (`g = 0`).
- **Surface BSDFs**: Diffuse only. Conductor and Dielectric excluded.
- **Lights**: all existing `PointLight`s + `EnvLight` (NEE for direct light at surface and medium scatter).
- **Output**: write per-pixel radiance into capture accumulation buffer → EXR (reuse capture pipeline).
- **No OpenGL** — pure CPU rendering; display is capture-mode only.

## Key Equations

**Transmittance** (Beer-Lambert, homogeneous):
```
Tr(a → b) = exp(-sigma_t * |b - a|)
```

**Direct lighting estimate at medium scatter point** `x`:
```
L_direct = sum_lights [ Tr(x → light) * phase(wo, wi) * light.power / dist² ]
```

**Direct lighting estimate at surface hit** `p`:
```
L_direct = sum_lights [ Tr(p → light) * f_r(p, wi, wo) * cos(theta_i) * light.power / dist² ]
```

**Phase function** (isotropic):
```
p(wo, wi) = 1 / (4π)
```

## Implementation Plan

### New files
- `include/path_tracer.hpp` + `src/path_tracer.cpp` — `PathTracer` class

### `PathTracer` interface
```cpp
class PathTracer {
public:
    PathTracer(const Scene& scene, int max_depth, int spp);
    // renders into a float RGB buffer (same layout as capture accumFbo readback)
    void render(std::vector<float>& out, int width, int height);
};
```

### Reuse
- `PhotonTracer`'s BVH, scene geometry, BSDFs, medium lookup — all passed by const-ref.
- Capture pipeline for EXR export.
- `sample_cosine_hemisphere`, `sample_uniform_sphere` from `sampling.hpp`.

## Parameters (scene JSON)

```json
"path_tracer": {
    "max_depth": 8,
    "spp":       64
}
```

## TODOs

| # | Task | Files |
|---|---|---|
| 1 | ~~`PathTracer` class: ray generation, Li loop, NEE~~ ✓ | `include/path_tracer.hpp`, `src/path_tracer.cpp` |
| 2 | ~~Wire into capture mode (`main.cpp`) as an alternative renderer~~ ✓ | `src/main.cpp`, `src/debug_ui.cpp` |
| 3 | ~~Tests: Beer-Lambert single slab, single-scatter energy balance~~ ✓ | `tests/path_tracer_test.cpp` |
| 4 | Validate against Mitsuba 3 `volpath` integrator | — |

## Known Limitations

### Shadow ray medium attenuation (deferred)

`nee_surface` and `nee_medium` in `src/path_tracer.cpp` compute shadow-ray transmittance as:

```cpp
float Tr = std::exp(-sigma_t * dist);  // sigma_t of current medium only
```

This is **incorrect** when the shadow ray traverses multiple media (e.g. exits a MediumShell
into vacuum before reaching the light). The correct approach is to walk the shadow ray through
the scene, accumulating `exp(-sigma_t_i * segment_i)` for each medium segment.

**Impact**: wrong attenuation for any scene where the shadow ray crosses a medium boundary.
**Scope**: deferred until medium rendering validation. Vacuum scenes (sigma_t = 0 → Tr = 1)
and pure-diffuse surface renders are unaffected.
