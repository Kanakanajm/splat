#version 330 core

in vec2 vUV;

uniform sampler2D hdrTex;

out vec4 FragColor;

void main() {
    FragColor = vec4(texture(hdrTex, vUV).rgb, 1.0);
}
