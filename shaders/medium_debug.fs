#version 330 core
#extension GL_ARB_gpu_shader5 : enable

out vec4 FragColor;

in vec2 TexCoord;

uniform usampler2DArray mediumTextures;
uniform int layer;

vec3 palette(int id)
{
    if (id == 1) return vec3(0.95, 0.35, 0.35); // red
    if (id == 2) return vec3(0.30, 0.85, 0.45); // green
    if (id == 3) return vec3(0.25, 0.55, 0.95); // blue
    if (id == 4) return vec3(0.90, 0.80, 0.30); // yellow
    if (id == -1) return vec3(0.08, 0.08, 0.08); // vacuum
    return vec3(0.55); // bad!
}

void main()
{
    uint mask = texture(mediumTextures, vec3(TexCoord, layer)).r;
    int mediumId = findMSB(mask);
    vec3 color = palette(mediumId);

    FragColor = vec4(color, 1.0);
}
