#pragma once

#include "env_light.hpp"
#include "photon.hpp"
#include "photon_map.hpp"
#include "random.hpp"
#include "scene.hpp"
#include "tiny_bvh.h"

#include <cstdint>
#include <vector>

struct PinholeCamera;

class PhotonMapper {
public:
    PhotonMapper(const Scene& scene, const tinybvh::BVH& bvh,
                 std::vector<Light> lights,
                 int n_photons, float r_surf, float r_vol, int max_cam_depth);

    // Render into a flat float RGB buffer [width*height*3], row-major, top-left origin.
    void render(std::vector<float>& out, int width, int height,
                const PinholeCamera& cam, uint32_t start_medium = 0u) const;

    std::size_t surf_photon_count() const { return surf_tree_.size(); }
    std::size_t vol_photon_count()  const { return vol_tree_.size();  }

private:
    void emit(Rng& rng);
    tinybvh::bvhvec3 gather(tinybvh::Ray ray, uint32_t medium_id,
                             int depth, Rng& rng) const;

    const Scene&        scene_;
    const tinybvh::BVH& bvh_;
    std::vector<Light>  lights_;
    int                 n_photons_;
    float               r_surf_;
    float               r_vol_;
    int                 max_cam_depth_;

    PhotonKdTree<PhotonPoint>  surf_tree_;
    PhotonKdTree<VolumePhoton> vol_tree_;
};
