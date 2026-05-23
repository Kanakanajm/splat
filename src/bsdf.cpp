#include "bsdf.hpp"

#include "sampling.hpp"

#include <cmath>

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

// Unpolarized Fresnel reflectance for a dielectric interface.
// cos_theta_i must be positive (|dot(incoming, n_oriented)|).
// eta = n2/n1 (transmitted / incident). Returns 1 on TIR.
float fresnel_dielectric(float cos_theta_i, float eta) {
    const float inv_eta       = 1.0f / eta;
    const float cos_theta_t_sq = 1.0f - inv_eta * inv_eta * (1.0f - cos_theta_i * cos_theta_i);
    if (cos_theta_t_sq <= 0.0f) return 1.0f;  // total internal reflection

    const float cos_theta_t = std::sqrt(cos_theta_t_sq);
    const float rs = (eta * cos_theta_i - cos_theta_t) / (eta * cos_theta_i + cos_theta_t);
    const float rp = (cos_theta_i - eta * cos_theta_t) / (cos_theta_i + eta * cos_theta_t);
    return 0.5f * (rs * rs + rp * rp);
}

// Snell's law refraction. d: incoming (into surface); n: oriented to oppose d.
// cos_theta_i = -dot(d, n) > 0.  Returns the transmitted direction (through surface).
tinybvh::bvhvec3 refract(const tinybvh::bvhvec3& d, const tinybvh::bvhvec3& n,
                         float eta, float cos_theta_i) {
    const float inv_eta    = 1.0f / eta;
    const float cos_theta_t = std::sqrt(std::max(0.0f,
        1.0f - inv_eta * inv_eta * (1.0f - cos_theta_i * cos_theta_i)));
    const float coeff = inv_eta * cos_theta_i - cos_theta_t;
    return {inv_eta * d.x + coeff * n.x,
            inv_eta * d.y + coeff * n.y,
            inv_eta * d.z + coeff * n.z};
}

}  // namespace

BsdfSample Bsdf::sample(Rng& rng,
                        const tinybvh::bvhvec3& incoming,
                        const tinybvh::bvhvec3& normal) const {
    // Medium shell: pass-through, no deflection. Tracer handles medium switch via is_refract.
    if (kind == BsdfKind::MediumShell) {
        return {incoming, {1.0f, 1.0f, 1.0f}, /*is_refract=*/true};
    }

    // dot(normal, incoming) < 0: entering (front face) → eta = ior.
    // dot(normal, incoming) > 0: exiting  (back face)  → eta = 1/ior.
    const float ndi_raw = normal.x*incoming.x + normal.y*incoming.y + normal.z*incoming.z;
    const tinybvh::bvhvec3 n = orient_normal(normal, incoming);

    if (kind == BsdfKind::Conductor) {
        // Perfect mirror reflection; weight = constant reflectance (no Fresnel).
        return {tinybvh::tinybvh_normalize(reflect(incoming, n)), color, /*is_refract=*/false};
    }

    if (kind == BsdfKind::Dielectric) {
        const float eta         = (ndi_raw < 0.0f) ? ior : (1.0f / ior);
        const float cos_theta_i = -(incoming.x*n.x + incoming.y*n.y + incoming.z*n.z);
        const float F           = fresnel_dielectric(cos_theta_i, eta);

        if (rng.uniform() < F) {
            // Reflect — Fresnel cancels with sampling probability, weight = reflectance tint.
            return {tinybvh::tinybvh_normalize(reflect(incoming, n)), color, /*is_refract=*/false};
        } else {
            // Refract — weight = transmittance_color / eta^2 (adjoint Snell factor).
            const tinybvh::bvhvec3 dir = refract(incoming, n, eta, cos_theta_i);
            const float eta_sq = eta * eta;
            return {tinybvh::tinybvh_normalize(dir),
                    {transmittance_color.x / eta_sq,
                     transmittance_color.y / eta_sq,
                     transmittance_color.z / eta_sq},
                    /*is_refract=*/true};
        }
    }

    // Diffuse: cosine-hemisphere direction; weight = albedo (cosine and PDF cancel).
    return {sample_cosine_hemisphere(rng, n), color, /*is_refract=*/false};
}
