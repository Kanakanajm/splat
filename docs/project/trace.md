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
## V2: add proper power/flux updates during interactions