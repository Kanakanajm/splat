#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "bsdf.hpp"
#include "medium.hpp"
#include "photon_mapper.hpp"
#include "point_light.hpp"
#include "ray_camera.hpp"
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

// ─── Surface gather (Phase 1: vacuum, diffuse) ────────────────────────────────

// Note: PhotonTracer only stores photons at bounce_depth > 0 (indirect hits).
// Tests use a closed box so photons always bounce and get stored on the walls.

TEST_CASE("PhotonMapper: render returns non-zero pixels for indirect lit scene",
          "[photon_mapper][gather][surface]") {
    RayModel model{make_box({-1.f,-1.f,-1.f},{1.f,1.f,1.f}),
                   std::vector<uint32_t>(12u,0u), 1u};
    tinybvh::BVH bvh;
    bvh.Build(model.triangles().data(), model.triangle_count());
    Scene scene{model};
    PointLight light{{0.f,0.f,0.f}};

    // Camera inside the box, looking at the -z wall which has indirect photons.
    PinholeCamera cam;
    cam.eye    = {0.f, 0.f, 0.5f};
    cam.target = {0.f, 0.f, -1.f};
    cam.up     = {0.f, 1.f, 0.f};
    cam.fov_y  = 0.8f;
    cam.width  = 8;
    cam.height = 8;

    PhotonMapper mapper{scene, bvh, {Light{light}},
                        /*n_photons=*/50000, /*r_surf=*/0.3f, /*r_vol=*/0.1f,
                        /*max_cam_depth=*/4};

    std::vector<float> out;
    mapper.render(out, 8, 8, cam);

    REQUIRE(out.size() == 8u * 8u * 3u);
    int nonzero = 0;
    for (float v : out) if (v > 0.f) ++nonzero;
    REQUIRE(nonzero > 0);
}

TEST_CASE("PhotonMapper: render returns zero for non-diffuse (conductor) surface",
          "[photon_mapper][gather][surface]") {
    RayModel model{make_box({-1.f,-1.f,-1.f},{1.f,1.f,1.f}),
                   std::vector<uint32_t>(12u,0u), 1u};
    tinybvh::BVH bvh;
    bvh.Build(model.triangles().data(), model.triangle_count());
    Scene scene{model};
    scene.set_instance_bsdf(0u, 1u);
    scene.set_bsdf(1u, Bsdf{BsdfKind::Conductor, 1.0f});
    PointLight light{{0.f,0.f,0.f}};

    PinholeCamera cam;
    cam.eye    = {0.f, 0.f, 0.5f};
    cam.target = {0.f, 0.f, -1.f};
    cam.up     = {0.f, 1.f, 0.f};
    cam.fov_y  = 0.8f;
    cam.width  = 4;
    cam.height = 4;

    PhotonMapper mapper{scene, bvh, {Light{light}},
                        /*n_photons=*/5000, /*r_surf=*/0.2f, /*r_vol=*/0.1f,
                        /*max_cam_depth=*/4};

    std::vector<float> out;
    mapper.render(out, 4, 4, cam);

    for (float v : out) REQUIRE(v == 0.f);
}

// ─── Volume gather (Phase 2: medium only) ─────────────────────────────────────

TEST_CASE("PhotonMapper: volume gather returns non-zero inside participating medium",
          "[photon_mapper][gather][volume]") {
    // Dense medium fills the box, walls are conductor so only volume contributes.
    RayModel model{make_box({-1.f,-1.f,-1.f},{1.f,1.f,1.f}),
                   std::vector<uint32_t>(12u,0u), 1u};
    tinybvh::BVH bvh;
    bvh.Build(model.triangles().data(), model.triangle_count());
    Scene scene{model};
    scene.set_instance_bsdf(0u, 1u);
    scene.set_bsdf(1u, Bsdf{BsdfKind::Conductor, 1.0f});  // walls opaque, non-diffuse
    scene.set_medium(1u, Medium{/*sigma_s=*/5.f, /*sigma_a=*/0.f});
    PointLight light{{0.f,0.f,0.f},{1.f,1.f,1.f},/*medium_id=*/1u};

    PinholeCamera cam;
    cam.eye    = {0.f, 0.f, 0.f};
    cam.target = {0.f, 0.f, -1.f};
    cam.up     = {0.f, 1.f, 0.f};
    cam.fov_y  = 0.8f;
    cam.width  = 4;
    cam.height = 4;

    PhotonMapper mapper{scene, bvh, {Light{light}},
                        /*n_photons=*/50000, /*r_surf=*/0.1f, /*r_vol=*/0.3f,
                        /*max_cam_depth=*/8};

    std::vector<float> out;
    mapper.render(out, 4, 4, cam, /*start_medium=*/1u);

    int nonzero = 0;
    for (float v : out) if (v > 0.f) ++nonzero;
    REQUIRE(nonzero > 0);
}

// ─── Combined gather (Phase 3: diffuse + medium) ─────────────────────────────

TEST_CASE("PhotonMapper: combined scene has volume + surface contribution",
          "[photon_mapper][gather][combined]") {
    // Diffuse walls + participating medium.
    // Camera is inside the medium: some rays scatter in medium (volume),
    // others reach the wall (surface).
    RayModel model{make_box({-1.f,-1.f,-1.f},{1.f,1.f,1.f}),
                   std::vector<uint32_t>(12u,0u), 1u};
    tinybvh::BVH bvh;
    bvh.Build(model.triangles().data(), model.triangle_count());
    Scene scene{model};  // default Diffuse walls
    scene.set_medium(1u, Medium{/*sigma_s=*/2.f, /*sigma_a=*/0.f});
    PointLight light{{0.f,0.f,0.f},{1.f,1.f,1.f},/*medium_id=*/1u};

    PinholeCamera cam;
    cam.eye    = {0.f, 0.f, 0.f};
    cam.target = {0.f, 0.f, -1.f};
    cam.up     = {0.f, 1.f, 0.f};
    cam.fov_y  = 0.8f;
    cam.width  = 4;
    cam.height = 4;

    // Enough photons for both trees to be populated.
    PhotonMapper mapper{scene, bvh, {Light{light}},
                        /*n_photons=*/50000, /*r_surf=*/0.3f, /*r_vol=*/0.3f,
                        /*max_cam_depth=*/8};

    REQUIRE(mapper.surf_photon_count() > 0u);
    REQUIRE(mapper.vol_photon_count()  > 0u);

    std::vector<float> out;
    mapper.render(out, 4, 4, cam, /*start_medium=*/1u);

    int nonzero = 0;
    for (float v : out) if (v > 0.f) ++nonzero;
    REQUIRE(nonzero > 0);
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
