#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "point_light.hpp"
#include "random.hpp"
#include "ray_model.hpp"

#include <cmath>
#include <cstdint>
#include <string>

namespace {
constexpr const char* kSuzannePath = TEST_ASSETS_DIR "/meshes/Suzanne.ply";
}

TEST_CASE("PointLight: emitted rays originate at light position", "[point_light]") {
    PointLight light{tinybvh::bvhvec3{1.0f, 2.0f, 3.0f}};
    Rng rng{99};
    for (int i = 0; i < 1000; ++i) {
        const tinybvh::Ray ray = light.emit_ray(rng);
        REQUIRE(ray.O.x == light.position.x);
        REQUIRE(ray.O.y == light.position.y);
        REQUIRE(ray.O.z == light.position.z);
    }
}

TEST_CASE("PointLight: emitted directions are unit length", "[point_light]") {
    PointLight light{tinybvh::bvhvec3{0.0f, 0.0f, 0.0f}};
    Rng rng{2024};
    for (int i = 0; i < 1000; ++i) {
        const tinybvh::Ray ray = light.emit_ray(rng);
        const float len2 = ray.D.x * ray.D.x + ray.D.y * ray.D.y + ray.D.z * ray.D.z;
        REQUIRE(len2 == Catch::Approx(1.0f).margin(1e-5f));
    }
}

TEST_CASE("PointLight + BVH: photons cast from outside Suzanne hit the mesh",
          "[point_light][bvh]") {
    RayModel model{std::string{kSuzannePath}};
    REQUIRE(model.triangle_count() > 0);

    tinybvh::BVH bvh;
    bvh.Build(model.triangles().data(), model.triangle_count());

    // Suzanne is roughly centered at the origin with extent ~1; place the light
    // close enough that a meaningful solid angle is covered.
    PointLight light{tinybvh::bvhvec3{0.0f, 0.0f, 3.0f}};
    Rng rng{2025};

    constexpr int kRays = 10000;
    int hits = 0;
    for (int i = 0; i < kRays; ++i) {
        tinybvh::Ray ray = light.emit_ray(rng);
        bvh.Intersect(ray);
        if (ray.hit.t < BVH_FAR) ++hits;
    }

    // Suzanne occupies a meaningful fraction of the sphere from (0,0,3).
    // Expect at minimum a few percent of rays to hit.
    INFO("hits/" << kRays << " = " << hits);
    REQUIRE(hits > kRays / 100);   // > 1%
    REQUIRE(hits < kRays / 2);     // sanity: not everything hits
}
