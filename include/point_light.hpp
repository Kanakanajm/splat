#pragma once

#include "random.hpp"
#include "tiny_bvh.h"

#include <cstdint>

struct PointLight {
    tinybvh::bvhvec3 position;
    tinybvh::bvhvec3 power     = {1.0f, 1.0f, 1.0f};  // total RGB flux
    uint32_t         medium_id = 0u;
    // When non-zero: restrict emission to a cosine-weighted hemisphere around this direction.
    // power then represents the hemispherical flux (not full-sphere).
    tinybvh::bvhvec3 emit_dir  = {0.0f, 0.0f, 0.0f};

    // Sample a primary ray: uniform sphere, or cosine hemisphere if emit_dir is set.
    tinybvh::Ray emit_ray(Rng& rng) const;
};
