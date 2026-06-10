#include "env_light.hpp"

#include "sampling.hpp"

#include <cmath>
#include <cstddef>

tinybvh::bvhvec3 EnvLight::total_power() const {
    constexpr float kPi = 3.14159265358979323846f;
    const float s = 4.0f * kPi * kPi * scene_radius * scene_radius;
    return {color.x * s, color.y * s, color.z * s};
}

tinybvh::bvhvec3 AreaLight::sample_point(Rng& rng) const {
    // Select triangle by area using CDF; fall back to last if float rounding overshoots.
    const float r = rng.uniform() * total_area;
    size_t idx = tris.size() - 1u;
    float cumsum = 0.0f;
    for (size_t i = 0; i < tris.size(); ++i) {
        const auto& t = tris[i];
        const tinybvh::bvhvec3 e1{t.v1.x-t.v0.x, t.v1.y-t.v0.y, t.v1.z-t.v0.z};
        const tinybvh::bvhvec3 e2{t.v2.x-t.v0.x, t.v2.y-t.v0.y, t.v2.z-t.v0.z};
        const tinybvh::bvhvec3 cr = tinybvh::tinybvh_cross(e1, e2);
        cumsum += 0.5f * std::sqrt(cr.x*cr.x + cr.y*cr.y + cr.z*cr.z);
        if (r <= cumsum) { idx = i; break; }
    }
    // Uniform barycentric sampling via folding: avoids sqrt.
    const auto& t = tris[idx];
    float u = rng.uniform(), v = rng.uniform();
    if (u + v > 1.0f) { u = 1.0f - u; v = 1.0f - v; }
    const float w = 1.0f - u - v;
    return {w*t.v0.x + u*t.v1.x + v*t.v2.x,
            w*t.v0.y + u*t.v1.y + v*t.v2.y,
            w*t.v0.z + u*t.v1.z + v*t.v2.z};
}

tinybvh::Ray AreaLight::emit_ray(Rng& rng) const {
    constexpr float kEps = 1e-4f;
    const tinybvh::bvhvec3 origin = sample_point(rng);
    // Offset origin slightly along the outward normal to avoid self-intersection.
    const tinybvh::bvhvec3 o{origin.x + kEps*normal.x,
                              origin.y + kEps*normal.y,
                              origin.z + kEps*normal.z};
    return tinybvh::Ray{o, sample_cosine_hemisphere(rng, normal)};
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
