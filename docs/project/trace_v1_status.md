# Photon Tracer V1 — Status & Plan

Working branch: `trace`. Driven by `docs/project/trace.md`.

## Where we are

The CPU-side closest-hit infrastructure is complete; the photon tracer now does
real multi-bounce diffuse transport in vacuum (light → emit → BVH closest-hit →
store diffuse `PhotonPoint` → cosine-hemisphere bounce → repeat to a hard depth
cap). What's missing for V1 is media (free-flight + `PhotonBeam`), dielectric/
medium-shell BSDFs, and the OpenGL visualization pass.

Note: Russian roulette is deferred — photon power is not tracked yet, so paths
terminate only on the hard depth cap or a miss.

## What's done

| Chunk | Files | Tests |
|---|---|---|
| Vendor tinybvh | `external/tinybvh/tiny_bvh.h`, `src/tiny_bvh.cpp` (`TINYBVH_IMPLEMENTATION`) | (compile only) |
| RayModel — assimp → flat fat-triangle buffer | `include/ray_model.hpp`, `src/ray_model.cpp` | `tests/ray_model_test.cpp` |
| BVH boundary tests on simple primitives | — | `tests/bvh_intersect_test.cpp` |
| Pinhole camera + ImageBuffer + PPM writer | `include/ray_camera.hpp`, `src/ray_camera.cpp`, `include/image_buffer.hpp`, `src/image_buffer.cpp` | (used by render tests) |
| Image-render tests w/ analytic ground truth | — | `tests/bvh_intersect_image_test.cpp` |
| PRNG + uniform sphere sampling | `include/random.hpp`, `include/sampling.hpp`, `src/sampling.cpp` | `tests/sampling_test.cpp` |
| Point light + emit_ray | `include/point_light.hpp`, `src/point_light.cpp` | `tests/point_light_test.cpp` |
| Multi-bounce PhotonTracer (diffuse, vacuum, depth cap) | `include/photon.hpp`, `include/photon_tracer.hpp`, `src/photon_tracer.cpp` | `tests/photon_tracer_test.cpp` |
| Scene: instance → bsdf/medium lookup | `include/scene.hpp`, `src/scene.cpp` | `tests/scene_test.cpp` |
| Diffuse BSDF + cosine-hemisphere sampling + Scene bsdf table | `include/bsdf.hpp`, `src/bsdf.cpp`, `include/sampling.hpp`, `src/sampling.cpp`, `include/scene.hpp`, `src/scene.cpp` | `tests/bsdf_test.cpp` |

33/33 tests passing as of this snapshot.

### Key API surface

```cpp
class RayModel {
    explicit RayModel(const std::string& path);                       // load via assimp
    RayModel(std::vector<bvhvec4>, std::vector<uint32_t>, uint32_t);  // synthetic (tests)
    const std::vector<bvhvec4>& triangles() const;                    // tinybvh fat-tri layout
    uint32_t triangle_count() const;
    uint32_t instance_id(uint32_t prim) const;
    uint32_t instance_count() const;
};

class Scene {
    explicit Scene(const RayModel&);
    void set_instance_bsdf(uint32_t instance_id, uint32_t bsdf_id);
    void set_instance_medium(uint32_t instance_id, uint32_t in, uint32_t out);
    uint32_t bsdf_id_at(uint32_t prim)    const;  // 0 default
    uint32_t medium_in_at(uint32_t prim)  const;  // 0 default (vacuum convention)
    uint32_t medium_out_at(uint32_t prim) const;
};

struct PinholeCamera {  // aggregate
    bvhvec3 eye, target, up; float fov_y; uint32_t width, height;
    tinybvh::Ray generate_ray(uint32_t px, uint32_t py) const;
    std::optional<std::pair<int,int>> project(const bvhvec3& world_p) const;
};

class Rng { explicit Rng(uint64_t seed); float uniform(); uint32_t uniform_u32(); };
bvhvec3 sample_unit_sphere(Rng&);
bvhvec3 sample_cosine_hemisphere(Rng&, const bvhvec3& normal);  // PDF = cos θ / π

enum class BsdfKind { Diffuse, Conductor, Dielectric, MediumShell };
struct Bsdf {
    BsdfKind kind = BsdfKind::Diffuse; float ior = 1.0f;
    bvhvec3 sample(Rng&, const bvhvec3& incoming, const bvhvec3& normal) const;  // diffuse only so far
};
// Scene also gains: set_bsdf(id, Bsdf), bsdf(id), bsdf_at(prim). id 0 = default Diffuse.

struct PointLight { bvhvec3 position; uint32_t medium_id; tinybvh::Ray emit_ray(Rng&) const; };

struct PhotonPoint { bvhvec3 position; uint32_t bsdf_id;   };
struct PhotonBeam  { bvhvec3 start, end; uint32_t medium_id; };

class PhotonTracer {
    PhotonTracer(const Scene&, const tinybvh::BVH&, const PointLight&);
    void trace(uint32_t N, uint32_t max_depth, Rng&);  // diffuse multi-bounce, vacuum
    const std::vector<PhotonPoint>& points() const;
    const std::vector<PhotonBeam>&  beams()  const;   // unused until media chunk
};
```

### Conventions established

- **`bsdf_id` / `medium_id` of 0** ⇒ defaults / vacuum convention. Real tables come later.
- **tinybvh impl macro is `TINYBVH_IMPLEMENTATION`** (no underscore between TINY and BVH). Define once in `src/tiny_bvh.cpp`.
- **`aiProcess_Triangulate | aiProcess_PreTransformVertices`** in `RayModel` — collapses node graph; each remaining assimp mesh = one instance.
- **Tests emit PPM (P6) artifacts** to `${TEST_OUTPUT_DIR}` = `build/tests/output/`. Reference images are not committed; instead each render test renders twice (tinybvh vs analytic intersector) and compares with a per-channel tolerance derivable from the math.
- **Catch2 test case names must not contain commas inside brackets** — `catch_discover_tests` splits on `,` and silently collapses subsequent tests into one filter.
- **Designated initializers** (e.g. `PinholeCamera{.eye=..., .target=...}`) are accepted by GCC in C++17 as an extension — relied on in tests.

## What's next, in order

Each chunk follows the project's TDD red-green flow.

1. ~~**BSDFs — diffuse first.**~~ ✅ DONE — see `tests/bsdf_test.cpp`.
   - `BsdfKind`, `Bsdf{kind,ior}`, Scene bsdf table, `sample_cosine_hemisphere`, `Bsdf::sample`.
   - `Bsdf::sample` flips the geometric normal onto the incident side, then cosine-samples.
   - Cosine-hemisphere basis: branchless Duff et al. 2017 ONB. Verified E[cos θ]=2/3, mean dir=(2/3)n.

2. ~~**PhotonTracer: actual bouncing (still vacuum).**~~ ✅ DONE — see `tests/photon_tracer_test.cpp`.
   - Loop: emit → closest hit → store point (diffuse only) → BSDF sample → offset+respawn → repeat to `max_depth`.
   - Hard depth cap only; **Russian roulette deferred** (no photon-power tracking in V1).
   - Points stored only at diffuse surfaces; non-diffuse hits scatter without storing.
   - Closed-box test verifies point count scales with depth (bounces happen) and all points stay inside the box.
   - Ray respawn offsets the origin by `1e-4` along the new direction to escape the surface just hit.

3. **Media + free-flight sampling.**
   - `struct Medium { float sigma_s, sigma_a; };` + `Scene` table.
   - `sample_free_flight(σ_t, ξ) = -ln(1-ξ)/σ_t`.
   - Branch in tracer per `trace.md` step 5: `t_media < t_hit` ⇒ store `PhotonBeam{start=ray.O, end=ray.O+t_hit*D, medium_id=m}`, scatter isotropically. Else ⇒ surface hit path (existing).
   - Tests: distribution of free-flight samples matches exponential; one-medium scene emits PhotonBeams whose total length matches `N/σ_t` within tolerance.

4. **Medium switch on refraction (dielectric/medium-shell BSDF).**
   - Refraction direction from incoming + normal + IOR ratio; Fresnel decides reflect vs refract.
   - Update `m ← scene.medium_in_at(prim)` / `medium_out_at(prim)` based on incoming · normal sign.

5. **Visualization (OpenGL).**
   - Replace the PPM debug dump with a real GL pass: draw `PhotonPoint` as `GL_POINTS` colored by `bsdf_id`, `PhotonBeam` as `GL_LINES` colored by `medium_id`. Reuse the existing `Model`/`Mesh` for wireframes.
   - ImGui toggles: show points / show beams / show wireframe / show BVH.

## Reproducibility

```
cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -G Ninja
cmake --build build -j8
ctest --test-dir build --output-on-failure
```

Inspect PPM artifacts at `build/tests/output/*.ppm`. Notable ones for sanity:
- `triangle_bary.{bvh,ref}.ppm` — the iconic RGB barycentric gradient.
- `two_spheres_instance.{bvh,ref}.ppm` — palette-shaded by instance id, rehearses the bsdf/medium fake-color path.
- `photon_points_suzanne.ppm` — 50k photons from a co-located light splatted into the camera view.
