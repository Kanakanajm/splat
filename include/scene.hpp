#pragma once

#include "bsdf.hpp"
#include "medium.hpp"
#include "photon.hpp"
#include "ray_model.hpp"

#include <cstdint>
#include <vector>
#include <glm/mat4x4.hpp>

class Shader;  // GL draw helpers take a Shader; defined in scene_gl.cpp (app only)

// Scene-side attribution layered on top of RayModel's geometry. Holds per-instance
// bsdf and medium-inside/-outside ids; defaults all to 0. The tracer looks up
// per-triangle ids via prim → instance → assigned id.
class Scene {
public:
    explicit Scene(const RayModel& model);

    void set_instance_bsdf(uint32_t instance_id, uint32_t bsdf_id);
    void set_instance_medium(uint32_t instance_id, uint32_t medium_in_id, uint32_t medium_out_id);

    uint32_t bsdf_id_at(uint32_t prim)    const;
    uint32_t medium_in_at(uint32_t prim)  const;
    uint32_t medium_out_at(uint32_t prim) const;

    // Bsdf table keyed by bsdf id. Id 0 defaults to a Diffuse bsdf; set_bsdf
    // grows the table as needed, leaving gaps as default-constructed (Diffuse).
    void set_bsdf(uint32_t bsdf_id, const Bsdf& bsdf);
    const Bsdf& bsdf(uint32_t bsdf_id) const { return bsdf_table_[bsdf_id]; }
    const Bsdf& bsdf_at(uint32_t prim) const { return bsdf_table_[bsdf_id_at(prim)]; }

    // Medium table keyed by medium id. Id 0 is vacuum; set_medium grows the
    // table as needed, leaving gaps as default-constructed (vacuum).
    void set_medium(uint32_t medium_id, const Medium& medium);
    const Medium& medium(uint32_t medium_id) const { return medium_table_[medium_id]; }
    uint32_t medium_count() const { return static_cast<uint32_t>(medium_table_.size()); }

    const RayModel& model() const { return model_; }

    // --- OpenGL visualization (implemented in scene_gl.cpp, app-only).
    // Upload* needs a current GL context; it builds VAO/VBOs from the scene data.
    void upload_geometry();
    void draw_geometry(Shader& shader, int aov_mode, const std::vector<bool>& instance_visible,
                       bool skip_media = false) const;

    void upload_points(const std::vector<PhotonPoint>& points);
    uint32_t max_bounce_depth() const { return points_max_bounce_; }

    // aov_mode matches ViewState::PointAov ordinal. Re-uploads if filters changed.
    // bounce_filter = -1 shows all bounces; >= 0 shows only that depth.
    void draw_points(Shader& shader, int aov_mode,
                     const std::vector<bool>& instance_visible, int bounce_filter = -1);
    void upload_beams(const std::vector<PhotonBeam>& beams);
    uint32_t beam_max_bounce() const { return static_cast<uint32_t>(beam_max_bounce_); }
    // aov_mode matches ViewState::BeamAov ordinal. Re-uploads if medium filter or bounce filter changed.
    // bounce_filter = -1 shows all bounces; >= 0 shows only that depth.
    void draw_beams(Shader& shader, int aov_mode, const std::vector<bool>& medium_visible,
                    int bounce_filter = -1);

    // --- Surface photon splats (kernel-density estimate on surfaces).
    void upload_splats(const std::vector<PhotonPoint>& points);
    // Renders all opaque geometry into an RGB16F texture storing world-space face normals.
    // Must be called before draw_splats to enable per-face anti-bleeding.
    void render_face_normal(Shader& shader, const glm::mat4& view, const glm::mat4& proj, int w, int h);
    // aov_mode: 0=Radiance (additive), 1=Wireframe, 2=Normal.
    // Uses face normal buffer (if render_face_normal was called) to discard fragments off the hit face.
    void draw_splats(Shader& splat_shader, float h, float exposure, int aov_mode = 0);

    // --- Depth peel (camera-side transmittance maps).
    // Allocates GL_TEXTURE_2D_ARRAY depth and medium arrays + FBO.
    void         init_depth_peel(int width, int height, int max_layers = 8);
    // Draws all scene geometry per-instance, setting mediumId uniform from instance_medium_in_.
    void         draw_geometry_peel(Shader& shader) const;
    unsigned int peel_fbo()          const { return peel_fbo_; }
    unsigned int peel_depth_array()  const { return peel_depth_array_; }
    unsigned int peel_medium_array() const { return peel_medium_array_; }
    int          peel_max_layers()   const { return peel_max_layers_; }

private:
    const RayModel&       model_;
    std::vector<uint32_t> instance_bsdf_;
    std::vector<uint32_t> instance_medium_in_;
    std::vector<uint32_t> instance_medium_out_;
    std::vector<Bsdf>     bsdf_table_;
    std::vector<Medium>   medium_table_;

    // GL handles for scene geometry (0 until first upload).
    unsigned int geom_vao_ = 0;
    unsigned int geom_vbo_ = 0;
    struct InstanceRange { uint32_t start; uint32_t count; };
    std::vector<InstanceRange> geom_ranges_;  // one per instance

    // GL handles for the photon point cloud (0 until first upload).
    unsigned int points_vao_  = 0;
    unsigned int points_vbo_  = 0;
    uint32_t     point_count_ = 0;
    std::vector<PhotonPoint> points_cache_;
    std::vector<bool>        points_filter_cache_;
    int                      points_bounce_filter_cache_ = -2;  // -2 = uninitialized
    uint32_t                 points_max_bounce_ = 4u;
    float                    points_power_max_  = 1.0f;

    // GL handles for the photon beam lines (0 until first upload).
    unsigned int beams_vao_         = 0;
    unsigned int beams_vbo_         = 0;
    uint32_t     beam_vertex_count_ = 0;  // 2 vertices per beam
    std::vector<PhotonBeam> beams_cache_;
    std::vector<bool>       beams_medium_filter_cache_;
    int                     beams_bounce_filter_cache_ = -2;  // -2 = uninitialized
    float                   beam_max_bounce_ = 1.0f;
    float                   beam_max_length_ = 1.0f;

    // GL handles for surface splats (0 until first upload).
    unsigned int splats_vao_         = 0;
    unsigned int splats_vbo_         = 0;
    uint32_t     splat_vertex_count_ = 0;
    unsigned int splat_kernel_tex_   = 0;
    struct SplatRange { uint32_t start; uint32_t count; };
    std::vector<SplatRange> splat_ranges_;

    // GL handles for face normal buffer (0 until first render_face_normal call).
    unsigned int face_normal_fbo_       = 0;
    unsigned int face_normal_tex_       = 0;   // RGB16F — world-space face normal
    unsigned int face_normal_depth_rb_  = 0;
    int          face_normal_w_         = 0;
    int          face_normal_h_         = 0;

    // GL handles for depth peel (0 until init_depth_peel).
    unsigned int peel_fbo_          = 0;
    unsigned int peel_depth_array_  = 0;
    unsigned int peel_medium_array_ = 0;
    int          peel_max_layers_   = 0;
};
