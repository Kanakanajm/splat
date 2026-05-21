#pragma once

#include "tiny_bvh.h"

#include <cstdint>
#include <string>
#include <vector>

class RayModel {
public:
    explicit RayModel(const std::string& path);

    // Build directly from in-memory data (no assimp). Useful for tests and
    // procedurally generated scenes.
    RayModel(std::vector<tinybvh::bvhvec4> tris,
             std::vector<uint32_t>         tri_instance,
             uint32_t                      instance_count);

    const std::vector<tinybvh::bvhvec4>& triangles() const { return tris_; }
    uint32_t triangle_count() const { return static_cast<uint32_t>(tri_instance_.size()); }

    uint32_t instance_id(uint32_t prim) const { return tri_instance_[prim]; }
    uint32_t instance_count() const { return instance_count_; }

private:
    std::vector<tinybvh::bvhvec4> tris_;          // 3 * triangle_count entries, tinybvh "fat triangle" layout
    std::vector<uint32_t>         tri_instance_;  // size == triangle_count
    uint32_t                      instance_count_ = 0;
};
