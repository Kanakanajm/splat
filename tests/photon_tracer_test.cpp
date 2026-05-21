#include <catch2/catch_test_macros.hpp>

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

namespace {
constexpr const char* kSuzannePath = TEST_ASSETS_DIR "/meshes/Suzanne.ply";
}

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
    tracer.trace(kN, rng);

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
