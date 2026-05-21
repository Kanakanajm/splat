#include "photon_tracer.hpp"

void PhotonTracer::trace(uint32_t photon_count, Rng& rng) {
    points_.clear();
    points_.reserve(photon_count);

    for (uint32_t i = 0; i < photon_count; ++i) {
        tinybvh::Ray ray = light_.emit_ray(rng);
        bvh_.Intersect(ray);
        if (ray.hit.t >= BVH_FAR) continue;

        const tinybvh::bvhvec3 p{
            ray.O.x + ray.hit.t * ray.D.x,
            ray.O.y + ray.hit.t * ray.D.y,
            ray.O.z + ray.hit.t * ray.D.z};
        points_.push_back({p, scene_.bsdf_id_at(ray.hit.prim)});
    }
}
