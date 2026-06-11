#include "photon_mapper.hpp"

#include "bsdf.hpp"
#include "direct_lighting.hpp"
#include "medium.hpp"
#include "photon_tracer.hpp"
#include "ray_camera.hpp"
#include "ray_model.hpp"
#include "sampling.hpp"

#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>

namespace {
constexpr float kPi = 3.14159265358979f;

tinybvh::bvhvec3 face_normal(const RayModel& model, uint32_t prim) {
    const auto& tris = model.triangles();
    const tinybvh::bvhvec3 v0{tris[prim*3+0].x, tris[prim*3+0].y, tris[prim*3+0].z};
    const tinybvh::bvhvec3 v1{tris[prim*3+1].x, tris[prim*3+1].y, tris[prim*3+1].z};
    const tinybvh::bvhvec3 v2{tris[prim*3+2].x, tris[prim*3+2].y, tris[prim*3+2].z};
    return tinybvh::tinybvh_normalize(tinybvh::tinybvh_cross(v1 - v0, v2 - v0));
}
}  // namespace

PhotonMapper::PhotonMapper(const Scene& scene, const tinybvh::BVH& bvh,
                           std::vector<Light> lights,
                           int n_photons, float r_surf, float r_vol,
                           int max_cam_depth, int max_emit_depth)
    : scene_(scene), bvh_(bvh), lights_(std::move(lights))
    , n_photons_(n_photons), r_surf_(r_surf), r_vol_(r_vol)
    , max_cam_depth_(max_cam_depth), max_emit_depth_(max_emit_depth)
{}

void PhotonMapper::emit(Rng& rng) {
    PhotonTracer tracer{scene_, bvh_, lights_};
    tracer.trace(static_cast<uint32_t>(n_photons_), static_cast<uint32_t>(max_emit_depth_), rng);
    surf_tree_.build(tracer.points());
    vol_tree_.build(tracer.vol_points());
}

tinybvh::bvhvec3 PhotonMapper::gather(tinybvh::Ray ray, uint32_t medium_id,
                                       int depth, Rng& rng) const {
    tinybvh::bvhvec3 L{};
    tinybvh::bvhvec3 weight{1.f, 1.f, 1.f};
    constexpr float  kEps = 1e-4f;

    for (int d = depth; d < max_cam_depth_; ++d) {
        bvh_.Intersect(ray);
        const float t_hit = ray.hit.t;
        const bool  hit   = t_hit < BVH_FAR;

        const auto&  med   = scene_.medium(medium_id);
        const float  sig_t = med.sigma_t();

        const float t_med = sig_t > 0.f
                             ? sample_free_flight(sig_t, rng.uniform())
                             : BVH_FAR;

        if (t_med < t_hit) {
            // ── Volume scatter ──────────────────────────────────────────────
            const tinybvh::bvhvec3 x{
                ray.O.x + t_med * ray.D.x,
                ray.O.y + t_med * ray.D.y,
                ray.O.z + t_med * ray.D.z,
            };

            // IS weight: σ_s/σ_t (transmittance cancels against free-flight PDF).
            const float alb = med.sigma_s / sig_t;
            weight.x *= alb; weight.y *= alb; weight.z *= alb;

            // // Direct via NEE.
            // const auto ld = dl::nee_medium(scene_, bvh_, lights_, x, medium_id, rng);
            // L.x += weight.x * ld.x;
            // L.y += weight.y * ld.y;
            // L.z += weight.z * ld.z;

            // Indirect via photon map: σ_s · k_vol · (1/4π) · Σ φ_j
            const auto nearby = vol_tree_.radius_search(x, r_vol_);
            if (!nearby.empty()) {
                tinybvh::bvhvec3 sum{};
                for (const auto* ph : nearby) {
                    sum.x += ph->power.x;
                    sum.y += ph->power.y;
                    sum.z += ph->power.z;
                }
                const float k_vol = 3.f / (4.f * kPi * r_vol_ * r_vol_ * r_vol_);
                const float coeff = med.sigma_s * k_vol / (4.f * kPi);
                L.x += weight.x * coeff * sum.x;
                L.y += weight.y * coeff * sum.y;
                L.z += weight.z * coeff * sum.z;
            }

            ray = tinybvh::Ray{x, sample_unit_sphere(rng)};
            continue;
        }

        // ── Surface or miss ─────────────────────────────────────────────────
        if (!hit) break;

        const uint32_t         prim   = ray.hit.prim;
        const tinybvh::bvhvec3 p{
            ray.O.x + t_hit * ray.D.x,
            ray.O.y + t_hit * ray.D.y,
            ray.O.z + t_hit * ray.D.z,
        };
        const tinybvh::bvhvec3 normal = face_normal(scene_.model(), prim);
        const Bsdf&             bsdf   = scene_.bsdf(scene_.bsdf_id_at(prim));

        // MediumShell: straight pass-through, switch medium.
        if (bsdf.kind == BsdfKind::MediumShell) {
            const uint32_t in_id  = scene_.medium_in_at(prim);
            const uint32_t out_id = scene_.medium_out_at(prim);
            medium_id = (medium_id == in_id) ? out_id : in_id;
            const float ns = (normal.x*ray.D.x + normal.y*ray.D.y + normal.z*ray.D.z) >= 0.f
                                 ? 1.f : -1.f;
            ray = tinybvh::Ray{
                {p.x + kEps*normal.x*ns, p.y + kEps*normal.y*ns, p.z + kEps*normal.z*ns},
                ray.D};
            continue;
        }

        // Back-face: terminate path.
        if ((ray.D.x*normal.x + ray.D.y*normal.y + ray.D.z*normal.z) >= 0.f) break;

        if (bsdf.kind == BsdfKind::Diffuse) {
            // // Direct via NEE.
            // const auto ld = dl::nee_surface(scene_, bvh_, lights_, p, normal, bsdf, medium_id, rng);
            // L.x += weight.x * ld.x;
            // L.y += weight.y * ld.y;
            // L.z += weight.z * ld.z;

            // Indirect via photon map: (albedo/π) · k_surf · Σ φ_j
            const auto nearby = surf_tree_.radius_search(p, r_surf_);
            if (!nearby.empty()) {
                tinybvh::bvhvec3 sum{};
                for (const auto* ph : nearby) {
                    sum.x += ph->power.x;
                    sum.y += ph->power.y;
                    sum.z += ph->power.z;
                }
                const float k_surf = 1.f / (kPi * r_surf_ * r_surf_);
                L.x += weight.x * (bsdf.color.x / kPi) * k_surf * sum.x;
                L.y += weight.y * (bsdf.color.y / kPi) * k_surf * sum.y;
                L.z += weight.z * (bsdf.color.z / kPi) * k_surf * sum.z;
            }
            // Photon map owns all indirect at this vertex; continuing would double-count
            // surface photons against subsequent NEE at volume scatter events.
            break;
        }

        // Non-diffuse opaque surface (e.g. Conductor): sample BSDF and continue.
        // No Le added if the ray hits a light — photon map + NEE own all contributions.
        const BsdfSample bs = bsdf.sample(rng, ray.D, normal);
        weight.x *= bs.weight.x; weight.y *= bs.weight.y; weight.z *= bs.weight.z;

        if (bs.is_refract) {
            const uint32_t in_id  = scene_.medium_in_at(prim);
            const uint32_t out_id = scene_.medium_out_at(prim);
            medium_id = (medium_id == in_id) ? out_id : in_id;
        }

        const float ns = (normal.x*bs.dir.x + normal.y*bs.dir.y + normal.z*bs.dir.z) >= 0.f
                             ? 1.f : -1.f;
        ray = tinybvh::Ray{
            {p.x + kEps*normal.x*ns, p.y + kEps*normal.y*ns, p.z + kEps*normal.z*ns},
            bs.dir};
    }
    return L;
}

void PhotonMapper::render(std::vector<float>& out, int width, int height,
                          const PinholeCamera& cam, uint32_t start_medium, int spp) {
    out.assign(static_cast<std::size_t>(width * height * 3), 0.0f);
    const int n = std::max(1, spp);
    for (int s = 0; s < n; ++s) {
        Rng emit_rng{static_cast<uint64_t>(s) * 0x9E3779B97F4A7C15ULL};
        emit(emit_rng);

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                Rng rng{static_cast<uint64_t>(y * width + x) * static_cast<uint64_t>(n) + static_cast<uint64_t>(s)};
                tinybvh::Ray ray = cam.generate_ray(
                    static_cast<uint32_t>(x), static_cast<uint32_t>(y));
                const auto L = gather(ray, start_medium, 0, rng);
                const int idx = (y * width + x) * 3;
                out[idx + 0] += L.x;
                out[idx + 1] += L.y;
                out[idx + 2] += L.z;
            }
        }
        std::cout << "\r[PM] spp " << (s + 1) << "/" << n << std::flush;
    }
    std::cout << "\n";
    const float inv = 1.f / static_cast<float>(n);
    for (float& v : out) v *= inv;
}

void PhotonMapper::render_checkpointed(int width, int height, const PinholeCamera& cam,
                                       const std::vector<int>& checkpoints,
                                       const CheckpointFn& on_checkpoint,
                                       uint32_t start_medium) {
    if (checkpoints.empty()) return;

    const int    total = checkpoints.back();
    const size_t npix  = static_cast<size_t>(width * height * 3);
    std::vector<float> accum(npix, 0.0f);
    std::vector<float> out(npix);

    int ci = 0;
    for (int s = 0; s < total; ++s) {
        Rng emit_rng{static_cast<uint64_t>(s) * 0x9E3779B97F4A7C15ULL};
        emit(emit_rng);

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                Rng rng{(static_cast<uint64_t>(y * width + x) << 20)
                        | static_cast<uint64_t>(s)};
                tinybvh::Ray ray = cam.generate_ray(
                    static_cast<uint32_t>(x), static_cast<uint32_t>(y));
                const auto L = gather(ray, start_medium, 0, rng);
                const int idx = (y * width + x) * 3;
                accum[idx + 0] += L.x;
                accum[idx + 1] += L.y;
                accum[idx + 2] += L.z;
            }
        }

        const int done = s + 1;
        std::cout << "\r[PM] spp " << done << " / " << total << std::flush;

        if (ci < static_cast<int>(checkpoints.size()) && done == checkpoints[ci]) {
            const float inv = 1.0f / static_cast<float>(done);
            for (size_t i = 0; i < npix; ++i) out[i] = accum[i] * inv;
            on_checkpoint(done, out);
            ++ci;
        }
    }
    std::cout << "\n";
}
