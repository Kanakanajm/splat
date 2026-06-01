#version 330 core

in vec2 TexCoord;
layout (location = 0) out vec4 FragColor;
layout (location = 1) out uint FragMediumId;

uniform sampler2D texture1;
uniform sampler2D texture2;
uniform int mediumId;

#include "medium_stack.glsl"

void main() {
    FragColor = mix(texture(texture1, TexCoord), texture(texture2, TexCoord), 0.2);

    // Layer 0: ray starts in vacuum.  Push mediumId on entry (front face).
    // TODO: handle camera-inside-medium by pre-pushing on back face.
    uint stack = 0u;
    int  top   = 0;
    if (gl_FrontFacing) {
        stack_push(stack, top, uint(mediumId));
    }
    FragMediumId = stack_pack(stack, top);
}
