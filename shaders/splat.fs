#version 330 core

in vec2  vUV;
in vec3  vPower;
in vec3  vBsdfColor;
in float vCosTheta;
in vec3  vNormal;

uniform sampler2D kernelTex;
uniform sampler2D faceNormalTex;
uniform float     h;
uniform float     exposure;
uniform int       aov_mode;       // 0=Radiance, 1=Wireframe, 2=Normal
uniform int       useFaceNormalTest;  // 1 = discard fragments whose face normal diverges

out vec4 FragColor;

const float PI = 3.14159265358979;

void main() {
    // if (useFaceNormalTest != 0) {
    //     vec3 visibleNormal = texelFetch(faceNormalTex, ivec2(gl_FragCoord.xy), 0).rgb;
    //     // Both normals are flat face normals: same face gives dot = ±1 exactly.
    //     // Threshold of 0.1 (cos 84°) allows splats to cross adjacent faces on curved
    //     // surfaces (sphere edges are typically <30° apart) while still rejecting
    //     // perpendicular cross-surface bleeding (box wall/floor, dot = 0).
    //     if (abs(dot(visibleNormal, vNormal)) < 0.1) discard;
    // }

    // vec3 visibleNormal = texelFetch(faceNormalTex, ivec2(gl_FragCoord.xy), 0).rgb;
    // if (abs(dot(visibleNormal, vNormal)) < 0.9) discard;


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
    // k zeros out fragments outside the disk (triangle corners), ensuring ∫K dA = 1.
    vec3 radiance = (k / (h * h * PI)) * vPower * (vBsdfColor / PI);
    FragColor     = vec4(radiance * exposure, 1.0);
}
