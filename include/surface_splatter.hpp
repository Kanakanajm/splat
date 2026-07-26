#pragma once

#include "env_light.hpp"
#include "ray_camera.hpp"
#include "scene.hpp"
#include "tiny_bvh.h"

#include <functional>
#include <vector>

class Shader;

class SurfaceSplatter {
public:
    SurfaceSplatter(Scene& scene, const tinybvh::BVH_SoA& bvh,
                    std::vector<Light> lights,
                    int n_photons_total, int photons_per_pass,
                    int max_emit_depth = 20);

    // Renders surface photon splats into `out` (width×height×3 RGB, row-major, top-left origin).
    // Requires a current GL context. `fbo` must be an RGBA32F framebuffer with a depth attachment.
    // `geom_shader` is used for a depth pre-pass so opaque geometry occludes splats behind it.
    void render(std::vector<float>& out, int width, int height,
                const PinholeCamera& cam,
                Shader& geom_shader, Shader& splat_shader, Shader& face_normal_shader,
                unsigned int fbo,
                float h = 0.01f, float exposure = 1.0f);

    // Like render(), but saves a snapshot at each pass index listed in `checkpoints`.
    // `checkpoints` values are 1-based cumulative pass counts (use generate_checkpoints).
    using CheckpointFn = std::function<void(int pass, const std::vector<float>&)>;
    // should_cancel is polled once per pass; returning true stops the render
    // (checkpoints already delivered remain valid).
    using CancelFn = std::function<bool()>;
    void render_checkpointed(int width, int height,
                             const PinholeCamera& cam,
                             Shader& geom_shader, Shader& splat_shader, Shader& face_normal_shader,
                             unsigned int fbo,
                             const std::vector<int>& checkpoints,
                             const CheckpointFn& on_checkpoint,
                             float h = 0.01f, float exposure = 1.0f,
                             const CancelFn& should_cancel = {});

private:
    Scene&              scene_;
    const tinybvh::BVH_SoA& bvh_;
    std::vector<Light>  lights_;
    int                 n_photons_total_;
    int                 photons_per_pass_;
    int                 max_emit_depth_;
};
