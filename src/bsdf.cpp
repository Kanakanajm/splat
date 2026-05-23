#include "bsdf.hpp"

#include "sampling.hpp"

namespace {

// Orient `normal` so it opposes `incoming` (faces the incident side).
tinybvh::bvhvec3 orient_normal(const tinybvh::bvhvec3& normal,
                               const tinybvh::bvhvec3& incoming) {
    const float ndi = normal.x*incoming.x + normal.y*incoming.y + normal.z*incoming.z;
    return (ndi <= 0.0f) ? normal : tinybvh::bvhvec3{-normal.x, -normal.y, -normal.z};
}

// Perfect specular reflection of `d` about `n` (n must oppose d: dot(d,n) <= 0).
tinybvh::bvhvec3 reflect(const tinybvh::bvhvec3& d, const tinybvh::bvhvec3& n) {
    const float dn = d.x*n.x + d.y*n.y + d.z*n.z;
    return {d.x - 2.0f*dn*n.x, d.y - 2.0f*dn*n.y, d.z - 2.0f*dn*n.z};
}

}  // namespace

BsdfSample Bsdf::sample(Rng& rng,
                        const tinybvh::bvhvec3& incoming,
                        const tinybvh::bvhvec3& normal) const {
    // Medium shell: pass-through, no deflection. Tracer handles medium switch via is_refract.
    if (kind == BsdfKind::MediumShell) {
        return {incoming, {1.0f, 1.0f, 1.0f}, /*is_refract=*/true};
    }

    const tinybvh::bvhvec3 n = orient_normal(normal, incoming);

    if (kind == BsdfKind::Conductor) {
        // Perfect mirror reflection; weight = constant reflectance (no Fresnel).
        return {tinybvh::tinybvh_normalize(reflect(incoming, n)), color, /*is_refract=*/false};
    }

    // Diffuse: cosine-hemisphere direction; weight = albedo (cosine and PDF cancel).
    return {sample_cosine_hemisphere(rng, n), color, /*is_refract=*/false};
}
