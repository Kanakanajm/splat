#include "photon_mapper.hpp"

#include "bsdf.hpp"
#include "medium.hpp"
#include "photon_tracer.hpp"
#include "ray_camera.hpp"
#include "sampling.hpp"

#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>

namespace {
constexpr float kPi = 3.14159265358979f;
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

    for (int d = depth; d < max_cam_depth_; ++d) {
        bvh_.Intersect(ray);
        const float t_hit = ray.hit.t;
        const bool  hit   = t_hit < BVH_FAR;

        const auto&  med   = scene_.medium(medium_id);
        const float  sig_t = med.sigma_t();

        // Free-flight sampling: vacuum never scatters.
        const float t_med = sig_t > 0.f
                             ? sample_free_flight(sig_t, rng.uniform())
                             : BVH_FAR;

        if (t_med < t_hit) {
            // ── Volume scatter ──────────────────────────────────────────────
            const float Tr = std::exp(-sig_t * t_med);
            weight.x *= Tr; weight.y *= Tr; weight.z *= Tr;

            const tinybvh::bvhvec3 x{
                ray.O.x + t_med * ray.D.x,
                ray.O.y + t_med * ray.D.y,
                ray.O.z + t_med * ray.D.z,
            };

            // L_vol = σ_s · k_vol · p_isotropic · Σ φ_j
            // k_vol = 3/(4π r³),  p_isotropic = 1/(4π)
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

            // Attenuate by single-scatter albedo for continuation.
            const float alb = med.sigma_s / sig_t;
            weight.x *= alb; weight.y *= alb; weight.z *= alb;

            // Continue with a new isotropic direction.
            const tinybvh::bvhvec3 nd = sample_unit_sphere(rng);
            ray = tinybvh::Ray{x, nd};
            continue;
        }

        // ── Surface or miss ─────────────────────────────────────────────────
        if (!hit) break;

        // Apply transmittance from last origin to surface.
        if (sig_t > 0.f) {
            const float Tr = std::exp(-sig_t * t_hit);
            weight.x *= Tr; weight.y *= Tr; weight.z *= Tr;
        }

        const uint32_t prim = ray.hit.prim;
        const Bsdf&    bsdf = scene_.bsdf(scene_.bsdf_id_at(prim));
        if (bsdf.kind != BsdfKind::Diffuse) break;

        const tinybvh::bvhvec3 p{
            ray.O.x + t_hit * ray.D.x,
            ray.O.y + t_hit * ray.D.y,
            ray.O.z + t_hit * ray.D.z,
        };

        // L_surf = f_r · k_surf · Σ φ_j   where f_r = albedo/π, k_surf = 1/(π r²)
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
        break;  // stop at first diffuse surface
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
