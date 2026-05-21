#include "ray_camera.hpp"

#include <cmath>

tinybvh::Ray PinholeCamera::generate_ray(uint32_t px, uint32_t py) const {
    using tinybvh::bvhvec3;
    using tinybvh::tinybvh_cross;
    using tinybvh::tinybvh_normalize;

    const bvhvec3 forward = tinybvh_normalize(target - eye);
    const bvhvec3 right   = tinybvh_normalize(tinybvh_cross(forward, up));
    const bvhvec3 cam_up  = tinybvh_cross(right, forward);

    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    const float scale  = std::tan(0.5f * fov_y);

    const float nx = (2.0f * (static_cast<float>(px) + 0.5f) / static_cast<float>(width)  - 1.0f) * aspect * scale;
    const float ny = (1.0f - 2.0f * (static_cast<float>(py) + 0.5f) / static_cast<float>(height)) * scale;

    const bvhvec3 dir = tinybvh_normalize(
        bvhvec3{forward.x + nx * right.x + ny * cam_up.x,
                forward.y + nx * right.y + ny * cam_up.y,
                forward.z + nx * right.z + ny * cam_up.z});

    return tinybvh::Ray{eye, dir};
}
