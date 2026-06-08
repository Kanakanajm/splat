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

}  // namespace

PathTracer::PathTracer(const Scene& scene, const tinybvh::BVH& bvh,
                       std::vector<Light> lights, int max_depth, int spp)
    : scene_(scene), bvh_(bvh), lights_(std::move(lights)),
      max_depth_(max_depth), spp_(spp) {}

tinybvh::bvhvec3 PathTracer::nee_surface(const tinybvh::bvhvec3& p,
                                          const tinybvh::bvhvec3& n,
                                          const Bsdf& bsdf,
                                          uint32_t medium_id,
                                          Rng& rng) const {
    tinybvh::bvhvec3 result{0.0f, 0.0f, 0.0f};
    const float sigma_t = scene_.medium(medium_id).sigma_t();
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

            tinybvh::Ray shadow{offset, wi};
            bvh_.Intersect(shadow);
            if (shadow.hit.t < dist - kEps) continue;

            const float Tr = sigma_t > 0.0f ? std::exp(-sigma_t * dist) : 1.0f;
            const float fac = Tr * kInvPi * cos_theta / (dist * dist);
            result.x += fac * bsdf.color.x * pl->power.x;
            result.y += fac * bsdf.color.y * pl->power.y;
            result.z += fac * bsdf.color.z * pl->power.z;

        } else if (const auto* el = std::get_if<EnvLight>(&light)) {
            // Cosine-weighted hemisphere sample: f_r*cos/pdf = bsdf.color (Lambertian)
            const tinybvh::bvhvec3 wi = sample_cosine_hemisphere(rng, n);
            tinybvh::Ray shadow{offset, wi};
            bvh_.Intersect(shadow);
            if (shadow.hit.t < BVH_FAR) continue;

            result.x += el->color.x * bsdf.color.x;
            result.y += el->color.y * bsdf.color.y;
            result.z += el->color.z * bsdf.color.z;
        }
    }
    return result;
}

tinybvh::bvhvec3 PathTracer::nee_medium(const tinybvh::bvhvec3& p,
                                         uint32_t medium_id,
                                         Rng& rng) const {
    tinybvh::bvhvec3 result{0.0f, 0.0f, 0.0f};
    const float sigma_t = scene_.medium(medium_id).sigma_t();

    for (const auto& light : lights_) {
        if (const auto* pl = std::get_if<PointLight>(&light)) {
            const tinybvh::bvhvec3 dir{pl->position.x - p.x,
                                       pl->position.y - p.y,
                                       pl->position.z - p.z};
            const float dist = vec_len(dir);
            if (dist < kEps) continue;
            const tinybvh::bvhvec3 wi = tinybvh::tinybvh_normalize(dir);

            tinybvh::Ray shadow{p, wi};
            bvh_.Intersect(shadow);
            if (shadow.hit.t < dist - kEps) continue;

            const float Tr  = sigma_t > 0.0f ? std::exp(-sigma_t * dist) : 1.0f;
            const float fac = Tr * kInv4Pi / (dist * dist);
            result.x += fac * pl->power.x;
            result.y += fac * pl->power.y;
            result.z += fac * pl->power.z;

        } else if (const auto* el = std::get_if<EnvLight>(&light)) {
            // Uniform sphere sample: phase/pdf = 1/(4pi) / (1/(4pi)) = 1
            const tinybvh::bvhvec3 wi = sample_unit_sphere(rng);
            tinybvh::Ray shadow{p, wi};
            bvh_.Intersect(shadow);
            if (shadow.hit.t < BVH_FAR) continue;

            result.x += el->color.x;
            result.y += el->color.y;
            result.z += el->color.z;
        }
    }
    return result;
}

tinybvh::bvhvec3 PathTracer::Li(tinybvh::Ray ray, uint32_t medium_id, Rng& rng) const {
    tinybvh::bvhvec3 Lo{0.0f, 0.0f, 0.0f};
    tinybvh::bvhvec3 weight{1.0f, 1.0f, 1.0f};

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

            // Isotropic phase: sample uniform direction
            ray = tinybvh::Ray{scatter, sample_unit_sphere(rng)};

        } else {
            // --- Surface hit or miss ---
            if (!surface_hit) {
                for (const auto& l : lights_)
                    if (const auto* el = std::get_if<EnvLight>(&l)) {
                        Lo.x += weight.x * el->color.x;
                        Lo.y += weight.y * el->color.y;
                        Lo.z += weight.z * el->color.z;
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

            const Bsdf& bsdf = scene_.bsdf_at(prim);
            if (bsdf.kind == BsdfKind::Diffuse) {
                const tinybvh::bvhvec3 ld = nee_surface(p, oriented_n, bsdf, medium_id, rng);
                Lo.x += weight.x * ld.x;
                Lo.y += weight.y * ld.y;
                Lo.z += weight.z * ld.z;
            }

            const BsdfSample bs = bsdf.sample(rng, ray.D, normal);
            weight.x *= bs.weight.x; weight.y *= bs.weight.y; weight.z *= bs.weight.z;

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
