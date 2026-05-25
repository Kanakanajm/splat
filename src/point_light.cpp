#include "point_light.hpp"

#include "sampling.hpp"

tinybvh::Ray PointLight::emit_ray(Rng& rng) const {
    const float len2 = emit_dir.x*emit_dir.x + emit_dir.y*emit_dir.y + emit_dir.z*emit_dir.z;
    if (len2 > 0.0f) {
        const float inv = 1.0f / std::sqrt(len2);
        const tinybvh::bvhvec3 n{emit_dir.x*inv, emit_dir.y*inv, emit_dir.z*inv};
        return tinybvh::Ray{position, sample_cosine_hemisphere(rng, n)};
    }
    return tinybvh::Ray{position, sample_unit_sphere(rng)};
}
