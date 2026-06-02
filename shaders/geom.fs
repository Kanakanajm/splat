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

// Camera-side medium attenuation (active when beam Splat mode is on)
uniform int attenuateMedium;

uniform sampler2DArray  depthMap;
uniform usampler2DArray mediumMap;
uniform int   numPeelLayers;
uniform mat4  invViewProj;
uniform vec3  cameraWorldPos;
uniform vec2  resolution;

#define MAX_MEDIA 16
uniform float mediaSigmaT[MAX_MEDIA];

out vec4 FragColor;

#include "medium_stack.glsl"

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

vec3 reconstructWorldPos(float z) {
    vec2 ndc_xy = ((gl_FragCoord.xy - vec2(0.5)) / resolution) * 2.0 - 1.0;
    vec4 clipPos = vec4(ndc_xy, z * 2.0 - 1.0, 1.0);
    vec4 worldPos = invViewProj * clipPos;
    return worldPos.xyz / worldPos.w;
}

float cameraSideTransmittance() {
    float t         = 1.0;
    vec3  lastPos   = cameraWorldPos;
    int   lastMedId = -1;

    for (int i = 0; i < numPeelLayers; ++i) {
        float layerDepth = texelFetch(depthMap,
            ivec3(int(gl_FragCoord.x), int(gl_FragCoord.y), i), 0).r;
        if (layerDepth >= gl_FragCoord.z - 1e-5) break;

        vec3  layerPos = reconstructWorldPos(layerDepth);
        float st = (lastMedId >= 0 && lastMedId < MAX_MEDIA) ? mediaSigmaT[lastMedId] : 0.0;
        t *= exp(-st * length(layerPos - lastPos));

        lastPos = layerPos;

        uint encodedStack = texelFetch(mediumMap,
            ivec3(int(gl_FragCoord.x), int(gl_FragCoord.y), i), 0).r;
        uint stk; int top;
        stack_unpack(encodedStack, stk, top);
        lastMedId = stack_empty(top) ? -1 : int(stack_peek(stk, top));
    }

    // Final segment from last peel layer to this surface fragment
    float st    = (lastMedId >= 0 && lastMedId < MAX_MEDIA) ? mediaSigmaT[lastMedId] : 0.0;
    vec3 fragPos = reconstructWorldPos(gl_FragCoord.z);
    t *= exp(-st * length(fragPos - lastPos));

    return t;
}

void main() {
    vec3 N = normalize(vNormal);

    if (aov_mode == 1) {
        // Diffuse: Lambertian shading with the BSDF's own color as albedo (no specular, no ambient)
        vec3 L = normalize(lightPos - vFragPos);
        float diff = max(dot(N, L), 0.0);
        FragColor = vec4(bsdfColor * diff, 1.0);
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

    if (attenuateMedium != 0) {
        FragColor.rgb *= cameraSideTransmittance();
    }
}
