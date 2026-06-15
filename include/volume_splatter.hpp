#pragma once

#include "env_light.hpp"
#include "ray_camera.hpp"
#include "scene.hpp"
#include "tiny_bvh.h"

#include <functional>
#include <vector>

class Shader;

class VolumeSplatter {
public:
    VolumeSplatter(Scene& scene, const tinybvh::BVH& bvh,
                   std::vector<Light> lights,
                   int n_photons_total, int photons_per_pass,
                   int max_emit_depth = 20);

    // Renders medium beam splats into `out` (width×height×3 RGB, row-major, top-left origin).
    // Requires a current GL context. `accum_fbo` must be a GL_RGBA32F framebuffer.
    // Runs depth peel internally for camera-side transmittance. Targeted at medium-only scenes.
    void render(std::vector<float>& out, int width, int height,
                const PinholeCamera& cam,
                Shader& depth_peel_init_shader,
                Shader& depth_peel_shader,
                Shader& quad_shader,
                Shader& vol_splat_shader,
                unsigned int accum_fbo,
                float beam_radius = 0.05f, float exposure = 1.0f);

    using CheckpointFn = std::function<void(int pass, const std::vector<float>&)>;
    void render_checkpointed(int width, int height,
                             const PinholeCamera& cam,
                             Shader& depth_peel_init_shader,
                             Shader& depth_peel_shader,
                             Shader& quad_shader,
                             Shader& vol_splat_shader,
                             unsigned int accum_fbo,
                             const std::vector<int>& checkpoints,
                             const CheckpointFn& on_checkpoint,
                             float beam_radius = 0.05f, float exposure = 1.0f);

private:
    Scene&              scene_;
    const tinybvh::BVH& bvh_;
    std::vector<Light>  lights_;
    int                 n_photons_total_;
    int                 photons_per_pass_;
    int                 max_emit_depth_;
};
