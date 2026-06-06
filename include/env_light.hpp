#pragma once

#include "point_light.hpp"
#include "random.hpp"
#include "tiny_bvh.h"

#include <cstdint>
#include <variant>

struct EnvLight {
    tinybvh::bvhvec3 color        = {1.0f, 1.0f, 1.0f};
    tinybvh::bvhvec3 scene_center = {};
    float            scene_radius = 1.0f;
    uint32_t         medium_id    = 0u;  // always vacuum

    // Total flux: color * 4*pi^2 * R^2
    // = integral over sphere directions of (L * pi*R^2) d_omega
    tinybvh::bvhvec3 total_power() const;

    // Sample a primary ray entering the scene bounding sphere.
    // Origin lies on or outside the bounding sphere; direction is uniform on S^2.
    tinybvh::Ray emit_ray(Rng& rng) const;
};

using Light = std::variant<PointLight, EnvLight>;
