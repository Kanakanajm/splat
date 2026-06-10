#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "bsdf.hpp"
#include "medium.hpp"
#include "photon_mapper.hpp"
#include "point_light.hpp"
#include "ray_model.hpp"
#include "scene.hpp"

#include <vector>

namespace {

std::vector<tinybvh::bvhvec4> make_box(tinybvh::bvhvec3 lo, tinybvh::bvhvec3 hi) {
    const tinybvh::bvhvec3 c[8] = {
        {lo.x,lo.y,lo.z},{hi.x,lo.y,lo.z},{hi.x,hi.y,lo.z},{lo.x,hi.y,lo.z},
        {lo.x,lo.y,hi.z},{hi.x,lo.y,hi.z},{hi.x,hi.y,hi.z},{lo.x,hi.y,hi.z},
    };
    const int faces[6][4] = {
        {0,1,2,3},{5,4,7,6},{4,0,3,7},{1,5,6,2},{4,5,1,0},{3,2,6,7},
    };
    std::vector<tinybvh::bvhvec4> v;
    v.reserve(36);
    auto push = [&](const tinybvh::bvhvec3& p){ v.emplace_back(p.x,p.y,p.z,0.f); };
    for (const auto& f : faces) {
        push(c[f[0]]); push(c[f[1]]); push(c[f[2]]);
        push(c[f[0]]); push(c[f[2]]); push(c[f[3]]);
    }
    return v;
}

}  // namespace

// ─── Emit pass ────────────────────────────────────────────────────────────────

TEST_CASE("PhotonMapper: emit stores surface photons for diffuse scene",
          "[photon_mapper][emit]") {
    RayModel model{make_box({-1.f,-1.f,-1.f},{1.f,1.f,1.f}),
                   std::vector<uint32_t>(12u,0u), 1u};
    tinybvh::BVH bvh;
    bvh.Build(model.triangles().data(), model.triangle_count());

    Scene scene{model};  // default BSDF is Diffuse
    PointLight light{{0.f,0.f,0.f}};

    PhotonMapper mapper{scene, bvh, {Light{light}}, /*n_photons=*/10000,
                        /*r_surf=*/0.1f, /*r_vol=*/0.1f, /*max_cam_depth=*/4};

    REQUIRE(mapper.surf_photon_count() > 0u);
    REQUIRE(mapper.vol_photon_count()  == 0u);
}

TEST_CASE("PhotonMapper: emit stores volume photons for participating medium",
          "[photon_mapper][emit]") {
    RayModel model{make_box({-1.f,-1.f,-1.f},{1.f,1.f,1.f}),
                   std::vector<uint32_t>(12u,0u), 1u};
    tinybvh::BVH bvh;
    bvh.Build(model.triangles().data(), model.triangle_count());

    Scene scene{model};
    scene.set_medium(1u, Medium{/*sigma_s=*/10.f, /*sigma_a=*/0.f});
    PointLight light{{0.f,0.f,0.f},{1.f,1.f,1.f},/*medium_id=*/1u};

    PhotonMapper mapper{scene, bvh, {Light{light}}, /*n_photons=*/10000,
                        /*r_surf=*/0.1f, /*r_vol=*/0.1f, /*max_cam_depth=*/4};

    REQUIRE(mapper.vol_photon_count() > 0u);
}

TEST_CASE("PhotonMapper: no volume photons in vacuum scene",
          "[photon_mapper][emit]") {
    RayModel model{make_box({-1.f,-1.f,-1.f},{1.f,1.f,1.f}),
                   std::vector<uint32_t>(12u,0u), 1u};
    tinybvh::BVH bvh;
    bvh.Build(model.triangles().data(), model.triangle_count());

    Scene scene{model};
    PointLight light{{0.f,0.f,0.f}};

    PhotonMapper mapper{scene, bvh, {Light{light}}, /*n_photons=*/5000,
                        /*r_surf=*/0.1f, /*r_vol=*/0.1f, /*max_cam_depth=*/4};

    REQUIRE(mapper.vol_photon_count() == 0u);
}

TEST_CASE("PhotonMapper: surface photon count scales with n_photons",
          "[photon_mapper][emit]") {
    RayModel model{make_box({-1.f,-1.f,-1.f},{1.f,1.f,1.f}),
                   std::vector<uint32_t>(12u,0u), 1u};
    tinybvh::BVH bvh;
    bvh.Build(model.triangles().data(), model.triangle_count());
    Scene scene{model};
    PointLight light{{0.f,0.f,0.f}};

    PhotonMapper small_map{scene, bvh, {Light{light}}, /*n_photons=*/1000,
                           0.1f, 0.1f, 4};
    PhotonMapper large_map{scene, bvh, {Light{light}}, /*n_photons=*/10000,
                           0.1f, 0.1f, 4};

    // More photons → more stored points (closed box, photons always hit something).
    REQUIRE(large_map.surf_photon_count() > small_map.surf_photon_count());
}
