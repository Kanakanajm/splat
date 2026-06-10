#pragma once

#include "env_light.hpp"
#include "photon.hpp"
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
    // Single-light convenience constructor.
    PhotonTracer(const Scene& scene, const tinybvh::BVH& bvh, Light light)
        : scene_(scene), bvh_(bvh), lights_({std::move(light)}) {}

    // Multi-light constructor.
    PhotonTracer(const Scene& scene, const tinybvh::BVH& bvh, std::vector<Light> lights)
        : scene_(scene), bvh_(bvh), lights_(std::move(lights)) {}

    // norm_count overrides the divisor for photon power normalization; use
    // N_total here when tracing in passes of N_per_pass so accumulated passes sum correctly.
    void trace(uint32_t photon_count, uint32_t max_depth, Rng& rng,
               uint32_t norm_count = 0);

    const std::vector<PhotonPoint>&   points()     const { return points_;     }
    const std::vector<PhotonBeam>&    beams()      const { return beams_;      }
    const std::vector<VolumePhoton>&  vol_points() const { return vol_points_; }

    void release_cpu_memory() {
        std::vector<PhotonPoint>().swap(points_);
        std::vector<PhotonBeam>().swap(beams_);
        std::vector<VolumePhoton>().swap(vol_points_);
    }

private:
    const Scene&              scene_;
    const tinybvh::BVH&       bvh_;
    std::vector<Light>        lights_;
    std::vector<PhotonPoint>  points_;
    std::vector<PhotonBeam>   beams_;
    std::vector<VolumePhoton> vol_points_;
};
