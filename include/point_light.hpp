#pragma once

#include "random.hpp"
#include "tiny_bvh.h"

#include <cstdint>

struct PointLight {
    tinybvh::bvhvec3 position;
    tinybvh::bvhvec3 power     = {1.0f, 1.0f, 1.0f};  // total RGB flux
    uint32_t         medium_id = 0u;

    // Sample a primary ray: origin = position, direction uniformly on the unit sphere.
    tinybvh::Ray emit_ray(Rng& rng) const;
};
