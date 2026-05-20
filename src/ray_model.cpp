#include "ray_model.hpp"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <iostream>

RayModel::RayModel(const std::string& path) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(
        path,
        aiProcess_Triangulate | aiProcess_PreTransformVertices);

    if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode) {
        std::cerr << "ERROR::ASSIMP::" << importer.GetErrorString() << std::endl;
        return;
    }

    instance_count_ = scene->mNumMeshes;
    for (uint32_t m = 0; m < scene->mNumMeshes; ++m) {
        const aiMesh* mesh = scene->mMeshes[m];
        for (uint32_t f = 0; f < mesh->mNumFaces; ++f) {
            const aiFace& face = mesh->mFaces[f];
            if (face.mNumIndices != 3) continue;
            for (uint32_t i = 0; i < 3; ++i) {
                const aiVector3D& v = mesh->mVertices[face.mIndices[i]];
                tris_.emplace_back(v.x, v.y, v.z, 0.0f);
            }
            tri_instance_.push_back(m);
        }
    }
}
