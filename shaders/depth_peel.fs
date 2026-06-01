#version 330 core

in vec2 TexCoord;
layout (location = 0) out vec4 FragColor;
layout (location = 1) out uint FragMediumId;

uniform sampler2D texture1;
uniform sampler2D texture2;
uniform sampler2DArray previousDepths;
uniform usampler2DArray previousMedia;
uniform int previousLayerIdx;
uniform float peelEpsilon;
uniform int mediumId;

#include "medium_stack.glsl"

void main()
{
    vec2 screenUv = gl_FragCoord.xy / vec2(textureSize(previousDepths, 0));
    float previousLayerDepth = texture(previousDepths, vec3(screenUv, float(previousLayerIdx))).r;
    float currentDepth = gl_FragCoord.z;

    if (currentDepth <= previousLayerDepth + peelEpsilon) {
        discard;
    }

    FragColor = mix(texture(texture1, TexCoord), texture(texture2, TexCoord), 0.2);

    // Unpack the medium stack from the previous peel layer
    uint prevPacked = texture(previousMedia, vec3(screenUv, float(previousLayerIdx))).r;
    uint stack;
    int  top;
    stack_unpack(prevPacked, stack, top);

    if (gl_FrontFacing) {
        stack_push(stack, top, uint(mediumId)); // entering medium
    } else if (!stack_empty(top)) {
        stack_pop(stack, top);                  // exiting medium
    }

    FragMediumId = stack_pack(stack, top);
}
