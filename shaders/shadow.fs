#version 330 core
in  vec4  vFragPos;

uniform vec3  lightPos;
uniform float farPlane;

void main() {
    gl_FragDepth = length(vFragPos.xyz - lightPos) / farPlane;
}
