#version 330 core

in vec2  vUV;
in vec3  vPower;
in vec3  vBsdfColor;
in float vCosTheta;
in vec3  vNormal;

uniform sampler2D kernelTex;
uniform float     h;
uniform float     exposure;
uniform int       aov_mode;  // 0=Radiance, 1=Wireframe, 2=Normal

out vec4 FragColor;

const float PI = 3.14159265358979;

void main() {
    if (aov_mode == 1) {
        // Wireframe: flat yellow-white so lines are visible on dark background.
        FragColor = vec4(1.0, 0.9, 0.4, 1.0);
        return;
    }
    if (aov_mode == 2) {
        // Normal: world-space normal mapped to [0,1].
        FragColor = vec4(vNormal * 0.5 + 0.5, 1.0);
        return;
    }
    // aov_mode == 0: Radiance
    float k   = texture(kernelTex, vUV).r;
    // Irradiance estimate: Σ Φ_k · K(r/h) / h².  Outgoing radiance = f_r · E.
    // vPower is actual photon flux (W), so the incoming cosine is already baked
    // into the photon density — do NOT multiply by cosTheta here.
    vec3 radiance = (k / (h * h)) * vPower * (vBsdfColor / PI);
    FragColor     = vec4(radiance * exposure, 1.0);
}
