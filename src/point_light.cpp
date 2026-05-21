#include "point_light.hpp"

#include "sampling.hpp"

tinybvh::Ray PointLight::emit_ray(Rng& rng) const {
    return tinybvh::Ray{position, sample_unit_sphere(rng)};
}
