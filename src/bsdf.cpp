#include "bsdf.hpp"

#include "sampling.hpp"

tinybvh::bvhvec3 Bsdf::sample(Rng& rng,
                              const tinybvh::bvhvec3& incoming,
                              const tinybvh::bvhvec3& normal) const {
    // Orient the shading normal onto the incident side so the scattered photon
    // leaves on the same side it arrived from (regardless of the geometric
    // normal's winding).
    const float ndi = normal.x * incoming.x + normal.y * incoming.y + normal.z * incoming.z;
    const tinybvh::bvhvec3 n =
        (ndi <= 0.0f) ? normal : tinybvh::bvhvec3{-normal.x, -normal.y, -normal.z};

    // Only diffuse is implemented in this chunk.
    return sample_cosine_hemisphere(rng, n);
}
