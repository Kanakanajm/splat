#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "bsdf.hpp"
#include "image_buffer.hpp"
#include "photon_tracer.hpp"
#include "point_light.hpp"
#include "random.hpp"
#include "ray_camera.hpp"
#include "ray_model.hpp"
#include "scene.hpp"

#include <array>
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
    constexpr uint32_t kN = 20000;
    // Power set to kN so init_weight = 1; prr ≈ 0.95, giving ~3× points at depth 3.
    PointLight light{tinybvh::bvhvec3{0.0f, 0.0f, 0.0f},
                     {static_cast<float>(kN), static_cast<float>(kN), static_cast<float>(kN)}};

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

    // Power = N so init_weight = 1 and prr ≈ 0.95; photons survive all three shells.
    constexpr uint32_t kN3 = 100000;
    PointLight light{tinybvh::bvhvec3{0.0f, 1.2f, 0.0f},
                     {static_cast<float>(kN3), static_cast<float>(kN3), static_cast<float>(kN3)}};
    PhotonTracer tracer{scene, bvh, light};
    Rng rng{321u};
    tracer.trace(/*photon_count=*/kN3, /*max_depth=*/64, rng);

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

TEST_CASE("PhotonTracer: stored point carries light power / N on first diffuse hit",
          "[photon_tracer][power]") {
    // N=1: init_weight = power / 1. Closed box guarantees a hit.
    // RR is applied after storing the point, so the stored power equals init_weight.
    RayModel model{make_box({-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}),
                   std::vector<uint32_t>(12u, 0u), 1u};
    tinybvh::BVH bvh;
    bvh.Build(model.triangles().data(), model.triangle_count());

    Scene scene{model};
    PointLight light{tinybvh::bvhvec3{0.0f, 0.0f, 0.0f}, {2.0f, 4.0f, 6.0f}};
    PhotonTracer tracer{scene, bvh, light};
    Rng rng{0u};
    tracer.trace(/*photon_count=*/1, /*max_depth=*/1, rng);

    REQUIRE(tracer.points().size() == 1u);
    const auto& p = tracer.points()[0];
    REQUIRE(p.power.x == Catch::Approx(2.0f));
    REQUIRE(p.power.y == Catch::Approx(4.0f));
    REQUIRE(p.power.z == Catch::Approx(6.0f));
}

// ─── Sanity checks: energy conservation ──────────────────────────────────

TEST_CASE("PhotonTracer [sanity]: depth-0 surface power sum equals light flux",
          "[photon_tracer][power][sanity]") {
    // Closed box, default diffuse, vacuum. All N photons hit at depth 0.
    // Each stores power = light.power/N, so the aggregate sum = light.power.
    RayModel model{make_box({-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}),
                   std::vector<uint32_t>(12u, 0u), 1u};
    tinybvh::BVH bvh;
    bvh.Build(model.triangles().data(), model.triangle_count());
    Scene scene{model};
    constexpr uint32_t kN = 100'000;
    PointLight light{tinybvh::bvhvec3{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}};
    PhotonTracer tracer{scene, bvh, light};
    Rng rng{42u};
    tracer.trace(kN, /*max_depth=*/1, rng);

    float sum_r = 0.0f, sum_g = 0.0f, sum_b = 0.0f;
    for (const auto& p : tracer.points())
        if (p.bounce_depth == 0) { sum_r += p.power.x; sum_g += p.power.y; sum_b += p.power.z; }

    REQUIRE(sum_r == Catch::Approx(1.0f).epsilon(0.02f));
    REQUIRE(sum_g == Catch::Approx(1.0f).epsilon(0.02f));
    REQUIRE(sum_b == Catch::Approx(1.0f).epsilon(0.02f));
}

TEST_CASE("PhotonTracer [sanity]: surface power ratio per depth equals albedo",
          "[photon_tracer][power][sanity]") {
    // Closed box, diffuse albedo=0.5, no medium.
    // Each bounce multiplies the running weight by albedo; RR is unbiased, so
    // E[sum at depth k+1] / E[sum at depth k] = albedo.
    // light.power = N so init_weight = 1.0 and prr ≈ 0.95 rather than the 0.05 clamp.
    constexpr float    kAlbedo = 0.5f;
    constexpr uint32_t kN      = 100'000;
    constexpr uint32_t kDepths = 5;

    RayModel model{make_box({-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}),
                   std::vector<uint32_t>(12u, 0u), 1u};
    tinybvh::BVH bvh;
    bvh.Build(model.triangles().data(), model.triangle_count());
    Scene scene{model};
    scene.set_bsdf(1u, Bsdf{BsdfKind::Diffuse, 1.0f, {kAlbedo, kAlbedo, kAlbedo}});
    scene.set_instance_bsdf(0u, 1u);
    PointLight light{tinybvh::bvhvec3{0.0f, 0.0f, 0.0f},
                     {static_cast<float>(kN), static_cast<float>(kN), static_cast<float>(kN)}};
    PhotonTracer tracer{scene, bvh, light};
    Rng rng{42u};
    tracer.trace(kN, kDepths, rng);

    std::array<float, kDepths> depth_sum{};
    for (const auto& p : tracer.points())
        if (p.bounce_depth < kDepths) depth_sum[p.bounce_depth] += p.power.x;

    for (uint32_t k = 0; k + 1 < kDepths; ++k) {
        REQUIRE(depth_sum[k] > 0.0f);
        const float ratio = depth_sum[k + 1] / depth_sum[k];
        INFO("k=" << k << " sum[k]=" << depth_sum[k] << " ratio=" << ratio);
        REQUIRE(ratio == Catch::Approx(kAlbedo).epsilon(0.05f));
    }
}

TEST_CASE("PhotonTracer [sanity]: depth-0 beam power sum equals light flux",
          "[photon_tracer][power][sanity][medium]") {
    // Dense medium (sigma_s=100): mean free path = 0.01, well inside the 10x10x10 box.
    // All N photons scatter at depth 0. Each beam stores power = light.power/N, sum = light.power.
    RayModel model{make_box({-5.0f, -5.0f, -5.0f}, {5.0f, 5.0f, 5.0f}),
                   std::vector<uint32_t>(12u, 0u), 1u};
    tinybvh::BVH bvh;
    bvh.Build(model.triangles().data(), model.triangle_count());
    Scene scene{model};
    scene.set_medium(1u, Medium{/*sigma_s=*/100.0f, /*sigma_a=*/0.0f});
    constexpr uint32_t kN = 100'000;
    PointLight light{tinybvh::bvhvec3{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, /*medium_id=*/1u};
    PhotonTracer tracer{scene, bvh, light};
    Rng rng{42u};
    tracer.trace(kN, /*max_depth=*/1, rng);

    float sum_r = 0.0f, sum_g = 0.0f, sum_b = 0.0f;
    for (const auto& b : tracer.beams())
        if (b.bounce_depth == 0) { sum_r += b.power.x; sum_g += b.power.y; sum_b += b.power.z; }

    REQUIRE(sum_r == Catch::Approx(1.0f).epsilon(0.02f));
    REQUIRE(sum_g == Catch::Approx(1.0f).epsilon(0.02f));
    REQUIRE(sum_b == Catch::Approx(1.0f).epsilon(0.02f));
}

TEST_CASE("PhotonTracer [sanity]: beam power ratio per depth equals single-scatter albedo",
          "[photon_tracer][power][sanity][medium]") {
    // Dense medium (sigma_s=100): photons scatter within 0.05 units across 5 bounces,
    // never reaching the 10x10x10 walls — all depth-k records are beams.
    // At each scatter weight *= sigma_s/sigma_t; RR is unbiased, so
    // E[sum at depth k+1] / E[sum at depth k] = sigma_s/sigma_t.
    // light.power = N so init_weight = 1.0 (avoids the 0.05 prr clamp).
    constexpr uint32_t kN      = 100'000;
    constexpr uint32_t kDepths = 5;
    constexpr float    kSigmaS = 100.0f;

    RayModel model{make_box({-5.0f, -5.0f, -5.0f}, {5.0f, 5.0f, 5.0f}),
                   std::vector<uint32_t>(12u, 0u), 1u};
    tinybvh::BVH bvh;
    bvh.Build(model.triangles().data(), model.triangle_count());

    auto run = [&](float sigma_a) {
        const float expected_albedo = kSigmaS / (kSigmaS + sigma_a);
        Scene scene{model};
        scene.set_medium(1u, Medium{kSigmaS, sigma_a});
        PointLight light{tinybvh::bvhvec3{0.0f, 0.0f, 0.0f},
                         {static_cast<float>(kN), static_cast<float>(kN), static_cast<float>(kN)},
                         /*medium_id=*/1u};
        PhotonTracer tracer{scene, bvh, light};
        Rng rng{42u};
        tracer.trace(kN, kDepths, rng);

        std::array<float, kDepths> depth_sum{};
        for (const auto& b : tracer.beams())
            if (b.bounce_depth < kDepths) depth_sum[b.bounce_depth] += b.power.x;

        for (uint32_t k = 0; k + 1 < kDepths; ++k) {
            REQUIRE(depth_sum[k] > 0.0f);
            const float ratio = depth_sum[k + 1] / depth_sum[k];
            INFO("sigma_a=" << sigma_a << " k=" << k << " ratio=" << ratio
                            << " expected=" << expected_albedo);
            REQUIRE(ratio == Catch::Approx(expected_albedo).epsilon(0.05f));
        }
    };

    SECTION("no absorption: albedo = 1")            { run(0.0f);  }
    SECTION("partial absorption: sigma_a=20 → 0.83") { run(20.0f); }
    SECTION("heavy absorption: sigma_a=50 → 0.67")   { run(50.0f); }
}

TEST_CASE("PhotonTracer [sanity]: combined per-depth power sum conserved in lossless scene",
          "[photon_tracer][power][sanity]") {
    // All-white diffuse walls (albedo=1) + purely-scattering medium (sigma_a=0) in a
    // closed box. No energy loss anywhere. RR preserves expected power.
    // sum(beams at depth k) + sum(points at depth k) ≈ light.power for all k.
    constexpr uint32_t kN      = 100'000;
    constexpr uint32_t kDepths = 4;

    RayModel model{make_box({-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}),
                   std::vector<uint32_t>(12u, 0u), 1u};
    tinybvh::BVH bvh;
    bvh.Build(model.triangles().data(), model.triangle_count());
    Scene scene{model};
    scene.set_bsdf(1u, Bsdf{BsdfKind::Diffuse, 1.0f, {1.0f, 1.0f, 1.0f}});
    scene.set_instance_bsdf(0u, 1u);
    scene.set_medium(1u, Medium{/*sigma_s=*/5.0f, /*sigma_a=*/0.0f});
    PointLight light{tinybvh::bvhvec3{0.0f, 0.0f, 0.0f},
                     {static_cast<float>(kN), static_cast<float>(kN), static_cast<float>(kN)},
                     /*medium_id=*/1u};
    PhotonTracer tracer{scene, bvh, light};
    Rng rng{42u};
    tracer.trace(kN, kDepths, rng);

    const float kExpected = static_cast<float>(kN);
    for (uint32_t k = 0; k < kDepths; ++k) {
        float sum = 0.0f;
        for (const auto& b : tracer.beams())
            if (b.bounce_depth == k) sum += b.power.x;
        for (const auto& p : tracer.points())
            if (p.bounce_depth == k) sum += p.power.x;
        INFO("k=" << k << " sum=" << sum << " expected=" << kExpected);
        REQUIRE(sum == Catch::Approx(kExpected).epsilon(0.05f));
    }
}

TEST_CASE("PhotonTracer: stored point normal is unit-length and oriented toward photon origin",
          "[photon_tracer][splat]") {
    RayModel model{make_box({-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}),
                   std::vector<uint32_t>(12u, 0u), 1u};
    tinybvh::BVH bvh;
    bvh.Build(model.triangles().data(), model.triangle_count());
    Scene scene{model};
    constexpr uint32_t kN = 5000;
    PointLight light{tinybvh::bvhvec3{0.0f, 0.0f, 0.0f},
                     {static_cast<float>(kN), static_cast<float>(kN), static_cast<float>(kN)}};
    PhotonTracer tracer{scene, bvh, light};
    Rng rng{7u};
    tracer.trace(kN, /*max_depth=*/1, rng);

    REQUIRE(!tracer.points().empty());
    for (const auto& p : tracer.points()) {
        const float nlen = std::sqrt(p.normal.x*p.normal.x + p.normal.y*p.normal.y + p.normal.z*p.normal.z);
        REQUIRE(nlen == Catch::Approx(1.0f).epsilon(1e-5f));

        const float d = p.incoming_dir.x*p.normal.x + p.incoming_dir.y*p.normal.y + p.incoming_dir.z*p.normal.z;
        REQUIRE(d < 0.0f);
    }
}

TEST_CASE("PhotonTracer: stored point incoming_dir is unit-length",
          "[photon_tracer][splat]") {
    RayModel model{make_box({-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}),
                   std::vector<uint32_t>(12u, 0u), 1u};
    tinybvh::BVH bvh;
    bvh.Build(model.triangles().data(), model.triangle_count());
    Scene scene{model};
    constexpr uint32_t kN = 5000;
    PointLight light{tinybvh::bvhvec3{0.0f, 0.0f, 0.0f},
                     {static_cast<float>(kN), static_cast<float>(kN), static_cast<float>(kN)}};
    PhotonTracer tracer{scene, bvh, light};
    Rng rng{8u};
    tracer.trace(kN, /*max_depth=*/3, rng);

    REQUIRE(!tracer.points().empty());
    for (const auto& p : tracer.points()) {
        const float len = std::sqrt(p.incoming_dir.x*p.incoming_dir.x +
                                    p.incoming_dir.y*p.incoming_dir.y +
                                    p.incoming_dir.z*p.incoming_dir.z);
        REQUIRE(len == Catch::Approx(1.0f).epsilon(1e-5f));
    }
}

TEST_CASE("PhotonTracer: beam power equals incident weight at scatter point",
          "[photon_tracer][power][medium]") {
    // Very dense medium (sigma_s=1000) forces an immediate scatter; beam.power = init_weight.
    RayModel model{make_box({-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}),
                   std::vector<uint32_t>(12u, 0u), 1u};
    tinybvh::BVH bvh;
    bvh.Build(model.triangles().data(), model.triangle_count());

    Scene scene{model};
    scene.set_medium(1u, Medium{/*sigma_s=*/1000.0f, /*sigma_a=*/0.0f});
    PointLight light{tinybvh::bvhvec3{0.0f, 0.0f, 0.0f}, {3.0f, 5.0f, 7.0f}, /*medium_id=*/1u};
    PhotonTracer tracer{scene, bvh, light};
    Rng rng{0u};
    tracer.trace(/*photon_count=*/1, /*max_depth=*/1, rng);

    REQUIRE(!tracer.beams().empty());
    const auto& b = tracer.beams()[0];
    REQUIRE(b.power.x == Catch::Approx(3.0f));
    REQUIRE(b.power.y == Catch::Approx(5.0f));
    REQUIRE(b.power.z == Catch::Approx(7.0f));
}
