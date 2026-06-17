#version 330 core

flat in vec3 faceNormal;

out vec3 fragFaceNormal;

void main() {
    fragFaceNormal = faceNormal;
}
