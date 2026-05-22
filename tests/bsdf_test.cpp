#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "bsdf.hpp"
#include "random.hpp"
#include "ray_model.hpp"
#include "sampling.hpp"
#include "scene.hpp"
#include "tiny_bvh.h"

#include <cmath>

using tinybvh::bvhvec3;

namespace {

float dot(const bvhvec3& a, const bvhvec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

bvhvec3 normalize(const bvhvec3& v) {
    const float inv = 1.0f / std::sqrt(dot(v, v));
    return bvhvec3{v.x * inv, v.y * inv, v.z * inv};
}

}  // namespace

TEST_CASE("sample_cosine_hemisphere: unit length and in upper hemisphere", "[bsdf][sampling]") {
    Rng rng{99};
    const bvhvec3 normals[] = {
        {0.0f, 0.0f, 1.0f},
        {0.0f, 0.0f, -1.0f},
        normalize({1.0f, 2.0f, 3.0f}),
        normalize({-3.0f, 0.5f, -1.0f}),
    };
    for (const auto& n : normals) {
        for (int i = 0; i < 5000; ++i) {
            const bvhvec3 d = sample_cosine_hemisphere(rng, n);
            REQUIRE(dot(d, d) == Catch::Approx(1.0f).margin(1e-4f));
            REQUIRE(dot(d, n) >= -1e-5f);
        }
    }
}

TEST_CASE("sample_cosine_hemisphere: mean cos theta is 2/3", "[bsdf][sampling]") {
    constexpr int N = 200000;
    Rng rng{2024};
    const bvhvec3 n = normalize({1.0f, 2.0f, 3.0f});

    double sum_cos = 0.0;
    for (int i = 0; i < N; ++i) {
        sum_cos += dot(sample_cosine_hemisphere(rng, n), n);
    }
    const double mean_cos = sum_cos / N;
    // Cosine-weighted hemisphere: E[cos theta] = 2/3.
    INFO("mean cos theta = " << mean_cos);
    REQUIRE(std::fabs(mean_cos - 2.0 / 3.0) < 0.005);
}

TEST_CASE("sample_cosine_hemisphere: mean direction is (2/3) * normal", "[bsdf][sampling]") {
    constexpr int N = 200000;
    Rng rng{7};
    const bvhvec3 n = normalize({0.0f, 1.0f, 0.0f});

    double sx = 0.0, sy = 0.0, sz = 0.0;
    for (int i = 0; i < N; ++i) {
        const bvhvec3 d = sample_cosine_hemisphere(rng, n);
        sx += d.x;
        sy += d.y;
        sz += d.z;
    }
    // Tangential components average to 0; component along n averages to 2/3.
    REQUIRE(std::fabs(sx / N) < 0.005);
    REQUIRE(std::fabs(sy / N - 2.0 / 3.0) < 0.005);
    REQUIRE(std::fabs(sz / N) < 0.005);
}

TEST_CASE("Bsdf: diffuse scatters to the incident side", "[bsdf]") {
    Rng rng{55};
    const Bsdf diffuse{};  // default kind = Diffuse
    REQUIRE(diffuse.kind == BsdfKind::Diffuse);

    const bvhvec3 normal{0.0f, 0.0f, 1.0f};
    // Photon arrives from above, traveling downward into the surface.
    const bvhvec3 incoming = normalize({0.3f, 0.0f, -1.0f});

    for (int i = 0; i < 5000; ++i) {
        const bvhvec3 out = diffuse.sample(rng, incoming, normal);
        REQUIRE(dot(out, out) == Catch::Approx(1.0f).margin(1e-4f));
        // Reflects back to the side the photon came from (same side as the geometric normal).
        REQUIRE(dot(out, normal) >= -1e-5f);
    }
}

TEST_CASE("Bsdf: diffuse handles a back-facing geometric normal", "[bsdf]") {
    Rng rng{56};
    const Bsdf diffuse{};
    // Geometric normal points the same way the photon travels, so it must be flipped.
    const bvhvec3 normal{0.0f, 0.0f, -1.0f};
    const bvhvec3 incoming{0.0f, 0.0f, -1.0f};

    for (int i = 0; i < 5000; ++i) {
        const bvhvec3 out = diffuse.sample(rng, incoming, normal);
        // Scatters back toward where the photon came from (opposite to travel direction).
        REQUIRE(dot(out, incoming) <= 1e-5f);
    }
}

TEST_CASE("Scene: bsdf table defaults id 0 to diffuse and stores entries", "[bsdf][scene]") {
    // Minimal single-triangle synthetic model: one instance.
    std::vector<tinybvh::bvhvec4> tris = {
        {0.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f}};
    std::vector<uint32_t> inst = {0u};
    RayModel model{std::move(tris), std::move(inst), 1u};
    Scene scene{model};

    REQUIRE(scene.bsdf(0).kind == BsdfKind::Diffuse);

    scene.set_bsdf(3, Bsdf{BsdfKind::Dielectric, 1.5f});
    REQUIRE(scene.bsdf(3).kind == BsdfKind::Dielectric);
    REQUIRE(scene.bsdf(3).ior == Catch::Approx(1.5f));
    // Untouched ids between fall back to default diffuse.
    REQUIRE(scene.bsdf(1).kind == BsdfKind::Diffuse);
}
