#include <catch2/catch_test_macros.hpp>

#include "ray_model.hpp"

#include <string>

namespace {
constexpr const char* kSuzannePath  = TEST_ASSETS_DIR "/meshes/Suzanne.ply";
constexpr const char* kBackpackPath = TEST_ASSETS_DIR "/models/backpack/backpack.obj";
}  // namespace

TEST_CASE("RayModel imports a single-mesh PLY", "[ray_model]") {
    RayModel model{std::string{kSuzannePath}};

    REQUIRE(model.triangle_count() > 0);
    REQUIRE(model.triangles().size() == model.triangle_count() * 3u);
    REQUIRE(model.instance_count() == 1u);

    for (uint32_t i = 0; i < model.triangle_count(); ++i) {
        REQUIRE(model.instance_id(i) == 0u);
    }
}

TEST_CASE("RayModel imports a multi-mesh OBJ", "[ray_model]") {
    RayModel model{std::string{kBackpackPath}};

    REQUIRE(model.triangle_count() > 0);
    REQUIRE(model.triangles().size() == model.triangle_count() * 3u);
    REQUIRE(model.instance_count() >= 1u);

    uint32_t max_instance = 0;
    for (uint32_t i = 0; i < model.triangle_count(); ++i) {
        max_instance = std::max(max_instance, model.instance_id(i));
    }
    REQUIRE(max_instance < model.instance_count());
}

TEST_CASE("RayModel triangles feed tinybvh without crashing", "[ray_model][tinybvh]") {
    RayModel model{std::string{kSuzannePath}};
    REQUIRE(model.triangle_count() > 0);

    tinybvh::BVH bvh;
    bvh.Build(model.triangles().data(), model.triangle_count());

    tinybvh::Ray ray{tinybvh::bvhvec3{0.0f, 0.0f, 10.0f},
                     tinybvh::bvhvec3{0.0f, 0.0f, -1.0f}};
    bvh.Intersect(ray);
    REQUIRE(ray.hit.t < BVH_FAR);
    REQUIRE(ray.hit.prim > 0);
    REQUIRE(ray.hit.prim < model.triangle_count());
}
