// OpenGL-side rendering for Scene. Kept in its own translation unit so the test
// target (which compiles scene.cpp but does not link glad/glfw) stays GL-free.

#include "scene.hpp"
#include "shader.hpp"
#include "kernel_texture.hpp"

#include <cstdint>
#include <glad/glad.h>

#include <array>
#include <cmath>
#include <vector>

namespace {

// Distinct color per instance id (cycles through the palette).
std::array<float, 3> instance_color(uint32_t id) {
    static const std::array<std::array<float, 3>, 6> kPalette{{
        {0.90f, 0.30f, 0.30f}, {0.30f, 0.80f, 0.40f}, {0.30f, 0.50f, 0.90f},
        {0.90f, 0.80f, 0.30f}, {0.70f, 0.40f, 0.85f}, {0.30f, 0.80f, 0.85f},
    }};
    return kPalette[id % kPalette.size()];
}

tinybvh::bvhvec3 cross3(const tinybvh::bvhvec3& a, const tinybvh::bvhvec3& b) {
    return { a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x };
}

tinybvh::bvhvec3 normalize3(const tinybvh::bvhvec3& v) {
    const float len = std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
    if (len < 1e-8f) return {0.f, 1.f, 0.f};
    return {v.x/len, v.y/len, v.z/len};
}

}  // namespace

// ---- Geometry ---------------------------------------------------------------

void Scene::upload_geometry() {
    const auto& tris = model_.triangles();
    const uint32_t tri_count = model_.triangle_count();
    const uint32_t n_inst    = model_.instance_count();

    // Count triangles per instance for range allocation.
    std::vector<uint32_t> tri_per_inst(n_inst, 0u);
    for (uint32_t t = 0; t < tri_count; ++t)
        ++tri_per_inst[model_.instance_id(t)];

    // Build instance ranges (start vertex index, vertex count).
    geom_ranges_.resize(n_inst);
    uint32_t cursor = 0;
    for (uint32_t i = 0; i < n_inst; ++i) {
        geom_ranges_[i] = { cursor, tri_per_inst[i] * 3u };
        cursor += geom_ranges_[i].count;
    }

    // Build per-instance vertex lists in order so ranges are contiguous.
    // Each triangle contributes 3 vertices; layout: [x,y,z, nx,ny,nz].
    std::vector<std::vector<float>> inst_data(n_inst);
    for (uint32_t i = 0; i < n_inst; ++i)
        inst_data[i].reserve(tri_per_inst[i] * 18u);  // 3 verts * 6 floats

    for (uint32_t t = 0; t < tri_count; ++t) {
        const tinybvh::bvhvec3 v0 = {tris[3*t+0].x, tris[3*t+0].y, tris[3*t+0].z};
        const tinybvh::bvhvec3 v1 = {tris[3*t+1].x, tris[3*t+1].y, tris[3*t+1].z};
        const tinybvh::bvhvec3 v2 = {tris[3*t+2].x, tris[3*t+2].y, tris[3*t+2].z};
        const tinybvh::bvhvec3 e0 = {v1.x-v0.x, v1.y-v0.y, v1.z-v0.z};
        const tinybvh::bvhvec3 e1 = {v2.x-v0.x, v2.y-v0.y, v2.z-v0.z};
        const tinybvh::bvhvec3 n  = normalize3(cross3(e0, e1));

        const uint32_t iid = model_.instance_id(t);
        auto& d = inst_data[iid];
        for (const auto& v : {v0, v1, v2}) {
            d.push_back(v.x); d.push_back(v.y); d.push_back(v.z);
            d.push_back(n.x); d.push_back(n.y); d.push_back(n.z);
        }
    }

    // Flatten instance data in range order into one buffer.
    std::vector<float> flat;
    flat.reserve(tri_count * 18u);
    for (uint32_t i = 0; i < n_inst; ++i)
        flat.insert(flat.end(), inst_data[i].begin(), inst_data[i].end());

    if (geom_vao_ == 0) glGenVertexArrays(1, &geom_vao_);
    if (geom_vbo_ == 0) glGenBuffers(1, &geom_vbo_);
    glBindVertexArray(geom_vao_);
    glBindBuffer(GL_ARRAY_BUFFER, geom_vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(flat.size() * sizeof(float)),
                 flat.data(), GL_STATIC_DRAW);
    constexpr GLsizei stride = 6 * sizeof(float);
    glEnableVertexAttribArray(0);  // aPos
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(1);  // aNormal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glBindVertexArray(0);
}

void Scene::draw_geometry(Shader& shader, int aov_mode, const std::vector<bool>& instance_visible) const {
    if (geom_vao_ == 0 || geom_ranges_.empty()) return;

    shader.use();
    glBindVertexArray(geom_vao_);
    // Wireframe for None and Backface; filled faces for Diffuse/Normal/Depth.
    const bool wireframe = (aov_mode == 0 || aov_mode == 4);
    glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);

    for (uint32_t i = 0; i < static_cast<uint32_t>(geom_ranges_.size()); ++i) {
        if (!instance_visible.empty() && i < instance_visible.size() && !instance_visible[i])
            continue;
        shader.setInt("instanceId", static_cast<int>(i));
        const auto& col = bsdf_table_[instance_bsdf_[i]].color;
        shader.setVec3("bsdfColor", col.x, col.y, col.z);
        const auto& r = geom_ranges_[i];
        glDrawArrays(GL_TRIANGLES, static_cast<GLint>(r.start), static_cast<GLsizei>(r.count));
    }

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glBindVertexArray(0);
}

// ---- Photon Points ----------------------------------------------------------

namespace {

// Build VBO data for a filtered subset of points.
// Layout per vertex: [x, y, z, instance_id, bsdf_kind, bounce_depth, pr, pg, pb] — 9 floats.
std::vector<float> encode_points(const std::vector<PhotonPoint>& points,
                                 const std::vector<bool>& instance_visible,
                                 int bounce_filter,
                                 float& out_power_max) {
    std::vector<float> data;
    data.reserve(points.size() * 9);
    out_power_max = 1.0f;
    for (const auto& p : points) {
        if (!instance_visible.empty() &&
            p.instance_id < instance_visible.size() &&
            !instance_visible[p.instance_id])
            continue;
        if (bounce_filter >= 0 && static_cast<int>(p.bounce_depth) != bounce_filter)
            continue;
        const float lum = std::max(p.power.x, std::max(p.power.y, p.power.z));
        if (lum > out_power_max) out_power_max = lum;
        data.push_back(p.position.x);
        data.push_back(p.position.y);
        data.push_back(p.position.z);
        data.push_back(static_cast<float>(p.instance_id));
        data.push_back(static_cast<float>(static_cast<int>(p.bsdf_id)));
        data.push_back(static_cast<float>(p.bounce_depth));
        data.push_back(p.power.x);
        data.push_back(p.power.y);
        data.push_back(p.power.z);
    }
    return data;
}

void upload_points_vbo(unsigned int& vao, unsigned int& vbo, const std::vector<float>& data) {
    if (vao == 0) glGenVertexArrays(1, &vao);
    if (vbo == 0) glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(data.size() * sizeof(float)),
                 data.data(), GL_STATIC_DRAW);
    constexpr GLsizei stride = 9 * sizeof(float);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride, (void*)(4 * sizeof(float)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride, (void*)(5 * sizeof(float)));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
    glBindVertexArray(0);
}

}  // namespace (points helpers)

void Scene::upload_points(const std::vector<PhotonPoint>& points) {
    points_cache_ = points;
    points_filter_cache_.clear();
    points_bounce_filter_cache_ = -2;
    points_max_bounce_ = 1u;
    for (const auto& p : points)
        if (p.bounce_depth > points_max_bounce_) points_max_bounce_ = p.bounce_depth;
    const auto data = encode_points(points, {}, -1, points_power_max_);
    point_count_ = static_cast<uint32_t>(data.size() / 9);
    upload_points_vbo(points_vao_, points_vbo_, data);
}

void Scene::draw_points(Shader& shader, int aov_mode,
                        const std::vector<bool>& instance_visible, int bounce_filter) {
    if (instance_visible != points_filter_cache_ ||
        bounce_filter    != points_bounce_filter_cache_) {
        points_filter_cache_        = instance_visible;
        points_bounce_filter_cache_ = bounce_filter;
        float tmp_power_max;
        const auto data = encode_points(points_cache_, instance_visible, bounce_filter,
                                        tmp_power_max);
        point_count_ = static_cast<uint32_t>(data.size() / 9);
        upload_points_vbo(points_vao_, points_vbo_, data);
    }
    shader.use();
    shader.setInt("aov_mode", aov_mode);
    shader.setFloat("maxBounce", static_cast<float>(points_max_bounce_));
    shader.setFloat("maxPower",  points_power_max_);
    glBindVertexArray(points_vao_);
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(point_count_));
    glBindVertexArray(0);
}

// ---- Photon Beams -----------------------------------------------------------

namespace {

// Layout per vertex: [x, y, z, medium_id, t, bounce_depth, length, sigma_t, pr, pg, pb] — 11 floats.
// Two vertices per beam: start (t=0) and end (t=1).
std::vector<float> encode_beams(const std::vector<PhotonBeam>& beams,
                                const std::vector<bool>& medium_visible,
                                int bounce_filter,
                                const std::vector<Medium>& mediums,
                                float& out_max_bounce, float& out_max_length) {
    std::vector<float> data;
    data.reserve(beams.size() * 22);
    out_max_bounce = 1.0f;
    out_max_length = 1.0f;
    for (const auto& b : beams) {
        if (!medium_visible.empty() &&
            b.medium_id < medium_visible.size() &&
            !medium_visible[b.medium_id])
            continue;
        if (bounce_filter >= 0 && static_cast<int>(b.bounce_depth) != bounce_filter)
            continue;
        const float dx  = b.end.x - b.start.x;
        const float dy  = b.end.y - b.start.y;
        const float dz  = b.end.z - b.start.z;
        const float len = std::sqrt(dx*dx + dy*dy + dz*dz);
        const float bd  = static_cast<float>(b.bounce_depth);
        const float mid = static_cast<float>(b.medium_id);
        const float st  = (b.medium_id < mediums.size()) ? mediums[b.medium_id].sigma_t() : 0.0f;
        if (bd  > out_max_bounce) out_max_bounce = bd;
        if (len > out_max_length) out_max_length = len;
        auto push = [&](const tinybvh::bvhvec3& p, float t) {
            data.push_back(p.x); data.push_back(p.y); data.push_back(p.z);
            data.push_back(mid); data.push_back(t); data.push_back(bd); data.push_back(len);
            data.push_back(st);
            data.push_back(b.power.x); data.push_back(b.power.y); data.push_back(b.power.z);
        };
        push(b.start, 0.0f);
        push(b.end,   1.0f);
    }
    return data;
}

void upload_beams_vbo(unsigned int& vao, unsigned int& vbo, const std::vector<float>& data) {
    if (vao == 0) glGenVertexArrays(1, &vao);
    if (vbo == 0) glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(data.size() * sizeof(float)),
                 data.data(), GL_STATIC_DRAW);
    constexpr GLsizei stride = 11 * sizeof(float);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride, (void*)(4 * sizeof(float)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride, (void*)(5 * sizeof(float)));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, stride, (void*)(7 * sizeof(float)));
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 3, GL_FLOAT, GL_FALSE, stride, (void*)(8 * sizeof(float)));
    glBindVertexArray(0);
}

}  // namespace (beam helpers)

void Scene::upload_beams(const std::vector<PhotonBeam>& beams) {
    beams_cache_ = beams;
    beams_medium_filter_cache_.clear();
    beams_bounce_filter_cache_ = -2;
    const auto data = encode_beams(beams, {}, -1, medium_table_, beam_max_bounce_, beam_max_length_);
    beam_vertex_count_ = static_cast<uint32_t>(data.size() / 11);
    upload_beams_vbo(beams_vao_, beams_vbo_, data);
}

void Scene::draw_beams(Shader& shader, int aov_mode, const std::vector<bool>& medium_visible,
                       int bounce_filter) {
    if (medium_visible != beams_medium_filter_cache_ ||
        bounce_filter  != beams_bounce_filter_cache_) {
        beams_medium_filter_cache_ = medium_visible;
        beams_bounce_filter_cache_ = bounce_filter;
        // Use temporaries so beam_max_bounce_/beam_max_length_ (set from all beams
        // in upload_beams) are never overwritten by a filtered subset's max.
        float tmp_bounce, tmp_length;
        const auto data = encode_beams(beams_cache_, medium_visible, bounce_filter,
                                       medium_table_, tmp_bounce, tmp_length);
        beam_vertex_count_ = static_cast<uint32_t>(data.size() / 11);
        upload_beams_vbo(beams_vao_, beams_vbo_, data);
    }
    shader.use();
    shader.setInt("aov_mode", aov_mode);
    shader.setFloat("maxBounce", beam_max_bounce_);
    shader.setFloat("maxLength", beam_max_length_);
    glBindVertexArray(beams_vao_);
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(beam_vertex_count_));
    glBindVertexArray(0);
}

// ---- Splat pass -------------------------------------------------------------

namespace {

// Layout per vertex (15 floats = 60 bytes):
//   [0..2]   position    → shader location 0
//   [3..5]   normal      → shader location 1
//   [6..8]   incoming_dir → shader location 2 (reserved for glossy BRDF)
//   [9..11]  power       → shader location 3
//   [12..14] bsdf_color  → shader location 4
std::vector<float> encode_splats(const std::vector<PhotonPoint>& points,
                                 const std::vector<Bsdf>& bsdf_table) {
    std::vector<float> data;
    data.reserve(points.size() * 15);
    for (const auto& p : points) {
        const auto& col = bsdf_table[p.bsdf_id < bsdf_table.size() ? p.bsdf_id : 0].color;
        data.push_back(p.position.x);    data.push_back(p.position.y);    data.push_back(p.position.z);
        data.push_back(p.normal.x);      data.push_back(p.normal.y);      data.push_back(p.normal.z);
        data.push_back(p.incoming_dir.x);data.push_back(p.incoming_dir.y);data.push_back(p.incoming_dir.z);
        data.push_back(p.power.x);       data.push_back(p.power.y);       data.push_back(p.power.z);
        data.push_back(col.x);           data.push_back(col.y);           data.push_back(col.z);
    }
    return data;
}

void upload_splats_vbo(unsigned int& vao, unsigned int& vbo, const std::vector<float>& data) {
    if (vao == 0) glGenVertexArrays(1, &vao);
    if (vbo == 0) glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(data.size() * sizeof(float)),
                 data.data(), GL_STATIC_DRAW);
    constexpr GLsizei stride = 15 * sizeof(float);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3  * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)(6  * sizeof(float)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)(9  * sizeof(float)));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, stride, (void*)(12 * sizeof(float)));
    glBindVertexArray(0);
}

unsigned int upload_kernel_tex() {
    constexpr int kSize = 64;
    const auto data = build_kernel_texture(kSize);
    unsigned int tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, kSize, kSize, 0, GL_RED, GL_FLOAT, data.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    const float border[] = {0.0f, 0.0f, 0.0f, 0.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

}  // namespace (splat helpers)

void Scene::upload_splats(const std::vector<PhotonPoint>& points) {
    const auto data = encode_splats(points, bsdf_table_);
    splat_vertex_count_ = static_cast<uint32_t>(data.size() / 15);
    upload_splats_vbo(splats_vao_, splats_vbo_, data);
    if (splat_kernel_tex_ == 0)
        splat_kernel_tex_ = upload_kernel_tex();
}

void Scene::draw_splats(Shader& shader, float h) {
    if (splats_vao_ == 0 || splat_vertex_count_ == 0) return;

    shader.use();
    shader.setFloat("h", h);
    shader.setInt("kernelTex", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, splat_kernel_tex_);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-1.0f, -1.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glDepthMask(GL_FALSE);

    glBindVertexArray(splats_vao_);
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(splat_vertex_count_));
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDepthFunc(GL_LESS);
    glDisable(GL_CULL_FACE);
    glBindTexture(GL_TEXTURE_2D, 0);
}
