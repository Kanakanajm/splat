#include "ray_model.hpp"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <iostream>
#include <utility>

static const std::string kEmptyName;

RayModel::RayModel(std::vector<tinybvh::bvhvec4> tris,
                   std::vector<uint32_t>         tri_instance,
                   uint32_t                      instance_count)
    : tris_(std::move(tris)),
      tri_instance_(std::move(tri_instance)),
      instance_count_(instance_count) {}

RayModel::RayModel(const std::string& path) {
    Assimp::Importer importer;
    // Smooth normals only across edges narrower than 89°: sphere facets get
    // interpolated, but 90° box edges stay sharp (no cross-corner averaging).
    importer.SetPropertyFloat(AI_CONFIG_PP_GSN_MAX_SMOOTHING_ANGLE, 89.0f);
    const aiScene* scene = importer.ReadFile(
        path,
        aiProcess_Triangulate | aiProcess_PreTransformVertices | aiProcess_GenSmoothNormals);

    if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode) {
        std::cerr << "ERROR::ASSIMP::" << importer.GetErrorString() << std::endl;
        return;
    }

    instance_count_ = scene->mNumMeshes;
    instance_names_.reserve(instance_count_);
    for (uint32_t m = 0; m < scene->mNumMeshes; ++m) {
        const aiMesh* mesh = scene->mMeshes[m];
        instance_names_.emplace_back(mesh->mName.C_Str());
        const bool has_normals = mesh->HasNormals();
        for (uint32_t f = 0; f < mesh->mNumFaces; ++f) {
            const aiFace& face = mesh->mFaces[f];
            if (face.mNumIndices != 3) continue;
            for (uint32_t i = 0; i < 3; ++i) {
                const uint32_t idx = face.mIndices[i];
                const aiVector3D& v = mesh->mVertices[idx];
                tris_.emplace_back(v.x, v.y, v.z, 0.0f);
                if (has_normals) {
                    const aiVector3D& n = mesh->mNormals[idx];
                    vertex_normals_.emplace_back(n.x, n.y, n.z, 0.0f);
                }
            }
            tri_instance_.push_back(m);
        }
    }
}

const std::string& RayModel::instance_name(uint32_t id) const {
    if (id < instance_names_.size()) return instance_names_[id];
    return kEmptyName;
}

std::optional<uint32_t> RayModel::find_instance(const std::string& name) const {
    for (uint32_t i = 0; i < static_cast<uint32_t>(instance_names_.size()); ++i) {
        if (instance_names_[i] == name) return i;
    }
    return std::nullopt;
}

tinybvh::bvhvec3 RayModel::vertex_normal(uint32_t prim, uint32_t i) const {
    if (vertex_normals_.empty()) {
        const tinybvh::bvhvec3 v0{tris_[prim*3+0].x, tris_[prim*3+0].y, tris_[prim*3+0].z};
        const tinybvh::bvhvec3 v1{tris_[prim*3+1].x, tris_[prim*3+1].y, tris_[prim*3+1].z};
        const tinybvh::bvhvec3 v2{tris_[prim*3+2].x, tris_[prim*3+2].y, tris_[prim*3+2].z};
        return tinybvh::tinybvh_normalize(tinybvh::tinybvh_cross(v1 - v0, v2 - v0));
    }
    const auto& n = vertex_normals_[prim * 3 + i];
    return {n.x, n.y, n.z};
}

tinybvh::bvhvec3 RayModel::smooth_normal(uint32_t prim, float u, float v) const {
    if (vertex_normals_.empty()) {
        // Fallback: flat geometric normal (e.g. procedural test meshes).
        const tinybvh::bvhvec3 v0{tris_[prim*3+0].x, tris_[prim*3+0].y, tris_[prim*3+0].z};
        const tinybvh::bvhvec3 v1{tris_[prim*3+1].x, tris_[prim*3+1].y, tris_[prim*3+1].z};
        const tinybvh::bvhvec3 v2{tris_[prim*3+2].x, tris_[prim*3+2].y, tris_[prim*3+2].z};
        return tinybvh::tinybvh_normalize(tinybvh::tinybvh_cross(v1 - v0, v2 - v0));
    }
    const float w = 1.0f - u - v;
    const auto& n0 = vertex_normals_[prim * 3 + 0];
    const auto& n1 = vertex_normals_[prim * 3 + 1];
    const auto& n2 = vertex_normals_[prim * 3 + 2];
    return tinybvh::tinybvh_normalize(tinybvh::bvhvec3{
        w * n0.x + u * n1.x + v * n2.x,
        w * n0.y + u * n1.y + v * n2.y,
        w * n0.z + u * n1.z + v * n2.z});
}
