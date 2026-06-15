#version 330 core

in float vMediumId;
in float vT;
in float vUt;
in float vLength;
in float vSigmaT;
in vec3  vPower;
in vec3  vBeamDir;

uniform float beamRadius;
uniform float exposure;

uniform sampler2DArray  depthMap;
uniform usampler2DArray mediumMap;
uniform int   numPeelLayers;
uniform mat4  invViewProj;
uniform vec3  cameraWorldPos;
uniform vec2  resolution;

#define MAX_MEDIA 16
uniform float mediaSigmaT[MAX_MEDIA];
uniform float mediaSigmaS[MAX_MEDIA];

uniform vec3 cameraDir;

out vec4 FragColor;

#include "medium_stack.glsl"

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

    float st     = (lastMedId >= 0 && lastMedId < MAX_MEDIA) ? mediaSigmaT[lastMedId] : 0.0;
    vec3 fragPos = reconstructWorldPos(gl_FragCoord.z);
    t *= exp(-st * length(fragPos - lastPos));

    return t;
}

void main() {
    float Tr_beam = exp(-vSigmaT * vT * vLength);
    float Tr_cam  = cameraSideTransmittance();

    const float INV_4PI = 0.07957747154594767;

    float cos_wv = abs(dot(normalize(cameraDir), vBeamDir));
    float sin_wv = max(sqrt(max(0.0, 1.0 - cos_wv * cos_wv)), 1e-4);

    float u_n = vUt / beamRadius;
    // float k_r = (3.0 / (4.0 * beamRadius)) * max(0.0, 1.0 - u_n * u_n);
    float k_r = 1.0 / (2.0 * beamRadius);
    int   medId   = int(vMediumId);
    float sigma_s = (medId >= 0 && medId < MAX_MEDIA) ? mediaSigmaS[medId] : 0.0;

    vec3 radiance = (exposure * k_r * sigma_s * Tr_cam * INV_4PI / sin_wv) * vPower;
    FragColor = vec4(radiance, 1.0);
}
