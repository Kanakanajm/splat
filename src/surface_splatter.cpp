#include "surface_splatter.hpp"

#include "photon_tracer.hpp"
#include "random.hpp"
#include "shader.hpp"

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cstdint>
#include <iostream>
#include <vector>

SurfaceSplatter::SurfaceSplatter(Scene& scene, const tinybvh::BVH& bvh,
                                 std::vector<Light> lights,
                                 int n_photons_total, int photons_per_pass,
                                 int max_emit_depth)
    : scene_(scene), bvh_(bvh), lights_(std::move(lights))
    , n_photons_total_(n_photons_total), photons_per_pass_(photons_per_pass)
    , max_emit_depth_(max_emit_depth)
{}

void SurfaceSplatter::render(std::vector<float>& out, int width, int height,
                              const PinholeCamera& cam,
                              Shader& geom_shader, Shader& splat_shader, Shader& face_normal_shader,
                              unsigned int fbo,
                              float h, float exposure) {
    const auto N_total    = static_cast<uint32_t>(n_photons_total_);
    const auto N_per_pass = static_cast<uint32_t>(photons_per_pass_);
    const uint32_t K = (N_total + N_per_pass - 1u) / N_per_pass;

    const float aspect = (height > 0) ? static_cast<float>(width) / static_cast<float>(height) : 1.0f;
    const glm::mat4 view = glm::lookAt(
        glm::vec3(cam.eye.x,    cam.eye.y,    cam.eye.z),
        glm::vec3(cam.target.x, cam.target.y, cam.target.z),
        glm::vec3(cam.up.x,     cam.up.y,     cam.up.z));
    const glm::mat4 proj  = glm::perspective(cam.fov_y, aspect, 0.1f, 100.0f);
    const glm::mat4 model(1.0f);

    scene_.render_face_normal(face_normal_shader, view, proj, width, height);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Depth pre-pass: fill depth buffer with opaque geometry so splats behind
    // boxes/walls are culled by GL_LEQUAL in draw_splats.
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    geom_shader.use();
    geom_shader.setMat4("projection", proj);
    geom_shader.setMat4("view",       view);
    geom_shader.setMat4("model",      model);
    geom_shader.setInt("aov_mode",    1);  // Diffuse → filled polygons
    scene_.draw_geometry(geom_shader, 1, {}, /*skip_media=*/true);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    splat_shader.use();
    splat_shader.setMat4("projection", proj);
    splat_shader.setMat4("view",       view);
    splat_shader.setMat4("model",      model);

    PhotonTracer tracer(scene_, bvh_, lights_);

    for (uint32_t pass = 0; pass < K; ++pass) {
        Rng rng(static_cast<uint64_t>(pass) * 0x9E3779B97F4A7C15ULL);
        tracer.trace(N_per_pass, static_cast<uint32_t>(max_emit_depth_), rng, N_total);
        scene_.upload_splats(tracer.points());
        tracer.release_cpu_memory();

        scene_.draw_splats(splat_shader, h, exposure, /*aov_mode=*/0);
        glFinish();
        std::cout << "\r[SS] pass " << (pass + 1) << "/" << K << std::flush;
    }
    std::cout << "\n";

    // Read back RGBA32F, flip Y (GL origin is bottom-left), strip alpha.
    const size_t npix = static_cast<size_t>(width * height);
    std::vector<float> raw(npix * 4);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_FLOAT, raw.data());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    out.resize(npix * 3);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int src = ((height - 1 - y) * width + x) * 4;
            const int dst = (y * width + x) * 3;
            out[dst + 0] = raw[src + 0];
            out[dst + 1] = raw[src + 1];
            out[dst + 2] = raw[src + 2];
        }
    }
}

void SurfaceSplatter::render_checkpointed(int width, int height,
                                           const PinholeCamera& cam,
                                           Shader& geom_shader, Shader& splat_shader, Shader& face_normal_shader,
                                           unsigned int fbo,
                                           const std::vector<int>& checkpoints,
                                           const CheckpointFn& on_checkpoint,
                                           float h, float exposure) {
    if (checkpoints.empty()) return;

    const auto N_total    = static_cast<uint32_t>(n_photons_total_);
    const auto N_per_pass = static_cast<uint32_t>(photons_per_pass_);
    const int  K          = static_cast<int>((N_total + N_per_pass - 1u) / N_per_pass);

    const float aspect = (height > 0) ? static_cast<float>(width) / static_cast<float>(height) : 1.0f;
    const glm::mat4 view  = glm::lookAt(
        glm::vec3(cam.eye.x,    cam.eye.y,    cam.eye.z),
        glm::vec3(cam.target.x, cam.target.y, cam.target.z),
        glm::vec3(cam.up.x,     cam.up.y,     cam.up.z));
    const glm::mat4 proj  = glm::perspective(cam.fov_y, aspect, 0.1f, 100.0f);
    const glm::mat4 model(1.0f);

    scene_.render_face_normal(face_normal_shader, view, proj, width, height);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Depth pre-pass (once).
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    geom_shader.use();
    geom_shader.setMat4("projection", proj);
    geom_shader.setMat4("view",       view);
    geom_shader.setMat4("model",      model);
    geom_shader.setInt("aov_mode",    1);
    scene_.draw_geometry(geom_shader, 1, {}, /*skip_media=*/true);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    splat_shader.use();
    splat_shader.setMat4("projection", proj);
    splat_shader.setMat4("view",       view);
    splat_shader.setMat4("model",      model);

    PhotonTracer tracer(scene_, bvh_, lights_);
    const size_t npix = static_cast<size_t>(width * height);
    std::vector<float> raw(npix * 4);
    std::vector<float> snap(npix * 3);

    int ci = 0;
    for (int pass = 1; pass <= K; ++pass) {
        Rng rng(static_cast<uint64_t>(pass - 1) * 0x9E3779B97F4A7C15ULL);
        tracer.trace(N_per_pass, static_cast<uint32_t>(max_emit_depth_), rng, N_total);
        scene_.upload_splats(tracer.points());
        tracer.release_cpu_memory();
        scene_.draw_splats(splat_shader, h, exposure, 0);
        glFinish();
        std::cout << "\r[SS] pass " << pass << "/" << K << std::flush;

        if (ci < static_cast<int>(checkpoints.size()) && pass == checkpoints[ci]) {
            glReadPixels(0, 0, width, height, GL_RGBA, GL_FLOAT, raw.data());
            for (int y = 0; y < height; ++y)
                for (int x = 0; x < width; ++x) {
                    const int src = ((height - 1 - y) * width + x) * 4;
                    const int dst = (y * width + x) * 3;
                    snap[dst]     = raw[src];
                    snap[dst + 1] = raw[src + 1];
                    snap[dst + 2] = raw[src + 2];
                }
            on_checkpoint(pass, snap);
            ++ci;
        }
    }
    std::cout << "\n";
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
