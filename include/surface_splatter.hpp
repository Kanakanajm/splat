#pragma once

#include "env_light.hpp"
#include "ray_camera.hpp"
#include "scene.hpp"
#include "tiny_bvh.h"

#include <vector>

class Shader;

class SurfaceSplatter {
public:
    SurfaceSplatter(Scene& scene, const tinybvh::BVH& bvh,
                    std::vector<Light> lights,
                    int n_photons_total, int photons_per_pass,
                    int max_emit_depth = 20);

    // Renders surface photon splats into `out` (width×height×3 RGB, row-major, top-left origin).
    // Requires a current GL context. `fbo` must be an RGBA32F framebuffer with a depth attachment.
    // `geom_shader` is used for a depth pre-pass so opaque geometry occludes splats behind it.
    void render(std::vector<float>& out, int width, int height,
                const PinholeCamera& cam,
                Shader& geom_shader, Shader& splat_shader,
                unsigned int fbo,
                float h = 0.01f, float exposure = 1.0f);

private:
    Scene&              scene_;
    const tinybvh::BVH& bvh_;
    std::vector<Light>  lights_;
    int                 n_photons_total_;
    int                 photons_per_pass_;
    int                 max_emit_depth_;
};
