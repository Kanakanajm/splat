#include "photon_tracer.hpp"

#include "bsdf.hpp"
#include "medium.hpp"
#include "sampling.hpp"

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

}  // namespace

void PhotonTracer::trace(uint32_t photon_count, uint32_t max_depth, Rng& rng) {
    points_.clear();
    beams_.clear();
    points_.reserve(photon_count);

    constexpr float kEps = 1e-4f;  // ray-origin offset to escape the interaction point

    for (uint32_t i = 0; i < photon_count; ++i) {
        tinybvh::Ray ray = light_.emit_ray(rng);
        uint32_t     m   = light_.medium_id;  // current medium (initially the light's)

        for (uint32_t depth = 0; depth < max_depth; ++depth) {
            bvh_.Intersect(ray);
            const bool  hit   = ray.hit.t < BVH_FAR;
            const float t_hit = ray.hit.t;  // BVH_FAR when there is no surface

            // Free-flight: only participating media (sigma_t > 0) scatter; vacuum never does.
            const float sigma_t = scene_.medium(m).sigma_t();
            const float t_media = sigma_t > 0.0f
                                      ? sample_free_flight(sigma_t, rng.uniform())
                                      : BVH_FAR;

            if (t_media < t_hit) {
                // A participating medium with no surface ahead means the photon has
                // escaped into open space (V1 media are bounded by geometry; this also
                // guards against medium desync at coincident surfaces). Terminate.
                if (!hit) break;

                // (a) Medium scatter event at t_media. Record a "long beam" from the
                // ray origin to the medium exit (the surface at t_hit), then respawn
                // isotropically from the scatter point.
                const tinybvh::bvhvec3 p{ray.O.x + t_media * ray.D.x,
                                         ray.O.y + t_media * ray.D.y,
                                         ray.O.z + t_media * ray.D.z};

                beams_.push_back({p,
                                  {ray.O.x + t_hit * ray.D.x,
                                   ray.O.y + t_hit * ray.D.y,
                                   ray.O.z + t_hit * ray.D.z},
                                  m});

                
                const tinybvh::bvhvec3 dir = sample_unit_sphere(rng);
                ray = tinybvh::Ray{p,
                                   dir};
                continue;
            }

            // (b) Surface hit.
            if (!hit) break;

            const uint32_t prim = ray.hit.prim;
            const tinybvh::bvhvec3 p{ray.O.x + t_hit * ray.D.x,
                                     ray.O.y + t_hit * ray.D.y,
                                     ray.O.z + t_hit * ray.D.z};

            const uint32_t bsdf_id = scene_.bsdf_id_at(prim);
            const Bsdf&    bsdf    = scene_.bsdf(bsdf_id);
            if (bsdf.kind == BsdfKind::Diffuse) {
                points_.push_back({p, bsdf_id, scene_.model().instance_id(prim), depth});
            }

            const tinybvh::bvhvec3 n   = face_normal(scene_.model(), prim);
            const tinybvh::bvhvec3 dir = bsdf.sample(rng, ray.D, n);

            // Medium switch on a transmissive (pass-through) event: flip between the
            // surface's inside/outside media. Flipping (rather than picking by the
            // normal's sign) is robust to inconsistent face winding, so a closed
            // shell contains its medium regardless of how the mesh is wound.
            // Diffuse/specular reflections keep the current medium; dielectric
            // refraction is deferred (see trace_v1_status.md chunk 6).
            if (bsdf.kind == BsdfKind::MediumShell) {
                const uint32_t in_id  = scene_.medium_in_at(prim);
                const uint32_t out_id = scene_.medium_out_at(prim);
                m = (m == in_id) ? out_id : in_id;
            }

            // Orient the unoriented face normal toward the new ray direction so the
            // offset always pushes the origin to the side the ray is leaving toward.
            const float ns = (n.x*dir.x + n.y*dir.y + n.z*dir.z) >= 0.0f ? 1.0f : -1.0f;

            // Offset along the normal (not dir) to avoid lateral drift that could push
            // the origin past an adjacent face at corners, desyncing the medium tracker.
            ray = tinybvh::Ray{
                {p.x + kEps * n.x * ns, p.y + kEps * n.y * ns, p.z + kEps * n.z * ns}, dir};
        }
    }
}
