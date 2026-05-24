#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "bsdf.hpp"
#include "point_light.hpp"
#include "ray_model.hpp"
#include "scene.hpp"
#include "scene_config.hpp"
#include "tiny_bvh.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

// Minimal one-triangle model for apply() to work against.
Scene make_empty_scene() {
    std::vector<tinybvh::bvhvec4> tris = {
        {0.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f}};
    std::vector<uint32_t> inst = {0u};
    return Scene{RayModel{std::move(tris), std::move(inst), 1u}};
}

// Write `content` to `path` and return path (for SceneConfig::load with a fake model extension).
std::string write_json(const std::string& stem, const std::string& content) {
    const std::string json_path = std::string(TEST_OUTPUT_DIR) + "/" + stem + ".json";
    std::filesystem::create_directories(TEST_OUTPUT_DIR);
    std::ofstream f(json_path);
    f << content;
    // SceneConfig::load replaces extension with .json; pass the same stem with .ply.
    return std::string(TEST_OUTPUT_DIR) + "/" + stem + ".ply";
}

}  // namespace

TEST_CASE("SceneConfig: light power round-trips through JSON", "[scene_config]") {
    const std::string model_path = write_json("light_power_test", R"({
        "light": {
            "position": [1.0, 2.0, 3.0],
            "power":    [0.5, 0.8, 1.2]
        }
    })");

    auto scene = make_empty_scene();
    const PointLight light = SceneConfig::load(model_path).apply(scene);

    REQUIRE(light.position.x == Catch::Approx(1.0f));
    REQUIRE(light.position.y == Catch::Approx(2.0f));
    REQUIRE(light.position.z == Catch::Approx(3.0f));
    REQUIRE(light.power.x == Catch::Approx(0.5f));
    REQUIRE(light.power.y == Catch::Approx(0.8f));
    REQUIRE(light.power.z == Catch::Approx(1.2f));
}

TEST_CASE("SceneConfig: light power defaults to white when omitted", "[scene_config]") {
    const std::string model_path = write_json("light_power_default_test", R"({
        "light": { "position": [0.0, 0.0, 0.0] }
    })");

    auto scene = make_empty_scene();
    const PointLight light = SceneConfig::load(model_path).apply(scene);

    REQUIRE(light.power.x == Catch::Approx(1.0f));
    REQUIRE(light.power.y == Catch::Approx(1.0f));
    REQUIRE(light.power.z == Catch::Approx(1.0f));
}

TEST_CASE("SceneConfig: bsdf color and transmittance_color round-trip", "[scene_config]") {
    const std::string model_path = write_json("bsdf_color_test", R"({
        "light": { "position": [0.0, 0.0, 0.0] },
        "bsdfs": {
            "mirror":  { "kind": "Conductor",  "color": [0.9, 0.7, 0.5] },
            "glass":   { "kind": "Dielectric",
                         "color": [0.95, 0.95, 0.95],
                         "transmittance_color": [0.8, 0.9, 1.0] }
        }
    })");

    auto scene = make_empty_scene();
    SceneConfig::load(model_path).apply(scene);

    // bsdf IDs are assigned starting from 1, in iteration order (unordered_map).
    // We can't rely on which ID gets which name, so search by kind.
    bool found_conductor  = false;
    bool found_dielectric = false;
    for (uint32_t id = 1; id <= 2; ++id) {
        const Bsdf& b = scene.bsdf(id);
        if (b.kind == BsdfKind::Conductor) {
            REQUIRE(b.color.x == Catch::Approx(0.9f));
            REQUIRE(b.color.y == Catch::Approx(0.7f));
            REQUIRE(b.color.z == Catch::Approx(0.5f));
            found_conductor = true;
        }
        if (b.kind == BsdfKind::Dielectric) {
            REQUIRE(b.color.x == Catch::Approx(0.95f));
            REQUIRE(b.transmittance_color.x == Catch::Approx(0.8f));
            REQUIRE(b.transmittance_color.y == Catch::Approx(0.9f));
            REQUIRE(b.transmittance_color.z == Catch::Approx(1.0f));
            found_dielectric = true;
        }
    }
    REQUIRE(found_conductor);
    REQUIRE(found_dielectric);
}
