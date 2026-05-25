#version 330 core

in vec2  vUV;
in vec3  vPower;
in vec3  vBsdfColor;
in float vCosTheta;

uniform sampler2D kernelTex;
uniform float     h;

out vec4 FragColor;

const float PI = 3.14159265358979;

void main() {
    float k   = texture(kernelTex, vUV).r;
    // Diffuse BRDF: f_r = color / π.
    // Estimator weight: k(r_norm) / h²  (kernel normalization, eq. 2 in paper).
    vec3 radiance = (k / (h * h)) * vPower * (vBsdfColor / PI) * vCosTheta;
    FragColor     = vec4(radiance, 1.0);
}
