#pragma once

#include "photon.hpp"
#include "point_light.hpp"
#include "random.hpp"
#include "scene.hpp"
#include "tiny_bvh.h"

#include <cstdint>
#include <vector>

// V1 photon tracer: emit N rays from the light, then bounce each through the
// scene up to `max_depth` diffuse reflections (vacuum only — no media yet).
// A PhotonPoint is recorded at every diffuse surface hit; non-diffuse surfaces
// scatter without storing a point. No Russian roulette: photon power is not yet
// tracked, so paths terminate only at the hard depth cap or on a miss.
class PhotonTracer {
public:
    PhotonTracer(const Scene& scene, const tinybvh::BVH& bvh, const PointLight& light)
        : scene_(scene), bvh_(bvh), light_(light) {}

    void trace(uint32_t photon_count, uint32_t max_depth, Rng& rng);

    const std::vector<PhotonPoint>& points() const { return points_; }
    const std::vector<PhotonBeam>&  beams()  const { return beams_;  }

private:
    const Scene&             scene_;
    const tinybvh::BVH&      bvh_;
    const PointLight&        light_;
    std::vector<PhotonPoint> points_;
    std::vector<PhotonBeam>  beams_;
};
