#version 330 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2DArray colorTextures;
uniform int layer;

void main()
{
    FragColor = texture(colorTextures, vec3(TexCoord, float(layer)));
}
