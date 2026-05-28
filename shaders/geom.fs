#version 330 core
in vec3 vNormal;
in vec3 vFragPos;

uniform int instanceId;
uniform int aov_mode;   // matches ViewState::GeomAov: 0=None 1=Diffuse 2=Normal 3=Depth 4=Backface

uniform vec3  bsdfColor;
uniform vec3  cameraPos;
uniform vec3  lightPos;
uniform float nearPlane;
uniform float farPlane;

uniform samplerCube shadowMap;
uniform float       shadowFarPlane;
uniform bool        useShadow;

out vec4 FragColor;

float shadowFactor(vec3 fragPos) {
    vec3  fragToLight  = fragPos - lightPos;
    float closestDepth = texture(shadowMap, fragToLight).r * shadowFarPlane;
    float currentDepth = length(fragToLight);
    float bias = 0.05;
    return currentDepth - bias > closestDepth ? 0.0 : 1.0;
}

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

    if (aov_mode == 1) {
        // Diffuse: Lambertian shading with the BSDF's own color as albedo (no specular, no ambient)
        vec3 L = normalize(lightPos - vFragPos);
        float diff   = max(dot(N, L), 0.0);
        float shadow = useShadow ? shadowFactor(vFragPos) : 1.0;
        FragColor = vec4(bsdfColor * diff * shadow, 1.0);
    } else if (aov_mode == 2) {
        // Normal: map [-1,1] to [0,1] as RGB
        FragColor = vec4(N * 0.5 + 0.5, 1.0);
    } else if (aov_mode == 3) {
        // Depth: linear distance from camera, near=black, far=white
        float dist = length(vFragPos - cameraPos);
        float t = clamp((dist - nearPlane) / (farPlane - nearPlane), 0.0, 1.0);
        FragColor = vec4(vec3(t), 1.0);
    } else if (aov_mode == 4) {
        // Backface: green = front-facing, red = back-facing
        vec3 viewDir = normalize(cameraPos - vFragPos);
        FragColor = dot(N, viewDir) >= 0.0 ? vec4(0.3, 0.85, 0.3, 1.0)
                                           : vec4(0.9, 0.2,  0.2, 1.0);
    } else {
        // None: flat instance color (rendered as wireframe via polygon mode)
        FragColor = vec4(instance_color(instanceId), 1.0);
    }
}
