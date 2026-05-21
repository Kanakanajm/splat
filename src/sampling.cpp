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
