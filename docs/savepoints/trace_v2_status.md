# Photon Tracer V2 — Savepoint

Branch: `trace`. Depends on trace_v1 and aov_debug being complete.
Scope defined in `docs/project/trace.md` § V2.

## Goal

Add per-photon RGB power tracking through the full path. Power is initialized
from the light, propagated through medium and surface interactions, and stored
in each `PhotonBeam` and `PhotonPoint`. Russian roulette is added as a
termination criterion. Implements the two deferred V1 BSDFs (Conductor,
Dielectric). Adds power-based AOVs for visualization.

## Architecture

### `BsdfSample` — extended return type

`Bsdf::sample` changes from returning `bvhvec3 dir` to:

```cpp
struct BsdfSample {
    bvhvec3 dir;
    bvhvec3 weight;   // BSDF multiplier for the running path weight
    bool    is_refract;  // true for Dielectric refract and MediumShell
};
```

The tracer reads `is_refract` to trigger medium switch (previously only
`BsdfKind::MediumShell` did; now Dielectric also does).

### Power on structs

```cpp
struct PhotonBeam {
    bvhvec3  start;
    bvhvec3  end;
    bvhvec3  power;      // power entering the segment at start
    uint32_t medium_id;
    uint32_t bounce_depth;
};

struct PhotonPoint {
    bvhvec3  position;
    bvhvec3  power;
    uint32_t bsdf_id;
    uint32_t instance_id;
    uint32_t bounce_depth;
};
```

### `PointLight::power`

```cpp
struct PointLight {
    bvhvec3  position;
    bvhvec3  power;     // total RGB flux; per-photon share = power / N
    uint32_t medium_id;
};
```

Loaded from JSON as `"power": [r, g, b]`.

### Weight tracking in `PhotonTracer`

```
weight = light.power / N   // per-photon init

// medium scatter (t_media < t_hit):
beam.power = weight
prr = clamp(max_component(weight), 0.05, 0.95)
if ξ ≥ prr → terminate
weight /= prr
weight *= σ_s / σ_t       // single-scatter albedo (see design notes)

// surface hit:
// no weight update for medium traversal (see design notes)
if diffuse → point.power = weight
prr = clamp(max_component(weight), 0.05, 0.95)
if ξ ≥ prr → terminate
weight /= prr
weight *= bsdf_sample.weight
medium switch if bsdf_sample.is_refract
```

### `fresnelDielectric`

Full unpolarized Fresnel (not Schlick). Handles TIR by returning `F = 1`.

```
cosThetaT² = 1 − (1/η)² · (1 − cosθ²)
if cosThetaT² ≤ 0 → TIR, F = 1
Rs = (η·cosθ − cosThetaT) / (η·cosθ + cosThetaT)
Rp = (cosθ − η·cosThetaT) / (cosθ + η·cosThetaT)
F  = 0.5 · (Rs² + Rp²)
```

Dielectric `BsdfSample`:
- Reflect (`ξ < F`): `weight = bsdf.color`, `is_refract = false`
- Refract (`ξ ≥ F`): `weight = bsdf.transmittance_color / η²`, `is_refract = true`

Fresnel cancels with the sampling probability so no additional F-scaling.

### Power AOVs

Extends `ViewState` with new point and beam AOV modes.

| AOV | Target | Description |
|---|---|---|
| PowerColor | Points | Stored `power` RGB used directly as display color |
| PowerLuminance | Points | Grayscale by `max_component(power)` |
| PowerNormalized | Points | `max_component(power) / scene_max` → grayscale |
| BeamPowerStart | Beams | Beam colored uniformly by stored `power` |
| BeamTransmittancePreview | Beams | `power * exp(−σ_t · length)` — preview of power exiting the segment |

`BeamTransmittancePreview` requires `σ_t` passed as a per-beam uniform (keyed on `medium_id`).

## Design Decisions

**Scatter weight — `σ_s/σ_t` (chosen)**
Free-flight samples `d` from pdf = `σ_t · exp(−σ_t · d)`; transmittance and PDF
cancel, leaving only `σ_s/σ_t`. Alternative (beam.cpp): `weight *= exp(−σ_t · d)`.

**Surface transmittance — Option A: no explicit factor (chosen)**
Survival probability `exp(−σ_t · t_hit)` and transmittance cancel in the MC
estimator. Alternative — Option B (beam.cpp): `weight *= exp(−σ_t · t_hit)`.

## Tasks

| # | Task | Files |
|---|---|---|
| 1 | `BsdfSample` return struct + update `Bsdf::sample` signature | `include/bsdf.hpp`, `src/bsdf.cpp`, `src/photon_tracer.cpp` |
| 2 | Conductor BSDF — mirror direction, `weight = bsdf.color` | `src/bsdf.cpp` |
| 3 | `fresnelDielectric` + Dielectric BSDF | `include/bsdf.hpp`, `src/bsdf.cpp` |
| 4 | `PointLight::power` + JSON wiring | `include/point_light.hpp`, scene JSON config |
| 5 | Power fields on `PhotonBeam` / `PhotonPoint` | `include/photon.hpp` |
| 6 | Weight tracking in `PhotonTracer` — init, RR, scatter albedo, Dielectric medium switch | `src/photon_tracer.cpp` |
| 7 | Power AOVs — 5 new view modes | `src/scene_gl.cpp`, `src/debug_ui.cpp`, `shaders/point.{vs,fs}`, `shaders/beam.{vs,fs}` |

## Status: In Progress (0/7 tasks done)
