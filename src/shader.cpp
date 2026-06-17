#include "shader.hpp"
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>

// Recursively replace #include "file" directives with the referenced file contents.
static std::string resolveIncludes(const std::string& source,
                                   const std::filesystem::path& dir,
                                   std::set<std::string>& visited) {
    std::istringstream in(source);
    std::ostringstream out;
    std::string line;
    while (std::getline(in, line)) {
        const size_t incPos = line.find("#include");
        if (incPos != std::string::npos) {
            const size_t q1 = line.find('"', incPos);
            const size_t q2 = (q1 != std::string::npos) ? line.find('"', q1 + 1) : std::string::npos;
            if (q1 != std::string::npos && q2 != std::string::npos) {
                const std::string filename = line.substr(q1 + 1, q2 - q1 - 1);
                const std::filesystem::path inclPath = dir / filename;
                const std::string key = inclPath.lexically_normal().string();
                if (visited.insert(key).second) {
                    std::ifstream f(inclPath);
                    if (f) {
                        std::ostringstream ss;
                        ss << f.rdbuf();
                        out << resolveIncludes(ss.str(), inclPath.parent_path(), visited);
                    } else {
                        std::cerr << "ERROR::SHADER::INCLUDE_NOT_FOUND: " << key << '\n';
                    }
                }
                continue;
            }
        }
        out << line << '\n';
    }
    return out.str();
}

Shader::Shader(const char *vertexPath, const char *fragmentPath) {
  std::string vertexCode;
  std::string fragmentCode;
  std::ifstream vShaderFile;
  std::ifstream fShaderFile;
  vShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
  fShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
  try {
    vShaderFile.open(vertexPath);
    fShaderFile.open(fragmentPath);
    std::stringstream vShaderStream, fShaderStream;
    vShaderStream << vShaderFile.rdbuf();
    fShaderStream << fShaderFile.rdbuf();
    vShaderFile.close();
    fShaderFile.close();
    std::set<std::string> vVisited, fVisited;
    vertexCode   = resolveIncludes(vShaderStream.str(),
                                   std::filesystem::path(vertexPath).parent_path(), vVisited);
    fragmentCode = resolveIncludes(fShaderStream.str(),
                                   std::filesystem::path(fragmentPath).parent_path(), fVisited);
  } catch (std::ifstream::failure &e) {
    std::cout << "ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ: " << e.what() << std::endl;
  }
  const char *vShaderCode = vertexCode.c_str();
  const char *fShaderCode = fragmentCode.c_str();
  // 2. compile shaders
  unsigned int vertex, fragment;
  // vertex shader
  vertex = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertex, 1, &vShaderCode, NULL);
  glCompileShader(vertex);
  checkCompileErrors(vertex, "VERTEX");
  // fragment Shader
  fragment = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragment, 1, &fShaderCode, NULL);
  glCompileShader(fragment);
  checkCompileErrors(fragment, "FRAGMENT");
  // shader Program
  ID = glCreateProgram();
  glAttachShader(ID, vertex);
  glAttachShader(ID, fragment);
  glLinkProgram(ID);
  checkCompileErrors(ID, "PROGRAM");
  // delete the shaders as they're linked into our program now and no longer
  // necessary
  glDeleteShader(vertex);
  glDeleteShader(fragment);
}
Shader::Shader(const char *vertexPath, const char *geometryPath, const char *fragmentPath) {
  std::string vertexCode, geometryCode, fragmentCode;
  std::ifstream vFile, gFile, fFile;
  vFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
  gFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
  fFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
  try {
    std::stringstream vs, gs, fs;
    vFile.open(vertexPath);   vs << vFile.rdbuf(); vFile.close();
    gFile.open(geometryPath); gs << gFile.rdbuf(); gFile.close();
    fFile.open(fragmentPath); fs << fFile.rdbuf(); fFile.close();
    std::set<std::string> vV, gV, fV;
    vertexCode   = resolveIncludes(vs.str(), std::filesystem::path(vertexPath).parent_path(),   vV);
    geometryCode = resolveIncludes(gs.str(), std::filesystem::path(geometryPath).parent_path(), gV);
    fragmentCode = resolveIncludes(fs.str(), std::filesystem::path(fragmentPath).parent_path(), fV);
  } catch (std::ifstream::failure &e) {
    std::cout << "ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ: " << e.what() << std::endl;
  }
  const char *vCode = vertexCode.c_str();
  const char *gCode = geometryCode.c_str();
  const char *fCode = fragmentCode.c_str();

  unsigned int vertex = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertex, 1, &vCode, NULL);
  glCompileShader(vertex);
  checkCompileErrors(vertex, "VERTEX");

  unsigned int geometry = glCreateShader(GL_GEOMETRY_SHADER);
  glShaderSource(geometry, 1, &gCode, NULL);
  glCompileShader(geometry);
  checkCompileErrors(geometry, "GEOMETRY");

  unsigned int fragment = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragment, 1, &fCode, NULL);
  glCompileShader(fragment);
  checkCompileErrors(fragment, "FRAGMENT");

  ID = glCreateProgram();
  glAttachShader(ID, vertex);
  glAttachShader(ID, geometry);
  glAttachShader(ID, fragment);
  glLinkProgram(ID);
  checkCompileErrors(ID, "PROGRAM");

  glDeleteShader(vertex);
  glDeleteShader(geometry);
  glDeleteShader(fragment);
}

// activate the shader
// ------------------------------------------------------------------------
void Shader::use() { glUseProgram(ID); }
// utility uniform functions
// ------------------------------------------------------------------------
void Shader::setBool(const std::string &name, bool value) const {
  glUniform1i(glGetUniformLocation(ID, name.c_str()), (int)value);
}
// ------------------------------------------------------------------------
void Shader::setInt(const std::string &name, int value) const {
  glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
}
// ------------------------------------------------------------------------
void Shader::setUint(const std::string &name, unsigned int value) const {
  glUniform1ui(glGetUniformLocation(ID, name.c_str()), value);
}
// ------------------------------------------------------------------------
void Shader::setFloat(const std::string &name, float value) const {
  glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
}

void Shader::setVec2(const std::string &name, float v0, float v1) const {
  glUniform2f(glGetUniformLocation(ID, name.c_str()), v0, v1);
}

void Shader::setVec3(const std::string &name, float v0, float v1, float v2) const {
  glUniform3f(glGetUniformLocation(ID, name.c_str()), v0, v1, v2);
}

void Shader::setFloatArray(const std::string &name, const float* data, int count) const {
  glUniform1fv(glGetUniformLocation(ID, name.c_str()), count, data);
}

void Shader::setMat4(const std::string &name, const glm::mat4& m) const {
      glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, glm::value_ptr(m));
}


// utility function for checking shader compilation/linking errors.
// ------------------------------------------------------------------------
void Shader::checkCompileErrors(unsigned int shader, std::string type) {
  int success;
  char infoLog[1024];
  if (type != "PROGRAM") {
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
      glGetShaderInfoLog(shader, 1024, NULL, infoLog);
      std::cout
          << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n"
          << infoLog
          << "\n -- --------------------------------------------------- -- "
          << std::endl;
    }
  } else {
    glGetProgramiv(shader, GL_LINK_STATUS, &success);
    if (!success) {
      glGetProgramInfoLog(shader, 1024, NULL, infoLog);
      std::cout
          << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n"
          << infoLog
          << "\n -- --------------------------------------------------- -- "
          << std::endl;
    }
  }
}