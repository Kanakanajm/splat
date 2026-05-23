#include <catch2/catch_test_macros.hpp>

#include "bsdf.hpp"
#include "image_buffer.hpp"
#include "photon_tracer.hpp"
#include "point_light.hpp"
#include "random.hpp"
#include "ray_camera.hpp"
#include "ray_model.hpp"
#include "scene.hpp"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace {
constexpr const char* kSuzannePath = TEST_ASSETS_DIR "/meshes/Suzanne.ply";

// Axis-aligned closed box [lo, hi] as a flat fat-triangle buffer (12 triangles).
// Winding is arbitrary; the tracer orients normals onto the incident side.
std::vector<tinybvh::bvhvec4> make_box(tinybvh::bvhvec3 lo, tinybvh::bvhvec3 hi) {
    const tinybvh::bvhvec3 c[8] = {
        {lo.x, lo.y, lo.z}, {hi.x, lo.y, lo.z}, {hi.x, hi.y, lo.z}, {lo.x, hi.y, lo.z},
        {lo.x, lo.y, hi.z}, {hi.x, lo.y, hi.z}, {hi.x, hi.y, hi.z}, {lo.x, hi.y, hi.z},
    };
    const int faces[6][4] = {
        {0, 1, 2, 3}, {5, 4, 7, 6}, {4, 0, 3, 7}, {1, 5, 6, 2}, {4, 5, 1, 0}, {3, 2, 6, 7},
    };
    std::vector<tinybvh::bvhvec4> v;
    v.reserve(36);
    auto push = [&](const tinybvh::bvhvec3& p) { v.emplace_back(p.x, p.y, p.z, 0.0f); };
    for (const auto& f : faces) {
        push(c[f[0]]); push(c[f[1]]); push(c[f[2]]);
        push(c[f[0]]); push(c[f[2]]); push(c[f[3]]);
    }
    return v;
}

// Append a big axis-aligned quad in the z = z0 plane (extent +/- 5) to `v`.
// Wound so the geometric normal is +z (two CCW triangles seen from +z).
void push_z_quad(std::vector<tinybvh::bvhvec4>& v, float z0) {
    auto p = [&](float x, float y) { v.emplace_back(x, y, z0, 0.0f); };
    p(-5.0f, -5.0f); p(5.0f, -5.0f); p(5.0f, 5.0f);
    p(-5.0f, -5.0f); p(5.0f, 5.0f); p(-5.0f, 5.0f);
}
}  // namespace

TEST_CASE("PhotonTracer: photons land on Suzanne and project into a debug PPM",
          "[photon_tracer]") {
    RayModel model{std::string{kSuzannePath}};
    REQUIRE(model.triangle_count() > 0);

    tinybvh::BVH bvh;
    bvh.Build(model.triangles().data(), model.triangle_count());

    Scene scene{model};
    PointLight light{tinybvh::bvhvec3{0.0f, 0.0f, 3.0f}};
    PhotonTracer tracer{scene, bvh, light};
    Rng rng{0xDECAFu};

    constexpr uint32_t kN = 50000;
    tracer.trace(kN, /*max_depth=*/1, rng);

    INFO("photons stored=" << tracer.points().size() << " of " << kN);
    REQUIRE(tracer.points().size() > kN / 100);
    REQUIRE(tracer.points().size() <= kN);

    // Suzanne's geometry sits well within a [-2, 2]^3 box.
    for (const auto& p : tracer.points()) {
        REQUIRE(std::fabs(p.position.x) < 2.0f);
        REQUIRE(std::fabs(p.position.y) < 2.0f);
        REQUIRE(std::fabs(p.position.z) < 2.0f);
    }

    // Single-instance scene → every photon's bsdf_id placeholder == 0.
    for (const auto& p : tracer.points()) {
        REQUIRE(p.bsdf_id == 0u);
    }

    // Debug viz: splat each photon into the camera view.
    PinholeCamera cam{
        .eye    = {0.0f, 0.0f, 3.0f},
        .target = {0.0f, 0.0f, 0.0f},
        .up     = {0.0f, 1.0f, 0.0f},
        .fov_y  = 0.9f,
        .width  = 128,
        .height = 128,
    };
    ImageBuffer img(cam.width, cam.height);

    uint32_t splatted = 0;
    for (const auto& p : tracer.points()) {
        auto px = cam.project(p.position);
        if (!px) continue;
        const int x = px->first, y = px->second;
        if (x < 0 || y < 0 || x >= static_cast<int>(cam.width) || y >= static_cast<int>(cam.height))
            continue;
        img.set(static_cast<uint32_t>(x), static_cast<uint32_t>(y), 255, 255, 255);
        ++splatted;
    }

    namespace fs = std::filesystem;
    const fs::path dir = fs::path(TEST_OUTPUT_DIR);
    fs::create_directories(dir);
    write_ppm((dir / "photon_points_suzanne.ppm").string(), img);

    INFO("splatted=" << splatted);
    REQUIRE(splatted > tracer.points().size() / 4);  // light & cam co-located → most photons are in front
}

TEST_CASE("PhotonTracer: photons bounce inside a closed box — point count scales with depth",
          "[photon_tracer][bounce]") {
    RayModel model{make_box({-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}),
                   std::vector<uint32_t>(12u, 0u), 1u};
    tinybvh::BVH bvh;
    bvh.Build(model.triangles().data(), model.triangle_count());

    Scene scene{model};  // default bsdf id 0 == Diffuse
    PointLight light{tinybvh::bvhvec3{0.0f, 0.0f, 0.0f}};

    constexpr uint32_t kN = 20000;

    PhotonTracer t1{scene, bvh, light};
    Rng rng1{1u};
    t1.trace(kN, /*max_depth=*/1, rng1);
    const size_t c1 = t1.points().size();

    PhotonTracer t3{scene, bvh, light};
    Rng rng3{1u};
    t3.trace(kN, /*max_depth=*/3, rng3);
    const size_t c3 = t3.points().size();

    INFO("c1=" << c1 << " c3=" << c3);
    // Closed box: every emitted ray hits a wall, so depth 1 stores ~N points.
    REQUIRE(c1 > static_cast<size_t>(kN * 0.99));
    REQUIRE(c1 <= kN);
    // Each photon keeps bouncing; depth 3 stores points well past the first surface.
    REQUIRE(c3 > c1 * 5 / 2);
    // Vacuum scene ⇒ no medium scattering ⇒ no beams.
    REQUIRE(t3.beams().empty());

    // Every stored point lies on a box wall ⇒ strictly within the closed box.
    for (const auto& p : t3.points()) {
        REQUIRE(std::fabs(p.position.x) <= 1.0f + 1e-3f);
        REQUIRE(std::fabs(p.position.y) <= 1.0f + 1e-3f);
        REQUIRE(std::fabs(p.position.z) <= 1.0f + 1e-3f);
    }
}

TEST_CASE("PhotonTracer: only diffuse surfaces store photon points", "[photon_tracer][bsdf]") {
    RayModel model{make_box({-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}),
                   std::vector<uint32_t>(12u, 0u), 1u};
    tinybvh::BVH bvh;
    bvh.Build(model.triangles().data(), model.triangle_count());

    Scene scene{model};
    scene.set_instance_bsdf(0, 1u);
    scene.set_bsdf(1u, Bsdf{BsdfKind::Conductor, 1.0f});

    PointLight light{tinybvh::bvhvec3{0.0f, 0.0f, 0.0f}};
    PhotonTracer tracer{scene, bvh, light};
    Rng rng{2u};
    tracer.trace(/*photon_count=*/5000, /*max_depth=*/4, rng);

    // All geometry is non-diffuse, so no points are recorded even though photons hit.
    REQUIRE(tracer.points().empty());
}

TEST_CASE("PhotonTracer: photons inside a participating medium emit beams",
          "[photon_tracer][medium]") {
    RayModel model{make_box({-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}),
                   std::vector<uint32_t>(12u, 0u), 1u};
    tinybvh::BVH bvh;
    bvh.Build(model.triangles().data(), model.triangle_count());

    Scene scene{model};
    scene.set_medium(1u, Medium{/*sigma_s=*/3.0f, /*sigma_a=*/0.0f});

    // Light sits inside medium 1, so the photon starts in a participating medium.
    PointLight light{tinybvh::bvhvec3{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, /*medium_id=*/1u};
    PhotonTracer tracer{scene, bvh, light};
    Rng rng{4u};
    tracer.trace(/*photon_count=*/20000, /*max_depth=*/4, rng);

    REQUIRE(!tracer.beams().empty());
    for (const auto& b : tracer.beams()) {
        REQUIRE(b.medium_id == 1u);
        // Long beam ends at the medium exit — a box wall ⇒ within the closed box.
        REQUIRE(std::fabs(b.end.x) <= 1.0f + 1e-3f);
        REQUIRE(std::fabs(b.end.y) <= 1.0f + 1e-3f);
        REQUIRE(std::fabs(b.end.z) <= 1.0f + 1e-3f);
    }
}

TEST_CASE("PhotonTracer: beams stay inside a single medium cube in vacuum",
          "[photon_tracer][medium][shell]") {
    // Minimal containment scene: one closed cube of participating medium sitting in
    // vacuum, lit from above (also vacuum). Every beam must lie within the cube —
    // nothing should leak into the surrounding void.
    const tinybvh::bvhvec3 lo{-0.5f, -0.5f, -0.5f};
    const tinybvh::bvhvec3 hi{0.5f, 0.5f, 0.5f};
    RayModel model{make_box(lo, hi), std::vector<uint32_t>(12u, 0u), 1u};

    tinybvh::BVH bvh;
    bvh.Build(model.triangles().data(), model.triangle_count());

    Scene scene{model};
    scene.set_instance_bsdf(0u, 1u);
    scene.set_bsdf(1u, Bsdf{BsdfKind::MediumShell, 1.0f});
    scene.set_instance_medium(0u, /*in=*/1u, /*out=*/0u);
    scene.set_medium(1u, Medium{/*sigma_s=*/5.0f, /*sigma_a=*/0.0f});

    // Light directly above the cube, in vacuum (medium 0).
    PointLight light{tinybvh::bvhvec3{0.0f, 1.0f, 0.0f}};
    PhotonTracer tracer{scene, bvh, light};
    Rng rng{123u};
    tracer.trace(/*photon_count=*/50000, /*max_depth=*/32, rng);

    REQUIRE(!tracer.beams().empty());
    const float eps = 1e-3f;
    for (const auto& b : tracer.beams()) {
        REQUIRE(b.medium_id == 1u);
        for (const auto& q : {b.start, b.end}) {
            REQUIRE(q.x >= lo.x - eps);
            REQUIRE(q.x <= hi.x + eps);
            REQUIRE(q.y >= lo.y - eps);
            REQUIRE(q.y <= hi.y + eps);
            REQUIRE(q.z >= lo.z - eps);
            REQUIRE(q.z <= hi.z + eps);
        }
    }
}

TEST_CASE("PhotonTracer: beams stay contained across three nested medium cubes",
          "[photon_tracer][medium][shell]") {
    // Three concentric medium-shell cubes nesting four regions:
    //   vacuum (outside) | medium 1 | medium 2 | medium 3 (innermost).
    // Lit from above in vacuum. Flipping at each shell must keep every beam inside
    // the region of its medium; in particular nothing may leak outside the outer cube.
    const float outer = 0.6f, middle = 0.4f, inner = 0.2f;
    auto box = [](float h) {
        return make_box({-h, -h, -h}, {h, h, h});
    };
    std::vector<tinybvh::bvhvec4> verts;
    for (float h : {outer, middle, inner}) {
        auto b = box(h);
        verts.insert(verts.end(), b.begin(), b.end());
    }
    std::vector<uint32_t> inst;
    for (uint32_t i = 0; i < 3; ++i) inst.insert(inst.end(), 12u, i);
    RayModel model{std::move(verts), std::move(inst), 3u};

    tinybvh::BVH bvh;
    bvh.Build(model.triangles().data(), model.triangle_count());

    Scene scene{model};
    scene.set_bsdf(1u, Bsdf{BsdfKind::MediumShell, 1.0f});
    for (uint32_t i = 0; i < 3; ++i) scene.set_instance_bsdf(i, 1u);
    // Each shell separates its inside medium (i+1) from the medium just outside it (i).
    scene.set_instance_medium(0u, /*in=*/1u, /*out=*/0u);
    scene.set_instance_medium(1u, /*in=*/2u, /*out=*/1u);
    scene.set_instance_medium(2u, /*in=*/3u, /*out=*/2u);
    scene.set_medium(1u, Medium{/*sigma_s=*/4.0f, /*sigma_a=*/0.0f});
    scene.set_medium(2u, Medium{/*sigma_s=*/4.0f, /*sigma_a=*/0.0f});
    scene.set_medium(3u, Medium{/*sigma_s=*/4.0f, /*sigma_a=*/0.0f});

    PointLight light{tinybvh::bvhvec3{0.0f, 1.2f, 0.0f}};
    PhotonTracer tracer{scene, bvh, light};
    Rng rng{321u};
    tracer.trace(/*photon_count=*/100000, /*max_depth=*/64, rng);

    REQUIRE(!tracer.beams().empty());

    // Half-extent of the cube bounding each medium's region.
    auto region_half = [&](uint32_t medium_id) {
        switch (medium_id) {
            case 1: return outer;
            case 2: return middle;
            case 3: return inner;
            default: return 0.0f;
        }
    };
    const float eps = 1e-3f;
    bool seen[4] = {false, false, false, false};
    for (const auto& b : tracer.beams()) {
        REQUIRE(b.medium_id >= 1u);
        REQUIRE(b.medium_id <= 3u);
        seen[b.medium_id] = true;
        const float h = region_half(b.medium_id) + eps;
        for (const auto& q : {b.start, b.end}) {
            REQUIRE(std::fabs(q.x) <= h);
            REQUIRE(std::fabs(q.y) <= h);
            REQUIRE(std::fabs(q.z) <= h);
        }
    }
    // Photons reach every nested layer.
    REQUIRE(seen[1]);
    REQUIRE(seen[2]);
    REQUIRE(seen[3]);
}

TEST_CASE("PhotonTracer: passing through a medium-shell switches the current medium",
          "[photon_tracer][medium][shell]") {
    // Slab scene: a medium-shell wall at z=0 (normal +z, facing the light) backed by a
    // diffuse wall at z=-2. The light sits at z=+1 in vacuum. A photon heading down
    // crosses the shell, entering medium 1, then free-flights inside the slab.
    std::vector<tinybvh::bvhvec4> verts;
    push_z_quad(verts, 0.0f);   // instance 0: medium shell
    push_z_quad(verts, -2.0f);  // instance 1: diffuse backing
    std::vector<uint32_t> inst = {0u, 0u, 1u, 1u};
    RayModel model{std::move(verts), std::move(inst), 2u};

    tinybvh::BVH bvh;
    bvh.Build(model.triangles().data(), model.triangle_count());

    Scene scene{model};
    scene.set_instance_bsdf(0u, 1u);
    scene.set_bsdf(1u, Bsdf{BsdfKind::MediumShell, 1.0f});
    // Shell normal points +z (outside = vacuum), inside (−z side) = medium 1.
    scene.set_instance_medium(0u, /*in=*/1u, /*out=*/0u);
    scene.set_medium(1u, Medium{/*sigma_s=*/2.0f, /*sigma_a=*/0.0f});

    PointLight light{tinybvh::bvhvec3{0.0f, 0.0f, 1.0f}};
    PhotonTracer tracer{scene, bvh, light};
    Rng rng{9u};
    tracer.trace(/*photon_count=*/20000, /*max_depth=*/4, rng);

    // Beams only exist if the photon entered medium 1 on crossing the shell — in
    // vacuum (no switch) sigma_t == 0 and nothing scatters.
    REQUIRE(!tracer.beams().empty());
    for (const auto& b : tracer.beams()) {
        REQUIRE(b.medium_id == 1u);
    }
}
