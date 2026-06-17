#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

uniform mat4 view;
uniform mat4 projection;

flat out vec3 faceNormal;

void main() {
    gl_Position = projection * view * vec4(aPos, 1.0);
    faceNormal  = aNormal;  // world-space (model = identity)
}
