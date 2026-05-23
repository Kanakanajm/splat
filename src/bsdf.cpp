#include "bsdf.hpp"

#include "sampling.hpp"

BsdfSample Bsdf::sample(Rng& rng,
                        const tinybvh::bvhvec3& incoming,
                        const tinybvh::bvhvec3& normal) const {
    // Medium shell: pass-through, no deflection. Tracer handles medium switch via is_refract.
    if (kind == BsdfKind::MediumShell) {
        return {incoming, {1.0f, 1.0f, 1.0f}, /*is_refract=*/true};
    }

    // Orient the shading normal onto the incident side so the scattered photon
    // leaves on the same side it arrived from (regardless of the geometric
    // normal's winding).
    const float ndi = normal.x * incoming.x + normal.y * incoming.y + normal.z * incoming.z;
    const tinybvh::bvhvec3 n =
        (ndi <= 0.0f) ? normal : tinybvh::bvhvec3{-normal.x, -normal.y, -normal.z};

    // Diffuse: cosine-hemisphere direction; weight = albedo (cosine and PDF cancel).
    return {sample_cosine_hemisphere(rng, n), color, /*is_refract=*/false};
}
