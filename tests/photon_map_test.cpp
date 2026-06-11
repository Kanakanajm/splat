#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "photon.hpp"
#include "photon_map.hpp"

#include <cmath>
#include <vector>

namespace {

PhotonPoint make_surf(float x, float y, float z) {
    PhotonPoint p{};
    p.position = {x, y, z};
    p.power    = {1.0f, 1.0f, 1.0f};
    return p;
}

VolumePhoton make_vol(float x, float y, float z) {
    VolumePhoton v{};
    v.position    = {x, y, z};
    v.incoming_dir = {0.0f, -1.0f, 0.0f};
    v.power       = {1.0f, 1.0f, 1.0f};
    return v;
}

// Brute-force reference: returns indices of pts within radius r of pos.
template <typename T>
std::vector<int> brute_force(const std::vector<T>& pts,
                              const tinybvh::bvhvec3& pos, float r) {
    std::vector<int> out;
    const float r2 = r * r;
    for (int i = 0; i < static_cast<int>(pts.size()); ++i) {
        const auto& p = pts[i].position;
        const float dx = p.x - pos.x, dy = p.y - pos.y, dz = p.z - pos.z;
        if (dx*dx + dy*dy + dz*dz <= r2)
            out.push_back(i);
    }
    return out;
}

}  // namespace

// ─── Empty / single-element edge cases ────────────────────────────────────────

TEST_CASE("PhotonKdTree: empty build returns no results", "[photon_map]") {
    PhotonKdTree<PhotonPoint> tree;
    tree.build({});
    REQUIRE(tree.empty());
    REQUIRE(tree.radius_search({0.0f, 0.0f, 0.0f}, 1e6f).empty());
}

TEST_CASE("PhotonKdTree: single point found inside radius, missed outside", "[photon_map]") {
    PhotonKdTree<PhotonPoint> tree;
    tree.build({make_surf(1.0f, 0.0f, 0.0f)});
    REQUIRE(tree.size() == 1u);

    // Query at origin with r=1.5 → should find the point (dist=1).
    auto hits = tree.radius_search({0.0f, 0.0f, 0.0f}, 1.5f);
    REQUIRE(hits.size() == 1u);
    REQUIRE(hits[0]->position.x == Catch::Approx(1.0f));

    // Query at origin with r=0.5 → too far.
    REQUIRE(tree.radius_search({0.0f, 0.0f, 0.0f}, 0.5f).empty());
}

TEST_CASE("PhotonKdTree: point exactly on sphere boundary is included", "[photon_map]") {
    PhotonKdTree<PhotonPoint> tree;
    tree.build({make_surf(1.0f, 0.0f, 0.0f)});
    // dist = 1.0, r = 1.0 → dist² == r², should be included (<=).
    REQUIRE(tree.radius_search({0.0f, 0.0f, 0.0f}, 1.0f).size() == 1u);
}

// ─── Correctness vs brute force ───────────────────────────────────────────────

TEST_CASE("PhotonKdTree: radius_search matches brute force on grid of points", "[photon_map]") {
    // 5x5x5 grid with unit spacing centered at origin.
    std::vector<PhotonPoint> pts;
    for (int x = -2; x <= 2; ++x)
        for (int y = -2; y <= 2; ++y)
            for (int z = -2; z <= 2; ++z)
                pts.push_back(make_surf(static_cast<float>(x),
                                        static_cast<float>(y),
                                        static_cast<float>(z)));

    PhotonKdTree<PhotonPoint> tree;
    tree.build(pts);  // pts passed by value; original still valid
    REQUIRE(tree.size() == pts.size());

    const struct { tinybvh::bvhvec3 pos; float r; } queries[] = {
        {{0.0f,  0.0f, 0.0f}, 1.1f},  // small ball near center
        {{0.0f,  0.0f, 0.0f}, 2.5f},  // larger ball
        {{2.0f,  2.0f, 2.0f}, 0.5f},  // corner, no neighbors
        {{-1.0f, 0.0f, 1.0f}, 1.5f},  // off-center
    };

    for (const auto& q : queries) {
        const auto expected = brute_force(pts, q.pos, q.r);
        const auto hits     = tree.radius_search(q.pos, q.r);

        // Same count.
        INFO("pos=(" << q.pos.x << "," << q.pos.y << "," << q.pos.z
             << ") r=" << q.r << " expected=" << expected.size()
             << " got=" << hits.size());
        REQUIRE(hits.size() == expected.size());

        // Every returned pointer points into the original data.
        for (const auto* h : hits) {
            bool found = false;
            for (const auto& p : pts)
                if (&p.position.x == &h->position.x) { found = true; break; }
            // Pointer is into the tree's internal copy, so check by value instead.
            (void)found;
            const float r2 = q.r * q.r;
            const float dx = h->position.x - q.pos.x;
            const float dy = h->position.y - q.pos.y;
            const float dz = h->position.z - q.pos.z;
            REQUIRE(dx*dx + dy*dy + dz*dz <= r2 + 1e-5f);
        }
    }
}

// ─── Template instantiation with VolumePhoton ─────────────────────────────────

TEST_CASE("PhotonKdTree<VolumePhoton>: radius_search matches brute force", "[photon_map]") {
    std::vector<VolumePhoton> pts;
    for (int i = 0; i < 20; ++i)
        pts.push_back(make_vol(static_cast<float>(i) * 0.5f, 0.0f, 0.0f));

    PhotonKdTree<VolumePhoton> tree;
    tree.build(pts);
    REQUIRE(tree.size() == pts.size());

    const tinybvh::bvhvec3 pos{3.0f, 0.0f, 0.0f};
    const float r = 1.1f;
    const auto expected = brute_force(pts, pos, r);
    const auto hits     = tree.radius_search(pos, r);

    REQUIRE(hits.size() == expected.size());
    for (const auto* h : hits) {
        const float dx = h->position.x - pos.x;
        REQUIRE(dx*dx <= r*r + 1e-5f);
    }
}

// ─── Large random cloud ───────────────────────────────────────────────────────

TEST_CASE("PhotonKdTree: large random cloud matches brute force at multiple query points",
          "[photon_map][large]") {
    // Deterministic pseudo-random cloud (LCG).
    std::vector<PhotonPoint> pts;
    pts.reserve(10000);
    uint32_t s = 0xABCD1234u;
    auto rnd = [&]() {
        s = s * 1664525u + 1013904223u;
        return (static_cast<float>(s >> 8) / static_cast<float>(1u << 24)) * 2.0f - 1.0f;
    };
    for (int i = 0; i < 10000; ++i)
        pts.push_back(make_surf(rnd(), rnd(), rnd()));

    PhotonKdTree<PhotonPoint> tree;
    tree.build(pts);

    const struct { tinybvh::bvhvec3 pos; float r; } queries[] = {
        {{0.0f, 0.0f, 0.0f}, 0.1f},
        {{0.5f, 0.3f, -0.4f}, 0.15f},
        {{-0.9f, 0.9f, 0.0f}, 0.05f},
    };
    for (const auto& q : queries) {
        const auto expected = brute_force(pts, q.pos, q.r);
        const auto hits     = tree.radius_search(q.pos, q.r);
        INFO("r=" << q.r << " expected=" << expected.size() << " got=" << hits.size());
        REQUIRE(hits.size() == expected.size());
    }
}
