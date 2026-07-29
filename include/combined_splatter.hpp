#pragma once

#include "env_light.hpp"
#include "ray_camera.hpp"
#include "scene.hpp"
#include "tiny_bvh.h"

#include <functional>
#include <vector>

class Shader;

class CombinedSplatter {
public:
    CombinedSplatter(Scene& scene, const tinybvh::BVH_SoA& bvh,
                     std::vector<Light> lights,
                     int n_photons_total, int photons_per_pass,
                     int max_emit_depth = 20);

    // Renders combined surface + medium photon splats into `out`.
    // Performs depth peel (for T_cam), opaque geometry depth pre-pass, background
    // quad, then per-pass traces feeding both draw_splats and draw_beams.
    void render(std::vector<float>& out, int width, int height,
                const PinholeCamera& cam,
                Shader& geom_shader,
                Shader& depth_peel_init_shader,
                Shader& depth_peel_shader,
                Shader& quad_shader,
                Shader& splat_shader,
                Shader& vol_splat_shader,
                Shader& surface_id_shader,
                unsigned int accum_fbo,
                float h = 0.01f, float beam_radius = 0.05f, float exposure = 1.0f);

    using CheckpointFn = std::function<void(int pass, const std::vector<float>&)>;
    // should_cancel is polled once per pass; returning true stops the render
    // (checkpoints already delivered remain valid).
    using CancelFn = std::function<bool()>;
    void render_checkpointed(int width, int height,
                             const PinholeCamera& cam,
                             Shader& geom_shader,
                             Shader& depth_peel_init_shader,
                             Shader& depth_peel_shader,
                             Shader& quad_shader,
                             Shader& splat_shader,
                             Shader& vol_splat_shader,
                             Shader& surface_id_shader,
                             unsigned int accum_fbo,
                             const std::vector<int>& checkpoints,
                             const CheckpointFn& on_checkpoint,
                             float h = 0.01f, float beam_radius = 0.05f, float exposure = 1.0f,
                             const CancelFn& should_cancel = {});

private:
    Scene&              scene_;
    const tinybvh::BVH_SoA& bvh_;
    std::vector<Light>  lights_;
    int                 n_photons_total_;
    int                 photons_per_pass_;
    int                 max_emit_depth_;
};
