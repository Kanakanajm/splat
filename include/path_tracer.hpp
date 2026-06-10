#pragma once

#include "env_light.hpp"
#include "random.hpp"
#include "scene.hpp"
#include "tiny_bvh.h"

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

struct PinholeCamera;

class PathTracer {
public:
    PathTracer(const Scene& scene, const tinybvh::BVH& bvh,
               std::vector<Light> lights, int max_depth, int spp);

    // Render into a flat float RGB buffer [width*height*3], row-major, top-left origin.
    // start_medium: medium id the camera ray begins in (0 = vacuum).
    void render(std::vector<float>& out, int width, int height,
                const PinholeCamera& cam, uint32_t start_medium = 0u) const;

    // Accumulate samples up to checkpoints.back() SPP, calling on_checkpoint
    // with a normalised RGB buffer at each value in checkpoints (must be sorted,
    // positive, and non-empty — otherwise a no-op).
    using CheckpointFn = std::function<void(int spp, const std::vector<float>&)>;
    void render_checkpointed(int width, int height, const PinholeCamera& cam,
                             const std::vector<int>& checkpoints,
                             const CheckpointFn& on_checkpoint,
                             uint32_t start_medium = 0u) const;

private:
    tinybvh::bvhvec3 Li(tinybvh::Ray ray, uint32_t medium_id, Rng& rng) const;

    // NEE from a surface point p with oriented normal n.
    tinybvh::bvhvec3 nee_surface(const tinybvh::bvhvec3& p,
                                  const tinybvh::bvhvec3& n,
                                  const Bsdf& bsdf,
                                  uint32_t medium_id, Rng& rng) const;

    // NEE from a medium scatter point p inside medium_id.
    tinybvh::bvhvec3 nee_medium(const tinybvh::bvhvec3& p,
                                 uint32_t medium_id, Rng& rng) const;

    const Scene&        scene_;
    const tinybvh::BVH& bvh_;
    std::vector<Light>  lights_;
    std::unordered_map<uint32_t, size_t> prim_to_area_light_;  // prim → index into lights_
    int                 max_depth_;
    int                 spp_;
};
