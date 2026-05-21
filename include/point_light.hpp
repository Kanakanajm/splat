#pragma once

#include "random.hpp"
#include "tiny_bvh.h"

#include <cstdint>

struct PointLight {
    tinybvh::bvhvec3 position;
    uint32_t         medium_id;  // index into the tracer's medium table (0 = vacuum convention)

    // Sample a primary ray: origin = position, direction uniformly on the unit sphere.
    tinybvh::Ray emit_ray(Rng& rng) const;
};
