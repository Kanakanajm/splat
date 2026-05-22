# Photon Tracing

CPU pass that stochastically samples photon paths from the scene's lights. 
When such path interacts with a surface (e.g. path traverses in vacuum and hit a surface), it is recorded as a surface photon point.
When the path interacts within a medium (e.g. path traverses in a medium and triggered medium scattering event by the free-flight sample), it is recorded as a medium photon beam. 
Photons that interact with the surface and those interact within the medium are stored separately. 

Theoretical basis: `jensen98` (photon mapping), `jarosz11comprehensive` (comprehensive framework of point and beam methods), `jarosz11progressive` (progressive photon beam). Citations resolved in `docs/literatures/guide.md`.

# Libraries
- **tinybvh** — single-header BVH builder + traverser.

# V1

### Scope
In v1, we implement the minimum backbone of the photon tracing phase.
We omit radiance calculation but focus on the path's emit, scatter and store logic.
- **Lights** Point Light
- **Medium** Homogenous with isotropic phase function and absorption & scattering coefficient (note that `σ_t = σ_s + σ_a`)
- **Surface BSDF** Diffuse, conductor (specular), dielectric and a special medium shell (pass-through) BSDF
- **Visualize** Final scene visualized as rasterized photon points/beams (rasterized points/lines) with object wireframes

### Algorithm
Per photon. Let `m` denotes the current medium (initially set to light's medium).

1. **Emit.** Sample a direction from unit sphere (point light).
2. **Closest-hit query** for the current ray against the BVH, yielding `t_hit` (∞ if no surface).
3. **Vacuum sample** If `m` is vacuum, go to step 5.b
4. **Free-flight sample.** If `m` is non-vacuum, sample `t_media = -ln(1 - ξ) / σ_t`; if `m` is vacuum, set `t_media = ∞`.
5. **Branch:**
    - **(a) `t_media < t_hit`** — media scatter event at `p = ray(t_media)`:
        1. Record a long beam from the ray origin to the medium exit (`t_hit`).
        2. Sample a new direction from unit sphere and generate new ray at media scatter event.
    - **(b) Otherwise** — surface hit at `p = ray(t_hit)`:
        1. If `p` is diffuse surface, store photon point.
        3. Sample a BSDF event — diffuse, specular, dielectric or pass-through — yielding new direction
        4. **Medium switch on refraction:** if the BSDF event is a refraction, set `m ← instance.medium_inside()` when entering (incoming direction opposes the shading normal) or `m ← instance.medium_outside()` when exiting. Diffuse/specular events do not change `m`.
5. **Terminate** via Russian roulette and the hard depth cap (see below); otherwise continue at step 2.

### Photon Beam Record
```cpp
struct PhotonPoint {
    glm::vec3 position; // world-space interaction point
    uint32_t bsdf_id;   // index into the bsdf type table
};

struct PhotonBeam {
    glm::vec3 start;     // world-space segment start
    glm::vec3 end;       // world-space segment end
    uint32_t  medium_id; // index into the medium table
};
```

Stored as a `std::vector<PhotonBeam>` and `std::vector<PhotonPoint>` owned by the tracer.
After the photon tracing phase ends, visualize them with options in `imgui` to switch showing photon points (as points) or photon beams (as lines) in real-time (camera can move around).
Color photon point with fake color based on `bsdf_id` and beams on `medium_id`. Wireframe of the scene objects can be blended in as a reference.

# Further Versions
## V2: Power/Flux Tracking During Interactions

### Scope
Add per-photon RGB power tracking through the full path. Power is initialized from the light source, propagated through medium and surface interactions via physically-based weight updates, and stored in each `PhotonBeam` and `PhotonPoint` record. Russian roulette replaces stochastic termination alongside the hard depth cap. Also implements the two deferred V1 BSDFs: **Conductor** (mirror) and **Dielectric** (Fresnel reflect/refract with medium switch).

### Struct Changes
- `PointLight` — add `bvhvec3 power` (JSON-configurable, e.g. `"power": [1, 1, 1]`)
- `PhotonBeam` — add `bvhvec3 power` (power entering the segment at `start`)
- `PhotonPoint` — add `bvhvec3 power`
- `Bsdf` — add `bvhvec3 transmittance_color` for Dielectric (existing `color` field becomes the reflectance tint); existing `color` is already the reflectance for Diffuse and Conductor
- `Bsdf::sample` return type extended from `bvhvec3` to `BsdfSample { bvhvec3 dir; bvhvec3 weight; bool is_refract; }` — the tracer reads `is_refract` for medium switch on Dielectric (previously only `BsdfKind::MediumShell` triggered medium switch); the BSDF encapsulates its own weight so the tracer does not recompute it

### Algorithm

Per photon. Initialize `weight = light.power / N` (per-photon share of total flux).

1. **Emit.** As V1.
2. **Closest-hit query.** As V1.
3. **Free-flight sample.** As V1.
4. **Branch:**
   - **(a) `t_media < t_hit` — medium scatter:**
     1. Record `PhotonBeam { start = ray.O, end = ray(t_hit), medium_id, power = weight }`.
     2. **Russian roulette:** `prr = clamp(max_component(weight), 0.05, 0.95)`. If `ξ ≥ prr` terminate; else `weight /= prr`.
     3. `weight *= σ_s / σ_t` (single-scatter albedo — see design note).
     4. Sample isotropic direction, spawn new ray from scatter point.
   - **(b) Surface hit:**
     1. **No weight correction for medium traversal** (see design note).
     2. If diffuse: record `PhotonPoint { position = p, power = weight, ... }`.
     3. **Russian roulette** (same formula as above).
     4. Call `bsdf.sample(rng, incoming, normal)` → `BsdfSample { dir, weight: bsdf_w, is_refract }`.
     5. `weight *= bsdf_w`.
     6. Medium switch if `is_refract` (Dielectric) or always (MediumShell), same flip logic as V1.
     7. Spawn new ray with ε-offset as V1.
5. **Terminate** if `weight` is black (all components zero after RR), or at the hard depth cap.

### BSDF Weight Rules (from lightwave bsdf reference)
| BSDF | Direction | `bsdf_w` | `is_refract` |
|---|---|---|---|
| Diffuse | cosine-hemisphere | `bsdf.color` (albedo) | `false` |
| Conductor | perfect mirror `reflect(D, n)` | `bsdf.color` (reflectance) | `false` |
| Dielectric (reflect) | `reflect(D, n)` | `bsdf.color` | `false` |
| Dielectric (refract) | `refract(D, n, η)` | `bsdf.transmittance_color / η²` | `true` |
| MediumShell | pass-through | `{1,1,1}` | `true` |

Dielectric branch chosen stochastically: compute `F = fresnelDielectric(cosθ, η)`; sample `ξ`; reflect if `ξ < F`, refract otherwise. Fresnel cancels with the sampling probability so no additional `F`-scaling is needed. TIR is handled inside `fresnelDielectric` (returns `F = 1`).

`fresnelDielectric` — full unpolarized Fresnel (not Schlick):
```
cosThetaT² = 1 - (1/η)² · (1 - cosθ²)
if cosThetaT² ≤ 0: TIR → F = 1
Rs = (η·cosθ - cosThetaT) / (η·cosθ + cosThetaT)
Rp = (cosθ - η·cosThetaT) / (cosθ + η·cosThetaT)
F  = 0.5 · (Rs² + Rp²)
```

### Design Notes
> These decisions were made during V2 planning and are kept for future toggling / comparison experiments.

**Scatter weight — `σ_s/σ_t` (chosen)**
When free-flight samples `d` from pdf = `σ_t · exp(−σ_t · d)`, the transmittance `exp(−σ_t · d)` and the PDF denominator cancel, leaving only the single-scatter albedo `σ_s / σ_t`. Alternative from beam.cpp: `weight *= exp(−σ_t · d) · (1 / prr)`.

**Surface transmittance — Option A: no explicit factor (chosen)**
With free-flight sampling, the probability of reaching the surface without scatter is `exp(−σ_t · t_hit)`. This survival probability and the transmittance cancel in the MC estimator, so the running weight needs no update at a surface hit through a medium. Alternative — Option B (beam.cpp): `weight *= exp(−σ_t · t_hit)` before the surface interaction.