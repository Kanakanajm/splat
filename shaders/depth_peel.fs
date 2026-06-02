#version 330 core
layout(location = 0) out uint FragMediumId;

uniform sampler2DArray  previousDepths;
uniform usampler2DArray previousMedia;
uniform int   previousLayerIdx;
uniform float peelEpsilon;
uniform int   mediumId;

#include "medium_stack.glsl"

void main()
{
    vec2  screenUv = gl_FragCoord.xy / vec2(textureSize(previousDepths, 0).xy);
    float prevDepth = texture(previousDepths, vec3(screenUv, float(previousLayerIdx))).r;

    if (gl_FragCoord.z <= prevDepth + peelEpsilon)
        discard;

    uint prevPacked = texture(previousMedia, vec3(screenUv, float(previousLayerIdx))).r;
    uint stack;
    int  top;
    stack_unpack(prevPacked, stack, top);

    if (gl_FrontFacing) {
        stack_push(stack, top, uint(mediumId));
    } else if (!stack_empty(top)) {
        stack_pop(stack, top);
    }

    FragMediumId = stack_pack(stack, top);
}
