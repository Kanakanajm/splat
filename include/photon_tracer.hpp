#pragma once

#include "photon.hpp"
#include "point_light.hpp"
#include "random.hpp"
#include "scene.hpp"
#include "tiny_bvh.h"

#include <cstdint>
#include <vector>

// V1 single-bounce skeleton: emit N rays from the light, BVH closest-hit, store
// each surface hit as a PhotonPoint with bsdf_id sourced from Scene. No BSDFs,
// no media, no bounces yet.
class PhotonTracer {
public:
    PhotonTracer(const Scene& scene, const tinybvh::BVH& bvh, const PointLight& light)
        : scene_(scene), bvh_(bvh), light_(light) {}

    void trace(uint32_t photon_count, Rng& rng);

    const std::vector<PhotonPoint>& points() const { return points_; }
    const std::vector<PhotonBeam>&  beams()  const { return beams_;  }

private:
    const Scene&             scene_;
    const tinybvh::BVH&      bvh_;
    const PointLight&        light_;
    std::vector<PhotonPoint> points_;
    std::vector<PhotonBeam>  beams_;
};
