#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "kernel_texture.hpp"

#include <cmath>

TEST_CASE("kernel_texture: peak at center equals 2/π", "[kernel]") {
    const auto data = build_kernel_texture(64);
    // Centre texel: row=31, col=31 (0-indexed, texel centre at (31.5/64, 31.5/64)).
    // r = 2 * |0.5078 − 0.5| ≈ 0.016 → k ≈ (2/π)(1−0.016²) ≈ 2/π.
    // Use the exact centre pair (row=32, col=32) to get the closest texel.
    // Actually the two centre-most texels are at row/col = 31 and 32 (centres 31.5/64 and 32.5/64).
    // Both are equally far from 0.5; choose row=31,col=31 and accept a small r offset.
    constexpr float kPi   = 3.14159265358979f;
    constexpr float kPeak = 2.0f / kPi;
    const float u = (31 + 0.5f) / 64.0f;  // 0.4921875
    const float r = 2.0f * std::sqrt(2.0f * (u - 0.5f) * (u - 0.5f));
    const float expected = kPeak * (1.0f - r * r);
    REQUIRE(data[31 * 64 + 31] == Catch::Approx(expected).epsilon(1e-5f));
    // Peak is close to 2/π.
    REQUIRE(data[31 * 64 + 31] == Catch::Approx(kPeak).epsilon(0.01f));
}

TEST_CASE("kernel_texture: zero outside kernel support", "[kernel]") {
    const auto data = build_kernel_texture(64);
    // Corners have r = 2*sqrt(0.5²+0.5²) = sqrt(2) > 1 → must be 0.
    REQUIRE(data[0]            == 0.0f);
    REQUIRE(data[63]           == 0.0f);
    REQUIRE(data[63 * 64]      == 0.0f);
    REQUIRE(data[63 * 64 + 63] == 0.0f);
}

TEST_CASE("kernel_texture: all values non-negative", "[kernel]") {
    const auto data = build_kernel_texture(64);
    for (float v : data) REQUIRE(v >= 0.0f);
}

TEST_CASE("kernel_texture: UV-space area integral equals 1/4", "[kernel]") {
    // ∫₀¹∫₀¹ k(UV) du dv = 1/4 for the 2D Epanechnikov kernel with r=2|UV−0.5|.
    // Discrete approximation: Σ k[i,j] / size² ≈ 0.25.
    constexpr int kSize = 64;
    const auto data = build_kernel_texture(kSize);
    float sum = 0.0f;
    for (float v : data) sum += v;
    REQUIRE(sum / (kSize * kSize) == Catch::Approx(0.25f).epsilon(0.02f));
}
