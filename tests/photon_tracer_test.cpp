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
}  // namespace

TEST_CASE("PhotonTracer: photons land on Suzanne and project into a debug PPM",
          "[photon_tracer]") {
    RayModel model{std::string{kSuzannePath}};
    REQUIRE(model.triangle_count() > 0);

    tinybvh::BVH bvh;
    bvh.Build(model.triangles().data(), model.triangle_count());

    Scene scene{model};
    PointLight light{tinybvh::bvhvec3{0.0f, 0.0f, 3.0f}, /*medium_id=*/0u};
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
    PointLight light{tinybvh::bvhvec3{0.0f, 0.0f, 0.0f}, /*medium_id=*/0u};

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

    PointLight light{tinybvh::bvhvec3{0.0f, 0.0f, 0.0f}, /*medium_id=*/0u};
    PhotonTracer tracer{scene, bvh, light};
    Rng rng{2u};
    tracer.trace(/*photon_count=*/5000, /*max_depth=*/4, rng);

    // All geometry is non-diffuse, so no points are recorded even though photons hit.
    REQUIRE(tracer.points().empty());
}
