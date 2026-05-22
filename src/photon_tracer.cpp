#include "photon_tracer.hpp"

#include "bsdf.hpp"

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

    constexpr float kEps = 1e-4f;  // ray-origin offset to escape the surface just hit

    for (uint32_t i = 0; i < photon_count; ++i) {
        tinybvh::Ray ray = light_.emit_ray(rng);

        for (uint32_t depth = 0; depth < max_depth; ++depth) {
            bvh_.Intersect(ray);
            if (ray.hit.t >= BVH_FAR) break;

            const uint32_t prim = ray.hit.prim;
            const tinybvh::bvhvec3 p{
                ray.O.x + ray.hit.t * ray.D.x,
                ray.O.y + ray.hit.t * ray.D.y,
                ray.O.z + ray.hit.t * ray.D.z};

            const uint32_t bsdf_id = scene_.bsdf_id_at(prim);
            const Bsdf&    bsdf    = scene_.bsdf(bsdf_id);
            if (bsdf.kind == BsdfKind::Diffuse) {
                points_.push_back({p, bsdf_id});
            }

            const tinybvh::bvhvec3 n   = face_normal(scene_.model(), prim);
            const tinybvh::bvhvec3 dir = bsdf.sample(rng, ray.D, n);
            const tinybvh::bvhvec3 o{p.x + kEps * dir.x, p.y + kEps * dir.y, p.z + kEps * dir.z};
            ray = tinybvh::Ray{o, dir};
        }
    }
}
