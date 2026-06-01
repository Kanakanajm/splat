#version 330 core

out vec4 FragColor;

uniform sampler2DArray depthMap;
uniform usampler2DArray mediumMap;
uniform int numLayers;

#include "medium_stack.glsl"

vec3 palette(int id)
{
    if (id == 1) return vec3(0.95, 0.35, 0.35);
    if (id == 2) return vec3(0.30, 0.85, 0.45);
    if (id == 3) return vec3(0.25, 0.55, 0.95);
    if (id == 4) return vec3(0.90, 0.80, 0.30);
    if (id == -1) return vec3(0.08, 0.08, 0.08); // vacuum
    return vec3(0.55); // bad!
}

void main()
{
    uint lastPacked = 0u;
    for (int i = 0; i < numLayers; ++i)
    {
        float depthValue = texelFetch(depthMap, ivec3(int(gl_FragCoord.x), int(gl_FragCoord.y), i), 0).r;
        if (depthValue >= gl_FragCoord.z + 0.00001)
            break;
        lastPacked = texelFetch(mediumMap, ivec3(int(gl_FragCoord.x), int(gl_FragCoord.y), i), 0).r;
    }

    uint stack;
    int  top;
    stack_unpack(lastPacked, stack, top);
    int lastMediumId = stack_empty(top) ? -1 : int(stack_peek(stack, top));

    FragColor = vec4(palette(lastMediumId), 0.6);
}
