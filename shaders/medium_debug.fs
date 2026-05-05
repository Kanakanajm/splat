#version 330 core

out vec4 FragColor;

in vec2 TexCoord;

uniform isampler2D mediumTex;

vec3 palette(int mediumId)
{
    int id = abs(mediumId);
    if (id == 1) return vec3(0.95, 0.35, 0.35);
    if (id == 2) return vec3(0.30, 0.85, 0.45);
    if (id == 3) return vec3(0.25, 0.55, 0.95);
    if (id == 4) return vec3(0.90, 0.80, 0.30);
    return vec3(0.55);
}

void main()
{
    int signedId = texture(mediumTex, TexCoord).r;
    if (signedId == 0) {
        FragColor = vec4(0.08, 0.08, 0.08, 1.0);
        return;
    }

    vec3 color = palette(signedId);
    if (signedId < 0) {
        color *= 0.45;
    }

    FragColor = vec4(color, 1.0);
}
