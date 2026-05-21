#pragma once

#include "tiny_bvh.h"

#include <cstdint>

// Pinhole camera for CPU ray tracing. Aggregate type so callers can use
// designated initializers, e.g.
//   PinholeCamera cam{ .eye = {0,0,2}, .target = {0,0,0}, .up = {0,1,0},
//                      .fov_y = 0.6f, .width = 64, .height = 64 };
struct PinholeCamera {
    tinybvh::bvhvec3 eye;
    tinybvh::bvhvec3 target;
    tinybvh::bvhvec3 up;
    float            fov_y;   // vertical field of view, radians
    uint32_t         width;
    uint32_t         height;

    // Generate a primary ray through the center of pixel (px, py).
    // Pixel (0,0) is top-left; +x goes right, +y goes down (standard image convention).
    tinybvh::Ray generate_ray(uint32_t px, uint32_t py) const;
};
