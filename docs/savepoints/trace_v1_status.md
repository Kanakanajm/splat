# Photon Tracer V1 — Savepoint

Working branch: `trace`. Scope defined in `docs/project/trace.md`.

## Status: On-going (39/39 tests passing)

Core infrastructure is done. Remaining for V1: Dielectric BSDF and PhotonBeam GL visualization.

Russian roulette is deferred — paths terminate only on the hard depth cap.

## Completed Chunks

| Chunk | Files | Tests |
|---|---|---|
| tinybvh vendor | `external/tinybvh/tiny_bvh.h`, `src/tiny_bvh.cpp` | compile only |
| RayModel (assimp → fat-triangle buffer) | `include/ray_model.hpp`, `src/ray_model.cpp` | `tests/ray_model_test.cpp` |
| BVH boundary tests | — | `tests/bvh_intersect_test.cpp` |
| Pinhole camera + ImageBuffer + PPM writer | `include/ray_camera.hpp`, `include/image_buffer.hpp` | image render tests |
| Image-render tests w/ analytic ground truth | — | `tests/bvh_intersect_image_test.cpp` |
| PRNG + uniform sphere sampling | `include/random.hpp`, `include/sampling.hpp`, `src/sampling.cpp` | `tests/sampling_test.cpp` |
| Point light + emit_ray | `include/point_light.hpp`, `src/point_light.cpp` | `tests/point_light_test.cpp` |
| Scene: instance → bsdf/medium lookup | `include/scene.hpp`, `src/scene.cpp` | `tests/scene_test.cpp` |
| Diffuse BSDF + cosine-hemisphere sampling | `include/bsdf.hpp`, `src/bsdf.cpp`, `include/sampling.hpp` | `tests/bsdf_test.cpp` |
| Multi-bounce PhotonTracer (diffuse, vacuum, depth cap) | `include/photon_tracer.hpp`, `src/photon_tracer.cpp` | `tests/photon_tracer_test.cpp` |
| Media + free-flight + PhotonBeam emission | `include/medium.hpp`, `src/sampling.cpp`, `src/photon_tracer.cpp` | `tests/sampling_test.cpp`, `tests/photon_tracer_test.cpp` |
| Medium-shell pass-through BSDF + medium switch | `src/bsdf.cpp`, `src/photon_tracer.cpp` | `tests/bsdf_test.cpp`, `tests/photon_tracer_test.cpp` |
| Photon-point GL visualization (instance-id colored) | `src/scene_gl.cpp`, `shaders/point.{vs,fs}`, `src/main.cpp` | manual (app) |

## Key API

```cpp
// Scene
class Scene {
    void set_instance_bsdf(uint32_t instance_id, uint32_t bsdf_id);
    void set_instance_medium(uint32_t instance_id, uint32_t in, uint32_t out);
    void set_bsdf(uint32_t id, Bsdf);
    void set_medium(uint32_t id, Medium);
    uint32_t bsdf_id_at(uint32_t prim) const;   // 0 = default
    uint32_t medium_in_at(uint32_t prim) const;  // 0 = vacuum
    uint32_t medium_out_at(uint32_t prim) const;
};

// PhotonTracer
class PhotonTracer {
    PhotonTracer(const Scene&, const tinybvh::BVH&, const PointLight&);
    void trace(uint32_t N, uint32_t max_depth, Rng&);
    const std::vector<PhotonPoint>& points() const;
    const std::vector<PhotonBeam>&  beams()  const;
};

// Medium / BSDF
struct Medium { float sigma_s, sigma_a; float sigma_t() const; };
enum class BsdfKind { Diffuse, Conductor, Dielectric, MediumShell };
struct Bsdf { BsdfKind kind; float ior; bvhvec3 sample(Rng&, incoming, normal) const; };

// Sampling
bvhvec3 sample_cosine_hemisphere(Rng&, const bvhvec3& normal);  // PDF = cosθ/π
float   sample_free_flight(float sigma_t, float xi);            // = -ln(1-ξ)/σ_t
```

## Key Conventions

- **id 0** ⇒ default BSDF (diffuse) / vacuum medium.
- **Long-beam variant**: beam end is the medium exit (`t_hit`), scatter point spawns the new ray.
- **Medium switch**: `D·n < 0` (entering) → `medium_in`; `D·n > 0` (exiting) → `medium_out`. Only on `MediumShell` (and future `Dielectric`) hits.
- **`TINYBVH_IMPLEMENTATION`** defined once in `src/tiny_bvh.cpp`.
- **`aiProcess_Triangulate | aiProcess_PreTransformVertices`** — collapses node graph; each assimp mesh = one instance.
- GL code isolated in `src/scene_gl.cpp` (app-only TU); test target stays GL-free.

## Planned: Next Steps

### Immediate (still V1 polish)
1. **PhotonBeam GL visualization** — draw beams as `GL_LINES` colored by `medium_id`; ImGui toggles (points / beams / wireframe).

### V1 Extensions
2. **Dielectric BSDF** — Fresnel reflect/refract; total internal reflection; medium switch on transmission reuses the existing `medium_in/out` plumbing.
3. **Conductor BSDF** — mirror reflection; `Bsdf::sample` returns the perfect specular direction.

### V2 and Beyond
4. **Photon power / flux** — track power per photon; Russian roulette termination; weight stored points/beams.
5. **Opacity pass** — frustum slicing or depth peeling for camera-side attenuation maps.
6. **Splat pass** — rasterize beams as camera-facing billboard quads sampling the attenuation maps.
