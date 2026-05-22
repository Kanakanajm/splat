// OpenGL-side rendering for Scene. Kept in its own translation unit so the test
// target (which compiles scene.cpp but does not link glad/glfw) stays GL-free.

#include "scene.hpp"
#include "shader.hpp"

#include <cstdint>
#include <glad/glad.h>

#include <array>
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

// Distinct color per medium id (id 0 = vacuum is never drawn as a beam).
std::array<float, 3> medium_color(uint32_t id) {
    static const std::array<std::array<float, 3>, 5> kPalette{{
        {0.60f, 0.60f, 0.60f}, {0.40f, 0.85f, 0.95f}, {0.95f, 0.55f, 0.30f},
        {0.65f, 0.95f, 0.45f}, {0.85f, 0.45f, 0.95f},
    }};
    return kPalette[id % kPalette.size()];
}

// Build (or refresh) a VAO/VBO from interleaved [x,y,z, r,g,b] vertex data.
void upload_interleaved(unsigned int& vao, unsigned int& vbo, const std::vector<float>& data) {
    if (vao == 0) glGenVertexArrays(1, &vao);
    if (vbo == 0) glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(data.size() * sizeof(float)),
                 data.data(), GL_STATIC_DRAW);
    constexpr GLsizei stride = 6 * sizeof(float);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glBindVertexArray(0);
}

}  // namespace

void Scene::upload_points(const std::vector<PhotonPoint>& points) {
    // Interleaved [x, y, z, r, g, b] per point; color by instance id.
    std::vector<float> data;
    data.reserve(points.size() * 6);
    for (const auto& p : points) {
        const auto c = instance_color(p.instance_id);
        data.push_back(p.position.x);
        data.push_back(p.position.y);
        data.push_back(p.position.z);
        data.push_back(c[0]);
        data.push_back(c[1]);
        data.push_back(c[2]);
    }
    point_count_ = static_cast<uint32_t>(points.size());
    upload_interleaved(points_vao_, points_vbo_, data);
}

void Scene::draw_points(Shader& shader) const {
    shader.use();
    glBindVertexArray(points_vao_);
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(point_count_));
    glBindVertexArray(0);
}

void Scene::upload_beams(const std::vector<PhotonBeam>& beams) {
    // Two vertices (start, end) per beam; color both by medium id.
    std::vector<float> data;
    data.reserve(beams.size() * 12);
    auto push = [&](const tinybvh::bvhvec3& p, const std::array<float, 3>& c) {
        data.push_back(p.x);
        data.push_back(p.y);
        data.push_back(p.z);
        data.push_back(c[0]);
        data.push_back(c[1]);
        data.push_back(c[2]);
    };
    unsigned int count = 0;
    for (const auto& b : beams) {
        if (b.medium_id == 2) 
        {
            count++;
const auto c = medium_color(b.medium_id);
            push(b.start, c);
            push(b.end, c);
        }
                   

    }
    beam_vertex_count_ = static_cast<uint32_t>(count * 2);
    upload_interleaved(beams_vao_, beams_vbo_, data);
}

void Scene::draw_beams(Shader& shader) const {
    shader.use();
    glBindVertexArray(beams_vao_);
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(beam_vertex_count_));
    glBindVertexArray(0);
}
