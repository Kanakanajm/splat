#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "tiny_bvh.h"

#include <cmath>
#include <cstdint>
#include <vector>

namespace {

using tinybvh::bvhvec3;
using tinybvh::bvhvec4;

inline void push_vertex(std::vector<bvhvec4>& out, float x, float y, float z) {
    out.emplace_back(x, y, z, 0.0f);
}

// Single triangle in the z=0 plane: (0,0,0), (1,0,0), (0,1,0).
std::vector<bvhvec4> make_triangle() {
    std::vector<bvhvec4> verts;
    push_vertex(verts, 0.0f, 0.0f, 0.0f);
    push_vertex(verts, 1.0f, 0.0f, 0.0f);
    push_vertex(verts, 0.0f, 1.0f, 0.0f);
    return verts;
}

// Axis-aligned quad in the z=0 plane, x,y in [-1, 1], tessellated as 2 triangles.
std::vector<bvhvec4> make_quad() {
    std::vector<bvhvec4> verts;
    push_vertex(verts, -1.0f, -1.0f, 0.0f);
    push_vertex(verts,  1.0f, -1.0f, 0.0f);
    push_vertex(verts,  1.0f,  1.0f, 0.0f);
    push_vertex(verts, -1.0f, -1.0f, 0.0f);
    push_vertex(verts,  1.0f,  1.0f, 0.0f);
    push_vertex(verts, -1.0f,  1.0f, 0.0f);
    return verts;
}

// UV sphere centered at origin with the given radius.
// stacks ≥ 2, slices ≥ 3. Triangles inscribe the sphere (so tessellated radius < r).
std::vector<bvhvec4> make_uv_sphere(float r, uint32_t stacks, uint32_t slices) {
    constexpr float kPi = 3.14159265358979323846f;
    std::vector<bvhvec3> ring_verts((stacks + 1) * (slices + 1));
    for (uint32_t i = 0; i <= stacks; ++i) {
        const float phi = kPi * static_cast<float>(i) / static_cast<float>(stacks);
        const float sp = std::sin(phi), cp = std::cos(phi);
        for (uint32_t j = 0; j <= slices; ++j) {
            const float theta = 2.0f * kPi * static_cast<float>(j) / static_cast<float>(slices);
            const float st = std::sin(theta), ct = std::cos(theta);
            ring_verts[i * (slices + 1) + j] = bvhvec3{r * sp * ct, r * cp, r * sp * st};
        }
    }
    std::vector<bvhvec4> verts;
    auto emit = [&](const bvhvec3& p) { push_vertex(verts, p.x, p.y, p.z); };
    for (uint32_t i = 0; i < stacks; ++i) {
        for (uint32_t j = 0; j < slices; ++j) {
            const auto& a = ring_verts[ i      * (slices + 1) + j    ];
            const auto& b = ring_verts[(i + 1) * (slices + 1) + j    ];
            const auto& c = ring_verts[(i + 1) * (slices + 1) + j + 1];
            const auto& d = ring_verts[ i      * (slices + 1) + j + 1];
            emit(a); emit(b); emit(c);
            emit(a); emit(c); emit(d);
        }
    }
    return verts;
}

tinybvh::BVH build(const std::vector<bvhvec4>& verts) {
    tinybvh::BVH bvh;
    bvh.Build(verts.data(), static_cast<uint32_t>(verts.size() / 3));
    return bvh;
}

}  // namespace

TEST_CASE("BVH intersect: single triangle hit/miss boundaries", "[bvh][triangle]") {
    auto verts = make_triangle();
    auto bvh = build(verts);

    // Interior point (0.2, 0.2, 0) — inside u+v <= 1. Ray from z=+1 along -Z must hit at t=1.
    {
        tinybvh::Ray ray{bvhvec3{0.2f, 0.2f, 1.0f}, bvhvec3{0.0f, 0.0f, -1.0f}};
        bvh.Intersect(ray);
        REQUIRE(ray.hit.t == Catch::Approx(1.0f).margin(1e-5f));
        REQUIRE(ray.hit.prim == 0u);
    }

    // Exterior point (0.8, 0.8, 0) — outside u+v <= 1. Must miss.
    {
        tinybvh::Ray ray{bvhvec3{0.8f, 0.8f, 1.0f}, bvhvec3{0.0f, 0.0f, -1.0f}};
        bvh.Intersect(ray);
        REQUIRE(ray.hit.t >= BVH_FAR);
    }

    // Pointing away (+Z) — must miss.
    {
        tinybvh::Ray ray{bvhvec3{0.2f, 0.2f, 1.0f}, bvhvec3{0.0f, 0.0f, 1.0f}};
        bvh.Intersect(ray);
        REQUIRE(ray.hit.t >= BVH_FAR);
    }
}

TEST_CASE("BVH intersect: tessellated quad hit/miss boundaries", "[bvh][quad]") {
    auto verts = make_quad();
    auto bvh = build(verts);

    // Several interior points — all must hit at t=2 (camera at z=+2 firing -Z).
    for (auto [x, y] : std::vector<std::pair<float, float>>{
             {0.0f, 0.0f}, {0.5f, 0.5f}, {-0.5f, 0.5f}, {0.9f, -0.9f}}) {
        tinybvh::Ray ray{bvhvec3{x, y, 2.0f}, bvhvec3{0.0f, 0.0f, -1.0f}};
        bvh.Intersect(ray);
        INFO("xy = (" << x << ", " << y << ")");
        REQUIRE(ray.hit.t == Catch::Approx(2.0f).margin(1e-5f));
    }

    // Just outside the quad in +X — must miss.
    {
        tinybvh::Ray ray{bvhvec3{1.1f, 0.0f, 2.0f}, bvhvec3{0.0f, 0.0f, -1.0f}};
        bvh.Intersect(ray);
        REQUIRE(ray.hit.t >= BVH_FAR);
    }

    // Parallel to the quad's plane — must miss (no intersection with a z=0 quad).
    {
        tinybvh::Ray ray{bvhvec3{0.0f, 0.0f, 2.0f}, bvhvec3{1.0f, 0.0f, 0.0f}};
        bvh.Intersect(ray);
        REQUIRE(ray.hit.t >= BVH_FAR);
    }
}

TEST_CASE("BVH intersect: tessellated unit sphere", "[bvh][sphere]") {
    constexpr float kRadius   = 1.0f;
    constexpr float kCamDist  = 5.0f;
    auto verts = make_uv_sphere(kRadius, /*stacks=*/64, /*slices=*/64);
    auto bvh = build(verts);

    // A radial ray from far away must hit at approximately (cam_dist - radius).
    // Tessellation makes the surface slightly inside the analytic sphere, so use a tolerance.
    for (auto axis : std::vector<bvhvec3>{
             {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}) {
        const bvhvec3 origin{axis.x * kCamDist, axis.y * kCamDist, axis.z * kCamDist};
        const bvhvec3 dir{-axis.x, -axis.y, -axis.z};
        tinybvh::Ray ray{origin, dir};
        bvh.Intersect(ray);
        REQUIRE(ray.hit.t == Catch::Approx(kCamDist - kRadius).margin(2e-3f));
    }

    // A ray that passes far outside the sphere must miss.
    {
        tinybvh::Ray ray{bvhvec3{kCamDist, 0.0f, 0.0f}, bvhvec3{0.0f, 1.0f, 0.0f}};
        bvh.Intersect(ray);
        REQUIRE(ray.hit.t >= BVH_FAR);
    }

    // A ray from inside the sphere outward must hit at approximately t=radius.
    {
        tinybvh::Ray ray{bvhvec3{0.0f, 0.0f, 0.0f}, bvhvec3{1.0f, 0.0f, 0.0f}};
        bvh.Intersect(ray);
        REQUIRE(ray.hit.t == Catch::Approx(kRadius).margin(2e-3f));
    }
}
