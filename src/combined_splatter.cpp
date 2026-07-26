#include "combined_splatter.hpp"

#include "photon_tracer.hpp"
#include "random.hpp"
#include "shader.hpp"

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cstdint>
#include <iostream>
#include <vector>

static constexpr int kMaxMedia = 16;
static constexpr int kNoAov    = 0;

CombinedSplatter::CombinedSplatter(Scene& scene, const tinybvh::BVH_SoA& bvh,
                                   std::vector<Light> lights,
                                   int n_photons_total, int photons_per_pass,
                                   int max_emit_depth)
    : scene_(scene), bvh_(bvh), lights_(std::move(lights))
    , n_photons_total_(n_photons_total), photons_per_pass_(photons_per_pass)
    , max_emit_depth_(max_emit_depth)
{}

namespace {

glm::mat4 make_view(const PinholeCamera& cam) {
    return glm::lookAt(
        glm::vec3(cam.eye.x,    cam.eye.y,    cam.eye.z),
        glm::vec3(cam.target.x, cam.target.y, cam.target.z),
        glm::vec3(cam.up.x,     cam.up.y,     cam.up.z));
}

glm::mat4 make_proj(const PinholeCamera& cam, int width, int height) {
    const float aspect = (height > 0) ? static_cast<float>(width) / height : 1.0f;
    return glm::perspective(cam.fov_y, aspect, 0.1f, 100.0f);
}

int run_depth_peel(Scene& scene, const glm::mat4& proj, const glm::mat4& view,
                   Shader& init_shader, Shader& peel_shader) {
    const glm::mat4 model(1.0f);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);

    glBindFramebuffer(GL_FRAMEBUFFER, scene.peel_fbo());

    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              scene.peel_depth_array(), 0, 0);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              scene.peel_medium_array(), 0, 0);
    glClear(GL_DEPTH_BUFFER_BIT);
    const GLuint zeroClear[4] = {0u, 0u, 0u, 0u};
    glClearBufferuiv(GL_COLOR, 0, zeroClear);

    init_shader.use();
    init_shader.setMat4("projection", proj);
    init_shader.setMat4("view",       view);
    init_shader.setMat4("model",      model);
    scene.draw_geometry_peel(init_shader);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, scene.peel_depth_array());
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D_ARRAY, scene.peel_medium_array());
    glActiveTexture(GL_TEXTURE0);

    peel_shader.use();
    peel_shader.setInt("previousDepths", 0);
    peel_shader.setInt("previousMedia",  1);
    peel_shader.setFloat("peelEpsilon",  1e-5f);

    const int max_layers = scene.peel_max_layers();
    for (int layer = 1; layer < max_layers; ++layer) {
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                  scene.peel_depth_array(), 0, layer);
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                  scene.peel_medium_array(), 0, layer);
        glClear(GL_DEPTH_BUFFER_BIT);
        glClearBufferuiv(GL_COLOR, 0, zeroClear);

        peel_shader.setMat4("projection",      proj);
        peel_shader.setMat4("view",            view);
        peel_shader.setMat4("model",           model);
        peel_shader.setInt("previousLayerIdx", layer - 1);
        scene.draw_geometry_peel(peel_shader);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return max_layers;
}

void setup_vol_splat_shader(Shader& shader,
                             const glm::mat4& proj, const glm::mat4& view,
                             const glm::mat4& inv_view_proj,
                             const glm::vec3& cam_pos, const glm::vec3& cam_dir,
                             int width, int height, int peel_layers,
                             const float sigma_t[], const float sigma_s[],
                             float beam_radius, float exposure) {
    shader.use();
    shader.setMat4("projection",       proj);
    shader.setMat4("view",             view);
    shader.setMat4("model",            glm::mat4(1.0f));
    shader.setVec3("cameraDir",        cam_dir.x, cam_dir.y, cam_dir.z);
    shader.setFloat("beamRadius",      beam_radius);
    shader.setFloat("exposure",        exposure);
    shader.setFloatArray("mediaSigmaT", sigma_t, kMaxMedia);
    shader.setFloatArray("mediaSigmaS", sigma_s, kMaxMedia);
    shader.setInt("depthMap",          0);
    shader.setInt("mediumMap",         1);
    shader.setInt("numPeelLayers",     peel_layers);
    shader.setMat4("invViewProj",      inv_view_proj);
    shader.setVec3("cameraWorldPos",   cam_pos.x, cam_pos.y, cam_pos.z);
    shader.setVec2("resolution",       static_cast<float>(width), static_cast<float>(height));
}

void readback_rgb(std::vector<float>& out, int width, int height) {
    const size_t npix = static_cast<size_t>(width * height);
    std::vector<float> raw(npix * 4);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_FLOAT, raw.data());
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

}  // namespace

// Common setup shared by render() and render_checkpointed(). Returns peel_layers.
// After this call the accum FBO is bound with the opaque depth pre-pass done and
// the background quad rendered. Peel textures are bound on units 0 and 1.
// vol_splat_shader uniforms are set.
static int common_setup(Scene& scene_, std::vector<Light>& lights_,
                        int width, int height, const PinholeCamera& cam,
                        Shader& geom_shader,
                        Shader& depth_peel_init_shader,
                        Shader& depth_peel_shader,
                        Shader& quad_shader,
                        Shader& splat_shader,
                        Shader& vol_splat_shader,
                        Shader& face_normal_shader,
                        unsigned int accum_fbo,
                        float beam_radius, float exposure) {
    const glm::mat4 view          = make_view(cam);
    const glm::mat4 proj          = make_proj(cam, width, height);
    const glm::mat4 inv_view_proj = glm::inverse(proj * view);
    const glm::vec3 cam_pos{ cam.eye.x, cam.eye.y, cam.eye.z };
    const glm::vec3 cam_dir = glm::normalize(glm::vec3{
        cam.target.x - cam.eye.x,
        cam.target.y - cam.eye.y,
        cam.target.z - cam.eye.z });

    // 0. Face ID buffer — renders per-pixel triangle IDs once (camera fixed across passes).
    scene_.render_face_normal(face_normal_shader, view, proj, width, height);

    // 1. Depth peel — builds T_cam maps used by background quad and vol_splat shader.
    const int peel_layers = run_depth_peel(scene_, proj, view,
                                           depth_peel_init_shader, depth_peel_shader);

    // 2. Collect medium coefficients.
    float sigma_t[kMaxMedia] = {};
    float sigma_s[kMaxMedia] = {};
    const uint32_t nm = std::min(scene_.medium_count(), static_cast<uint32_t>(kMaxMedia));
    for (uint32_t mi = 0; mi < nm; ++mi) {
        sigma_t[mi] = scene_.medium(mi).sigma_t();
        sigma_s[mi] = scene_.medium(mi).sigma_s;
    }

    // 3. Bind accum FBO and clear.
    glBindFramebuffer(GL_FRAMEBUFFER, accum_fbo);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 4. Opaque geometry depth pre-pass — fills main FBO depth buffer with opaque
    //    surfaces only (skip_media=true). Surface splats use GL_LEQUAL against this;
    //    beam cones use GL_LESS to avoid contributing behind opaque walls.
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    geom_shader.use();
    geom_shader.setMat4("projection", proj);
    geom_shader.setMat4("view",       view);
    geom_shader.setMat4("model",      glm::mat4(1.0f));
    geom_shader.setInt("aov_mode",    1);
    scene_.draw_geometry(geom_shader, 1, {}, /*skip_media=*/true);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    // 5. Bind peel textures — shared by background quad and vol_splat shader.
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, scene_.peel_depth_array());
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D_ARRAY, scene_.peel_medium_array());
    glActiveTexture(GL_TEXTURE0);

    // 6. Background quad — attenuated environment (T_cam * L_bg).
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);

    glm::vec3 bg_color{0.0f};
    for (const auto& l : lights_)
        if (const auto* el = std::get_if<EnvLight>(&l))
            { bg_color = {el->color.x, el->color.y, el->color.z}; break; }

    quad_shader.use();
    quad_shader.setInt("depthMap",        0);
    quad_shader.setInt("mediumMap",       1);
    quad_shader.setInt("numPeelLayers",   peel_layers);
    quad_shader.setMat4("invViewProj",    inv_view_proj);
    quad_shader.setVec3("cameraWorldPos", cam_pos.x, cam_pos.y, cam_pos.z);
    quad_shader.setVec2("resolution",     static_cast<float>(width), static_cast<float>(height));
    quad_shader.setFloatArray("mediaSigmaT", sigma_t, kMaxMedia);
    quad_shader.setVec3("bgColor",        bg_color.x, bg_color.y, bg_color.z);

    unsigned int empty_vao;
    glGenVertexArrays(1, &empty_vao);
    glBindVertexArray(empty_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
    glDeleteVertexArrays(1, &empty_vao);

    // 7. Splat shader — camera matrices (set once, reused across all passes).
    splat_shader.use();
    splat_shader.setMat4("projection", proj);
    splat_shader.setMat4("view",       view);
    splat_shader.setMat4("model",      glm::mat4(1.0f));

    // 8. Vol splat shader — constant uniforms for all beam passes.
    setup_vol_splat_shader(vol_splat_shader, proj, view, inv_view_proj,
                           cam_pos, cam_dir, width, height, peel_layers,
                           sigma_t, sigma_s, beam_radius, exposure);

    return peel_layers;
}

// Draw one pass worth of splats + beams, both already uploaded to the GPU.
// Expects the accum FBO to be bound and peel textures on units 0/1.
static void draw_pass(Scene& scene_, Shader& splat_shader, Shader& vol_splat_shader,
                      float h, float exposure) {
    // Surface photons — draw_splats manages its own depth state:
    //   enables GL_LEQUAL + polygon offset (-1,-1), additive blend, depth write OFF,
    //   then restores GL_LESS + depth write ON on exit.
    scene_.draw_splats(splat_shader, h, exposure, kNoAov);

    // Medium beams — GL_LESS depth test against the opaque surface depth buffer
    // (populated by the depth pre-pass, unchanged since depth write stays OFF).
    // This culls beam cone fragments behind opaque walls without a shader discard.
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    scene_.draw_beams(vol_splat_shader, kNoAov, {});
    glDisable(GL_BLEND);
}

void CombinedSplatter::render(std::vector<float>& out, int width, int height,
                               const PinholeCamera& cam,
                               Shader& geom_shader,
                               Shader& depth_peel_init_shader,
                               Shader& depth_peel_shader,
                               Shader& quad_shader,
                               Shader& splat_shader,
                               Shader& vol_splat_shader,
                               Shader& face_normal_shader,
                               unsigned int accum_fbo,
                               float h, float beam_radius, float exposure) {
    const auto N_total    = static_cast<uint32_t>(n_photons_total_);
    const auto N_per_pass = static_cast<uint32_t>(photons_per_pass_);
    const uint32_t K      = (N_total + N_per_pass - 1u) / N_per_pass;

    common_setup(scene_, lights_, width, height, cam,
                 geom_shader, depth_peel_init_shader, depth_peel_shader,
                 quad_shader, splat_shader, vol_splat_shader, face_normal_shader, accum_fbo,
                 beam_radius, exposure);

    PhotonTracer tracer(scene_, bvh_, lights_);

    for (uint32_t pass = 0; pass < K; ++pass) {
        Rng rng(static_cast<uint64_t>(pass) * 0x9E3779B97F4A7C15ULL);
        tracer.trace(N_per_pass, static_cast<uint32_t>(max_emit_depth_), rng, N_total);
        scene_.upload_splats(tracer.points());
        scene_.upload_beams(tracer.beams());
        tracer.release_cpu_memory();

        draw_pass(scene_, splat_shader, vol_splat_shader, h, exposure);
        glFinish();
        std::cout << "\r[PVS] pass " << (pass + 1) << "/" << K << std::flush;
    }
    std::cout << "\n";

    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);

    readback_rgb(out, width, height);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void CombinedSplatter::render_checkpointed(int width, int height,
                                            const PinholeCamera& cam,
                                            Shader& geom_shader,
                                            Shader& depth_peel_init_shader,
                                            Shader& depth_peel_shader,
                                            Shader& quad_shader,
                                            Shader& splat_shader,
                                            Shader& vol_splat_shader,
                                            Shader& face_normal_shader,
                                            unsigned int accum_fbo,
                                            const std::vector<int>& checkpoints,
                                            const CheckpointFn& on_checkpoint,
                                            float h, float beam_radius, float exposure,
                                            const CancelFn& should_cancel) {
    if (checkpoints.empty()) return;

    const auto N_total    = static_cast<uint32_t>(n_photons_total_);
    const auto N_per_pass = static_cast<uint32_t>(photons_per_pass_);
    const int  K          = static_cast<int>((N_total + N_per_pass - 1u) / N_per_pass);

    common_setup(scene_, lights_, width, height, cam,
                 geom_shader, depth_peel_init_shader, depth_peel_shader,
                 quad_shader, splat_shader, vol_splat_shader, face_normal_shader, accum_fbo,
                 beam_radius, exposure);

    PhotonTracer tracer(scene_, bvh_, lights_);
    std::vector<float> snap;

    int ci = 0;
    for (int pass = 1; pass <= K; ++pass) {
        if (should_cancel && should_cancel()) {
            std::cout << "\n[PVS] cancelled at pass " << (pass - 1) << "\n";
            break;
        }
        Rng rng(static_cast<uint64_t>(pass - 1) * 0x9E3779B97F4A7C15ULL);
        tracer.trace(N_per_pass, static_cast<uint32_t>(max_emit_depth_), rng, N_total);
        scene_.upload_splats(tracer.points());
        scene_.upload_beams(tracer.beams());
        tracer.release_cpu_memory();

        draw_pass(scene_, splat_shader, vol_splat_shader, h, exposure);
        glFinish();
        std::cout << "\r[PVS] pass " << pass << "/" << K << std::flush;

        if (ci < static_cast<int>(checkpoints.size()) && pass == checkpoints[ci]) {
            readback_rgb(snap, width, height);
            on_checkpoint(pass, snap);
            ++ci;
        }
    }
    std::cout << "\n";

    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
