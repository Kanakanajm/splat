#include "photon_mapper.hpp"

#include "photon_tracer.hpp"
#include "ray_camera.hpp"

PhotonMapper::PhotonMapper(const Scene& scene, const tinybvh::BVH& bvh,
                           std::vector<Light> lights,
                           int n_photons, float r_surf, float r_vol, int max_cam_depth)
    : scene_(scene), bvh_(bvh), lights_(std::move(lights))
    , n_photons_(n_photons), r_surf_(r_surf), r_vol_(r_vol)
    , max_cam_depth_(max_cam_depth)
{
    Rng rng{0xCAFEBABEu};
    emit(rng);
}

void PhotonMapper::emit(Rng& rng) {
    PhotonTracer tracer{scene_, bvh_, lights_};
    tracer.trace(static_cast<uint32_t>(n_photons_), /*max_depth=*/20, rng);
    surf_tree_.build(tracer.points());
    vol_tree_.build(tracer.vol_points());
}

tinybvh::bvhvec3 PhotonMapper::gather(tinybvh::Ray /*ray*/, uint32_t /*medium_id*/,
                                       int /*depth*/, Rng& /*rng*/) const {
    return {0.0f, 0.0f, 0.0f};
}

void PhotonMapper::render(std::vector<float>& out, int width, int height,
                          const PinholeCamera& /*cam*/, uint32_t /*start_medium*/) const {
    out.assign(static_cast<std::size_t>(width * height * 3), 0.0f);
}
