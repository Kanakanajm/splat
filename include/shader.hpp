#pragma once

#include <string>
#include <glm/glm.hpp>

class Shader
{
public:
    unsigned int ID;
    std::string vertexPath;
    Shader(const char* vertexPath, const char* fragmentPath);
    void use();
    // utility uniform functions
    void setBool(const std::string &name, bool value) const;
    void setInt(const std::string &name, int value) const;
    void setFloat(const std::string &name, float value) const;
    void setVec3(const std::string &name, float v0, float v1, float v2) const;
    void setMat4(const std::string &name, const glm::mat4& m) const;

private:
    // utility function for checking shader compilation/linking errors.
    void checkCompileErrors(unsigned int shader, std::string type);
};