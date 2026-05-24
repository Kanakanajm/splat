// Image-based render tests.
//
// Each test renders the same primitive scene twice:
//   1. via tinybvh (the path under test)
//   2. via a closed-form analytic intersector (the reference)
// Both buffers are dumped to ${TEST_OUTPUT_DIR} as P6 PPM files for visual
// inspection, then compared per-channel with a small tolerance.

#include <catch2/catch_test_macros.hpp>

#include "image_buffer.hpp"
#include "tiny_bvh.h"
#include "ray_camera.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace {

using tinybvh::bvhvec3;
using tinybvh::bvhvec4;

// --- geometry builders (duplicated from bvh_intersect_test.cpp; extract later if a
//     third user appears) -------------------------------------------------------

void push_vertex(std::vector<bvhvec4>& out, float x, float y, float z) {
    out.emplace_back(x, y, z, 0.0f);
}

std::vector<bvhvec4> make_triangle() {
    std::vector<bvhvec4> v;
    push_vertex(v, -0.5f, -0.5f, 0.0f);
    push_vertex(v,  0.5f, -0.5f, 0.0f);
    push_vertex(v,  0.0f,  0.5f, 0.0f);
    return v;
}

std::vector<bvhvec4> make_quad() {
    std::vector<bvhvec4> v;
    push_vertex(v, -1.0f, -1.0f, 0.0f);
    push_vertex(v,  1.0f, -1.0f, 0.0f);
    push_vertex(v,  1.0f,  1.0f, 0.0f);
    push_vertex(v, -1.0f, -1.0f, 0.0f);
    push_vertex(v,  1.0f,  1.0f, 0.0f);
    push_vertex(v, -1.0f,  1.0f, 0.0f);
    return v;
}

std::vector<bvhvec4> make_uv_sphere(float r, uint32_t stacks, uint32_t slices) {
    constexpr float kPi = 3.14159265358979323846f;
    std::vector<bvhvec3> ring((stacks + 1) * (slices + 1));
    for (uint32_t i = 0; i <= stacks; ++i) {
        const float phi = kPi * static_cast<float>(i) / static_cast<float>(stacks);
        const float sp = std::sin(phi), cp = std::cos(phi);
        for (uint32_t j = 0; j <= slices; ++j) {
            const float th = 2.0f * kPi * static_cast<float>(j) / static_cast<float>(slices);
            ring[i * (slices + 1) + j] = bvhvec3{r * sp * std::cos(th), r * cp, r * sp * std::sin(th)};
        }
    }
    std::vector<bvhvec4> verts;
    auto emit = [&](const bvhvec3& p) { push_vertex(verts, p.x, p.y, p.z); };
    for (uint32_t i = 0; i < stacks; ++i) {
        for (uint32_t j = 0; j < slices; ++j) {
            const auto& a = ring[ i      * (slices + 1) + j    ];
            const auto& b = ring[(i + 1) * (slices + 1) + j    ];
            const auto& c = ring[(i + 1) * (slices + 1) + j + 1];
            const auto& d = ring[ i      * (slices + 1) + j + 1];
            emit(a); emit(b); emit(c);
            emit(a); emit(c); emit(d);
        }
    }
    return verts;
}

// --- analytic intersectors -----------------------------------------------------

// Möller–Trumbore. Returns true on hit; sets t (along D), barycentric (u,v).
bool ray_triangle(const bvhvec3& O, const bvhvec3& D,
                  const bvhvec3& v0, const bvhvec3& v1, const bvhvec3& v2,
                  float& t, float& u, float& v) {
    const bvhvec3 e1 = v1 - v0;
    const bvhvec3 e2 = v2 - v0;
    const bvhvec3 h  = tinybvh::tinybvh_cross(D, e2);
    const float   a  = tinybvh::tinybvh_dot(e1, h);
    if (std::fabs(a) < 1e-8f) return false;
    const float f = 1.0f / a;
    const bvhvec3 s = O - v0;
    u = f * tinybvh::tinybvh_dot(s, h);
    if (u < 0.0f || u > 1.0f) return false;
    const bvhvec3 q = tinybvh::tinybvh_cross(s, e1);
    v = f * tinybvh::tinybvh_dot(D, q);
    if (v < 0.0f || u + v > 1.0f) return false;
    t = f * tinybvh::tinybvh_dot(e2, q);
    return t > 0.0f;
}

// Ray vs axis-aligned quad: z=0, x,y in [-1,1].
bool ray_quad_xy(const bvhvec3& O, const bvhvec3& D, float& t) {
    if (std::fabs(D.z) < 1e-8f) return false;
    t = -O.z / D.z;
    if (t <= 0.0f) return false;
    const float x = O.x + t * D.x;
    const float y = O.y + t * D.y;
    return x >= -1.0f && x <= 1.0f && y >= -1.0f && y <= 1.0f;
}

// Ray vs sphere at origin, radius r. Returns near positive root; outputs normal.
bool ray_sphere(const bvhvec3& O, const bvhvec3& D, float r,
                float& t, bvhvec3& n) {
    const float b = tinybvh::tinybvh_dot(O, D);
    const float c = tinybvh::tinybvh_dot(O, O) - r * r;
    const float disc = b * b - c;
    if (disc < 0.0f) return false;
    const float sd = std::sqrt(disc);
    float tn = -b - sd;
    if (tn <= 0.0f) tn = -b + sd;
    if (tn <= 0.0f) return false;
    t = tn;
    const bvhvec3 p{O.x + t * D.x, O.y + t * D.y, O.z + t * D.z};
    const float inv_r = 1.0f / r;
    n = bvhvec3{p.x * inv_r, p.y * inv_r, p.z * inv_r};
    return true;
}

// --- shading kernels -----------------------------------------------------------

std::array<uint8_t, 3> shade_depth(float t, float near, float far) {
    const float c = std::clamp((t - near) / (far - near), 0.0f, 1.0f);
    const auto v = static_cast<uint8_t>(255.0f * (1.0f - c));
    return {v, v, v};
}

std::array<uint8_t, 3> shade_bary(float u, float v) {
    return {static_cast<uint8_t>(std::clamp(255.0f * u,         0.0f, 255.0f)),
            static_cast<uint8_t>(std::clamp(255.0f * v,         0.0f, 255.0f)),
            static_cast<uint8_t>(std::clamp(255.0f * (1.0f - u - v), 0.0f, 255.0f))};
}

std::array<uint8_t, 3> shade_normal(const bvhvec3& n) {
    return {static_cast<uint8_t>(std::clamp(255.0f * (0.5f * n.x + 0.5f), 0.0f, 255.0f)),
            static_cast<uint8_t>(std::clamp(255.0f * (0.5f * n.y + 0.5f), 0.0f, 255.0f)),
            static_cast<uint8_t>(std::clamp(255.0f * (0.5f * n.z + 0.5f), 0.0f, 255.0f))};
}

// Geometric normal from triangle verts, oriented toward the camera.
bvhvec3 tri_normal(const bvhvec3& v0, const bvhvec3& v1, const bvhvec3& v2,
                   const bvhvec3& ray_dir) {
    bvhvec3 n = tinybvh::tinybvh_normalize(
        tinybvh::tinybvh_cross(v1 - v0, v2 - v0));
    if (tinybvh::tinybvh_dot(n, ray_dir) > 0.0f) {
        n = bvhvec3{-n.x, -n.y, -n.z};
    }
    return n;
}

// Face normal from a tinybvh prim index into a flat fat-triangle vertex buffer.
bvhvec3 face_normal(const std::vector<bvhvec4>& verts, uint32_t prim, const bvhvec3& ray_dir) {
    const bvhvec3 v0{verts[prim * 3 + 0].x, verts[prim * 3 + 0].y, verts[prim * 3 + 0].z};
    const bvhvec3 v1{verts[prim * 3 + 1].x, verts[prim * 3 + 1].y, verts[prim * 3 + 1].z};
    const bvhvec3 v2{verts[prim * 3 + 2].x, verts[prim * 3 + 2].y, verts[prim * 3 + 2].z};
    return tri_normal(v0, v1, v2, ray_dir);
}

// Fake palette indexed by instance id (cycles through 4 distinct colors).
std::array<uint8_t, 3> instance_color(uint32_t id) {
    static const std::array<std::array<uint8_t, 3>, 4> kPalette{{
        {220,  80,  80},
        { 80, 200, 100},
        { 80, 120, 220},
        {220, 200,  80},
    }};
    return kPalette[id % kPalette.size()];
}

// Translate every vertex of a fat-triangle buffer.
void translate(std::vector<bvhvec4>& verts, float dx, float dy, float dz) {
    for (auto& v : verts) {
        v.x += dx;
        v.y += dy;
        v.z += dz;
    }
}

// --- compare + dump ------------------------------------------------------------

struct DiffStats {
    int      max_channel_diff = 0;
    uint64_t channels_over    = 0;
};

DiffStats compare(const ImageBuffer& a, const ImageBuffer& b, int channel_tol) {
    REQUIRE(a.width  == b.width);
    REQUIRE(a.height == b.height);
    REQUIRE(a.rgb.size() == b.rgb.size());
    DiffStats s;
    for (size_t i = 0; i < a.rgb.size(); ++i) {
        const int d = std::abs(int(a.rgb[i]) - int(b.rgb[i]));
        s.max_channel_diff = std::max(s.max_channel_diff, d);
        if (d > channel_tol) ++s.channels_over;
    }
    return s;
}

void dump_pair(const std::string& name, const ImageBuffer& bvh_img, const ImageBuffer& ref_img) {
    namespace fs = std::filesystem;
    const fs::path dir = fs::path(TEST_OUTPUT_DIR);
    fs::create_directories(dir);
    write_ppm((dir / (name + ".bvh.ppm")).string(), bvh_img);
    write_ppm((dir / (name + ".ref.ppm")).string(), ref_img);
}

constexpr uint32_t kW = 64;
constexpr uint32_t kH = 64;

}  // namespace

TEST_CASE("Render triangle: barycentric — BVH matches analytic exactly", "[render][triangle][bary]") {
    const auto verts = make_triangle();
    tinybvh::BVH bvh;
    bvh.Build(verts.data(), static_cast<uint32_t>(verts.size() / 3));

    PinholeCamera cam{
        .eye    = {0.0f, 0.0f, 2.0f},
        .target = {0.0f, 0.0f, 0.0f},
        .up     = {0.0f, 1.0f, 0.0f},
        .fov_y  = 0.6f,
        .width  = kW,
        .height = kH,
    };

    ImageBuffer bvh_img(kW, kH);
    ImageBuffer ref_img(kW, kH);

    const bvhvec3 v0{verts[0].x, verts[0].y, verts[0].z};
    const bvhvec3 v1{verts[1].x, verts[1].y, verts[1].z};
    const bvhvec3 v2{verts[2].x, verts[2].y, verts[2].z};

    for (uint32_t y = 0; y < kH; ++y) {
        for (uint32_t x = 0; x < kW; ++x) {
            tinybvh::Ray ray = cam.generate_ray(x, y);

            tinybvh::Ray probe = cam.generate_ray(x, y);
            bvh.Intersect(probe);
            if (probe.hit.t < BVH_FAR) {
                const auto c = shade_bary(probe.hit.u, probe.hit.v);
                bvh_img.set(x, y, c[0], c[1], c[2]);
            }

            float t, u, v;
            if (ray_triangle(ray.O, ray.D, v0, v1, v2, t, u, v)) {
                const auto c = shade_bary(u, v);
                ref_img.set(x, y, c[0], c[1], c[2]);
            }
        }
    }

    dump_pair("triangle_bary", bvh_img, ref_img);
    const auto d = compare(bvh_img, ref_img, /*tol=*/2);
    INFO("max_channel_diff=" << d.max_channel_diff << " channels_over=" << d.channels_over);
    REQUIRE(d.max_channel_diff <= 2);
}

TEST_CASE("Render triangle: geometric normal — BVH matches analytic exactly", "[render][triangle][normal]") {
    const auto verts = make_triangle();
    tinybvh::BVH bvh;
    bvh.Build(verts.data(), static_cast<uint32_t>(verts.size() / 3));

    PinholeCamera cam{
        .eye = {0.3f, 0.0f, 2.0f}, .target = {0.0f, 0.0f, 0.0f}, .up = {0.0f, 1.0f, 0.0f},
        .fov_y = 0.6f, .width = kW, .height = kH,
    };

    ImageBuffer bvh_img(kW, kH), ref_img(kW, kH);

    const bvhvec3 v0{verts[0].x, verts[0].y, verts[0].z};
    const bvhvec3 v1{verts[1].x, verts[1].y, verts[1].z};
    const bvhvec3 v2{verts[2].x, verts[2].y, verts[2].z};

    for (uint32_t y = 0; y < kH; ++y) {
        for (uint32_t x = 0; x < kW; ++x) {
            tinybvh::Ray ray = cam.generate_ray(x, y);
            const bvhvec3 n = tri_normal(v0, v1, v2, ray.D);

            tinybvh::Ray probe = cam.generate_ray(x, y);
            bvh.Intersect(probe);
            if (probe.hit.t < BVH_FAR) {
                const auto c = shade_normal(n);
                bvh_img.set(x, y, c[0], c[1], c[2]);
            }

            float t, u, v;
            if (ray_triangle(ray.O, ray.D, v0, v1, v2, t, u, v)) {
                const auto c = shade_normal(n);
                ref_img.set(x, y, c[0], c[1], c[2]);
            }
        }
    }

    dump_pair("triangle_normal", bvh_img, ref_img);
    const auto d = compare(bvh_img, ref_img, /*tol=*/2);
    INFO("max_channel_diff=" << d.max_channel_diff << " channels_over=" << d.channels_over);
    REQUIRE(d.max_channel_diff <= 2);
}

TEST_CASE("Render quad: depth — BVH matches analytic exactly", "[render][quad][depth]") {
    const auto verts = make_quad();
    tinybvh::BVH bvh;
    bvh.Build(verts.data(), static_cast<uint32_t>(verts.size() / 3));

    PinholeCamera cam{
        .eye = {0.0f, 0.0f, 3.0f}, .target = {0.0f, 0.0f, 0.0f}, .up = {0.0f, 1.0f, 0.0f},
        .fov_y = 0.8f, .width = kW, .height = kH,
    };

    ImageBuffer bvh_img(kW, kH), ref_img(kW, kH);

    for (uint32_t y = 0; y < kH; ++y) {
        for (uint32_t x = 0; x < kW; ++x) {
            tinybvh::Ray ray = cam.generate_ray(x, y);

            tinybvh::Ray probe = cam.generate_ray(x, y);
            bvh.Intersect(probe);
            if (probe.hit.t < BVH_FAR) {
                const auto c = shade_depth(probe.hit.t, /*near=*/2.0f, /*far=*/4.0f);
                bvh_img.set(x, y, c[0], c[1], c[2]);
            }

            float t;
            if (ray_quad_xy(ray.O, ray.D, t)) {
                const auto c = shade_depth(t, 2.0f, 4.0f);
                ref_img.set(x, y, c[0], c[1], c[2]);
            }
        }
    }

    dump_pair("quad_depth", bvh_img, ref_img);
    const auto d = compare(bvh_img, ref_img, /*tol=*/2);
    INFO("max_channel_diff=" << d.max_channel_diff << " channels_over=" << d.channels_over);
    REQUIRE(d.max_channel_diff <= 2);
}

TEST_CASE("Render sphere: depth — BVH approximates analytic within tessellation tolerance",
          "[render][sphere][depth]") {
    constexpr float    kR      = 1.0f;
    constexpr uint32_t kStacks = 64;
    constexpr uint32_t kSlices = 64;
    const auto verts = make_uv_sphere(kR, kStacks, kSlices);
    tinybvh::BVH bvh;
    bvh.Build(verts.data(), static_cast<uint32_t>(verts.size() / 3));

    PinholeCamera cam{
        .eye = {0.0f, 0.0f, 5.0f}, .target = {0.0f, 0.0f, 0.0f}, .up = {0.0f, 1.0f, 0.0f},
        .fov_y = 0.6f, .width = kW, .height = kH,
    };

    ImageBuffer bvh_img(kW, kH), ref_img(kW, kH);

    for (uint32_t y = 0; y < kH; ++y) {
        for (uint32_t x = 0; x < kW; ++x) {
            tinybvh::Ray ray = cam.generate_ray(x, y);

            tinybvh::Ray probe = cam.generate_ray(x, y);
            bvh.Intersect(probe);
            if (probe.hit.t < BVH_FAR) {
                const auto c = shade_depth(probe.hit.t, /*near=*/3.9f, /*far=*/5.1f);
                bvh_img.set(x, y, c[0], c[1], c[2]);
            }

            float t; bvhvec3 n;
            if (ray_sphere(ray.O, ray.D, kR, t, n)) {
                const auto c = shade_depth(t, 3.9f, 5.1f);
                ref_img.set(x, y, c[0], c[1], c[2]);
            }
        }
    }

    dump_pair("sphere_depth", bvh_img, ref_img);
    // Tessellation makes the BVH surface inscribed; we allow a small channel tolerance
    // plus a small fraction of pixels above it (silhouette pixels diverge by ~1 row).
    const auto d = compare(bvh_img, ref_img, /*tol=*/8);
    INFO("max_channel_diff=" << d.max_channel_diff << " channels_over=" << d.channels_over);
    REQUIRE(d.channels_over < (kW * kH * 3) / 50);  // < 2% of channels exceed tol
}

TEST_CASE("Render sphere: normal — BVH face normal vs analytic smooth normal",
          "[render][sphere][normal]") {
    constexpr float    kR      = 1.0f;
    constexpr uint32_t kStacks = 64;
    constexpr uint32_t kSlices = 64;
    const auto verts = make_uv_sphere(kR, kStacks, kSlices);
    tinybvh::BVH bvh;
    bvh.Build(verts.data(), static_cast<uint32_t>(verts.size() / 3));

    PinholeCamera cam{
        .eye = {0.0f, 0.0f, 5.0f}, .target = {0.0f, 0.0f, 0.0f}, .up = {0.0f, 1.0f, 0.0f},
        .fov_y = 0.6f, .width = kW, .height = kH,
    };

    ImageBuffer bvh_img(kW, kH), ref_img(kW, kH);

    for (uint32_t y = 0; y < kH; ++y) {
        for (uint32_t x = 0; x < kW; ++x) {
            tinybvh::Ray ray = cam.generate_ray(x, y);

            tinybvh::Ray probe = cam.generate_ray(x, y);
            bvh.Intersect(probe);
            if (probe.hit.t < BVH_FAR) {
                // Flat-shaded face normal from the hit triangle.
                const bvhvec3 n = face_normal(verts, probe.hit.prim, probe.D);
                const auto    c = shade_normal(n);
                bvh_img.set(x, y, c[0], c[1], c[2]);
            }

            float t; bvhvec3 n_smooth;
            if (ray_sphere(ray.O, ray.D, kR, t, n_smooth)) {
                const auto c = shade_normal(n_smooth);
                ref_img.set(x, y, c[0], c[1], c[2]);
            }
        }
    }

    dump_pair("sphere_normal", bvh_img, ref_img);
    // Flat-vs-smooth normal differs by up to ~half the subdivision angle per face,
    // plus silhouette pixels with hit/miss mismatch.
    const auto d = compare(bvh_img, ref_img, /*tol=*/12);
    INFO("max_channel_diff=" << d.max_channel_diff << " channels_over=" << d.channels_over);
    REQUIRE(d.channels_over < (kW * kH * 3) / 20);  // < 5% of channels exceed tol
}

TEST_CASE("Render two spheres: instance-id fake color", "[render][sphere][instance]") {
    constexpr float    kR      = 1.0f;
    constexpr uint32_t kStacks = 64;
    constexpr uint32_t kSlices = 64;
    const bvhvec3 center_a{-1.5f, 0.0f, 0.0f};
    const bvhvec3 center_b{ 1.5f, 0.0f, 0.0f};

    auto verts_a = make_uv_sphere(kR, kStacks, kSlices);
    auto verts_b = make_uv_sphere(kR, kStacks, kSlices);
    translate(verts_a, center_a.x, center_a.y, center_a.z);
    translate(verts_b, center_b.x, center_b.y, center_b.z);

    const uint32_t tris_a = static_cast<uint32_t>(verts_a.size() / 3);
    const uint32_t tris_b = static_cast<uint32_t>(verts_b.size() / 3);

    std::vector<bvhvec4> all;
    all.reserve(verts_a.size() + verts_b.size());
    all.insert(all.end(), verts_a.begin(), verts_a.end());
    all.insert(all.end(), verts_b.begin(), verts_b.end());

    std::vector<uint32_t> tri_instance(tris_a + tris_b);
    std::fill(tri_instance.begin(),                 tri_instance.begin() + tris_a, 0u);
    std::fill(tri_instance.begin() + tris_a,        tri_instance.end(),            1u);

    tinybvh::BVH bvh;
    bvh.Build(all.data(), tris_a + tris_b);

    PinholeCamera cam{
        .eye = {0.0f, 0.0f, 6.0f}, .target = {0.0f, 0.0f, 0.0f}, .up = {0.0f, 1.0f, 0.0f},
        .fov_y = 0.9f, .width = kW, .height = kH,
    };

    ImageBuffer bvh_img(kW, kH), ref_img(kW, kH);

    for (uint32_t y = 0; y < kH; ++y) {
        for (uint32_t x = 0; x < kW; ++x) {
            tinybvh::Ray ray = cam.generate_ray(x, y);

            tinybvh::Ray probe = cam.generate_ray(x, y);
            bvh.Intersect(probe);
            if (probe.hit.t < BVH_FAR) {
                const uint32_t inst = tri_instance[probe.hit.prim];
                const auto     c    = instance_color(inst);
                bvh_img.set(x, y, c[0], c[1], c[2]);
            }

            // Analytic: ray vs each sphere (translated to origin), pick the nearest hit.
            const bvhvec3 oc_a{ray.O.x - center_a.x, ray.O.y - center_a.y, ray.O.z - center_a.z};
            const bvhvec3 oc_b{ray.O.x - center_b.x, ray.O.y - center_b.y, ray.O.z - center_b.z};
            float t_a = 0.0f, t_b = 0.0f;
            bvhvec3 n_a, n_b;
            const bool hit_a = ray_sphere(oc_a, ray.D, kR, t_a, n_a);
            const bool hit_b = ray_sphere(oc_b, ray.D, kR, t_b, n_b);
            uint32_t winner = 0;
            bool any_hit = true;
            if      (hit_a && (!hit_b || t_a < t_b)) winner = 0;
            else if (hit_b)                          winner = 1;
            else                                     any_hit = false;
            if (any_hit) {
                const auto c = instance_color(winner);
                ref_img.set(x, y, c[0], c[1], c[2]);
            }
        }
    }

    dump_pair("two_spheres_instance", bvh_img, ref_img);
    // Within a sphere's interior the two paths agree exactly (palette lookup is
    // deterministic). Mismatches live in the silhouette band of each sphere.
    const auto d = compare(bvh_img, ref_img, /*tol=*/0);
    INFO("max_channel_diff=" << d.max_channel_diff << " channels_over=" << d.channels_over);
    REQUIRE(d.channels_over < (kW * kH * 3) / 20);  // < 5% of channels exceed tol
}
