#pragma once

#include "mesh.hpp"
#include <assimp/scene.h>

class Model 
{
    public:
        Model(string const &path);
        void Draw(Shader &shader);	
    private:
        // model data
        vector<Mesh> meshes;
        string directory;
        vector<Texture> textures_loaded;

        void loadModel(string path);
        void processNode(aiNode *node, const aiScene *scene);
        Mesh processMesh(aiMesh *mesh, const aiScene *scene);
        vector<Texture> loadMaterialTextures(aiMaterial *mat, aiTextureType type, 
                                             string typeName);
};