#pragma once

#include "photon.hpp"
#include "point_light.hpp"
#include "random.hpp"
#include "scene.hpp"
#include "tiny_bvh.h"

#include <cstdint>
#include <vector>

// V1 photon tracer: emit N rays from the light, then march each through the
// scene up to `max_depth` interactions. At each step it free-flight samples the
// current medium: a medium scatter (t_media < t_hit) records a long PhotonBeam
// to the medium exit and respawns isotropically; otherwise a surface hit records
// a PhotonPoint (diffuse only) and scatters via the BSDF. Vacuum (sigma_t == 0)
// never scatters in-medium. Medium switching on refraction is not wired yet, so
// the current medium stays the light's. No Russian roulette: photon power is not
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
