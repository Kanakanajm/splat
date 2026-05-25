#include <catch2/catch_test_macros.hpp>

#include "photon_tracer.hpp"
#include "point_light.hpp"
#include "random.hpp"
#include "ray_model.hpp"
#include "scene.hpp"

#include <cstdint>
#include <string>

namespace {
constexpr const char* kSuzannePath  = TEST_ASSETS_DIR "/meshes/Suzanne.ply";
constexpr const char* kBackpackPath = TEST_ASSETS_DIR "/models/backpack/backpack.obj";
}

TEST_CASE("Scene: every prim defaults to bsdf_id=0 and medium ids=0", "[scene]") {
    RayModel model{std::string{kSuzannePath}};
    Scene    scene{model};

    REQUIRE(model.triangle_count() > 0);
    for (uint32_t prim = 0; prim < model.triangle_count(); ++prim) {
        REQUIRE(scene.bsdf_id_at(prim)     == 0u);
        REQUIRE(scene.medium_in_at(prim)   == 0u);
        REQUIRE(scene.medium_out_at(prim)  == 0u);
    }
}

TEST_CASE("Scene: per-instance bsdf/medium assignments are looked up by prim",
          "[scene]") {
    // Synthetic 2-instance model: 2 triangles for instance 0, 1 triangle for instance 1.
    std::vector<tinybvh::bvhvec4> tris{
        // instance 0, triangle 0
        {0,0,0,0}, {1,0,0,0}, {0,1,0,0},
        // instance 0, triangle 1
        {1,0,0,0}, {1,1,0,0}, {0,1,0,0},
        // instance 1, triangle 0
        {2,2,0,0}, {3,2,0,0}, {2,3,0,0},
    };
    std::vector<uint32_t> tri_instance{0u, 0u, 1u};
    RayModel model{std::move(tris), std::move(tri_instance), /*instance_count=*/2u};
    Scene    scene{model};

    scene.set_instance_bsdf(0u, 7u);
    scene.set_instance_bsdf(1u, 13u);
    scene.set_instance_medium(0u, /*in=*/2u, /*out=*/3u);
    scene.set_instance_medium(1u, /*in=*/5u, /*out=*/6u);

    REQUIRE(scene.bsdf_id_at(0u)    == 7u);
    REQUIRE(scene.bsdf_id_at(1u)    == 7u);
    REQUIRE(scene.bsdf_id_at(2u)    == 13u);
    REQUIRE(scene.medium_in_at(0u)  == 2u);
    REQUIRE(scene.medium_out_at(0u) == 3u);
    REQUIRE(scene.medium_in_at(2u)  == 5u);
    REQUIRE(scene.medium_out_at(2u) == 6u);
}

TEST_CASE("PhotonTracer: stored PhotonPoint.bsdf_id comes from Scene assignment",
          "[scene][photon_tracer]") {
    RayModel model{std::string{kSuzannePath}};
    tinybvh::BVH bvh;
    bvh.Build(model.triangles().data(), model.triangle_count());

    Scene scene{model};
    constexpr uint32_t kAssignedBsdf = 42u;
    scene.set_instance_bsdf(/*instance=*/0u, kAssignedBsdf);
    scene.set_bsdf(kAssignedBsdf, Bsdf{BsdfKind::Diffuse, 1.0f});

    PointLight light{tinybvh::bvhvec3{0.0f, 0.0f, 3.0f}, /*medium_id=*/0u};
    PhotonTracer tracer{scene, bvh, light};
    Rng rng{0xC0FFEEu};

    tracer.trace(10000, /*max_depth=*/2, rng);
    REQUIRE(!tracer.points().empty());
    for (const auto& p : tracer.points()) {
        REQUIRE(p.bsdf_id == kAssignedBsdf);
    }
}
