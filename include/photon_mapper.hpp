#pragma once

#include "env_light.hpp"
#include "photon.hpp"
#include "photon_map.hpp"
#include "random.hpp"
#include "scene.hpp"
#include "tiny_bvh.h"

#include <cstdint>
#include <functional>
#include <vector>

struct PinholeCamera;

class PhotonMapper {
public:
    PhotonMapper(const Scene& scene, const tinybvh::BVH_SoA& bvh,
                 std::vector<Light> lights,
                 int n_photons, float r_surf, float r_vol,
                 int max_cam_depth, int max_emit_depth = 20);

    // Render into a flat float RGB buffer [width*height*3], row-major, top-left origin.
    // Each sample emits a fresh photon map then gathers; results are averaged over spp.
    void render(std::vector<float>& out, int width, int height,
                const PinholeCamera& cam, uint32_t start_medium = 0u, int spp = 1);

    // Like render(), but saves a normalised snapshot at each SPP in checkpoints.
    using CheckpointFn = std::function<void(int spp, const std::vector<float>&)>;
    void render_checkpointed(int width, int height, const PinholeCamera& cam,
                             const std::vector<int>& checkpoints,
                             const CheckpointFn& on_checkpoint,
                             uint32_t start_medium = 0u);

    std::size_t surf_photon_count() const { return surf_tree_.size(); }
    std::size_t vol_photon_count()  const { return vol_tree_.size();  }

private:
    void emit(Rng& rng);
    tinybvh::bvhvec3 gather(tinybvh::Ray ray, uint32_t medium_id,
                             int depth, Rng& rng) const;

    const Scene&        scene_;
    const tinybvh::BVH_SoA& bvh_;
    std::vector<Light>  lights_;
    int                 n_photons_;
    float               r_surf_;
    float               r_vol_;
    int                 max_cam_depth_;
    int                 max_emit_depth_;

    PhotonKdTree<PhotonPoint>  surf_tree_;
    PhotonKdTree<VolumePhoton> vol_tree_;
};
