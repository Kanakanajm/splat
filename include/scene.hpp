#pragma once

#include "bsdf.hpp"
#include "medium.hpp"
#include "photon.hpp"
#include "ray_model.hpp"

#include <cstdint>
#include <vector>

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

    const RayModel& model() const { return model_; }

    // --- OpenGL visualization (implemented in scene_gl.cpp, app-only).
    // Upload* needs a current GL context; it builds VAO/VBOs from the scene data.
    void upload_geometry();
    void draw_geometry(Shader& shader, int aov_mode, const std::vector<bool>& instance_visible) const;

    void upload_points(const std::vector<PhotonPoint>& points);
    uint32_t max_bounce_depth() const { return points_max_bounce_; }

    // aov_mode matches ViewState::PointAov ordinal. Re-uploads if filters changed.
    // bounce_filter = -1 shows all bounces; >= 0 shows only that depth.
    void draw_points(Shader& shader, int aov_mode,
                     const std::vector<bool>& instance_visible, int bounce_filter = -1);
    void upload_beams(const std::vector<PhotonBeam>& beams);
    void draw_beams(Shader& shader) const;

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

    // GL handles for the photon beam lines (0 until first upload).
    unsigned int beams_vao_         = 0;
    unsigned int beams_vbo_         = 0;
    uint32_t     beam_vertex_count_ = 0;  // 2 vertices per beam
};
