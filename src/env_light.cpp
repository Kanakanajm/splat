#include "env_light.hpp"

#include "sampling.hpp"

#include <cmath>

tinybvh::bvhvec3 EnvLight::total_power() const {
    constexpr float kPi = 3.14159265358979323846f;
    const float s = 4.0f * kPi * kPi * scene_radius * scene_radius;
    return {color.x * s, color.y * s, color.z * s};
}

tinybvh::Ray EnvLight::emit_ray(Rng& rng) const {
    constexpr float kTwoPi = 6.28318530717958647692f;

    // Sample photon travel direction uniformly on sphere.
    const tinybvh::bvhvec3 d = sample_unit_sphere(rng);

    // Build ONB around d (Duff et al. 2017).
    const float sgn = std::copysign(1.0f, d.z);
    const float a   = -1.0f / (sgn + d.z);
    const float b   = d.x * d.y * a;
    const tinybvh::bvhvec3 t {1.0f + sgn * d.x * d.x * a, sgn * b,              -sgn * d.x};
    const tinybvh::bvhvec3 bn{b,                           sgn + d.y * d.y * a,  -d.y      };

    // Sample uniform disk of radius scene_radius perpendicular to d.
    const float r   = scene_radius * std::sqrt(rng.uniform());
    const float phi = kTwoPi * rng.uniform();
    const float u   = r * std::cos(phi);
    const float v   = r * std::sin(phi);

    // Origin: disk center (scene_center - R*d) offset by (u,v) in the tangent plane.
    // Distance^2 from scene_center = R^2 + u^2 + v^2 >= R^2, so always outside/on the sphere.
    const tinybvh::bvhvec3 origin{
        scene_center.x - scene_radius * d.x + u * t.x + v * bn.x,
        scene_center.y - scene_radius * d.y + u * t.y + v * bn.y,
        scene_center.z - scene_radius * d.z + u * t.z + v * bn.z,
    };

    return tinybvh::Ray{origin, d};
}
