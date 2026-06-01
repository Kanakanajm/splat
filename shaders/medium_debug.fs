#version 330 core

out vec4 FragColor;

in vec2 TexCoord;

uniform usampler2DArray mediumTextures;
uniform int layer;

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
    uint encoded = texture(mediumTextures, vec3(TexCoord, layer)).r;
    uint stack;
    int  top;
    stack_unpack(encoded, stack, top);
    int mediumId = stack_empty(top) ? -1 : int(stack_peek(stack, top));

    FragColor = vec4(palette(mediumId), 1.0);
}
