#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "random.hpp"
#include "sampling.hpp"

#include <cmath>
#include <cstdint>

TEST_CASE("Rng: same seed produces same sequence", "[rng]") {
    Rng a{42}, b{42};
    for (int i = 0; i < 1000; ++i) {
        REQUIRE(a.uniform() == b.uniform());
    }
}

TEST_CASE("Rng: different seeds diverge", "[rng]") {
    Rng a{1}, b{2};
    int matches = 0;
    for (int i = 0; i < 1000; ++i) {
        if (a.uniform() == b.uniform()) ++matches;
    }
    REQUIRE(matches < 5);  // ridiculously generous; expect ~0
}

TEST_CASE("Rng: uniform output stays in half-open unit interval", "[rng]") {
    Rng rng{12345};
    for (int i = 0; i < 100000; ++i) {
        const float v = rng.uniform();
        REQUIRE(v >= 0.0f);
        REQUIRE(v <  1.0f);
    }
}

TEST_CASE("sample_unit_sphere: all samples are unit length", "[sampling][sphere]") {
    Rng rng{7};
    for (int i = 0; i < 10000; ++i) {
        const auto d = sample_unit_sphere(rng);
        const float len2 = d.x * d.x + d.y * d.y + d.z * d.z;
        REQUIRE(len2 == Catch::Approx(1.0f).margin(1e-5f));
    }
}

TEST_CASE("sample_unit_sphere: per-component statistics match uniform-on-sphere",
          "[sampling][sphere]") {
    constexpr int N = 100000;
    Rng rng{1234};

    double sum_x = 0.0, sum_y = 0.0, sum_z = 0.0;
    double sum_xx = 0.0, sum_yy = 0.0, sum_zz = 0.0;
    for (int i = 0; i < N; ++i) {
        const auto d = sample_unit_sphere(rng);
        sum_x  += d.x;       sum_y  += d.y;       sum_z  += d.z;
        sum_xx += d.x * d.x; sum_yy += d.y * d.y; sum_zz += d.z * d.z;
    }
    const double mean_x = sum_x / N, mean_y = sum_y / N, mean_z = sum_z / N;
    const double var_x  = sum_xx / N, var_y  = sum_yy / N, var_z  = sum_zz / N;

    // For uniform-on-unit-sphere: E[X] = 0, E[X²] = 1/3 (same for Y, Z).
    // Std of the sample mean is sqrt(Var/N) = sqrt(1/(3*N)). For N=1e5, ≈ 1.83e-3.
    // 5σ ≈ 0.01 — comfortably permissive.
    constexpr double mean_tol = 0.01;
    constexpr double var_tol  = 0.01;

    INFO("means=("  << mean_x << "," << mean_y << "," << mean_z << ")"
         << " E[X²]=(" << var_x << "," << var_y << "," << var_z << ")");
    REQUIRE(std::fabs(mean_x) < mean_tol);
    REQUIRE(std::fabs(mean_y) < mean_tol);
    REQUIRE(std::fabs(mean_z) < mean_tol);
    REQUIRE(std::fabs(var_x - 1.0 / 3.0) < var_tol);
    REQUIRE(std::fabs(var_y - 1.0 / 3.0) < var_tol);
    REQUIRE(std::fabs(var_z - 1.0 / 3.0) < var_tol);
}
