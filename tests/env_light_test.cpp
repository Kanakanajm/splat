#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "env_light.hpp"
#include "random.hpp"
#include "ray_model.hpp"
#include "scene.hpp"
#include "scene_config.hpp"
#include "tiny_bvh.h"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {
constexpr const char* kSuzannePath = TEST_ASSETS_DIR "/meshes/Suzanne.ply";
constexpr float kPi = 3.14159265358979323846f;
}

TEST_CASE("EnvLight: emitted ray origins are outside the bounding sphere", "[env_light]") {
    const tinybvh::bvhvec3 center{1.0f, 2.0f, 3.0f};
    const float radius = 5.0f;
    EnvLight light{{1.0f, 1.0f, 1.0f}, center, radius};
    Rng rng{42u};

    for (int i = 0; i < 2000; ++i) {
        const tinybvh::Ray ray = light.emit_ray(rng);
        const float dx = ray.O.x - center.x;
        const float dy = ray.O.y - center.y;
        const float dz = ray.O.z - center.z;
        const float dist2 = dx*dx + dy*dy + dz*dz;
        REQUIRE(dist2 >= radius * radius - 1e-4f);
    }
}

TEST_CASE("EnvLight: emitted directions are unit length", "[env_light]") {
    EnvLight light{{1.0f, 1.0f, 1.0f}, {}, 2.0f};
    Rng rng{99u};

    for (int i = 0; i < 2000; ++i) {
        const tinybvh::Ray ray = light.emit_ray(rng);
        const float len2 = ray.D.x*ray.D.x + ray.D.y*ray.D.y + ray.D.z*ray.D.z;
        REQUIRE(len2 == Catch::Approx(1.0f).margin(1e-5f));
    }
}

TEST_CASE("EnvLight: total_power equals color * 4*pi^2 * R^2", "[env_light]") {
    const tinybvh::bvhvec3 color{2.0f, 3.0f, 4.0f};
    const float R = 5.0f;
    EnvLight light{color, {}, R};

    const tinybvh::bvhvec3 p = light.total_power();
    const float scale = 4.0f * kPi * kPi * R * R;

    REQUIRE(p.x == Catch::Approx(color.x * scale).epsilon(1e-5f));
    REQUIRE(p.y == Catch::Approx(color.y * scale).epsilon(1e-5f));
    REQUIRE(p.z == Catch::Approx(color.z * scale).epsilon(1e-5f));
}

TEST_CASE("EnvLight + BVH: photons enter scene and hit Suzanne", "[env_light][bvh]") {
    RayModel model{std::string{kSuzannePath}};
    REQUIRE(model.triangle_count() > 0);

    tinybvh::BVH bvh;
    bvh.Build(model.triangles().data(), model.triangle_count());

    // Suzanne is centered near origin with extent ~1; use R=3.
    EnvLight light{{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, 3.0f};
    Rng rng{2025u};

    constexpr int kRays = 10000;
    int hits = 0;
    for (int i = 0; i < kRays; ++i) {
        tinybvh::Ray ray = light.emit_ray(rng);
        bvh.Intersect(ray);
        if (ray.hit.t < BVH_FAR) ++hits;
    }

    INFO("hits/" << kRays << " = " << hits);
    REQUIRE(hits > kRays / 100);
}

TEST_CASE("SceneConfig: env_light parsed and bounding sphere matches model", "[env_light][scene_config]") {
    // Write a minimal env_light JSON alongside a fake .ply path.
    const std::string json_path = std::string(TEST_OUTPUT_DIR) + "/env_light_cfg_test.json";
    std::filesystem::create_directories(TEST_OUTPUT_DIR);
    {
        std::ofstream f(json_path);
        f << R"({"env_light": {"color": [0.5, 1.0, 2.0]}})";
    }
    const std::string model_path = std::string(TEST_OUTPUT_DIR) + "/env_light_cfg_test.ply";

    // Triangle at known positions so we can predict the bounding sphere.
    // Store RayModel separately — Scene holds a reference to it.
    std::vector<tinybvh::bvhvec4> tris = {
        {-2.0f, 0.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 2.0f, 0.0f, 0.0f}};
    std::vector<uint32_t> inst = {0u};
    RayModel model{std::move(tris), std::move(inst), 1u};
    Scene scene{model};

    const EnvLight el = std::get<EnvLight>(SceneConfig::load(model_path).apply(scene)[0]);

    REQUIRE(el.color.x == Catch::Approx(0.5f));
    REQUIRE(el.color.y == Catch::Approx(1.0f));
    REQUIRE(el.color.z == Catch::Approx(2.0f));
    // AABB: x in [-2,2], y in [0,2], z in [0,0] → center = (0,1,0), half-extents = (2,1,0)
    // radius_before_margin = sqrt(4+1+0) = sqrt(5) ≈ 2.236; * 1.1 ≈ 2.46
    REQUIRE(el.scene_center.x == Catch::Approx(0.0f).margin(1e-5f));
    REQUIRE(el.scene_center.y == Catch::Approx(1.0f));
    REQUIRE(el.scene_center.z == Catch::Approx(0.0f).margin(1e-5f));
    REQUIRE(el.scene_radius == Catch::Approx(std::sqrt(5.0f) * 1.1f).epsilon(1e-4f));
}
