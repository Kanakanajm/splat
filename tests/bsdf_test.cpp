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
        const auto out = diffuse.sample(rng, incoming, normal).dir;
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
        const auto out = diffuse.sample(rng, incoming, normal).dir;
        // Scatters back toward where the photon came from (opposite to travel direction).
        REQUIRE(dot(out, incoming) <= 1e-5f);
    }
}

TEST_CASE("Bsdf: conductor reflects to perfect mirror direction", "[bsdf]") {
    Rng rng{0};
    const bvhvec3 reflectance{0.9f, 0.6f, 0.3f};
    const Bsdf conductor{BsdfKind::Conductor, 1.0f, reflectance};
    const bvhvec3 normal{0.0f, 0.0f, 1.0f};

    // 45-degree incidence: incoming = normalize(1, 0, -1), expected mirror = normalize(1, 0, 1).
    const float inv_sqrt2 = 1.0f / std::sqrt(2.0f);
    const bvhvec3 incoming{inv_sqrt2, 0.0f, -inv_sqrt2};
    const auto bs = conductor.sample(rng, incoming, normal);

    REQUIRE(bs.dir.x == Catch::Approx(inv_sqrt2).margin(1e-5f));
    REQUIRE(bs.dir.y == Catch::Approx(0.0f).margin(1e-5f));
    REQUIRE(bs.dir.z == Catch::Approx(inv_sqrt2).margin(1e-5f));
    REQUIRE(dot(bs.dir, bs.dir) == Catch::Approx(1.0f).margin(1e-5f));
    REQUIRE(bs.is_refract == false);
}

TEST_CASE("Bsdf: conductor weight equals bsdf.color", "[bsdf]") {
    Rng rng{0};
    const bvhvec3 reflectance{0.7f, 0.5f, 0.2f};
    const Bsdf conductor{BsdfKind::Conductor, 1.0f, reflectance};
    const bvhvec3 normal{0.0f, 1.0f, 0.0f};
    const bvhvec3 incoming = normalize({0.3f, -1.0f, 0.1f});

    const auto bs = conductor.sample(rng, incoming, normal);
    REQUIRE(bs.weight.x == Catch::Approx(reflectance.x));
    REQUIRE(bs.weight.y == Catch::Approx(reflectance.y));
    REQUIRE(bs.weight.z == Catch::Approx(reflectance.z));
    // Reflected ray must be on the side the photon came from (opposite to incoming z-component).
    REQUIRE(dot(bs.dir, normal) >= 0.0f);
}

TEST_CASE("Bsdf: conductor handles back-facing geometric normal", "[bsdf]") {
    Rng rng{0};
    const Bsdf conductor{BsdfKind::Conductor, 1.0f, {1.0f, 1.0f, 1.0f}};
    // Normal points same way as photon — must be flipped before reflecting.
    const bvhvec3 normal{0.0f, 0.0f, -1.0f};
    const float inv_sqrt2 = 1.0f / std::sqrt(2.0f);
    const bvhvec3 incoming{inv_sqrt2, 0.0f, -inv_sqrt2};

    const auto bs = conductor.sample(rng, incoming, normal);
    // After flipping, the mirror is still normalize(1, 0, 1).
    REQUIRE(bs.dir.x == Catch::Approx(inv_sqrt2).margin(1e-5f));
    REQUIRE(bs.dir.y == Catch::Approx(0.0f).margin(1e-5f));
    REQUIRE(bs.dir.z == Catch::Approx(inv_sqrt2).margin(1e-5f));
}

TEST_CASE("Bsdf: medium shell passes the ray straight through", "[bsdf]") {
    Rng rng{77};
    const Bsdf shell{BsdfKind::MediumShell, 1.0f};
    const bvhvec3 normal{0.0f, 0.0f, 1.0f};
    const bvhvec3 incoming = normalize({0.2f, -0.3f, -1.0f});

    const auto bs = shell.sample(rng, incoming, normal);
    // No deflection: outgoing == incoming regardless of the normal orientation.
    REQUIRE(bs.dir.x == Catch::Approx(incoming.x));
    REQUIRE(bs.dir.y == Catch::Approx(incoming.y));
    REQUIRE(bs.dir.z == Catch::Approx(incoming.z));
    REQUIRE(bs.is_refract == true);
}

// ---------------------------------------------------------------------------
// Dielectric tests
// ---------------------------------------------------------------------------

TEST_CASE("Bsdf: dielectric TIR always reflects past critical angle", "[bsdf][dielectric]") {
    // Glass ior=1.5 from inside → exiting, eta = 1/1.5 ≈ 0.667.
    // Critical angle: sin_c = 0.667 → theta_c ≈ 41.8°.
    // Use 60° incidence (cos = 0.5) — past critical, so F = 1 on every sample.
    Rng rng{42};
    const Bsdf glass{BsdfKind::Dielectric, 1.5f};
    // normal = {0,0,1}, photon exits (ndi_raw > 0): incoming has positive z-component.
    const bvhvec3 normal{0.0f, 0.0f, 1.0f};
    const float   cos60 = 0.5f, sin60 = std::sqrt(0.75f);
    const bvhvec3 incoming{sin60, 0.0f, cos60};  // ndi_raw = cos60 > 0 → exiting

    for (int i = 0; i < 200; ++i) {
        const auto bs = glass.sample(rng, incoming, normal);
        REQUIRE(bs.is_refract == false);  // TIR: always reflect
    }
}

TEST_CASE("Bsdf: dielectric reflect fraction matches Fresnel at normal incidence", "[bsdf][dielectric]") {
    // eta = 1.5 entering, normal incidence: F = ((eta-1)/(eta+1))^2 = (0.5/2.5)^2 = 0.04
    constexpr float kExpectedF = 0.04f;
    constexpr int   N          = 100000;
    Rng rng{123};
    const Bsdf glass{BsdfKind::Dielectric, 1.5f};
    const bvhvec3 normal{0.0f, 0.0f, 1.0f};
    const bvhvec3 incoming{0.0f, 0.0f, -1.0f};  // normal incidence, entering

    int reflects = 0;
    for (int i = 0; i < N; ++i) {
        if (!glass.sample(rng, incoming, normal).is_refract) ++reflects;
    }
    const float measured = static_cast<float>(reflects) / N;
    INFO("measured reflect fraction = " << measured << ", expected ≈ " << kExpectedF);
    REQUIRE(std::fabs(measured - kExpectedF) < 0.005f);
}

TEST_CASE("Bsdf: dielectric refracted direction satisfies Snell's law", "[bsdf][dielectric]") {
    // eta = 1.5 entering, 30° incidence: sin_theta_i = 0.5, sin_theta_t = 0.5/1.5 = 1/3.
    Rng rng{7};
    const Bsdf    glass{BsdfKind::Dielectric, 1.5f};
    const bvhvec3 normal{0.0f, 0.0f, 1.0f};
    // incoming at 30° from -z: {sin30, 0, -cos30}
    const float   sin30 = 0.5f, cos30 = std::sqrt(0.75f);
    const bvhvec3 incoming = normalize({sin30, 0.0f, -cos30});

    constexpr float kExpectedSinT = 1.0f / 3.0f;
    int refract_count = 0;
    for (int i = 0; i < 10000; ++i) {
        const auto bs = glass.sample(rng, incoming, normal);
        if (!bs.is_refract) continue;
        ++refract_count;
        // Refracted ray goes through surface (negative z). sin_theta_t = |xy component|.
        const float sin_theta_t = std::sqrt(bs.dir.x*bs.dir.x + bs.dir.y*bs.dir.y);
        REQUIRE(sin_theta_t == Catch::Approx(kExpectedSinT).margin(1e-4f));
        REQUIRE(bs.dir.z < 0.0f);  // transmitted through surface
    }
    REQUIRE(refract_count > 9000);  // F ≈ 4% at 30°, so ~96% refract
}

TEST_CASE("Bsdf: dielectric branch weights are correct", "[bsdf][dielectric]") {
    // Run many samples; check weight for each branch independently via is_refract.
    const bvhvec3 refl_color{0.9f, 0.8f, 0.7f};
    const bvhvec3 trans_color{0.6f, 0.5f, 0.4f};
    const float   eta = 1.5f;
    Bsdf glass{BsdfKind::Dielectric, eta, refl_color, trans_color};
    const bvhvec3 normal{0.0f, 0.0f, 1.0f};
    const bvhvec3 incoming{0.0f, 0.0f, -1.0f};  // normal incidence, entering
    Rng rng{99};

    const float expected_refract_w = 1.0f / (eta * eta);  // trans_color / eta^2, per channel

    for (int i = 0; i < 2000; ++i) {
        const auto bs = glass.sample(rng, incoming, normal);
        if (!bs.is_refract) {
            REQUIRE(bs.weight.x == Catch::Approx(refl_color.x));
            REQUIRE(bs.weight.y == Catch::Approx(refl_color.y));
            REQUIRE(bs.weight.z == Catch::Approx(refl_color.z));
        } else {
            REQUIRE(bs.weight.x == Catch::Approx(trans_color.x * expected_refract_w).margin(1e-5f));
            REQUIRE(bs.weight.y == Catch::Approx(trans_color.y * expected_refract_w).margin(1e-5f));
            REQUIRE(bs.weight.z == Catch::Approx(trans_color.z * expected_refract_w).margin(1e-5f));
        }
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

TEST_CASE("Scene: medium table defaults id 0 to vacuum and stores entries", "[medium][scene]") {
    std::vector<tinybvh::bvhvec4> tris = {
        {0.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f}};
    std::vector<uint32_t> inst = {0u};
    RayModel model{std::move(tris), std::move(inst), 1u};
    Scene scene{model};

    // Id 0 is vacuum: no scattering or absorption.
    REQUIRE(scene.medium(0).sigma_s == 0.0f);
    REQUIRE(scene.medium(0).sigma_a == 0.0f);
    REQUIRE(scene.medium(0).sigma_t() == 0.0f);

    scene.set_medium(2u, Medium{/*sigma_s=*/1.5f, /*sigma_a=*/0.5f});
    REQUIRE(scene.medium(2).sigma_s == Catch::Approx(1.5f));
    REQUIRE(scene.medium(2).sigma_t() == Catch::Approx(2.0f));
    // Gap id falls back to vacuum.
    REQUIRE(scene.medium(1).sigma_t() == 0.0f);
}
