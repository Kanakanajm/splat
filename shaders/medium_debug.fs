#version 330 core
#extension GL_ARB_gpu_shader5 : require // for findMSB

out vec4 FragColor;

in vec2 TexCoord;

uniform usampler2DArray mediumTextures;
uniform int layer;

int myFindMSB(uint mask) {
    if (mask == 0u) return -1;
    int bit = 0;
    if ((mask & 0xFFFF0000u) != 0u) { bit += 16; mask >>= 16; }
    if ((mask & 0x0000FF00u) != 0u) { bit +=  8; mask >>=  8; }
    if ((mask & 0x000000F0u) != 0u) { bit +=  4; mask >>=  4; }
    if ((mask & 0x0000000Cu) != 0u) { bit +=  2; mask >>=  2; }
    if ((mask & 0x00000002u) != 0u) { bit +=  1;              }
    return bit;
}

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
    uint mask = texture(mediumTextures, vec3(TexCoord, layer)).r;
    int mediumId = myFindMSB(mask);
    vec3 color = palette(mediumId);

    FragColor = vec4(color, 1.0);
}
