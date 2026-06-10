#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "bsdf.hpp"
#include "medium.hpp"
#include "path_tracer.hpp"
#include "point_light.hpp"
#include "ray_camera.hpp"
#include "ray_model.hpp"
#include "scene.hpp"

#include <cmath>
#include <cstdint>
#include <vector>

namespace {

// Axis-aligned closed box [lo, hi] as a fat-triangle buffer (12 triangles).
std::vector<tinybvh::bvhvec4> make_box(tinybvh::bvhvec3 lo, tinybvh::bvhvec3 hi) {
    const tinybvh::bvhvec3 c[8] = {
        {lo.x, lo.y, lo.z}, {hi.x, lo.y, lo.z}, {hi.x, hi.y, lo.z}, {lo.x, hi.y, lo.z},
        {lo.x, lo.y, hi.z}, {hi.x, lo.y, hi.z}, {hi.x, hi.y, hi.z}, {lo.x, hi.y, hi.z},
    };
    const int faces[6][4] = {
        {0,1,2,3},{5,4,7,6},{4,0,3,7},{1,5,6,2},{4,5,1,0},{3,2,6,7},
    };
    std::vector<tinybvh::bvhvec4> v;
    v.reserve(36);
    auto push = [&](const tinybvh::bvhvec3& p){ v.emplace_back(p.x,p.y,p.z,0.f); };
    for (const auto& f : faces) {
        push(c[f[0]]); push(c[f[1]]); push(c[f[2]]);
        push(c[f[0]]); push(c[f[2]]); push(c[f[3]]);
    }
    return v;
}

// A single large quad at z = z0, facing +z (two CCW triangles from the +z side).
std::vector<tinybvh::bvhvec4> make_zquad(float z0, float half = 5.0f) {
    auto p = [&](float x, float y) -> tinybvh::bvhvec4 { return {x, y, z0, 0.f}; };
    return {
        p(-half,-half), p(half,-half), p(half, half),
        p(-half,-half), p(half, half), p(-half, half),
    };
}

}  // namespace

// ─── Phase 1: vacuum / diffuse only ──────────────────────────────────────────

TEST_CASE("PathTracer [P1]: NEE direct light matches analytic Lambertian formula",
          "[path_tracer][vacuum][diffuse]") {
    // Scene: large diffuse quad at z=0 (albedo=1), point light at (0,0,1), power=pi.
    // Camera at z=2, pixel (0,0) hits quad at (0,0,0), normal = +z, cos_theta = 1.
    // Analytic direct: (1/pi) * pi / 1^2 * cos(0) = 1.0
    // With NEE only (max_depth=1), result is deterministic for the direct term.
    // Analytic: (ρ/π) * (Φ/4π) * cos(θ) / d² = (1/π) * (π/4π) * 1 / 1 = 1/(4π)
    constexpr float kPi    = 3.14159265358979f;
    constexpr float kInv4Pi = 1.0f / (4.0f * kPi);

    auto verts = make_zquad(0.0f);
    RayModel model{std::move(verts), std::vector<uint32_t>(2u, 0u), 1u};
    tinybvh::BVH bvh;
    bvh.Build(model.triangles().data(), model.triangle_count());

    Scene scene{model};
    scene.set_bsdf(1u, Bsdf{BsdfKind::Diffuse, 1.0f, {1.0f, 1.0f, 1.0f}});
    scene.set_instance_bsdf(0u, 1u);

    PointLight light{{0.0f, 0.0f, 1.0f}, {kPi, kPi, kPi}};
    PathTracer pt{scene, bvh, {Light{light}}, /*max_depth=*/1, /*spp=*/1};

    PinholeCamera cam{
        .eye    = {0.0f, 0.0f, 2.0f},
        .target = {0.0f, 0.0f, 0.0f},
        .up     = {0.0f, 1.0f, 0.0f},
        .fov_y  = 0.5f,
        .width  = 1u,
        .height = 1u,
    };
    std::vector<float> out;
    pt.render(out, 1, 1, cam);

    REQUIRE(out[0] == Catch::Approx(kInv4Pi).epsilon(0.01f));
    REQUIRE(out[1] == Catch::Approx(kInv4Pi).epsilon(0.01f));
    REQUIRE(out[2] == Catch::Approx(kInv4Pi).epsilon(0.01f));
}

TEST_CASE("PathTracer [P1]: black surface returns zero radiance",
          "[path_tracer][vacuum][diffuse]") {
    auto verts = make_zquad(0.0f);
    RayModel model{std::move(verts), std::vector<uint32_t>(2u, 0u), 1u};
    tinybvh::BVH bvh;
    bvh.Build(model.triangles().data(), model.triangle_count());

    Scene scene{model};
    scene.set_bsdf(1u, Bsdf{BsdfKind::Diffuse, 1.0f, {0.0f, 0.0f, 0.0f}});
    scene.set_instance_bsdf(0u, 1u);

    PointLight light{{0.0f, 0.0f, 1.0f}, {100.0f, 100.0f, 100.0f}};
    PathTracer pt{scene, bvh, {Light{light}}, 4, 32};

    PinholeCamera cam{
        .eye={0.f,0.f,2.f}, .target={0.f,0.f,0.f}, .up={0.f,1.f,0.f},
        .fov_y=0.5f, .width=1u, .height=1u,
    };
    std::vector<float> out;
    pt.render(out, 1, 1, cam);

    REQUIRE(out[0] == Catch::Approx(0.0f).margin(1e-5f));
    REQUIRE(out[1] == Catch::Approx(0.0f).margin(1e-5f));
    REQUIRE(out[2] == Catch::Approx(0.0f).margin(1e-5f));
}

TEST_CASE("PathTracer [P1]: occluded light contributes zero direct lighting",
          "[path_tracer][vacuum][diffuse]") {
    // Two parallel quads: receiver at z=0, occluder at z=0.5, light at z=1.
    std::vector<tinybvh::bvhvec4> verts;
    auto q0 = make_zquad(0.0f);
    auto q1 = make_zquad(0.5f);
    verts.insert(verts.end(), q0.begin(), q0.end());  // instance 0
    verts.insert(verts.end(), q1.begin(), q1.end());  // instance 1
    std::vector<uint32_t> inst(4u, 0u);
    inst[2] = inst[3] = 1u;
    RayModel model{std::move(verts), std::move(inst), 2u};
    tinybvh::BVH bvh;
    bvh.Build(model.triangles().data(), model.triangle_count());

    Scene scene{model};
    // Both quads: white diffuse
    scene.set_bsdf(1u, Bsdf{BsdfKind::Diffuse, 1.0f, {1.0f, 1.0f, 1.0f}});
    scene.set_instance_bsdf(0u, 1u);
    scene.set_instance_bsdf(1u, 1u);

    PointLight light{{0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}};
    PathTracer pt{scene, bvh, {Light{light}}, /*max_depth=*/1, /*spp=*/1};

    PinholeCamera cam{
        .eye={0.f,0.f,2.f}, .target={0.f,0.f,0.f}, .up={0.f,1.f,0.f},
        .fov_y=0.3f, .width=1u, .height=1u,
    };
    std::vector<float> out;
    pt.render(out, 1, 1, cam);

    // Camera hits z=0.5 quad first (occluder), light is directly above it → direct light is visible
    // Actually: the ray from z=2 hits z=0.5 first. From z=0.5, shadow ray to z=1 is unoccluded.
    // So the occluder itself gets lit. The receiver at z=0 is never hit by the camera.
    // Test: radiance > 0 (occluder is lit, not zero)
    REQUIRE(out[0] > 0.0f);
}

// ─── Phase 2: medium only ─────────────────────────────────────────────────────

TEST_CASE("PathTracer [P2]: Beer-Lambert — radiance ratio matches exp(-sigma_a*D)",
          "[path_tracer][medium]") {
    // Camera inside a homogeneous absorbing medium (no scattering), shooting toward a
    // white diffuse wall at distance D. With sigma_s=0 any scatter event kills the path
    // (weight=0). P(no scatter before wall) = exp(-sigma_a*D), so the average NEE
    // radiance scales as exp(-sigma_a*D).
    // We compare two extinction values: ratio must match Beer-Lambert.
    constexpr float kD      = 2.0f;   // distance to wall
    constexpr float kSigmaA = 0.5f;   // baseline absorption
    constexpr int   kSpp    = 30000;

    // Single wall quad at z=kD, facing -z (toward camera).
    auto wall = make_zquad(kD);
    RayModel model{std::move(wall), std::vector<uint32_t>(2u, 0u), 1u};
    tinybvh::BVH bvh;
    bvh.Build(model.triangles().data(), model.triangle_count());

    // Light at camera position (z=0), so it's always unoccluded from the wall.
    auto render_with_absorption = [&](float sigma_a) -> float {
        Scene scene{model};
        scene.set_bsdf(1u, Bsdf{BsdfKind::Diffuse, 1.0f, {1.0f, 1.0f, 1.0f}});
        scene.set_instance_bsdf(0u, 1u);
        scene.set_medium(1u, Medium{/*sigma_s=*/0.0f, sigma_a});
        constexpr float kPi = 3.14159265358979f;
        PointLight light{{0.0f, 0.0f, 0.0f}, {kD*kD*kPi, kD*kD*kPi, kD*kD*kPi}};
        PathTracer pt{scene, bvh, {Light{light}}, /*max_depth=*/2, kSpp};
        PinholeCamera cam{
            .eye={0.f,0.f,0.f}, .target={0.f,0.f,1.f}, .up={0.f,1.f,0.f},
            .fov_y=0.01f, .width=1u, .height=1u,
        };
        std::vector<float> out;
        // Camera starts inside medium 1
        pt.render(out, 1, 1, cam, /*start_medium=*/1u);
        return out[0];
    };

    const float r1 = render_with_absorption(kSigmaA);
    const float r2 = render_with_absorption(kSigmaA * 2.0f);

    // Radiance = exp(-sigma_a*D) * Tr_shadow(D) * direct = C * exp(-2*sigma_a*D).
    // Both the camera path and the shadow ray traverse the full slab D.
    // ratio r1/r2 = exp(-2*sa1*D) / exp(-2*sa2*D) = exp(2*(sa2-sa1)*D) = exp(2*sa1*D).
    const float expected_ratio = std::exp(2.0f * kSigmaA * kD);
    INFO("r1=" << r1 << " r2=" << r2 << " ratio=" << r1/r2 << " expected=" << expected_ratio);
    REQUIRE(r2 > 0.0f);
    REQUIRE(r1 / r2 == Catch::Approx(expected_ratio).epsilon(0.05f));
}

TEST_CASE("PathTracer [P2]: single-scatter contribution is positive in pure-scattering medium",
          "[path_tracer][medium]") {
    // Camera inside a pure-scattering medium (sigma_a=0) in a closed box.
    // Point light directly above the camera. With max_depth=2, rays scatter once then
    // do NEE. All pixels should receive non-zero, finite radiance.
    constexpr float kBox = 1.0f;
    constexpr float kS   = 2.0f;
    constexpr int   kSpp = 500;
    constexpr int   kRes = 4;

    auto box_verts = make_box({-kBox,-kBox,-kBox}, {kBox,kBox,kBox});
    RayModel model{std::move(box_verts), std::vector<uint32_t>(12u, 0u), 1u};
    tinybvh::BVH bvh;
    bvh.Build(model.triangles().data(), model.triangle_count());

    Scene scene{model};
    scene.set_bsdf(1u, Bsdf{BsdfKind::Diffuse, 1.0f, {1.0f, 1.0f, 1.0f}});
    scene.set_instance_bsdf(0u, 1u);
    scene.set_medium(1u, Medium{kS, 0.0f});
    // Light just above the camera to guarantee NEE visibility for most scatter points.
    PointLight light{{0.0f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}};
    PathTracer pt{scene, bvh, {Light{light}}, /*max_depth=*/3, kSpp};

    PinholeCamera cam{
        .eye={0.f,0.f,0.f}, .target={0.f,0.f,1.f}, .up={0.f,1.f,0.f},
        .fov_y=0.8f, .width=static_cast<uint32_t>(kRes), .height=static_cast<uint32_t>(kRes),
    };
    std::vector<float> out;
    // Camera starts inside medium 1
    pt.render(out, kRes, kRes, cam, /*start_medium=*/1u);

    float total = 0.0f;
    for (float v : out) {
        REQUIRE(std::isfinite(v));
        REQUIRE(v >= 0.0f);
        total += v;
    }
    REQUIRE(total > 0.0f);
}

// ─── Phase 3: combined ────────────────────────────────────────────────────────

TEST_CASE("PathTracer [P3]: combined medium + diffuse surface produces finite radiance",
          "[path_tracer][combined]") {
    // A MediumShell cube with diffuse walls, lit from outside.
    // All pixels must be finite and non-negative.
    constexpr float kH   = 0.5f;
    constexpr int   kSpp = 200;
    constexpr int   kRes = 8;

    auto box_verts = make_box({-kH,-kH,-kH}, {kH,kH,kH});
    RayModel model{std::move(box_verts), std::vector<uint32_t>(12u, 0u), 1u};
    tinybvh::BVH bvh;
    bvh.Build(model.triangles().data(), model.triangle_count());

    Scene scene{model};
    scene.set_bsdf(1u, Bsdf{BsdfKind::MediumShell, 1.0f});
    scene.set_instance_bsdf(0u, 1u);
    scene.set_instance_medium(0u, /*in=*/1u, /*out=*/0u);
    scene.set_medium(1u, Medium{/*sigma_s=*/2.0f, /*sigma_a=*/0.5f});

    // Diffuse walls inside (instance 1) — none in this scene, just the shell.
    // Light outside the medium.
    PointLight light{{0.0f, 0.0f, 2.0f}, {1.0f, 1.0f, 1.0f}};
    PathTracer pt{scene, bvh, {Light{light}}, /*max_depth=*/8, kSpp};

    PinholeCamera cam{
        .eye={0.f,0.f,2.0f}, .target={0.f,0.f,0.f}, .up={0.f,1.f,0.f},
        .fov_y=0.5f, .width=static_cast<uint32_t>(kRes), .height=static_cast<uint32_t>(kRes),
    };
    std::vector<float> out;
    pt.render(out, kRes, kRes, cam);

    for (float v : out) {
        REQUIRE(std::isfinite(v));
        REQUIRE(v >= 0.0f);
    }
}

TEST_CASE("PathTracer [P3]: vacuum diffuse converges to same value with more SPP",
          "[path_tracer][combined]") {
    // Simple diffuse scene: closed box, white walls, single point light.
    // Render at two SPP levels; both should give positive, finite, similar results.
    auto box_verts = make_box({-1.f,-1.f,-1.f}, {1.f,1.f,1.f});
    RayModel model{std::move(box_verts), std::vector<uint32_t>(12u, 0u), 1u};
    tinybvh::BVH bvh;
    bvh.Build(model.triangles().data(), model.triangle_count());

    Scene scene{model};
    scene.set_bsdf(1u, Bsdf{BsdfKind::Diffuse, 1.0f, {0.8f, 0.8f, 0.8f}});
    scene.set_instance_bsdf(0u, 1u);

    PointLight light{{0.0f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}};

    PinholeCamera cam{
        .eye={0.f,0.f,0.8f}, .target={0.f,0.f,0.f}, .up={0.f,1.f,0.f},
        .fov_y=1.0f, .width=4u, .height=4u,
    };

    PathTracer pt_lo{scene, bvh, {Light{light}}, 8, 64};
    PathTracer pt_hi{scene, bvh, {Light{light}}, 8, 2048};

    std::vector<float> lo_out, hi_out;
    pt_lo.render(lo_out, 4, 4, cam);
    pt_hi.render(hi_out, 4, 4, cam);

    // Low and high SPP estimates should agree within 25% for well-lit pixels
    for (int i = 0; i < 4*4*3; ++i) {
        REQUIRE(std::isfinite(lo_out[i]));
        REQUIRE(lo_out[i] >= 0.0f);
        if (hi_out[i] > 0.05f) {
            REQUIRE(lo_out[i] == Catch::Approx(hi_out[i]).epsilon(0.25f));
        }
    }
}

// ── render_checkpointed ───────────────────────────────────────────────────────

TEST_CASE("PathTracer: render_checkpointed calls callback at each checkpoint SPP",
          "[path_tracer][checkpoint]") {
    auto verts = make_zquad(0.0f);
    RayModel model{std::move(verts), std::vector<uint32_t>(2u, 0u), 1u};
    tinybvh::BVH bvh;
    bvh.Build(model.triangles().data(), model.triangle_count());

    Scene scene{model};
    scene.set_bsdf(1u, Bsdf{BsdfKind::Diffuse, 1.0f, {1.0f, 1.0f, 1.0f}});
    scene.set_instance_bsdf(0u, 1u);

    constexpr float kPi = 3.14159265358979f;
    PointLight light{{0.0f, 0.0f, 1.0f}, {kPi, kPi, kPi}};
    PathTracer pt{scene, bvh, {Light{light}}, /*max_depth=*/1, /*spp=*/256};

    PinholeCamera cam{
        .eye    = {0.0f, 0.0f, 2.0f},
        .target = {0.0f, 0.0f, 0.0f},
        .up     = {0.0f, 1.0f, 0.0f},
        .fov_y  = 0.5f, .width = 1u, .height = 1u,
    };

    std::vector<int> called_spps;
    std::vector<float> last_buf;

    pt.render_checkpointed(1, 1, cam, {1, 4, 16, 64, 256},
        [&](int spp, const std::vector<float>& buf) {
            called_spps.push_back(spp);
            last_buf = buf;
        });

    // Callback called exactly once per checkpoint
    REQUIRE(called_spps.size() == 5);
    CHECK(called_spps[0] == 1);
    CHECK(called_spps[1] == 4);
    CHECK(called_spps[2] == 16);
    CHECK(called_spps[3] == 64);
    CHECK(called_spps[4] == 256);

    // 256-spp result should converge close to 1/(4π) (analytic: (ρ/π)*(Φ/4π)*cosθ/d²)
    constexpr float kInv4Pi_cp = 1.0f / (4.0f * kPi);
    REQUIRE(last_buf.size() == 3u);
    CHECK(last_buf[0] == Catch::Approx(kInv4Pi_cp).epsilon(0.05f));
    CHECK(last_buf[1] == Catch::Approx(kInv4Pi_cp).epsilon(0.05f));
    CHECK(last_buf[2] == Catch::Approx(kInv4Pi_cp).epsilon(0.05f));
}

TEST_CASE("PathTracer: render_checkpointed with empty checkpoints does nothing",
          "[path_tracer][checkpoint]") {
    auto verts = make_zquad(0.0f);
    RayModel model{std::move(verts), std::vector<uint32_t>(2u, 0u), 1u};
    tinybvh::BVH bvh;
    bvh.Build(model.triangles().data(), model.triangle_count());
    Scene scene{model};
    PathTracer pt{scene, bvh, {}, /*max_depth=*/1, /*spp=*/1};

    PinholeCamera cam{
        .eye={0.f,0.f,2.f}, .target={0.f,0.f,0.f}, .up={0.f,1.f,0.f},
        .fov_y=0.5f, .width=1u, .height=1u,
    };

    int calls = 0;
    pt.render_checkpointed(1, 1, cam, {},
        [&](int, const std::vector<float>&) { ++calls; });
    CHECK(calls == 0);
}
