#include "sampling.hpp"

#include <algorithm>
#include <cmath>

tinybvh::bvhvec3 sample_unit_sphere(Rng& rng) {
    constexpr float kTwoPi = 6.28318530717958647692f;
    const float z   = 1.0f - 2.0f * rng.uniform();
    const float phi = kTwoPi * rng.uniform();
    const float r   = std::sqrt(std::max(0.0f, 1.0f - z * z));
    return tinybvh::bvhvec3{r * std::cos(phi), r * std::sin(phi), z};
}

tinybvh::bvhvec3 sample_cosine_hemisphere(Rng& rng, const tinybvh::bvhvec3& normal) {
    constexpr float kTwoPi = 6.28318530717958647692f;
    // Malley's method: sample a disk, project up. z = cos(theta).
    const float u1  = rng.uniform();
    const float u2  = rng.uniform();
    const float r   = std::sqrt(u1);
    const float phi = kTwoPi * u2;
    const float x   = r * std::cos(phi);
    const float y   = r * std::sin(phi);
    const float z   = std::sqrt(std::max(0.0f, 1.0f - u1));

    // Branchless orthonormal basis around `normal` (Duff et al. 2017).
    const float sgn = std::copysign(1.0f, normal.z);
    const float a   = -1.0f / (sgn + normal.z);
    const float b   = normal.x * normal.y * a;
    const tinybvh::bvhvec3 t1{1.0f + sgn * normal.x * normal.x * a, sgn * b, -sgn * normal.x};
    const tinybvh::bvhvec3 t2{b, sgn + normal.y * normal.y * a, -normal.y};

    return tinybvh::bvhvec3{
        x * t1.x + y * t2.x + z * normal.x,
        x * t1.y + y * t2.y + z * normal.y,
        x * t1.z + y * t2.z + z * normal.z};
}

float sample_free_flight(float sigma_t, float xi) {
    return -std::log(1.0f - xi) / sigma_t;
}
