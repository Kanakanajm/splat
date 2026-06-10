#pragma once

#include "point_light.hpp"
#include "random.hpp"
#include "tiny_bvh.h"

#include <cstdint>
#include <variant>
#include <vector>

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

// Planar polygonal area light defined by its constituent triangles.
// Built by SceneConfig::apply() from a named instance in the model.
struct AreaLight {
    struct Tri { tinybvh::bvhvec3 v0, v1, v2; };
    std::vector<Tri>      tris;         // triangles making up the light surface
    std::vector<uint32_t> prim_indices; // corresponding prim IDs in the BVH
    tinybvh::bvhvec3      normal;       // outward surface normal (planar assumption)
    tinybvh::bvhvec3      emission;     // Le (W/m²/sr)
    float                 total_area = 0.0f;

    uint32_t medium_id = 0u;  // always vacuum

    // Total power: Le * π * A (Lambertian emitter integrated over hemisphere and area).
    tinybvh::bvhvec3 total_power() const {
        constexpr float kPi = 3.14159265358979f;
        return {emission.x * kPi * total_area,
                emission.y * kPi * total_area,
                emission.z * kPi * total_area};
    }

    // Sample a point uniformly on the light surface; PDF = 1/total_area (area measure).
    tinybvh::bvhvec3 sample_point(Rng& rng) const;

    // Emit a photon ray from the light surface (cosine-weighted hemisphere from normal).
    tinybvh::Ray emit_ray(Rng& rng) const;
};

using Light = std::variant<PointLight, EnvLight, AreaLight>;
