#version 330 core

// Camera-side transmittance (stack-based depth peel maps)
uniform sampler2DArray  depthMap;
uniform usampler2DArray mediumMap;
uniform int   numPeelLayers;
uniform mat4  invViewProj;
uniform vec3  cameraWorldPos;
uniform vec2  resolution;

#define MAX_MEDIA 16
uniform float mediaSigmaT[MAX_MEDIA];

uniform vec3 bgColor;  // background radiance (L_bg)

out vec4 FragColor;

#include "medium_stack.glsl"

vec3 reconstructWorldPos(float z) {
    vec2 ndc_xy = (gl_FragCoord.xy / resolution) * 2.0 - 1.0;
    float z_ndc = z * 2.0 - 1.0;
    vec4 clipPos = vec4(ndc_xy, z_ndc, 1.0);
    vec4 worldPos = invViewProj * clipPos;
    return worldPos.xyz / worldPos.w;
}
// Transmittance from camera to gl_FragCoord.z using the stack-based peel maps.
float cameraSideTransmittance() {
    int lastMediumId = -1;
    vec3 lastDepthWorld = cameraWorldPos;
    float transmittance = 1.0;


    for (int i = 0; i < numPeelLayers; ++i)
    {
        float depthValue = texelFetch(depthMap, ivec3(int(gl_FragCoord.x), int(gl_FragCoord.y), i), 0).r;
        vec3 depthWorld = reconstructWorldPos(depthValue);
        float st = (lastMediumId >= 0 && lastMediumId < MAX_MEDIA) ? mediaSigmaT[lastMediumId] : 0.0;
        
        if (depthValue >= gl_FragCoord.z + 0.00001)
        {
            vec3 fragWorld = reconstructWorldPos(gl_FragCoord.z);
            transmittance *= exp(-st * length(fragWorld - lastDepthWorld));
            break;
        }

        transmittance *= exp(-st * length(depthWorld - lastDepthWorld));

        lastDepthWorld = depthWorld;

        uint encoded = texelFetch(mediumMap, ivec3(int(gl_FragCoord.x), int(gl_FragCoord.y), i), 0).r;
        uint mstack;
        int  mtop;
        stack_unpack(encoded, mstack, mtop);
        lastMediumId = stack_empty(mtop) ? -1 : int(stack_peek(mstack, mtop));
    }

    return transmittance;
}

void main() {
    float T = cameraSideTransmittance();
    FragColor = vec4(bgColor * T, 1.0);
}
