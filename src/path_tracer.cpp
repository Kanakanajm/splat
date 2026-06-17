#include "path_tracer.hpp"

#include "bsdf.hpp"
#include "medium.hpp"
#include "ray_camera.hpp"
#include "sampling.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace {

constexpr float kEps    = 1e-4f;
constexpr float kInvPi  = 1.0f / 3.14159265358979f;
constexpr float kInv4Pi = 1.0f / (4.0f * 3.14159265358979f);

tinybvh::bvhvec3 face_normal(const RayModel& model, uint32_t prim) {
    const auto& tris = model.triangles();
    const tinybvh::bvhvec3 v0{tris[prim*3+0].x, tris[prim*3+0].y, tris[prim*3+0].z};
    const tinybvh::bvhvec3 v1{tris[prim*3+1].x, tris[prim*3+1].y, tris[prim*3+1].z};
    const tinybvh::bvhvec3 v2{tris[prim*3+2].x, tris[prim*3+2].y, tris[prim*3+2].z};
    return tinybvh::tinybvh_normalize(tinybvh::tinybvh_cross(v1 - v0, v2 - v0));
}

float vec_len(const tinybvh::bvhvec3& v) {
    return std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
}

float max_component(const tinybvh::bvhvec3& v) {
    return std::max({v.x, v.y, v.z});
}

// Power heuristic (β=2) for two sampling strategies.
float power_heuristic(float pdf_a, float pdf_b) {
    const float a2 = pdf_a * pdf_a, b2 = pdf_b * pdf_b;
    return a2 / (a2 + b2);
}

}  // namespace

PathTracer::PathTracer(const Scene& scene, const tinybvh::BVH_SoA& bvh,
                       std::vector<Light> lights, int max_depth, int spp)
    : scene_(scene), bvh_(bvh), lights_(std::move(lights)),
      max_depth_(max_depth), spp_(spp) {
    for (size_t i = 0; i < lights_.size(); ++i) {
        if (const auto* al = std::get_if<AreaLight>(&lights_[i])) {
            for (uint32_t prim : al->prim_indices)
                prim_to_area_light_[prim] = i;
        }
    }
}

float PathTracer::shadow_Tr(const tinybvh::bvhvec3& origin,
                             const tinybvh::bvhvec3& wi,
                             float max_dist,
                             uint32_t medium_id) const {
    float Tr = 1.0f;
    tinybvh::bvhvec3 pos = origin;
    float remaining = max_dist;

    for (int i = 0; i < 32; ++i) {
        tinybvh::Ray ray{pos, wi};
        bvh_.Intersect(ray);
        const float hit_t = ray.hit.t;

        if (hit_t >= remaining - kEps) {
            // Reached target: accumulate final medium segment
            const float sigma_t = scene_.medium(medium_id).sigma_t();
            if (sigma_t > 0.0f)
                Tr *= std::exp(-sigma_t * remaining);
            return Tr;
        }

        const BsdfKind kind = scene_.bsdf_at(ray.hit.prim).kind;
        if (kind != BsdfKind::Dielectric && kind != BsdfKind::MediumShell)
            return 0.0f;  // opaque blocker

        // Transparent boundary: accumulate segment Tr and cross
        const float sigma_t = scene_.medium(medium_id).sigma_t();
        if (sigma_t > 0.0f)
            Tr *= std::exp(-sigma_t * hit_t);

        const uint32_t in_id  = scene_.medium_in_at(ray.hit.prim);
        const uint32_t out_id = scene_.medium_out_at(ray.hit.prim);
        medium_id = (medium_id == in_id) ? out_id : in_id;

        pos.x += (hit_t + kEps) * wi.x;
        pos.y += (hit_t + kEps) * wi.y;
        pos.z += (hit_t + kEps) * wi.z;
        remaining -= hit_t + kEps;
        if (remaining <= 0.0f) return Tr;
    }
    return Tr;  // safety: more than 32 boundaries
}

tinybvh::bvhvec3 PathTracer::nee_surface(const tinybvh::bvhvec3& p,
                                          const tinybvh::bvhvec3& n,
                                          const Bsdf& bsdf,
                                          uint32_t medium_id,
                                          Rng& rng) const {
    tinybvh::bvhvec3 result{0.0f, 0.0f, 0.0f};
    const tinybvh::bvhvec3 offset{p.x + kEps*n.x, p.y + kEps*n.y, p.z + kEps*n.z};

    for (const auto& light : lights_) {
        if (const auto* pl = std::get_if<PointLight>(&light)) {
            const tinybvh::bvhvec3 dir{pl->position.x - p.x,
                                       pl->position.y - p.y,
                                       pl->position.z - p.z};
            const float dist = vec_len(dir);
            if (dist < kEps) continue;
            const tinybvh::bvhvec3 wi = tinybvh::tinybvh_normalize(dir);

            const float cos_theta = wi.x*n.x + wi.y*n.y + wi.z*n.z;
            if (cos_theta <= 0.0f) continue;

            const float Tr = shadow_Tr(offset, wi, dist, medium_id);
            if (Tr == 0.0f) continue;

            const float fac = Tr * kInvPi * kInv4Pi * cos_theta / (dist * dist);
            result.x += fac * bsdf.color.x * pl->power.x;
            result.y += fac * bsdf.color.y * pl->power.y;
            result.z += fac * bsdf.color.z * pl->power.z;

        } else if (const auto* el = std::get_if<EnvLight>(&light)) {
            // Cosine-weighted hemisphere sample: f_r*cos/pdf = bsdf.color (Lambertian).
            // NEE and BSDF continuation share the same cosine-weighted distribution,
            // so powerHeuristic(p_light, p_bsdf) = 0.5.
            const tinybvh::bvhvec3 wi = sample_cosine_hemisphere(rng, n);
            const float Tr = shadow_Tr(offset, wi, BVH_FAR, medium_id);
            if (Tr == 0.0f) continue;

            result.x += 0.5f * Tr * el->color.x * bsdf.color.x;
            result.y += 0.5f * Tr * el->color.y * bsdf.color.y;
            result.z += 0.5f * Tr * el->color.z * bsdf.color.z;

        } else if (const auto* al = std::get_if<AreaLight>(&light)) {
            const tinybvh::bvhvec3 q   = al->sample_point(rng);
            const tinybvh::bvhvec3 dir{q.x-p.x, q.y-p.y, q.z-p.z};
            const float dist = vec_len(dir);
            if (dist < kEps) continue;
            const tinybvh::bvhvec3 wi = tinybvh::tinybvh_normalize(dir);

            const float cos_i = wi.x*n.x + wi.y*n.y + wi.z*n.z;
            if (cos_i <= 0.0f) continue;
            const float cos_l = -(wi.x*al->normal.x + wi.y*al->normal.y + wi.z*al->normal.z);
            if (cos_l <= 0.0f) continue;

            // Shadow ray aims from offset to q so the ray hits the light prim at
            // exactly t = dist_shadow, preventing false self-occlusion.
            const tinybvh::bvhvec3 dir_s{q.x-offset.x, q.y-offset.y, q.z-offset.z};
            const float dist_shadow = vec_len(dir_s);
            const float Tr = shadow_Tr(offset, tinybvh::tinybvh_normalize(dir_s),
                                       dist_shadow, medium_id);
            if (Tr == 0.0f) continue;

            const float p_light = dist * dist / (al->total_area * cos_l);
            const float p_bsdf  = cos_i * kInvPi;
            const float w       = power_heuristic(p_light, p_bsdf);
            const float fac     = w * Tr * kInvPi * cos_i / p_light;
            result.x += fac * bsdf.color.x * al->emission.x;
            result.y += fac * bsdf.color.y * al->emission.y;
            result.z += fac * bsdf.color.z * al->emission.z;
        }
    }
    return result;
}

tinybvh::bvhvec3 PathTracer::nee_medium(const tinybvh::bvhvec3& p,
                                         uint32_t medium_id,
                                         Rng& rng) const {
    tinybvh::bvhvec3 result{0.0f, 0.0f, 0.0f};

    for (const auto& light : lights_) {
        if (const auto* pl = std::get_if<PointLight>(&light)) {
            const tinybvh::bvhvec3 dir{pl->position.x - p.x,
                                       pl->position.y - p.y,
                                       pl->position.z - p.z};
            const float dist = vec_len(dir);
            if (dist < kEps) continue;
            const tinybvh::bvhvec3 wi = tinybvh::tinybvh_normalize(dir);

            const float Tr = shadow_Tr(p, wi, dist, medium_id);
            if (Tr == 0.0f) continue;

            const float fac = Tr * kInv4Pi * kInv4Pi / (dist * dist);
            result.x += fac * pl->power.x;
            result.y += fac * pl->power.y;
            result.z += fac * pl->power.z;

        } else if (const auto* el = std::get_if<EnvLight>(&light)) {
            // Uniform sphere sample: phase/pdf = 1.  Same distribution as the
            // continued path, so powerHeuristic(p_light, p_bsdf) = 0.5.
            const tinybvh::bvhvec3 wi = sample_unit_sphere(rng);
            const float Tr = shadow_Tr(p, wi, BVH_FAR, medium_id);
            if (Tr == 0.0f) continue;

            result.x += 0.5f * Tr * el->color.x;
            result.y += 0.5f * Tr * el->color.y;
            result.z += 0.5f * Tr * el->color.z;

        } else if (const auto* al = std::get_if<AreaLight>(&light)) {
            const tinybvh::bvhvec3 q   = al->sample_point(rng);
            const tinybvh::bvhvec3 dir{q.x-p.x, q.y-p.y, q.z-p.z};
            const float dist = vec_len(dir);
            if (dist < kEps) continue;
            const tinybvh::bvhvec3 wi = tinybvh::tinybvh_normalize(dir);

            const float cos_l = -(wi.x*al->normal.x + wi.y*al->normal.y + wi.z*al->normal.z);
            if (cos_l <= 0.0f) continue;

            const float Tr = shadow_Tr(p, wi, dist, medium_id);
            if (Tr == 0.0f) continue;

            const float p_light = dist * dist / (al->total_area * cos_l);
            const float p_bsdf  = kInv4Pi;
            const float w       = power_heuristic(p_light, p_bsdf);
            const float fac     = w * Tr * kInv4Pi / p_light;
            result.x += fac * al->emission.x;
            result.y += fac * al->emission.y;
            result.z += fac * al->emission.z;
        }
    }
    return result;
}

tinybvh::bvhvec3 PathTracer::Li(tinybvh::Ray ray, uint32_t medium_id, Rng& rng) const {
    tinybvh::bvhvec3 Lo{0.0f, 0.0f, 0.0f};
    tinybvh::bvhvec3 weight{1.0f, 1.0f, 1.0f};
    // prev_specular: true when the previous vertex was specular or this is the
    // camera ray.  Controls whether the BSDF-sampled ray may accumulate emitter Le.
    // prev_bsdf_pdf: solid-angle PDF of the direction that produced the current ray;
    // used in the power heuristic against the light-sampling PDF.
    bool  prev_specular  = true;
    float prev_bsdf_pdf  = 0.0f;

    for (int depth = 0; depth < max_depth_; ++depth) {
        ray.hit.t = BVH_FAR;
        bvh_.Intersect(ray);
        const float t_hit        = ray.hit.t;
        const bool  surface_hit  = t_hit < BVH_FAR;

        const float sigma_t = scene_.medium(medium_id).sigma_t();
        const float t_med   = sigma_t > 0.0f
            ? sample_free_flight(sigma_t, rng.uniform())
            : BVH_FAR;

        if (t_med < t_hit) {
            // --- Medium scatter ---
            const tinybvh::bvhvec3 scatter{ray.O.x + t_med*ray.D.x,
                                            ray.O.y + t_med*ray.D.y,
                                            ray.O.z + t_med*ray.D.z};

            const float albedo = scene_.medium(medium_id).sigma_s / sigma_t;
            weight.x *= albedo; weight.y *= albedo; weight.z *= albedo;

            const tinybvh::bvhvec3 ld = nee_medium(scatter, medium_id, rng);
            Lo.x += weight.x * ld.x;
            Lo.y += weight.y * ld.y;
            Lo.z += weight.z * ld.z;

            // Isotropic phase: sample uniform direction; PDF = 1/(4π)
            ray = tinybvh::Ray{scatter, sample_unit_sphere(rng)};
            prev_bsdf_pdf = kInv4Pi;
            prev_specular = false;

        } else {
            // --- Surface hit or miss ---
            if (!surface_hit) {
                for (const auto& l : lights_)
                    if (const auto* el = std::get_if<EnvLight>(&l)) {
                        // MIS weight for BSDF-sampled env light:
                        // p_light (cosine-weighted NEE) == prev_bsdf_pdf, so
                        // powerHeuristic(prev_bsdf_pdf, prev_bsdf_pdf) = 0.5.
                        // Specular/camera bounces skipped NEE → full weight (1.0).
                        const float w = prev_specular ? 1.0f
                                      : power_heuristic(prev_bsdf_pdf, prev_bsdf_pdf);
                        Lo.x += w * weight.x * el->color.x;
                        Lo.y += w * weight.y * el->color.y;
                        Lo.z += w * weight.z * el->color.z;
                    }
                break;
            }

            const uint32_t prim = ray.hit.prim;
            const tinybvh::bvhvec3 p{ray.O.x + t_hit*ray.D.x,
                                      ray.O.y + t_hit*ray.D.y,
                                      ray.O.z + t_hit*ray.D.z};

            const tinybvh::bvhvec3 normal = face_normal(scene_.model(), prim);
            const float orient = (normal.x*ray.D.x + normal.y*ray.D.y + normal.z*ray.D.z) < 0.0f
                ? 1.0f : -1.0f;
            const tinybvh::bvhvec3 oriented_n{normal.x*orient, normal.y*orient, normal.z*orient};

            // Emissive surface: add Le with MIS weight and terminate path.
            if (const auto it = prim_to_area_light_.find(prim); it != prim_to_area_light_.end()) {
                const auto& al = std::get<AreaLight>(lights_[it->second]);
                const float cos_l = -(ray.D.x*al.normal.x + ray.D.y*al.normal.y + ray.D.z*al.normal.z);
                if (cos_l > 0.0f) {
                    const float p_light = t_hit * t_hit / (al.total_area * cos_l);
                    const float w = prev_specular ? 1.0f : power_heuristic(prev_bsdf_pdf, p_light);
                    Lo.x += w * weight.x * al.emission.x;
                    Lo.y += w * weight.y * al.emission.y;
                    Lo.z += w * weight.z * al.emission.z;
                }
                break;
            }

            const Bsdf& bsdf = scene_.bsdf_at(prim);
            if (bsdf.kind == BsdfKind::Diffuse) {
                const tinybvh::bvhvec3 ld = nee_surface(p, oriented_n, bsdf, medium_id, rng);
                Lo.x += weight.x * ld.x;
                Lo.y += weight.y * ld.y;
                Lo.z += weight.z * ld.z;
            }

            const BsdfSample bs = bsdf.sample(rng, ray.D, normal);
            weight.x *= bs.weight.x; weight.y *= bs.weight.y; weight.z *= bs.weight.z;

            if (bsdf.kind == BsdfKind::Diffuse) {
                // Cosine-weighted hemisphere: PDF = cosθ / π
                const float cos_out = std::max(0.0f,
                    bs.dir.x * oriented_n.x +
                    bs.dir.y * oriented_n.y +
                    bs.dir.z * oriented_n.z);
                prev_bsdf_pdf = cos_out * kInvPi;
                prev_specular = false;
            } else {
                prev_bsdf_pdf = 0.0f;
                prev_specular = true;
            }

            if (bs.is_refract) {
                const uint32_t in_id  = scene_.medium_in_at(prim);
                const uint32_t out_id = scene_.medium_out_at(prim);
                medium_id = (medium_id == in_id) ? out_id : in_id;
            }

            // Russian roulette
            const float prr = std::max(0.05f, std::min(0.95f, max_component(weight)));
            if (rng.uniform() >= prr) break;
            weight.x /= prr; weight.y /= prr; weight.z /= prr;

            const float ns = (normal.x*bs.dir.x + normal.y*bs.dir.y + normal.z*bs.dir.z) >= 0.0f
                ? 1.0f : -1.0f;
            ray = tinybvh::Ray{
                {p.x + kEps*normal.x*ns, p.y + kEps*normal.y*ns, p.z + kEps*normal.z*ns},
                bs.dir};
        }
    }
    return Lo;
}

void PathTracer::render(std::vector<float>& out, int width, int height,
                         const PinholeCamera& cam, uint32_t start_medium) const {
    const size_t n = static_cast<size_t>(width * height * 3);
    out.assign(n, 0.0f);

    // Outer loop over samples so we can log per-sample progress.
    for (int s = 0; s < spp_; ++s) {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const uint64_t seed =
                    static_cast<uint64_t>(y * width + x) * static_cast<uint64_t>(spp_) + s;
                Rng rng{seed};
                tinybvh::Ray r = cam.generate_ray(
                    static_cast<uint32_t>(x), static_cast<uint32_t>(y));
                const auto lo = Li(r, start_medium, rng);
                const int idx = (y * width + x) * 3;
                out[idx + 0] += lo.x;
                out[idx + 1] += lo.y;
                out[idx + 2] += lo.z;
            }
        }
        std::cout << "\r[PT] sample " << (s + 1) << " / " << spp_ << std::flush;
    }
    std::cout << "\n";

    const float inv = 1.0f / static_cast<float>(spp_);
    for (size_t i = 0; i < n; ++i) out[i] *= inv;
}

void PathTracer::render_checkpointed(int width, int height, const PinholeCamera& cam,
                                      const std::vector<int>& checkpoints,
                                      const CheckpointFn& on_checkpoint,
                                      uint32_t start_medium) const {
    if (checkpoints.empty()) return;

    const int    total = checkpoints.back();
    const size_t n     = static_cast<size_t>(width * height * 3);
    std::vector<float> accum(n, 0.0f);
    std::vector<float> out(n);

    int ci = 0;
    for (int s = 0; s < total; ++s) {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                // Seed: high bits = pixel index, low 20 bits = sample index.
                // Intentionally independent of total SPP so checkpoints are nested.
                const uint64_t seed = (static_cast<uint64_t>(y * width + x) << 20)
                                    | static_cast<uint64_t>(s);
                Rng rng{seed};
                tinybvh::Ray r = cam.generate_ray(
                    static_cast<uint32_t>(x), static_cast<uint32_t>(y));
                const auto lo = Li(r, start_medium, rng);
                const int idx = (y * width + x) * 3;
                accum[idx + 0] += lo.x;
                accum[idx + 1] += lo.y;
                accum[idx + 2] += lo.z;
            }
        }

        const int done = s + 1;
        std::cout << "\r[PT] sample " << done << " / " << total << std::flush;

        if (ci < static_cast<int>(checkpoints.size()) && done == checkpoints[ci]) {
            const float inv = 1.0f / static_cast<float>(done);
            for (size_t i = 0; i < n; ++i) out[i] = accum[i] * inv;
            on_checkpoint(done, out);
            ++ci;
        }
    }
    std::cout << "\n";
}
