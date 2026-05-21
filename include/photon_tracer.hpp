#pragma once

#include "photon.hpp"
#include "point_light.hpp"
#include "random.hpp"
#include "ray_model.hpp"
#include "tiny_bvh.h"

#include <cstdint>
#include <vector>

// V1 single-bounce skeleton: emit N rays from the light, BVH closest-hit, store
// each surface hit as a PhotonPoint. No BSDFs, no media, no bounces yet.
// `bsdf_id` is sourced from `RayModel::instance_id(prim)` as a placeholder until
// the per-instance bsdf table is wired up in the next chunk.
class PhotonTracer {
public:
    PhotonTracer(const RayModel& model, const tinybvh::BVH& bvh, const PointLight& light)
        : model_(model), bvh_(bvh), light_(light) {}

    void trace(uint32_t photon_count, Rng& rng);

    const std::vector<PhotonPoint>& points() const { return points_; }
    const std::vector<PhotonBeam>&  beams()  const { return beams_;  }

private:
    const RayModel&          model_;
    const tinybvh::BVH&      bvh_;
    const PointLight&        light_;
    std::vector<PhotonPoint> points_;
    std::vector<PhotonBeam>  beams_;
};
