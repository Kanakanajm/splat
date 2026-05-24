#include "photon_tracer.hpp"

#include "bsdf.hpp"
#include "medium.hpp"
#include "sampling.hpp"

#include <algorithm>

namespace {

// Geometric (face) normal of a triangle from the model's flat fat-triangle buffer.
// Not oriented — Bsdf::sample flips it onto the incident side.
tinybvh::bvhvec3 face_normal(const RayModel& model, uint32_t prim) {
    const auto& tris = model.triangles();
    const tinybvh::bvhvec3 v0{tris[prim * 3 + 0].x, tris[prim * 3 + 0].y, tris[prim * 3 + 0].z};
    const tinybvh::bvhvec3 v1{tris[prim * 3 + 1].x, tris[prim * 3 + 1].y, tris[prim * 3 + 1].z};
    const tinybvh::bvhvec3 v2{tris[prim * 3 + 2].x, tris[prim * 3 + 2].y, tris[prim * 3 + 2].z};
    return tinybvh::tinybvh_normalize(tinybvh::tinybvh_cross(v1 - v0, v2 - v0));
}

float max_component(const tinybvh::bvhvec3& v) {
    return std::max({v.x, v.y, v.z});
}

}  // namespace

void PhotonTracer::trace(uint32_t photon_count, uint32_t max_depth, Rng& rng) {
    points_.clear();
    beams_.clear();
    points_.reserve(photon_count);

    constexpr float kEps = 1e-4f;  // ray-origin offset to escape the interaction point

    const float n = static_cast<float>(photon_count);
    const tinybvh::bvhvec3 init_weight{light_.power.x / n,
                                       light_.power.y / n,
                                       light_.power.z / n};

    for (uint32_t i = 0; i < photon_count; ++i) {
        tinybvh::Ray     ray    = light_.emit_ray(rng);
        uint32_t         m      = light_.medium_id;
        tinybvh::bvhvec3 weight = init_weight;

        for (uint32_t depth = 0; depth < max_depth; ++depth) {
            bvh_.Intersect(ray);
            const bool  hit   = ray.hit.t < BVH_FAR;
            const float t_hit = ray.hit.t;

            // Free-flight: only participating media (sigma_t > 0) scatter; vacuum never does.
            const float sigma_t = scene_.medium(m).sigma_t();
            const float t_media = sigma_t > 0.0f
                                      ? sample_free_flight(sigma_t, rng.uniform())
                                      : BVH_FAR;

            if (t_media < t_hit) {
                // A participating medium with no surface ahead means the photon has
                // escaped into open space. Terminate.
                if (!hit) break;

                // Medium scatter at t_media. Beam segment = [ray.O, scatter point].
                const tinybvh::bvhvec3 scatter{ray.O.x + t_media * ray.D.x,
                                               ray.O.y + t_media * ray.D.y,
                                               ray.O.z + t_media * ray.D.z};

                beams_.push_back({ray.O, scatter, m, depth, weight});

                // Russian roulette.
                const float prr = std::max(0.05f, std::min(0.95f, max_component(weight)));
                if (rng.uniform() >= prr) break;
                weight.x /= prr; weight.y /= prr; weight.z /= prr;

                // Single-scatter albedo: σ_s / σ_t.
                const float albedo = scene_.medium(m).sigma_s / sigma_t;
                weight.x *= albedo; weight.y *= albedo; weight.z *= albedo;

                ray = tinybvh::Ray{scatter, sample_unit_sphere(rng)};
                continue;
            }

            // Surface hit.
            if (!hit) break;

            const uint32_t prim = ray.hit.prim;
            const tinybvh::bvhvec3 p{ray.O.x + t_hit * ray.D.x,
                                     ray.O.y + t_hit * ray.D.y,
                                     ray.O.z + t_hit * ray.D.z};

            const uint32_t bsdf_id = scene_.bsdf_id_at(prim);
            const Bsdf&    bsdf    = scene_.bsdf(bsdf_id);
            if (bsdf.kind == BsdfKind::Diffuse) {
                points_.push_back({p, bsdf_id, scene_.model().instance_id(prim), depth, weight});
            }

            // Russian roulette.
            const float prr = std::max(0.05f, std::min(0.95f, max_component(weight)));
            if (rng.uniform() >= prr) break;
            weight.x /= prr; weight.y /= prr; weight.z /= prr;

            const tinybvh::bvhvec3 normal = face_normal(scene_.model(), prim);
            const BsdfSample       bs     = bsdf.sample(rng, ray.D, normal);
            weight.x *= bs.weight.x; weight.y *= bs.weight.y; weight.z *= bs.weight.z;

            // Medium switch on any transmissive event (MediumShell pass-through or
            // Dielectric refraction).
            if (bs.is_refract) {
                const uint32_t in_id  = scene_.medium_in_at(prim);
                const uint32_t out_id = scene_.medium_out_at(prim);
                m = (m == in_id) ? out_id : in_id;
            }

            // Orient the unoriented face normal toward the new ray direction so the
            // offset always pushes the origin to the side the ray is leaving toward.
            const tinybvh::bvhvec3& dir = bs.dir;
            const float ns = (normal.x*dir.x + normal.y*dir.y + normal.z*dir.z) >= 0.0f
                                 ? 1.0f : -1.0f;

            // Offset along the normal to avoid lateral drift at corners.
            ray = tinybvh::Ray{
                {p.x + kEps * normal.x * ns,
                 p.y + kEps * normal.y * ns,
                 p.z + kEps * normal.z * ns}, dir};
        }
    }
}
