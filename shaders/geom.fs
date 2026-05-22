#version 330 core
in vec3 vNormal;
in vec3 vFragPos;

uniform int instanceId;
uniform int aov_mode;   // 0=Diffuse(flat color)  1=Normal  2=Depth  3=Backface

uniform vec3  cameraPos;
uniform float nearPlane;
uniform float farPlane;

out vec4 FragColor;

vec3 instance_color(int id) {
    vec3 palette[6] = vec3[6](
        vec3(0.90, 0.30, 0.30),
        vec3(0.30, 0.80, 0.40),
        vec3(0.30, 0.50, 0.90),
        vec3(0.90, 0.80, 0.30),
        vec3(0.70, 0.40, 0.85),
        vec3(0.30, 0.80, 0.85)
    );
    return palette[id % 6];
}

void main() {
    vec3 N = normalize(vNormal);

    // aov_mode matches ViewState::GeomAov: 0=None 1=Diffuse 2=Normal 3=Depth 4=Backface
    if (aov_mode == 2) {
        // Normal: map [-1,1] to [0,1]
        FragColor = vec4(N * 0.5 + 0.5, 1.0);
    } else if (aov_mode == 3) {
        // Depth: linear distance remapped to [0,1]
        float dist = length(vFragPos - cameraPos);
        float t = clamp((dist - nearPlane) / (farPlane - nearPlane), 0.0, 1.0);
        FragColor = vec4(vec3(t), 1.0);
    } else if (aov_mode == 4) {
        // Backface: green = front, red = back
        vec3 viewDir = normalize(cameraPos - vFragPos);
        float facing = dot(N, viewDir);
        FragColor = facing >= 0.0 ? vec4(0.3, 0.85, 0.3, 1.0)
                                  : vec4(0.9, 0.2, 0.2, 1.0);
    } else {
        // None / Diffuse: flat instance color (no material system yet)
        FragColor = vec4(instance_color(instanceId), 1.0);
    }
}
