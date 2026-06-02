#version 330 core
layout(location = 0) out uint FragMediumId;

uniform int mediumId;

#include "medium_stack.glsl"

void main()
{
    uint stack = 0u;
    int  top   = 0;
    if (gl_FrontFacing) {
        stack_push(stack, top, uint(mediumId));
    }
    // back-facing on init: camera is assumed outside all media, stack stays empty
    FragMediumId = stack_pack(stack, top);
}
