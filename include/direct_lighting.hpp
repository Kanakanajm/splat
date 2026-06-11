#pragma once

#include "bsdf.hpp"
#include "env_light.hpp"
#include "random.hpp"
#include "sampling.hpp"
#include "scene.hpp"
#include "tiny_bvh.h"

#include <cmath>
#include <cstdint>
#include <vector>

// Free-function direct-lighting helpers shared by PathTracer and PhotonMapper.
namespace dl {

constexpr float kEps    = 1e-4f;
constexpr float kInvPi  = 1.0f / 3.14159265358979f;
constexpr float kInv4Pi = 1.0f / (4.0f * 3.14159265358979f);

inline float vec_len(const tinybvh::bvhvec3& v) {
    return std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
}

inline float power_heuristic(float pdf_a, float pdf_b) {
    const float a2 = pdf_a * pdf_a, b2 = pdf_b * pdf_b;
    return a2 / (a2 + b2);
}

// Transmittance along a shadow ray. Returns 0 if occluded by an opaque surface,
// otherwise product of exp(-sigma_t * d) over all media segments.
inline float shadow_Tr(const Scene& scene, const tinybvh::BVH& bvh,
                        tinybvh::bvhvec3 pos, tinybvh::bvhvec3 wi,
                        float max_dist, uint32_t medium_id) {
    float Tr        = 1.0f;
    float remaining = max_dist;

    for (int i = 0; i < 32; ++i) {
        tinybvh::Ray ray{pos, wi};
        bvh.Intersect(ray);
        const float hit_t = ray.hit.t;

        if (hit_t >= remaining - kEps) {
            const float sigma_t = scene.medium(medium_id).sigma_t();
            if (sigma_t > 0.0f) Tr *= std::exp(-sigma_t * remaining);
            return Tr;
        }

        const BsdfKind kind = scene.bsdf_at(ray.hit.prim).kind;
        if (kind != BsdfKind::Dielectric && kind != BsdfKind::MediumShell)
            return 0.0f;

        const float sigma_t = scene.medium(medium_id).sigma_t();
        if (sigma_t > 0.0f) Tr *= std::exp(-sigma_t * hit_t);

        const uint32_t in_id  = scene.medium_in_at(ray.hit.prim);
        const uint32_t out_id = scene.medium_out_at(ray.hit.prim);
        medium_id = (medium_id == in_id) ? out_id : in_id;

        pos.x += (hit_t + kEps) * wi.x;
        pos.y += (hit_t + kEps) * wi.y;
        pos.z += (hit_t + kEps) * wi.z;
        remaining -= hit_t + kEps;
        if (remaining <= 0.0f) return Tr;
    }
    return Tr;
}

// NEE from a diffuse surface point p with oriented normal n.
inline tinybvh::bvhvec3 nee_surface(const Scene& scene, const tinybvh::BVH& bvh,
                                     const std::vector<Light>& lights,
                                     const tinybvh::bvhvec3& p,
                                     const tinybvh::bvhvec3& n,
                                     const Bsdf& bsdf,
                                     uint32_t medium_id, Rng& rng) {
    tinybvh::bvhvec3 result{0.0f, 0.0f, 0.0f};
    const tinybvh::bvhvec3 offset{p.x + kEps*n.x, p.y + kEps*n.y, p.z + kEps*n.z};

    for (const auto& light : lights) {
        if (const auto* pl = std::get_if<PointLight>(&light)) {
            const tinybvh::bvhvec3 dir{pl->position.x - p.x,
                                       pl->position.y - p.y,
                                       pl->position.z - p.z};
            const float dist = vec_len(dir);
            if (dist < kEps) continue;
            const tinybvh::bvhvec3 wi = tinybvh::tinybvh_normalize(dir);
            const float cos_theta = wi.x*n.x + wi.y*n.y + wi.z*n.z;
            if (cos_theta <= 0.0f) continue;
            const float Tr = shadow_Tr(scene, bvh, offset, wi, dist, medium_id);
            if (Tr == 0.0f) continue;
            const float fac = Tr * kInvPi * kInv4Pi * cos_theta / (dist * dist);
            result.x += fac * bsdf.color.x * pl->power.x;
            result.y += fac * bsdf.color.y * pl->power.y;
            result.z += fac * bsdf.color.z * pl->power.z;

        } else if (const auto* el = std::get_if<EnvLight>(&light)) {
            const tinybvh::bvhvec3 wi = sample_cosine_hemisphere(rng, n);
            const float Tr = shadow_Tr(scene, bvh, offset, wi, BVH_FAR, medium_id);
            if (Tr == 0.0f) continue;
            result.x += 0.5f * Tr * el->color.x * bsdf.color.x;
            result.y += 0.5f * Tr * el->color.y * bsdf.color.y;
            result.z += 0.5f * Tr * el->color.z * bsdf.color.z;

        } else if (const auto* al = std::get_if<AreaLight>(&light)) {
            const tinybvh::bvhvec3 q   = al->sample_point(rng);
            const tinybvh::bvhvec3 dir{q.x-p.x, q.y-p.y, q.z-p.z};
            const float dist = vec_len(dir);
            if (dist < kEps) continue;
            const tinybvh::bvhvec3 wi = tinybvh::tinybvh_normalize(dir);
            const float cos_i = wi.x*n.x + wi.y*n.y + wi.z*n.z;
            if (cos_i <= 0.0f) continue;
            const float cos_l = -(wi.x*al->normal.x + wi.y*al->normal.y + wi.z*al->normal.z);
            if (cos_l <= 0.0f) continue;
            const tinybvh::bvhvec3 dir_s{q.x-offset.x, q.y-offset.y, q.z-offset.z};
            const float dist_shadow = vec_len(dir_s);
            const float Tr = shadow_Tr(scene, bvh, offset,
                                       tinybvh::tinybvh_normalize(dir_s),
                                       dist_shadow, medium_id);
            if (Tr == 0.0f) continue;
            const float p_light = dist * dist / (al->total_area * cos_l);
            const float p_bsdf  = cos_i * kInvPi;
            const float w       = power_heuristic(p_light, p_bsdf);
            const float fac     = w * Tr * kInvPi * cos_i / p_light;
            result.x += fac * bsdf.color.x * al->emission.x;
            result.y += fac * bsdf.color.y * al->emission.y;
            result.z += fac * bsdf.color.z * al->emission.z;
        }
    }
    return result;
}

// NEE from a medium scatter point p inside medium_id.
inline tinybvh::bvhvec3 nee_medium(const Scene& scene, const tinybvh::BVH& bvh,
                                    const std::vector<Light>& lights,
                                    const tinybvh::bvhvec3& p,
                                    uint32_t medium_id, Rng& rng) {
    tinybvh::bvhvec3 result{0.0f, 0.0f, 0.0f};

    for (const auto& light : lights) {
        if (const auto* pl = std::get_if<PointLight>(&light)) {
            const tinybvh::bvhvec3 dir{pl->position.x - p.x,
                                       pl->position.y - p.y,
                                       pl->position.z - p.z};
            const float dist = vec_len(dir);
            if (dist < kEps) continue;
            const tinybvh::bvhvec3 wi = tinybvh::tinybvh_normalize(dir);
            const float Tr = shadow_Tr(scene, bvh, p, wi, dist, medium_id);
            if (Tr == 0.0f) continue;
            const float fac = Tr * kInv4Pi * kInv4Pi / (dist * dist);
            result.x += fac * pl->power.x;
            result.y += fac * pl->power.y;
            result.z += fac * pl->power.z;

        } else if (const auto* el = std::get_if<EnvLight>(&light)) {
            const tinybvh::bvhvec3 wi = sample_unit_sphere(rng);
            const float Tr = shadow_Tr(scene, bvh, p, wi, BVH_FAR, medium_id);
            if (Tr == 0.0f) continue;
            result.x += 0.5f * Tr * el->color.x;
            result.y += 0.5f * Tr * el->color.y;
            result.z += 0.5f * Tr * el->color.z;

        } else if (const auto* al = std::get_if<AreaLight>(&light)) {
            const tinybvh::bvhvec3 q   = al->sample_point(rng);
            const tinybvh::bvhvec3 dir{q.x-p.x, q.y-p.y, q.z-p.z};
            const float dist = vec_len(dir);
            if (dist < kEps) continue;
            const tinybvh::bvhvec3 wi = tinybvh::tinybvh_normalize(dir);
            const float cos_l = -(wi.x*al->normal.x + wi.y*al->normal.y + wi.z*al->normal.z);
            if (cos_l <= 0.0f) continue;
            const float Tr = shadow_Tr(scene, bvh, p, wi, dist, medium_id);
            if (Tr == 0.0f) continue;
            const float p_light = dist * dist / (al->total_area * cos_l);
            const float p_bsdf  = kInv4Pi;
            const float w       = power_heuristic(p_light, p_bsdf);
            const float fac     = w * Tr * kInv4Pi / p_light;
            result.x += fac * al->emission.x;
            result.y += fac * al->emission.y;
            result.z += fac * al->emission.z;
        }
    }
    return result;
}

}  // namespace dl
