#include "shader.hpp"
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>

// Recursively replace #include "file" directives with the referenced file's
// contents.  'dir' is the directory of the file currently being processed so
// that relative paths resolve correctly.  'visited' prevents the same file
// from being inlined more than once (guards against circular includes).
static std::string resolveIncludes(const std::string &source,
                                   const std::filesystem::path &dir,
                                   std::set<std::string> &visited) {
    std::istringstream in(source);
    std::ostringstream out;
    std::string line;
    while (std::getline(in, line)) {
        const size_t incPos = line.find("#include");
        if (incPos != std::string::npos) {
            const size_t q1 = line.find('"', incPos);
            const size_t q2 = (q1 != std::string::npos)
                                   ? line.find('"', q1 + 1)
                                   : std::string::npos;
            if (q1 != std::string::npos && q2 != std::string::npos) {
                const std::string filename = line.substr(q1 + 1, q2 - q1 - 1);
                const std::filesystem::path inclPath = dir / filename;
                const std::string key = inclPath.lexically_normal().string();

                if (visited.insert(key).second) {
                    // First time seeing this file — inline it
                    std::ifstream f(inclPath);
                    if (f) {
                        std::ostringstream ss;
                        ss << f.rdbuf();
                        out << resolveIncludes(ss.str(),
                                               inclPath.parent_path(), visited);
                    } else {
                        std::cerr << "ERROR::SHADER::INCLUDE_NOT_FOUND: "
                                  << key << '\n';
                    }
                }
                // Either way, consume the #include line itself
                continue;
            }
        }
        out << line << '\n';
    }
    return out.str();
}

Shader::Shader(const char *vertexPath, const char *fragmentPath) {
  this->vertexPath = vertexPath;
  // 1. retrieve the vertex/fragment source code from filePath
  std::string vertexCode;
  std::string fragmentCode;
  std::ifstream vShaderFile;
  std::ifstream fShaderFile;
  // ensure ifstream objects can throw exceptions:
  vShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
  fShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
  try {
    // open files
    vShaderFile.open(vertexPath);
    fShaderFile.open(fragmentPath);
    std::stringstream vShaderStream, fShaderStream;
    // read file's buffer contents into streams
    vShaderStream << vShaderFile.rdbuf();
    fShaderStream << fShaderFile.rdbuf();
    vShaderFile.close();
    fShaderFile.close();
    std::set<std::string> vVisited, fVisited;
    vertexCode   = resolveIncludes(vShaderStream.str(),
                                   std::filesystem::path(vertexPath).parent_path(),
                                   vVisited);
    fragmentCode = resolveIncludes(fShaderStream.str(),
                                   std::filesystem::path(fragmentPath).parent_path(),
                                   fVisited);
  } catch (std::ifstream::failure &e) {
    std::cout << "ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ: " << e.what()
              << std::endl;
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
void Shader::setFloat(const std::string &name, float value) const {
  glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
}

void Shader::setVec3(const std::string &name, float v0, float v1, float v2) const {
  glUniform3f(glGetUniformLocation(ID, name.c_str()), v0, v1, v2);
}

void Shader::setVec2(const std::string &name, float v0, float v1) const {
  glUniform2f(glGetUniformLocation(ID, name.c_str()), v0, v1);
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
          << "ERROR::SHADER_COMPILATION_ERROR in "<< vertexPath <<" of type: " << type << "\n"
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